// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Texture Distortion and Texture Density: see TextureMetrics.h and docs/TEXTURE_METRICS.md. The measures are a port of
// compute_triangle_measures, material_references, compute_relative_density, uv_islands and area_distortion in the FSS
// add-on's uv_heatmap.py; a change there is a change here.

#include "imgui/TextureMetrics.h"
#include "imgui/UiProbe.h"

#include "core/Logger.h"
#include "gltf/Accessor.h"
#include "gltf/BufferView.h"
#include "gltf/Gltf.h"
#include "gltf/GltfUtils.h"
#include "gltf/Image.h"
#include "gltf/Material.h"
#include "gltf/Mesh.h"
#include "gltf/Node.h"
#include "gltf/mesh/Primitive.h"
#include "gltf/Scene.h"
#include "gltf/Skin.h"
#include "gltf/Texture.h"
#include "renderer/Mesh.h"
#include "renderer/gltf/GltfRenderLayer.h"

#include <glm/glm.hpp>
#include <glm/gtc/packing.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <numeric>
#include <unordered_map>
#include <utility>

namespace ux3d::imgui {

namespace {

namespace gltf = ux3d::gltf;

constexpr double kDegeneracyRelative = 1e-12; // compute_triangle_measures' degeneracy_relative
constexpr double kAreaTop = 2.0; // AREA_TOP
constexpr GLsizei kTableWidth = 4096;

// One float array per accessor: an MSFS package shares vertex accessors between many primitives.
using Floats = std::shared_ptr<const std::vector<float>>;

float readComponent(const uint8_t* p, gltf::ComponentType type, bool normalized, bool halfFloat)
{
    switch (type) {
    case gltf::ComponentType::Float: {
        float v;
        std::memcpy(&v, p, sizeof(v));
        return v;
    }
    case gltf::ComponentType::UnsignedByte:
        return normalized ? *p / 255.0f : static_cast<float>(*p);
    case gltf::ComponentType::Byte: {
        const auto v = static_cast<float>(static_cast<int8_t>(*p));
        return normalized ? std::max(v / 127.0f, -1.0f) : v;
    }
    case gltf::ComponentType::UnsignedShort: {
        uint16_t v;
        std::memcpy(&v, p, sizeof(v));
        return halfFloat ? glm::unpackHalf1x16(v) : normalized ? v / 65535.0f : static_cast<float>(v);
    }
    case gltf::ComponentType::Short: {
        int16_t v;
        std::memcpy(&v, p, sizeof(v));
        if (halfFloat) {
            return glm::unpackHalf1x16(static_cast<uint16_t>(v));
        }
        return normalized ? std::max(v / 32767.0f, -1.0f) : static_cast<float>(v);
    }
    default:
        return 0.0f;
    }
}

class AccessorCache {
public:
    // components: how many are read per element (a packaged VEC4 NORMAL-style padding is skipped by the stride).
    // halfFloat: A1.9, packaged TEXCOORD_n stored as half-floats in SHORT / USHORT.
    Floats read(const gltf::Accessor* accessor, uint32_t components, bool halfFloat)
    {
        if (accessor == nullptr) {
            return nullptr;
        }
        const auto it = m_cache.find(accessor);
        if (it != m_cache.end()) {
            return it->second;
        }
        Floats result = decode(*accessor, components, halfFloat);
        m_cache.emplace(accessor, result);
        return result;
    }

private:
    static Floats decode(const gltf::Accessor& accessor, uint32_t components, bool halfFloat)
    {
        const uint32_t stored = accessor.getComponentCountFrom(accessor.getType());
        if (stored < components || accessor.getCount() == 0) {
            return nullptr;
        }
        const size_t componentSize = accessor.getByteLengthFrom(accessor.getComponentType());
        const size_t elementSize = accessor.getByteLengthFrom(accessor.getComponentType(), accessor.getType(), false);
        if (componentSize == 0 || elementSize == 0) {
            return nullptr;
        }
        core::Slice<uint8_t> sparse;
        core::MemoryRange<const uint8_t> range;
        size_t stride = elementSize;
        if (accessor.getBufferView() != nullptr) {
            stride = accessor.getBufferView()->getByteStride().value_or(static_cast<uint32_t>(elementSize));
        }
        if (accessor.getSparse().has_value()) {
            if (!accessor.getSparseDataAs(sparse)) {
                return nullptr;
            }
            const auto bytes = sparse.getMemoryRange();
            range = core::makeRange(static_cast<const uint8_t*>(bytes.data()), bytes.size());
        } else if (!accessor.getData(range)) {
            return nullptr;
        }
        const size_t count = accessor.getCount();
        if (range.empty() || stride * (count - 1) + elementSize > range.size()) {
            return nullptr;
        }
        auto out = std::make_shared<std::vector<float>>(count * components);
        const bool normalized = accessor.isNormalized();
        const auto type = accessor.getComponentType();
        for (size_t i = 0; i < count; ++i) {
            const uint8_t* element = range.data() + i * stride;
            for (uint32_t c = 0; c < components; ++c) {
                (*out)[i * components + c] = readComponent(element + c * componentSize, type, normalized, halfFloat);
            }
        }
        return out;
    }

    std::unordered_map<const gltf::Accessor*, Floats> m_cache;
};

// The texture a material's density is read from. Only its UV set, transform and pixel size matter: the colours are
// relative to the material's own median. The first present of the slots an artist paints on one atlas.
const gltf::TextureInfo* densityTexture(const gltf::Material* material, std::string& slot)
{
    if (material == nullptr) {
        return nullptr;
    }
    const auto usable = [](const std::optional<gltf::TextureInfo>& info) { return info.has_value() && info->getTexture() != nullptr; };
    const auto& mr = material->getPbrMetallicRoughness();
    if (mr.has_value() && usable(mr->getBaseColorTextureInfo())) {
        slot = "baseColorTexture";
        return &*mr->getBaseColorTextureInfo();
    }
    const auto& sg = material->getPbrSpecularGlossiness();
    if (sg.has_value() && usable(sg->getDiffuseTextureInfo())) {
        slot = "diffuseTexture";
        return &*sg->getDiffuseTextureInfo();
    }
    if (usable(material->getNormalTextureInfo())) {
        slot = "normalTexture";
        return &*material->getNormalTextureInfo();
    }
    if (mr.has_value() && usable(mr->getMetallicRoughnessTextureInfo())) {
        slot = "metallicRoughnessTexture";
        return &*mr->getMetallicRoughnessTextureInfo();
    }
    if (usable(material->getOcclusionTextureInfo())) {
        slot = "occlusionTexture";
        return &*material->getOcclusionTextureInfo();
    }
    if (usable(material->getEmissiveTextureInfo())) {
        slot = "emissiveTexture";
        return &*material->getEmissiveTextureInfo();
    }
    return nullptr;
}

// How a material's texture lies on the surface: A5.2's main-UV transform (UV set 0) then KHR_texture_transform, the
// order textures.glsl applies them.
struct UvMapping {
    uint32_t uvSet = 0;
    bool asobo = false;
    glm::dvec2 tiling { 1.0 };
    glm::dvec2 rotation { 1.0, 0.0 };
    glm::dvec2 offset { 0.0 };
    bool khr = false;
    glm::dmat3 khrMatrix { 1.0 };

    glm::dvec2 apply(double u, double v) const
    {
        glm::dvec2 uv(u, v);
        if (asobo) {
            const glm::dvec2 p = (uv - 0.5) * tiling;
            uv = glm::dvec2(rotation.x * p.x - rotation.y * p.y, rotation.y * p.x + rotation.x * p.y) + 0.5 + offset;
        }
        if (khr) {
            uv = glm::dvec2(khrMatrix * glm::dvec3(uv, 1.0));
        }
        return uv;
    }
};

// The transforms are affine: three points give the map (uv_grid.glsl).
struct UvAffine {
    std::array<float, 3> x;
    std::array<float, 3> y;
};

UvAffine uvAffine(const UvMapping& mapping)
{
    const glm::dvec2 origin = mapping.apply(0.0, 0.0);
    const glm::dvec2 alongU = mapping.apply(1.0, 0.0) - origin;
    const glm::dvec2 alongV = mapping.apply(0.0, 1.0) - origin;
    return { { static_cast<float>(alongU.x), static_cast<float>(alongV.x), static_cast<float>(origin.x) },
             { static_cast<float>(alongU.y), static_cast<float>(alongV.y), static_cast<float>(origin.y) } };
}

UvMapping uvMapping(const gltf::Material* material, const gltf::TextureInfo* texture, bool asoboUvOptions)
{
    UvMapping mapping;
    if (texture != nullptr) {
        mapping.uvSet = texture->getEffectiveTexCoord();
        if (texture->hasTextureTransform()) {
            mapping.khr = true;
            const math::dmat3 m = texture->calculateTransformMatrix();
            for (int c = 0; c < 3; ++c) {
                for (int r = 0; r < 3; ++r) {
                    mapping.khrMatrix[c][r] = m[c][r];
                }
            }
        }
    }
    if (material != nullptr && asoboUvOptions && mapping.uvSet == 0) {
        const auto& extensions = material->getUnknownExtensions();
        const auto entry = extensions.find("ASOBO_material_UV_options");
        if (entry != extensions.end()) {
            const auto json = nlohmann::json::parse(entry->second.c_str(), nullptr, false);
            if (json.is_object()) {
                const auto number = [&json](const char* key, const char* alias, double fallback) {
                    const auto it = json.find(key);
                    if (it != json.end() && it->is_number()) {
                        return it->get<double>();
                    }
                    const auto other = alias != nullptr ? json.find(alias) : json.end();
                    return other != json.end() && other->is_number() ? other->get<double>() : fallback;
                };
                mapping.asobo = true;
                mapping.tiling = glm::dvec2(number("UVTilingU", nullptr, 1.0), number("UVTilingV", nullptr, 1.0));
                const double angle = glm::radians(number("UVRotation", nullptr, 0.0));
                mapping.rotation = glm::dvec2(std::cos(angle), std::sin(angle));
                mapping.offset = glm::dvec2(number("UVOffsetU", "UVOffSetU", 0.0), number("UVOffsetV", "UVOffSetV", 0.0));
            }
        }
    }
    return mapping;
}

// One primitive's triangles in draw order (gl_PrimitiveID indexes them), with vertex indices into its accessors.
struct DecodedPrimitive {
    const gltf::Primitive* primitive = nullptr;
    const gltf::Material* material = nullptr;
    UvMapping mapping;
    Floats positions;
    Floats uvs;
    Floats joints;
    Floats weights;
    std::vector<std::array<uint32_t, 3>> triangles;
    uint32_t firstTriangle = 0; // within the glTF mesh, for the island labels
    bool drawnTriangles = false; // false: points or lines
};

bool readTriangles(const gltf::Primitive& primitive, const ux3d::renderer::Mesh& mesh, size_t vertexCount,
                   std::vector<std::array<uint32_t, 3>>& triangles)
{
    const gltf::Accessor* indexAccessor = primitive.getIndicesAccessor();
    const auto mode = primitive.getPrimitiveMode();
    size_t count = vertexCount;
    std::vector<uint32_t> indices;
    if (indexAccessor != nullptr) {
        core::MemoryRange<const uint8_t> range;
        if (!indexAccessor->getData(range) || range.empty()) {
            return false;
        }
        const size_t size = indexAccessor->getByteLengthFrom(indexAccessor->getComponentType());
        count = mesh.indexCount;
        if (size == 0 || mesh.indexByteOffset + count * size > range.size()) {
            return false;
        }
        indices.resize(count);
        const uint8_t* p = range.data() + mesh.indexByteOffset;
        for (size_t i = 0; i < count; ++i) {
            uint32_t value = 0;
            std::memcpy(&value, p + i * size, size); // little endian, as the file
            indices[i] = value + static_cast<uint32_t>(mesh.baseVertex);
        }
    }
    const auto index = [&indices, indexAccessor](size_t i) {
        return indexAccessor != nullptr ? indices[i] : static_cast<uint32_t>(i);
    };
    triangles.clear();
    if (mode == gltf::PrimitiveMode::Triangles) {
        triangles.reserve(count / 3);
        for (size_t i = 0; i + 2 < count; i += 3) {
            triangles.push_back({ index(i), index(i + 1), index(i + 2) });
        }
    } else if (mode == gltf::PrimitiveMode::TriangleStrip) {
        for (size_t i = 0; i + 2 < count; ++i) {
            triangles.push_back({ index(i), index(i + 1), index(i + 2) });
        }
    } else if (mode == gltf::PrimitiveMode::TriangleFan) {
        for (size_t i = 0; i + 2 < count; ++i) {
            triangles.push_back({ index(0), index(i + 1), index(i + 2) });
        }
    }
    for (const auto& triangle : triangles) {
        for (uint32_t vertex : triangle) {
            if (vertex >= vertexCount) {
                return false;
            }
        }
    }
    return true;
}

uint64_t mixHash(uint64_t h, uint64_t value)
{
    return (h ^ value) * 0x9E3779B97F4A7C15ULL ^ (h >> 29);
}

uint64_t vertexKey(const float* position, const glm::dvec2& uv)
{
    // glTF has no welded topology: a vertex is split at every hard edge, so the corner's position and UV stand for
    // Blender's (vertex, UV) pair; uv_islands rounds the UVs to 1e-6.
    uint32_t bits[3];
    std::memcpy(bits, position, sizeof(bits));
    uint64_t h = 0xCBF29CE484222325ULL;
    h = mixHash(h, bits[0]);
    h = mixHash(h, bits[1]);
    h = mixHash(h, bits[2]);
    h = mixHash(h, static_cast<uint64_t>(std::llround(uv.x * 1e6)));
    return mixHash(h, static_cast<uint64_t>(std::llround(uv.y * 1e6)));
}

// uv_islands: triangles that share an edge with equal UVs at both ends belong together; only edges with exactly two
// faces join, so a non-manifold edge does not weld islands. Across the mesh's primitives, as one Blender object.
std::vector<uint32_t> uvIslands(const std::vector<DecodedPrimitive>& primitives, uint32_t triangleCount, uint32_t& islandCount)
{
    std::vector<std::pair<uint64_t, uint32_t>> edges;
    edges.reserve(static_cast<size_t>(triangleCount) * 3);
    for (const DecodedPrimitive& primitive : primitives) {
        if (primitive.positions == nullptr || primitive.uvs == nullptr) {
            continue;
        }
        const float* positions = primitive.positions->data();
        const float* uvs = primitive.uvs->data();
        uint32_t triangleIndex = primitive.firstTriangle;
        for (const auto& triangle : primitive.triangles) {
            uint64_t corner[3];
            for (int i = 0; i < 3; ++i) {
                const uint32_t v = triangle[i];
                corner[i] = vertexKey(positions + v * 3, primitive.mapping.apply(uvs[v * 2], uvs[v * 2 + 1]));
            }
            for (int i = 0; i < 3; ++i) {
                const uint64_t a = std::min(corner[i], corner[(i + 1) % 3]);
                const uint64_t b = std::max(corner[i], corner[(i + 1) % 3]);
                edges.emplace_back(mixHash(a, b), triangleIndex);
            }
            ++triangleIndex;
        }
    }
    std::sort(edges.begin(), edges.end());
    std::vector<uint32_t> parent(triangleCount);
    std::iota(parent.begin(), parent.end(), 0u);
    const auto find = [&parent](uint32_t x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    };
    for (size_t i = 0; i < edges.size();) {
        size_t end = i + 1;
        while (end < edges.size() && edges[end].first == edges[i].first) {
            ++end;
        }
        if (end - i == 2) {
            const uint32_t a = find(edges[i].second);
            const uint32_t b = find(edges[i + 1].second);
            if (a != b) {
                parent[std::max(a, b)] = std::min(a, b);
            }
        }
        i = end;
    }
    std::vector<uint32_t> labels(triangleCount);
    std::unordered_map<uint32_t, uint32_t> compact;
    for (uint32_t t = 0; t < triangleCount; ++t) {
        const uint32_t root = find(t);
        const auto it = compact.try_emplace(root, static_cast<uint32_t>(compact.size())).first;
        labels[t] = it->second;
    }
    islandCount = static_cast<uint32_t>(compact.size());
    return labels;
}

struct DecodedMesh {
    std::vector<DecodedPrimitive> primitives;
    std::vector<uint32_t> islands;
    uint32_t islandCount = 0;
    uint32_t triangleCount = 0;
};

struct TriangleMeasures {
    double area3 = 0.0;
    double uvArea = 0.0;
    bool valid = false;
    bool degenerate = true;
    double shape = std::numeric_limits<double>::quiet_NaN();
    double rawDensity = std::numeric_limits<double>::quiet_NaN();
};

// compute_triangle_measures. The conformal ratio is the singular value ratio of the affine 3D-to-UV map, solved in
// closed form instead of by numpy's SVD.
TriangleMeasures measureTriangle(const glm::dvec3 p[3], const glm::dvec2 u[3])
{
    TriangleMeasures m;
    const glm::dvec3 e1 = p[1] - p[0];
    const glm::dvec3 e2 = p[2] - p[0];
    const double crossLength = glm::length(glm::cross(e1, e2));
    const double longestSquared = std::max({ glm::dot(e1, e1), glm::dot(e2, e2), glm::dot(e2 - e1, e2 - e1) });
    m.valid = crossLength > kDegeneracyRelative * longestSquared;
    m.area3 = crossLength * 0.5;
    const glm::dvec2 du = u[1] - u[0];
    const glm::dvec2 dv = u[2] - u[0];
    m.uvArea = std::abs(0.5 * (du.x * dv.y - du.y * dv.x));
    m.degenerate = !m.valid || m.uvArea <= 0.0;
    if (m.valid) {
        const double l = glm::length(e1);
        const double x = glm::dot(e1, e2) / l;
        const double h = crossLength / l;
        const glm::dvec2 c0 = du / l;
        const glm::dvec2 c1 = (dv - du * (x / l)) / h;
        const double energy = glm::dot(c0, c0) + glm::dot(c1, c1);
        const double determinant = std::abs(c0.x * c1.y - c0.y * c1.x);
        const double discriminant = std::sqrt(std::max(0.0, energy * energy - 4.0 * determinant * determinant));
        const double sigma1 = std::sqrt((energy + discriminant) * 0.5);
        const double sigma2 = sigma1 > 0.0 ? determinant / sigma1 : 0.0;
        m.shape = (sigma2 > 0.0 && m.uvArea > 0.0) ? sigma1 / sigma2 : std::numeric_limits<double>::infinity();
        m.rawDensity = m.area3 > 0.0 ? m.uvArea / m.area3 : std::numeric_limits<double>::quiet_NaN();
    }
    return m;
}

// area_weighted_median: the value where the cumulative weight first reaches half of the total; 1 without weight.
double areaWeightedMedian(std::vector<std::pair<double, double>>& samples)
{
    double total = 0.0;
    for (const auto& sample : samples) {
        total += sample.second;
    }
    if (samples.empty() || total <= 0.0) {
        return 1.0;
    }
    std::sort(samples.begin(), samples.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    const double cutoff = 0.5 * total;
    double cumulative = 0.0;
    for (const auto& sample : samples) {
        cumulative += sample.second;
        if (cumulative >= cutoff) {
            return sample.first;
        }
    }
    return samples.back().first;
}

// The face's relative density (compute_relative_density) against the default density range.
double relativeDensity(double raw, double reference)
{
    const double normalized = reference > 0.0 ? raw / reference : 1.0;
    if (normalized < TextureMetrics::kFreedomLow) {
        return normalized / TextureMetrics::kFreedomLow;
    }
    if (normalized > TextureMetrics::kFreedomHigh) {
        return normalized / TextureMetrics::kFreedomHigh;
    }
    return 1.0;
}

// One span of the table per node and primitive; the density channel holds the raw value until the references are known.
struct TableSpan {
    size_t first = 0;
    size_t count = 0;
    double weight = 1.0; // EXT_mesh_gpu_instancing: the instances drawn, each measured at the first instance's scale
    const gltf::Material* material = nullptr;
};

void addToHistogram(std::array<float, TextureMetrics::kHistogramBins>& bins, double position, double weight)
{
    const int bin = std::clamp(static_cast<int>(position * TextureMetrics::kHistogramBins), 0, TextureMetrics::kHistogramBins - 1);
    bins[static_cast<size_t>(bin)] += static_cast<float>(weight);
}

void normalize(std::array<float, TextureMetrics::kHistogramBins>& bins, double total)
{
    if (total > 0.0) {
        for (float& bin : bins) {
            bin = static_cast<float>(bin / total);
        }
    }
}

} // namespace

void TextureMetrics::ensureUvMaps(ux3d::renderer::GltfRenderLayer& layer, uint64_t contentStamp)
{
    const bool asobo = layer.asoboOptions.applies("ASOBO_material_UV_options");
    if (m_uvMapsSet && m_uvMapLayer == &layer && m_uvMapStamp == contentStamp && m_uvMapAsobo == asobo
        && m_uvMapGeneration == layer.getSceneGeneration()) {
        return;
    }
    m_uvMapsSet = true;
    m_uvMapLayer = &layer;
    m_uvMapStamp = contentStamp;
    m_uvMapAsobo = asobo;
    m_uvMapGeneration = layer.getSceneGeneration();
    for (const auto& [key, mesh] : layer.m_meshMapping.map) {
        const gltf::Primitive* primitive = std::get<2>(key);
        if (mesh == nullptr || primitive == nullptr) {
            continue;
        }
        std::string slot;
        const gltf::Material* material = primitive->getMaterial();
        const UvMapping mapping = uvMapping(material, densityTexture(material, slot), asobo);
        const bool present = gltf::GltfUtils::getAttributeAccessor(*primitive, "TEXCOORD_" + std::to_string(mapping.uvSet)) != nullptr;
        const UvAffine affine = uvAffine(mapping);
        mesh->setUniform("u_UvHeatmapUvSet", present && mapping.uvSet <= 1 ? static_cast<int>(mapping.uvSet) : -1);
        mesh->setUniform("u_UvHeatmapUvX", ux3d::math::vec3(affine.x[0], affine.x[1], affine.x[2]));
        mesh->setUniform("u_UvHeatmapUvY", ux3d::math::vec3(affine.y[0], affine.y[1], affine.y[2]));
    }
}

void TextureMetrics::release()
{
    if (m_texture != 0) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
    m_built = false;
    m_scene = nullptr;
    m_stats = Stats {};
    m_offsets.clear();
}

void TextureMetrics::applyOffsets(ux3d::renderer::GltfRenderLayer& layer) const
{
    for (const MeshOffset& entry : m_offsets) {
        const auto it = layer.m_meshMapping.map.find(std::tuple(entry.node, entry.mesh, entry.primitive));
        if (it != layer.m_meshMapping.map.end() && it->second != nullptr) {
            it->second->setUniform("u_UvHeatmapOffset", entry.offset);
            it->second->setUniform("u_UvHeatmapUvSet", entry.uvSet);
            it->second->setUniform("u_UvHeatmapUvX", ux3d::math::vec3(entry.uvX[0], entry.uvX[1], entry.uvX[2]));
            it->second->setUniform("u_UvHeatmapUvY", ux3d::math::vec3(entry.uvY[0], entry.uvY[1], entry.uvY[2]));
        }
    }
}

GLuint TextureMetrics::ensure(ux3d::renderer::GltfRenderLayer& layer, const ux3d::gltf::Gltf& model, uint64_t contentStamp)
{
    const bool asoboUvOptions = layer.asoboOptions.applies("ASOBO_material_UV_options");
    if (!m_built || m_scene != layer.getGltfScene() || m_contentStamp != contentStamp || m_asoboUvOptions != asoboUvOptions) {
        m_scene = layer.getGltfScene();
        m_contentStamp = contentStamp;
        m_asoboUvOptions = asoboUvOptions;
        build(layer, model);
        m_sceneGeneration = layer.getSceneGeneration();
        applyOffsets(layer);
    } else if (m_sceneGeneration != layer.getSceneGeneration()) {
        // A view setting synced the renderer: same model, same table.
        m_sceneGeneration = layer.getSceneGeneration();
        applyOffsets(layer);
    }
    return m_texture;
}

void TextureMetrics::build(ux3d::renderer::GltfRenderLayer& layer, const ux3d::gltf::Gltf& model)
{
    const auto start = std::chrono::steady_clock::now();
    if (m_texture != 0) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
    m_stats = Stats {};
    m_offsets.clear();
    m_built = true;
    ++m_builds;
    const gltf::Scene* scene = layer.getGltfScene();
    if (scene == nullptr) {
        return;
    }

    const bool packaged = model.isExtensionDeclared("ASOBO_asset_optimized");
    AccessorCache accessors;
    std::unordered_map<const gltf::Mesh*, DecodedMesh> decodedMeshes;
    std::unordered_map<const gltf::Material*, std::vector<std::pair<double, double>>> samples;
    std::unordered_map<const gltf::Material*, Material> materials;
    std::vector<float> table; // shear, area, density (raw until the references are known), degenerate
    std::vector<TableSpan> spans;

    GLint maxSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize);
    const uint64_t capacity = static_cast<uint64_t>(std::min<GLint>(kTableWidth, maxSize))
        * static_cast<uint64_t>(std::max(maxSize, 1));

    double validArea = 0.0;
    double degenerateArea = 0.0;
    double totalArea = 0.0;
    double shear110 = 0.0;
    double shear125 = 0.0;
    double area110 = 0.0;
    double area125 = 0.0;
    double islandsOverArea = 0.0;
    double islandsArea = 0.0;

    const auto decodeMesh = [&](const gltf::Node& node, const gltf::Mesh& gltfMesh) -> DecodedMesh& {
        const auto found = decodedMeshes.find(&gltfMesh);
        if (found != decodedMeshes.end()) {
            return found->second;
        }
        DecodedMesh& decoded = decodedMeshes[&gltfMesh];
        for (const gltf::Primitive& primitive : gltfMesh.getPrimitives()) {
            DecodedPrimitive item;
            item.primitive = &primitive;
            item.material = primitive.getMaterial();
            const auto meshIt = layer.m_meshMapping.map.find(std::tuple(&node, &gltfMesh, &primitive));
            std::string slot;
            item.mapping = uvMapping(item.material, densityTexture(item.material, slot), m_asoboUvOptions);
            const gltf::Accessor* positionAccessor = gltf::GltfUtils::getAttributeAccessor(primitive, "POSITION");
            const gltf::Accessor* uvAccessor = gltf::GltfUtils::getAttributeAccessor(primitive, "TEXCOORD_" + std::to_string(item.mapping.uvSet));
            const auto mode = primitive.getPrimitiveMode();
            item.drawnTriangles = mode == gltf::PrimitiveMode::Triangles || mode == gltf::PrimitiveMode::TriangleStrip
                || mode == gltf::PrimitiveMode::TriangleFan;
            item.positions = accessors.read(positionAccessor, 3, false);
            if (uvAccessor != nullptr) {
                const auto type = uvAccessor->getComponentType();
                const bool half = packaged && !uvAccessor->isNormalized()
                    && (type == gltf::ComponentType::Short || type == gltf::ComponentType::UnsignedShort);
                item.uvs = accessors.read(uvAccessor, 2, half);
            }
            item.joints = accessors.read(gltf::GltfUtils::getAttributeAccessor(primitive, "JOINTS_0"), 4, false);
            item.weights = accessors.read(gltf::GltfUtils::getAttributeAccessor(primitive, "WEIGHTS_0"), 4, false);
            if (!primitive.getTargets().empty()) {
                ++m_stats.morphedPrimitives;
            }
            if (!item.drawnTriangles) {
                ++m_stats.nonTrianglePrimitives;
            } else if (item.positions != nullptr && meshIt != layer.m_meshMapping.map.end()
                       && meshIt->second != nullptr
                       && !readTriangles(primitive, *meshIt->second, item.positions->size() / 3, item.triangles)) {
                core::logWarning("[TextureMetrics] Unreadable indices in a primitive of '%s'; it shows as no data", gltfMesh.getName().c_str());
                item.triangles.clear();
            }
            item.firstTriangle = decoded.triangleCount;
            decoded.triangleCount += static_cast<uint32_t>(item.triangles.size());
            decoded.primitives.push_back(std::move(item));
        }
        decoded.islands = uvIslands(decoded.primitives, decoded.triangleCount, decoded.islandCount);
        return decoded;
    };

    std::vector<double> islandUv;
    std::vector<double> island3d;
    std::vector<double> islandSum;
    std::vector<double> islandCount;
    std::vector<double> islandFaceArea;
    std::vector<uint8_t> islandMeasured;
    std::vector<TriangleMeasures> measures;

    core::Vector<gltf::Node*> nodes;
    const_cast<gltf::Scene*>(scene)->getNodesRecursively(nodes);
    for (const gltf::Node* node : nodes) {
        const gltf::Mesh* gltfMesh = node != nullptr ? node->getMesh() : nullptr;
        if (gltfMesh == nullptr) {
            continue;
        }
        DecodedMesh& decoded = decodeMesh(*node, *gltfMesh);
        if (decoded.primitives.empty()) {
            continue;
        }

        // Skinning (the node's own transform does not apply, glTF 3.7.3.3): each joint's current world matrix x its
        // inverse bind matrix, as the evaluated mesh in Blender.
        const gltf::Skin* skin = node->getSkin();
        std::vector<glm::dmat4> jointMatrices;
        if (skin != nullptr) {
            jointMatrices.reserve(skin->getJoints().size());
            Floats inverse = accessors.read(skin->getInverseBindMatricesAccessor(), 16, false);
            for (size_t j = 0; j < skin->getJoints().size(); ++j) {
                glm::dmat4 ibm(1.0);
                if (inverse != nullptr && (j + 1) * 16 <= inverse->size()) {
                    ibm = glm::dmat4(glm::make_mat4(inverse->data() + j * 16));
                }
                jointMatrices.push_back(glm::dmat4(glm::mat4(layer.getWorldTransform(skin->getJoints()[j]))) * ibm);
            }
        }

        const auto& islands = decoded.islands;
        measures.assign(decoded.triangleCount, TriangleMeasures {});
        islandUv.assign(decoded.islandCount, 0.0);
        island3d.assign(decoded.islandCount, 0.0);

        struct NodeSpan {
            const DecodedPrimitive* primitive = nullptr;
            ux3d::renderer::Mesh* mesh = nullptr;
            double weight = 1.0;
        };
        std::vector<NodeSpan> nodeSpans;
        bool nodeSkipped = false;
        for (const DecodedPrimitive& primitive : decoded.primitives) {
            const auto meshIt = layer.m_meshMapping.map.find(std::tuple(node, gltfMesh, primitive.primitive));
            if (meshIt == layer.m_meshMapping.map.end() || meshIt->second == nullptr) {
                continue;
            }
            ux3d::renderer::Mesh* mesh = meshIt->second;
            if (primitive.positions == nullptr || primitive.uvs == nullptr) {
                if (primitive.drawnTriangles && primitive.uvs == nullptr) {
                    nodeSkipped = true;
                }
                m_offsets.push_back({ node, gltfMesh, primitive.primitive, 0 });
                continue;
            }
            // The renderer's own world matrix; an EXT_mesh_gpu_instancing node is measured at its first instance.
            glm::dmat4 world(glm::mat4(mesh->getTransform()));
            double weight = 1.0;
            if (mesh->isInstanced() && !node->getGpuInstances().empty()) {
                const math::dmat4 local = gltf::gpu_instancing::localMatrix(node->getGpuInstances().front());
                world = world * glm::make_mat4(&local[0][0]);
                weight = static_cast<double>(mesh->getInstanceCount());
            }
            const bool skinned = skin != nullptr && !jointMatrices.empty() && primitive.joints != nullptr && primitive.weights != nullptr;
            const float* positions = primitive.positions->data();
            const float* uvs = primitive.uvs->data();
            const size_t uvCount = primitive.uvs->size() / 2;
            uint32_t triangleIndex = primitive.firstTriangle;
            for (const auto& triangle : primitive.triangles) {
                glm::dvec3 p[3];
                glm::dvec2 u[3];
                bool ok = true;
                for (int c = 0; c < 3; ++c) {
                    const uint32_t v = triangle[c];
                    if (v >= uvCount) {
                        ok = false;
                        break;
                    }
                    const glm::dvec4 local(positions[v * 3], positions[v * 3 + 1], positions[v * 3 + 2], 1.0);
                    if (skinned) {
                        glm::dmat4 blend(0.0);
                        for (int k = 0; k < 4; ++k) {
                            const auto joint = static_cast<size_t>((*primitive.joints)[v * 4 + k]);
                            if (joint < jointMatrices.size()) {
                                blend += jointMatrices[joint] * static_cast<double>((*primitive.weights)[v * 4 + k]);
                            }
                        }
                        p[c] = glm::dvec3(blend * local);
                    } else {
                        p[c] = glm::dvec3(world * local);
                    }
                    u[c] = primitive.mapping.apply(uvs[v * 2], uvs[v * 2 + 1]);
                }
                TriangleMeasures& m = measures[triangleIndex];
                if (ok) {
                    m = measureTriangle(p, u);
                }
                // area_distortion's ok mask: measurable faces with UV area.
                if (m.valid && !m.degenerate) {
                    islandUv[islands[triangleIndex]] += m.uvArea;
                    island3d[islands[triangleIndex]] += m.area3;
                }
                ++triangleIndex;
            }
            nodeSpans.push_back({ &primitive, mesh, weight });
        }
        if (nodeSkipped && m_stats.skippedNoUvs.size() < 32) {
            m_stats.skippedNoUvs.emplace_back(node->getName().c_str());
        }

        islandSum.assign(decoded.islandCount, 0.0);
        islandCount.assign(decoded.islandCount, 0.0);
        islandFaceArea.assign(decoded.islandCount, 0.0);
        islandMeasured.assign(decoded.islandCount, 0);
        for (const NodeSpan& span : nodeSpans) {
            const DecodedPrimitive& primitive = *span.primitive;
            Material& material = materials[primitive.material];
            auto& materialSamples = samples[primitive.material];
            const size_t triangles = primitive.triangles.size();
            if (table.size() / 4 + triangles > capacity) {
                m_stats.truncatedTriangles += triangles;
                m_offsets.push_back({ node, gltfMesh, primitive.primitive, 0 });
                continue;
            }
            const size_t first = table.size() / 4;
            // The renderer carries UV sets 0 and 1.
            const UvAffine affine = uvAffine(primitive.mapping);
            MeshOffset meshOffset { node, gltfMesh, primitive.primitive, static_cast<int>(first) + 1 };
            meshOffset.uvSet = primitive.mapping.uvSet <= 1 ? static_cast<int>(primitive.mapping.uvSet) : -1;
            meshOffset.uvX = affine.x;
            meshOffset.uvY = affine.y;
            m_offsets.push_back(meshOffset);
            spans.push_back({ first, triangles, span.weight, primitive.material });
            table.resize(table.size() + triangles * 4);
            float* out = table.data() + first * 4;
            uint32_t triangleIndex = primitive.firstTriangle;
            for (size_t t = 0; t < triangles; ++t, ++triangleIndex, out += 4) {
                const TriangleMeasures& m = measures[triangleIndex];
                const uint32_t island = islands[triangleIndex];
                const double weighted = m.area3 * span.weight;
                islandMeasured[island] = 1;
                totalArea += weighted;
                material.triangles += static_cast<uint64_t>(span.weight);
                material.surfaceSquareMetres += weighted;
                m_stats.triangles += static_cast<uint64_t>(span.weight);
                if (m.degenerate) {
                    degenerateArea += weighted;
                    m_stats.degenerateTriangles += static_cast<uint64_t>(span.weight);
                    out[0] = out[1] = out[2] = 1.0f;
                    out[3] = 1.0f;
                    continue;
                }
                // area_distortion: this face's UV area / 3D area against its island's, folded so half and double read 2.
                double area = (m.uvArea / m.area3) / (islandUv[island] / island3d[island]);
                area = area < 1.0 ? 1.0 / area : area;
                islandSum[island] += area;
                islandCount[island] += 1.0;
                islandFaceArea[island] += weighted;
                const double shear = std::isfinite(m.shape) ? m.shape : 1e4;
                out[0] = static_cast<float>(std::min(shear, 1e4));
                out[1] = static_cast<float>(std::min(area, 1e4));
                out[2] = static_cast<float>(m.rawDensity);
                out[3] = 0.0f;
                if (m.area3 > 0.0) {
                    validArea += weighted;
                    if (m.shape > 1.10) shear110 += weighted;
                    if (m.shape > 1.25) shear125 += weighted;
                    if (area > 1.10) area110 += weighted;
                    if (area > 1.25) area125 += weighted;
                    addToHistogram(m_stats.shear, std::clamp(m.shape - 1.0, 0.0, 1.0), weighted);
                    addToHistogram(m_stats.area, std::clamp(area - 1.0, 0.0, 1.0), weighted);
                }
                if (std::isfinite(m.rawDensity)) {
                    materialSamples.emplace_back(m.rawDensity, weighted);
                }
            }
        }
        for (uint32_t island = 0; island < decoded.islandCount; ++island) {
            m_stats.islands += islandMeasured[island]; // not the triangles of a primitive without UVs
            islandsArea += islandFaceArea[island];
            if (islandCount[island] > 0.0 && islandSum[island] / islandCount[island] > kIslandLimit + 1e-9) {
                ++m_stats.islandsOverLimit;
                islandsOverArea += islandFaceArea[island];
            }
        }
    }

    // The references: one texture set per material, over the whole scene (hidden nodes too, so isolating a part does
    // not change them), then each face's density against the default density range.
    const bool ddsDeclared = model.isExtensionDeclared("MSFT_texture_dds");
    for (auto& [gltfMaterial, material] : materials) {
        auto sampleIt = samples.find(gltfMaterial);
        material.referenceUvPerSquareMetre = sampleIt != samples.end() ? areaWeightedMedian(sampleIt->second) : 1.0;
        material.name = gltfMaterial != nullptr ? std::string(gltfMaterial->getName().c_str()) : std::string("(default material)");
        const gltf::TextureInfo* texture = densityTexture(gltfMaterial, material.texture);
        if (texture != nullptr) {
            material.uvSet = texture->getEffectiveTexCoord();
            const gltf::Image* image = ux3d::renderer::GltfRenderLayer::pickImageForTexture(texture->getTexture(), ddsDeclared);
            if (image != nullptr && !image->payloadOmitted()) {
                material.textureWidth = image->getData().getWidth();
                material.textureHeight = image->getData().getHeight();
            }
            if (material.textureWidth > 0 && material.textureHeight > 0) {
                material.pixelsPerMetre = std::sqrt(material.referenceUvPerSquareMetre
                                                    * static_cast<double>(material.textureWidth)
                                                    * static_cast<double>(material.textureHeight));
            }
        }
    }
    double under = 0.0;
    double over = 0.0;
    for (const TableSpan& span : spans) {
        const double reference = materials[span.material].referenceUvPerSquareMetre;
        float* out = table.data() + span.first * 4;
        for (size_t t = 0; t < span.count; ++t, out += 4) {
            if (out[3] != 0.0f) {
                continue;
            }
            const double raw = out[2];
            const double relative = std::isfinite(raw) ? relativeDensity(raw, reference) : 1.0;
            out[2] = static_cast<float>(relative);
        }
    }
    // The density shares and chart are weighted by the 3D area, so they read the samples.
    for (const auto& [gltfMaterial, entries] : samples) {
        const double reference = materials[gltfMaterial].referenceUvPerSquareMetre;
        for (const auto& [raw, weight] : entries) {
            const double relative = relativeDensity(raw, reference);
            if (relative < 0.9999) under += weight;
            if (relative > 1.0001) over += weight;
            addToHistogram(m_stats.density, std::clamp((std::log2(std::max(relative, 1e-12)) + 1.0) / 2.0, 0.0, 1.0), weight);
        }
    }

    m_stats.surfaceSquareMetres = totalArea;
    m_stats.shareDegenerate = totalArea > 0.0 ? degenerateArea / totalArea : 0.0;
    if (validArea > 0.0) {
        m_stats.shareShearOver110 = shear110 / validArea;
        m_stats.shareShearOver125 = shear125 / validArea;
        m_stats.shareAreaOver110 = area110 / validArea;
        m_stats.shareAreaOver125 = area125 / validArea;
        m_stats.shareUnder = under / validArea;
        m_stats.shareOver = over / validArea;
    }
    m_stats.islandsOverLimitShare = islandsArea > 0.0 ? islandsOverArea / islandsArea : 0.0;
    normalize(m_stats.shear, validArea);
    normalize(m_stats.area, validArea);
    normalize(m_stats.density, validArea);
    m_stats.materials.reserve(materials.size());
    for (auto& entry : materials) {
        m_stats.materials.push_back(std::move(entry.second));
    }
    std::sort(m_stats.materials.begin(), m_stats.materials.end(), [](const Material& a, const Material& b) {
        return a.surfaceSquareMetres != b.surfaceSquareMetres ? a.surfaceSquareMetres > b.surfaceSquareMetres : a.name < b.name;
    });

    if (!table.empty()) {
        const GLsizei width = std::min<GLint>(kTableWidth, maxSize);
        const size_t texels = table.size() / 4;
        const GLsizei height = static_cast<GLsizei>((texels + static_cast<size_t>(width) - 1) / static_cast<size_t>(width));
        table.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4, 0.0f);
        GLint previousTexture = 0;
        GLint previousAlignment = 4;
        GLint previousRowLength = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &previousRowLength);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glGenTextures(1, &m_texture);
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, table.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
        glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, previousRowLength);
    }
    m_stats.buildMilliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    core::logInfo("[TextureMetrics] %llu triangles, %zu materials, %llu islands in %.0f ms",
                  static_cast<unsigned long long>(m_stats.triangles), m_stats.materials.size(),
                  static_cast<unsigned long long>(m_stats.islands), m_stats.buildMilliseconds);
}

nlohmann::json TextureMetrics::stateJson() const
{
    using json = nlohmann::json;
    json state = {{"built", m_built}, {"builds", m_builds}};
    if (!m_built) {
        return state;
    }
    const Stats& s = m_stats;
    state["triangles"] = s.triangles;
    state["degenerateTriangles"] = s.degenerateTriangles;
    state["surfaceSquareMetres"] = s.surfaceSquareMetres;
    state["shareDegenerate"] = s.shareDegenerate;
    state["shear"] = {{"over110", s.shareShearOver110}, {"over125", s.shareShearOver125}};
    state["area"] = {{"over110", s.shareAreaOver110}, {"over125", s.shareAreaOver125}};
    state["density"] = {{"under", s.shareUnder}, {"over", s.shareOver}, {"rangeLow", kFreedomLow}, {"rangeHigh", kFreedomHigh}};
    state["islands"] = {{"count", s.islands}, {"overLimit", s.islandsOverLimit}, {"limit", kIslandLimit},
                        {"overLimitShare", s.islandsOverLimitShare}};
    state["skippedNoUvs"] = s.skippedNoUvs;
    state["nonTrianglePrimitives"] = s.nonTrianglePrimitives;
    state["truncatedTriangles"] = s.truncatedTriangles;
    state["morphedPrimitives"] = s.morphedPrimitives;
    state["buildMilliseconds"] = s.buildMilliseconds;
    json materials = json::array();
    for (const Material& material : s.materials) {
        materials.push_back({{"name", material.name},
                             {"triangles", material.triangles},
                             {"surfaceSquareMetres", material.surfaceSquareMetres},
                             {"referenceUvPerSquareMetre", material.referenceUvPerSquareMetre},
                             {"texture", material.texture},
                             {"uvSet", material.uvSet},
                             {"textureWidth", material.textureWidth},
                             {"textureHeight", material.textureHeight},
                             {"pixelsPerMetre", material.pixelsPerMetre}});
    }
    state["materials"] = std::move(materials);
    return state;
}

namespace {

// The add-on's colours, also in pbr.frag's uvHeatmapColor (RGB before the mix with the grey base).
const glm::vec3 kNeutral(0.92f, 0.92f, 0.92f);
const glm::vec3 kBlue(0.10f, 0.35f, 0.95f);
const glm::vec3 kRed(0.95f, 0.15f, 0.15f);
const glm::vec3 kGreen(0.15f, 0.80f, 0.30f);

ImU32 displayColour(const glm::vec3& colour)
{
    const glm::vec3 c = glm::mix(glm::vec3(0.8f), colour, 0.75f);
    return IM_COL32(static_cast<int>(c.r * 255.0f + 0.5f), static_cast<int>(c.g * 255.0f + 0.5f), static_cast<int>(c.b * 255.0f + 0.5f), 255);
}

std::string percent(double share)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%.1f%%", share * 100.0);
    return text;
}

} // namespace

void TextureMetrics::drawLegend(const ImVec2& imagePos, const ImVec2& imageSize, bool distortion, int distortionView, bool grid,
                                bool gridLabels, LegendPanels& panels, const std::function<void()>& settings) const
{
    if (!m_built) {
        return;
    }
    const Stats& s = m_stats;
    using Line = std::pair<std::string, ImU32>;
    std::vector<Line> statistics;
    std::vector<Line> legend;
    const ImU32 main = IM_COL32(255, 255, 255, 255);
    const ImU32 dim = IM_COL32(199, 199, 199, 255);
    const ImU32 warn = IM_COL32(255, 191, 64, 255);
    const std::string islands = "Islands over limit 1.25: " + std::to_string(s.islandsOverLimit) + " of " + std::to_string(s.islands);
    if (distortion) {
        if (distortionView == 0) {
            legend.emplace_back("White = none, blue = shear, green = area, cyan = both (full at 2.0), magenta = error", dim);
            statistics.emplace_back("Shear over 1.25: " + percent(s.shareShearOver125) + " | Area over 1.25: " + percent(s.shareAreaOver125) + " | " + islands, main);
        } else if (distortionView == 2) {
            legend.emplace_back("Area: 1.0 (neutral) to 2.0+ (green), magenta = error", dim);
            statistics.emplace_back("Surface over 1.10: " + percent(s.shareAreaOver110) + " | over 1.25: " + percent(s.shareAreaOver125) + " | " + islands, main);
        } else {
            legend.emplace_back("Shear: 1.0 (neutral) to 2.0+ (blue), magenta = error", dim);
            statistics.emplace_back("Surface over 1.10 (target): " + percent(s.shareShearOver110) + " | over 1.25: " + percent(s.shareShearOver125), main);
        }
    } else {
        legend.emplace_back("Against each material's median: 0.5 (red) - 1.0 (neutral) - 2.0 (blue), magenta = error", dim);
        statistics.emplace_back("Surface outside the 0.9 to 1.1 range: " + percent(s.shareUnder + s.shareOver) + " (too low: " + percent(s.shareUnder) + ", too high: " + percent(s.shareOver) + ")", main);
        const size_t shown = std::min<size_t>(s.materials.size(), 5);
        for (size_t i = 0; i < shown; ++i) {
            const Material& material = s.materials[i];
            char text[256];
            if (material.pixelsPerMetre > 0.0) {
                std::snprintf(text, sizeof(text), "%s: %.0f px/m (%ux%u)", material.name.c_str(), material.pixelsPerMetre, material.textureWidth, material.textureHeight);
            } else {
                std::snprintf(text, sizeof(text), "%s: no texture size", material.name.c_str());
            }
            statistics.emplace_back(text, dim);
        }
        if (s.materials.size() > shown) {
            statistics.emplace_back("and " + std::to_string(s.materials.size() - shown) + " more materials", dim);
        }
    }
    int warnings = 0;
    if (s.degenerateTriangles > 0) {
        legend.emplace_back("Error: " + percent(s.shareDegenerate) + " of surface (" + std::to_string(s.degenerateTriangles) + " faces without area in the UVs or the model)", warn);
        ++warnings;
    }
    if (!s.skippedNoUvs.empty()) {
        legend.emplace_back("No UVs (grey): " + s.skippedNoUvs.front() + (s.skippedNoUvs.size() > 1 ? " and others" : ""), warn);
        ++warnings;
    }
    if (s.truncatedTriangles > 0) {
        legend.emplace_back(std::to_string(s.truncatedTriangles) + " triangles over the table limit (grey)", warn);
        ++warnings;
    }
    if (grid) {
        legend.emplace_back("UV grid: violet = constant U, orange = constant V", dim);
        if (gridLabels) {
            legend.emplace_back("Cell labels: column letter, row number; A1 = image's lower left; dark text, colour varies by cell", dim);
        }
    }

    const auto lines = [](const std::vector<Line>& list) {
        for (const Line& line : list) {
            ImGui::PushStyleColor(ImGuiCol_Text, line.second);
            ImGui::TextUnformatted(line.first.c_str());
            ImGui::PopStyleColor();
        }
    };
    // A section's open state lives in the viewport, not in ImGui's saved tree state: session only, per viewport.
    const auto section = [](const char* label, const char* probe, bool& open, const std::function<void()>& body) {
        ImGui::SetNextItemOpen(open, ImGuiCond_Always);
        open = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_NoTreePushOnOpen);
        UiProbe::item(probe, 0);
        if (open) {
            body();
        }
    };

    // Stays in the image's lower left corner and takes the size of what is open; it scrolls only in a viewport too small
    // for it. A child window: the viewport does not count as hovered over it, so its clicks reach neither camera nor picking.
    const float margin = 8.0f;
    ImGui::SetCursorScreenPos(imagePos); // EndChild adds the child's size to the parent at the cursor: it must not grow the viewport's content
    ImGui::SetNextWindowPos(ImVec2(imagePos.x + margin, imagePos.y + imageSize.y - margin), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(std::max(imageSize.x - 2.0f * margin, 64.0f), std::max(imageSize.y - 2.0f * margin, 64.0f)));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(20, 20, 20, 200));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
    const bool visible = ImGui::BeginChild("##TextureMetricsLegend", ImVec2(0.0f, 0.0f),
                                           ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize,
                                           ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    if (visible) {
        ImGui::TextUnformatted(distortion ? "Texture Distortion" : "Texture Density");
        section("Statistics###Statistics", "texture_legend.statistics", panels.statistics, [&]() { lines(statistics); });
        char legendLabel[64];
        std::snprintf(legendLabel, sizeof(legendLabel), warnings > 0 ? "Legend (%d warning%s)###Legend" : "Legend###Legend", warnings, warnings == 1 ? "" : "s");
        // A warning stays noticed with the section closed.
        if (warnings > 0) {
            ImGui::PushStyleColor(ImGuiCol_Text, warn);
        }
        ImGui::SetNextItemOpen(panels.legend, ImGuiCond_Always);
        panels.legend = ImGui::TreeNodeEx(legendLabel, ImGuiTreeNodeFlags_NoTreePushOnOpen);
        UiProbe::item("texture_legend.legend", 0);
        if (warnings > 0) {
            ImGui::PopStyleColor();
        }
        if (panels.legend) {
            lines(legend);
        }

        // The chart, always shown.
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
        const float chartWidth = 288.0f;
        const bool both = distortion && distortionView == 0;
        const float histogramHeight = both ? 64.0f : 48.0f;
        const float bandHeight = 12.0f;
        const ImVec2 chart = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(chartWidth, 2.0f + histogramHeight + 2.0f + bandHeight + lineHeight));

        // Colour of bin i of the band, in the view's scale.
        const auto binColour = [&](int bin, bool areaRow) {
            const float t = (static_cast<float>(bin) + 0.5f) / static_cast<float>(kHistogramBins);
            if (!distortion) {
                const float signedT = t * 2.0f - 1.0f; // log2 of 0.5 .. 2
                return displayColour(signedT >= 0.0f ? glm::mix(kNeutral, kBlue, signedT) : glm::mix(kNeutral, kRed, -signedT));
            }
            const bool area = areaRow || distortionView == 2;
            return displayColour(glm::mix(kNeutral, area ? kGreen : kBlue, t));
        };
        const float binWidth = chartWidth / static_cast<float>(kHistogramBins);
        const auto histogram = [&](const std::array<float, kHistogramBins>& bins, float bottom, float top, bool areaRow) {
            float peak = 1e-12f;
            for (float bin : bins) {
                peak = std::max(peak, bin);
            }
            for (int i = 0; i < kHistogramBins; ++i) {
                const float share = bins[static_cast<size_t>(i)];
                if (share <= 0.0f) {
                    continue;
                }
                const float h = std::max((bottom - top) * std::sqrt(share / peak), 1.0f); // square root: small tails stay visible
                const float x = chart.x + binWidth * static_cast<float>(i);
                drawList->AddRectFilled(ImVec2(x, bottom - h), ImVec2(x + binWidth - 1.0f, bottom), binColour(i, areaRow));
            }
        };
        const float histogramTop = chart.y + 2.0f;
        const float histogramBottom = histogramTop + histogramHeight;
        if (both) {
            const float split = (histogramTop + histogramBottom) * 0.5f;
            histogram(m_stats.shear, split - 2.0f, histogramTop, false);
            histogram(m_stats.area, histogramBottom, split + 2.0f, true);
        } else if (distortion) {
            histogram(distortionView == 2 ? m_stats.area : m_stats.shear, histogramBottom, histogramTop, distortionView == 2);
        } else {
            histogram(m_stats.density, histogramBottom, histogramTop, false);
        }
        float y = histogramBottom + 2.0f;
        for (int i = 0; i < kHistogramBins; ++i) {
            const float x = chart.x + binWidth * static_cast<float>(i);
            if (both) {
                drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + binWidth, y + bandHeight * 0.5f), binColour(i, false));
                drawList->AddRectFilled(ImVec2(x, y + bandHeight * 0.5f), ImVec2(x + binWidth, y + bandHeight), binColour(i, true));
            } else {
                drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + binWidth, y + bandHeight), binColour(i, false));
            }
        }
        y += bandHeight;
        const char* const ticksDistortion[] = {"1.0", "1.25", "1.5", "2.0+"};
        const float positionsDistortion[] = {0.0f, 0.25f, 0.5f, 1.0f};
        const char* const ticksDensity[] = {"0.5-", "0.71", "1.0", "1.41", "2.0+"};
        const float positionsDensity[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
        const int tickCount = distortion ? 4 : 5;
        for (int i = 0; i < tickCount; ++i) {
            const char* label = distortion ? ticksDistortion[i] : ticksDensity[i];
            const float position = distortion ? positionsDistortion[i] : positionsDensity[i];
            const float labelWidth = ImGui::CalcTextSize(label).x;
            const float x = chart.x + std::clamp(chartWidth * position - labelWidth * 0.5f, 0.0f, chartWidth - labelWidth);
            drawList->AddText(ImVec2(x, y), dim, label);
        }

        section("Settings###Settings", "texture_legend.settings", panels.settings, [&]() {
            ImGui::PushItemWidth(chartWidth * 0.5f);
            settings();
            ImGui::PopItemWidth();
        });
    }
    ImGui::EndChild();
}

} // namespace ux3d::imgui
