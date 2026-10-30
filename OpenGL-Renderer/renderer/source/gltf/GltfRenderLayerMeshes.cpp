// Copyright (c) 2019 UX3D GmbH. All rights reserved.

// GltfRenderLayer meshes: accessor conversion, A1.11 MikkTSpace tangent generation and the A2.2 tangent cache,
// the mesh mapping (primitive -> renderer mesh, vertex attributes, defines) and the mesh update.

#include "GltfRenderLayerInternal.h"

namespace ux3d::renderer {

bool convert(const gltf::Accessor& accessor, std::vector<float>& output, bool applyPadding)
{
    core::Slice<uint8_t> rawData;
    if (accessor.getSparse().has_value()) {
        if (!accessor.getSparseDataAs(rawData)) {
            return false;
        }
    } else if (!accessor.getDataAs(rawData)) {
        return false;
    }

    core::MemoryRange<uint8_t> input = rawData.getMemoryRange();
    gltf::ComponentType componentType = accessor.getComponentType();
    gltf::AttributeType attributeType = accessor.getType();
    uint32_t componentCount = accessor.getComponentCountFrom(attributeType);
    size_t count = accessor.getCount();

    size_t paddingBytes = accessor.getByteLengthFrom(componentType, attributeType, true) - accessor.getByteLengthFrom(componentType, attributeType, false);
    size_t componentByteLength = accessor.getByteLengthFrom(componentType);
    size_t paddingElements = 0; // padding per attribute measured in component width units
    if (applyPadding) {
        paddingElements = paddingBytes / componentByteLength;
    }

    bool isNormalized = accessor.isNormalized();

    output.reserve(input.size());

    auto convertData = [&](auto dataView, auto func) -> void {
        for (size_t i = 0; i < count * (componentCount + paddingElements); i += componentCount + paddingElements) {
            for (size_t componentIdx = 0; componentIdx < componentCount; componentIdx++) {
                output.emplace_back(func(static_cast<float>(dataView[i + componentIdx])));
            }
            for (size_t paddingIdx = 0; paddingIdx < paddingElements; paddingIdx++) {
                // padding
            }
        }
    };

    std::function<float(float)> func;
    if (isNormalized) {
        switch (componentType) {
        case gltf::ComponentType::UnsignedByte:
            func = [](float c) -> float { return c / 255.f; };
            break;
        case gltf::ComponentType::Byte:
            func = [](float c) -> float { return std::max(c / 127.f, -1.f); };
            break;
        case gltf::ComponentType::Short:
            func = [](float c) -> float { return std::max(c / 32767.f, -1.f); };
            break;
        case gltf::ComponentType::UnsignedShort:
            func = [](float c) -> float { return c / 65535.f; };
            break;
        default:
            break;
        }
    } else {
        func = [](float c) -> float { return c; };
    }

    switch (componentType) {
    case gltf::ComponentType::UnsignedByte:
        convertData(input.viewAs<const uint8_t>(), func);
        return true;
    case gltf::ComponentType::Byte:
        convertData(input.viewAs<const int8_t>(), func);
        return true;
    case gltf::ComponentType::Short:
        convertData(input.viewAs<const int16_t>(), func);
        return true;
    case gltf::ComponentType::UnsignedShort:
        convertData(input.viewAs<const uint16_t>(), func);
        return true;
    default:
        break;
    }

    return false;
}

// A1.11: glTF primitives without TANGENT. The shader's screen-derivative frame keeps the
// tangent but always builds a right-handed bitangent, which is wrong on mirrored UV
// islands. Generate MikkTSpace tangents (with the face sign in w) at upload for the
// primitives whose material actually samples a normal map. Primitives that already store
// TANGENT, and files without normal maps, keep their vertex data unchanged.
struct TangentSpaceData {
    const gltf::Accessor* positions = nullptr;
    const gltf::Accessor* normals = nullptr;
    const gltf::Accessor* texcoords = nullptr;
    const gltf::Accessor* indices = nullptr;
    uint32_t faceCount = 0u;
    std::vector<glm::vec4> tangents;
};

uint32_t tangentVertexIndex(const TangentSpaceData& data, int face, int vertex)
{
    const uint32_t index = static_cast<uint32_t>(face * 3 + vertex);
    if (data.indices == nullptr) {
        return index;
    }
    const auto value = gltf::GltfUtils::getElementAtIndex(data.indices, index, false);
    const uint32_t* resolved = std::get_if<uint32_t>(&value);
    return resolved != nullptr ? *resolved : 0u;
}

int tangentNumFaces(const SMikkTSpaceContext* context)
{
    return static_cast<int>(static_cast<const TangentSpaceData*>(context->m_pUserData)->faceCount);
}

int tangentVerticesPerFace(const SMikkTSpaceContext*, int)
{
    return 3;
}

void tangentPosition(const SMikkTSpaceContext* context, float out[3], int face, int vertex)
{
    const auto& data = *static_cast<const TangentSpaceData*>(context->m_pUserData);
    const auto value = gltf::GltfUtils::getElementAtIndex(data.positions, tangentVertexIndex(data, face, vertex));
    const glm::vec3* position = std::get_if<glm::vec3>(&value);
    out[0] = position != nullptr ? position->x : 0.0f;
    out[1] = position != nullptr ? position->y : 0.0f;
    out[2] = position != nullptr ? position->z : 0.0f;
}

void tangentNormal(const SMikkTSpaceContext* context, float out[3], int face, int vertex)
{
    const auto& data = *static_cast<const TangentSpaceData*>(context->m_pUserData);
    const auto value = gltf::GltfUtils::getElementAtIndex(data.normals, tangentVertexIndex(data, face, vertex));
    const glm::vec3* normal = std::get_if<glm::vec3>(&value);
    out[0] = normal != nullptr ? normal->x : 0.0f;
    out[1] = normal != nullptr ? normal->y : 0.0f;
    out[2] = normal != nullptr ? normal->z : 1.0f;
}

void tangentTexCoord(const SMikkTSpaceContext* context, float out[2], int face, int vertex)
{
    const auto& data = *static_cast<const TangentSpaceData*>(context->m_pUserData);
    const auto value = gltf::GltfUtils::getElementAtIndex(data.texcoords, tangentVertexIndex(data, face, vertex));
    const glm::vec2* uv = std::get_if<glm::vec2>(&value);
    out[0] = uv != nullptr ? uv->x : 0.0f;
    out[1] = uv != nullptr ? uv->y : 0.0f;
}

void tangentSetSpace(const SMikkTSpaceContext* context, const float tangent[3], float sign, int face, int vertex)
{
    auto& data = *static_cast<TangentSpaceData*>(context->m_pUserData);
    glm::vec4& target = data.tangents[tangentVertexIndex(data, face, vertex)];
    target = glm::vec4(tangent[0], tangent[1], tangent[2], sign);
}

// A2.2: the UV set is part of the tangent cache key. A material's normal map may sample
// TEXCOORD_1 instead of TEXCOORD_0, and switching it must regenerate (not reuse) the tangents.
constexpr uint32_t kNoNormalMapUvSet = 0xFFFFFFFFu;

// A7.1: an anisotropic lobe orients along the tangent frame, so materials with a non-zero
// KHR_materials_anisotropy strength or with ASOBO_material_anisotropic_v2 need tangents as normal-
// mapped materials do. The KHR direction falls back to the tangent without its texture, so it needs
// them with omitted images too; ASOBO without its texture stays isotropic (no tangents then).
bool needsAnisotropyTangents(const gltf::Material& material, bool imagesOmitted)
{
    if (material.isUnlit()) {
        return false;
    }
    if (material.getAnisotropy().has_value()) {
        return material.getAnisotropy()->getAnisotropyStrengthOrDefault() > 0.0;
    }
    const auto& extensions = material.getUnknownExtensions();
    return !imagesOmitted && extensions.find("ASOBO_material_anisotropic_v2") != extensions.end();
}

uint32_t anisotropyUvSet(const gltf::Material& material)
{
    if (material.getAnisotropy().has_value()) {
        const auto& texture = material.getAnisotropy()->getAnisotropyTextureInfo();
        return texture.has_value() ? texture->getEffectiveTexCoord() : 0u;
    }
    const auto& extensions = material.getUnknownExtensions();
    const auto entry = extensions.find("ASOBO_material_anisotropic_v2");
    if (entry != extensions.end()) {
        const auto json = nlohmann::json::parse(entry->second.c_str(), nullptr, false);
        if (json.is_object() && json.contains("anisoDirectionRoughnessTexture")) {
            const auto& texture = json["anisoDirectionRoughnessTexture"];
            if (texture.is_object() && texture.contains("texCoord") && texture["texCoord"].is_number_unsigned()) {
                return texture["texCoord"].get<uint32_t>();
            }
        }
    }
    return 0u;
}

// The UV set the generated tangents follow: the normal map's (A1.11) when one is sampled, else the
// anisotropy texture's (A7.1), else kNoNormalMapUvSet (no tangents are generated).
uint32_t tangentFrameUvSet(const gltf::Material& material, bool imagesOmitted)
{
    if (!imagesOmitted && material.getNormalTextureInfo().has_value()
        && material.getNormalTextureInfo()->getTexture() != nullptr) {
        return material.getNormalTextureInfo()->getEffectiveTexCoord();
    }
    if (needsAnisotropyTangents(material, imagesOmitted)) {
        return anisotropyUvSet(material);
    }
    return kNoNormalMapUvSet;
}

std::vector<glm::vec4> generateMikkTangents(const gltf::Primitive& primitive, bool imagesOmitted)
{
    const gltf::Material* material = primitive.getMaterial();
    if (material == nullptr) {
        return {};
    }
    const uint32_t uvSet = tangentFrameUvSet(*material, imagesOmitted);
    if (uvSet == kNoNormalMapUvSet) {
        return {};
    }
    if (primitive.getPrimitiveMode() != gltf::PrimitiveMode::Triangles) {
        return {};
    }
    if (gltf::GltfUtils::getAttributeAccessor(primitive, "TANGENT") != nullptr) {
        return {};
    }

    TangentSpaceData data;
    data.positions = gltf::GltfUtils::getAttributeAccessor(primitive, "POSITION");
    data.normals = gltf::GltfUtils::getAttributeAccessor(primitive, "NORMAL");
    // MikkTSpace must use the UV set the normal map (or the anisotropy texture) samples, not always TEXCOORD_0.
    const std::string uvAttribute = "TEXCOORD_" + std::to_string(uvSet);
    data.texcoords = gltf::GltfUtils::getAttributeAccessor(primitive, uvAttribute);
    data.indices = primitive.getIndicesAccessor();
    if (data.positions == nullptr || data.normals == nullptr || data.texcoords == nullptr) {
        return {};
    }
    // The accessor readers currently support float vec2/vec3 data. Skip anything else and
    // let the shader's derivative frame handle it rather than generating wrong tangents.
    if (data.positions->getComponentType() != gltf::ComponentType::Float
        || data.normals->getComponentType() != gltf::ComponentType::Float
        || data.texcoords->getComponentType() != gltf::ComponentType::Float) {
        return {};
    }
    const uint32_t indexCount = data.indices != nullptr ? data.indices->getCount() : data.positions->getCount();
    if (indexCount < 3u || indexCount % 3u != 0u) {
        return {};
    }
    data.faceCount = indexCount / 3u;
    data.tangents.assign(data.positions->getCount(), glm::vec4(0.0f));

    SMikkTSpaceContext context {};
    SMikkTSpaceInterface interface {};
    context.m_pUserData = &data;
    context.m_pInterface = &interface;
    interface.m_getNumFaces = tangentNumFaces;
    interface.m_getNumVerticesOfFace = tangentVerticesPerFace;
    interface.m_getPosition = tangentPosition;
    interface.m_getNormal = tangentNormal;
    interface.m_getTexCoord = tangentTexCoord;
    interface.m_setTSpaceBasic = tangentSetSpace;

    if (!genTangSpaceDefault(&context)) {
        core::logWarning("MikkTSpace failed to generate tangents for a normal-mapped primitive");
        return {};
    }
    return data.tangents;
}

uint32_t generatedTangentUvSet(const gltf::Primitive& primitive, bool imagesOmitted)
{
    const gltf::Material* material = primitive.getMaterial();
    if (material == nullptr) {
        return 0u;
    }
    const uint32_t uvSet = tangentFrameUvSet(*material, imagesOmitted);
    return uvSet == kNoNormalMapUvSet ? 0u : uvSet;
}

// Material-level key for the A2.2 change detection: a material edit that changes whether (or on
// which UV set) tangents are generated rebuilds the meshes.
uint32_t materialNormalUvSet(const gltf::Material& material, bool imagesOmitted)
{
    return tangentFrameUvSet(material, imagesOmitted);
}
}

void GltfRenderLayer::initMeshMapping()
{
    // Mesh
    m_meshMapping.updateRendererobjectFromGltf = [this](std::tuple<const gltf::Node*, const gltf::Mesh*, const gltf::Primitive*> gltfTuple, renderer::Mesh* mesh) -> bool {
        auto [gltfNode, gltfMesh, gltfPrimitive] = gltfTuple;

        // A1.9: the packaged asset declares that its triangles are stored clockwise. Keep
        // the declaration on the drawable; CommandMapping combines it with the node rule
        // below in Mesh::effectiveFrontFace(), so every pass sees one winding.
        mesh->assetOptimizedWinding = m_assetOptimized;

        const gltf::Accessor* indexAccessor = gltfPrimitive->getIndicesAccessor();
        mesh->primitiveTopology = renderer::gl::PrimitiveTopology(gltfPrimitive->getPrimitiveMode());
        mesh->baseVertex = 0;

        if (indexAccessor != nullptr) {
            mesh->indexFormat = renderer::gl::PrimitiveType(indexAccessor->getComponentType());
            mesh->indexCount = indexAccessor->getCount();
            mesh->indexByteOffset = 0u; // offset is already resolved in accessor

            // MSFS 2020 packages share an index accessor between material primitives.
            // ASOBO_primitive selects a slice: StartIndex is an index (not a byte or
            // triangle offset), PrimitiveCount counts triangles. Without the slice,
            // every material paints the whole mesh, superimposing unrelated atlases.
            // A4 legacy package audit: PMDG 4.4; ordinary glTF stays accessor-based.
            if (m_assetOptimized && gltfPrimitive->getPrimitiveMode() == gltf::PrimitiveMode::Triangles) {
                const auto extras = nlohmann::json::parse(gltfPrimitive->getExtras(), nullptr, false);
                const auto meta = extras.is_object() ? extras.find("ASOBO_primitive") : extras.end();
                if (extras.is_object() && meta != extras.end() && meta->is_object() && meta->contains("PrimitiveCount")) {
                    const auto start = meta->value("StartIndex", nlohmann::json(0));
                    const auto& count = (*meta)["PrimitiveCount"];
                    const auto base = meta->value("BaseVertexIndex", nlohmann::json(0));
                    const size_t componentSize = gltf::Accessor::getByteLengthFrom(indexAccessor->getComponentType());
                    const auto nonnegativeInteger = [](const nlohmann::json& value) {
                        return value.is_number_unsigned() || (value.is_number_integer() && value.get<int64_t>() >= 0);
                    };
                    if (componentSize == 0 || !nonnegativeInteger(start) || !nonnegativeInteger(count)
                        || !nonnegativeInteger(base) || base.get<uint64_t>() > static_cast<uint64_t>(std::numeric_limits<GLint>::max())
                        || start.get<uint64_t>() > indexAccessor->getCount()
                        || count.get<uint64_t>() > (indexAccessor->getCount() - start.get<uint64_t>()) / 3u
                        || count.get<uint64_t>() > static_cast<uint64_t>(std::numeric_limits<GLsizei>::max()) / 3u
                        || start.get<uint64_t>() > static_cast<uint64_t>(std::numeric_limits<GLint>::max())
                            / componentSize) {
                        core::logWarning("[ASOBO_primitive] Invalid triangle index range; skipping primitive");
                        return false;
                    }
                    mesh->indexCount = static_cast<uint32_t>(count.get<uint64_t>() * 3u);
                    mesh->indexByteOffset = static_cast<uint32_t>(start.get<uint64_t>() * componentSize);
                    if (base.get<uint64_t>() != 0) {
                        uint64_t vertexLimit = std::numeric_limits<uint32_t>::max();
                        for (const auto& attribute : gltfPrimitive->getAttributes()) {
                            if (attribute.getAccessor() != nullptr) {
                                vertexLimit = std::min<uint64_t>(vertexLimit, attribute.getAccessor()->getCount());
                            }
                        }
                        if (base.get<uint64_t>() >= vertexLimit) {
                            core::logWarning("[ASOBO_primitive] Base vertex exceeds attributes; skipping primitive");
                            return false;
                        }
                        const uint64_t localLimit = vertexLimit - base.get<uint64_t>();
                        for (uint32_t i = 0; i < mesh->indexCount; ++i) {
                            const auto value = gltf::GltfUtils::getElementAtIndex(indexAccessor, static_cast<uint32_t>(start.get<uint64_t>()) + i, false);
                            const auto* index = std::get_if<uint32_t>(&value);
                            if (index == nullptr || *index >= localLimit) {
                                core::logWarning("[ASOBO_primitive] Rebased index exceeds attributes; skipping primitive");
                                return false;
                            }
                        }
                        mesh->baseVertex = static_cast<GLint>(base.get<uint64_t>());
                    }
                }
            }

            auto bufferIt = m_bufferMapping.map.find(indexAccessor);
            if (bufferIt != m_bufferMapping.map.end()) {
                mesh->indexBuffer = bufferIt->second;
            } else {
                core::logError("No gl index buffer found for gltf primitive");
                return false;
            }
        }

        auto nodeIt = m_transformHierarchyMapping.map.find(gltfNode);
        if (nodeIt != m_transformHierarchyMapping.map.end()) {
            nodeIt->second->addMesh(mesh);
        }

        uint32_t vertexCount = 0u;
        auto addAttribute = [&](const gltf::Accessor* accessor, const std::string& name) {
            if (accessor == nullptr)
                return;

            if (mesh->attributes.size() >= 16) {
                // more attributes than GL_MAX_VERTEX_ATTRIBUTES (on most platforms)
                return;
            }

            // check if accessor has bufferview
            if (accessor->getBufferView() == nullptr) {
                return;
            }

            if (vertexCount == 0u) {
                vertexCount = accessor->getCount();
            } else if (vertexCount != accessor->getCount()) {
                core::logError("Inconsistent attribute element count found for attribute %s", name.c_str());
                return;
            }

            renderer::gl::Attribute attribute("a_" + core::toLower(name));

            auto bufferIt = m_bufferMapping.map.find(accessor);
            if (bufferIt != m_bufferMapping.map.end()) {
                if (bufferIt->second != nullptr) {
                    attribute.buffer = bufferIt->second;
                    // skip uninitialized buffers (dont add them to the mesh) (can happen with broken gltfs)
                    if (attribute.buffer->getSize() == 0) {
                        return;
                    }
                }
            } else {
                core::logError("No gl buffer found for gltf attribute %s", name.c_str());
                return;
            }

            // The packaged layout pads NORMAL to VEC4 (the fourth byte is 127), while the
            // shaders read a vec3. Define and size the vertex attribute as VEC3 so the
            // padded component is ignored; the stored byte order is x, y, z, w.
            gltf::AttributeType attributeType = accessor->getType();
            if (m_assetOptimized && name == "NORMAL" && attributeType == gltf::AttributeType::Vec4) {
                attributeType = gltf::AttributeType::Vec3;
            }

            std::string define = "HAS_" + core::toUpper(name) + "_";
            switch (attributeType) {
            case gltf::AttributeType::Scalar:
                define += "FLOAT";
                break;
            case gltf::AttributeType::Vec2:
                define += "VEC2";
                break;
            case gltf::AttributeType::Vec3:
                define += "VEC3";
                break;
            case gltf::AttributeType::Vec4:
                define += "VEC4";
                break;
            case gltf::AttributeType::Mat2:
                define += "MAT2";
                break;
            case gltf::AttributeType::Mat3:
                define += "MAT3";
                break;
            case gltf::AttributeType::Mat4:
                define += "MAT4";
                break;
            default:
                break;
            }
            mesh->defines.define(define);

            const gltf::BufferView* bufferView = accessor->getBufferView();

            attribute.offset = 0u;
            attribute.stride = bufferView ? bufferView->getByteStride().value_or(0u) : 0u;
            attribute.normalized = accessor->isNormalized() ? GL_TRUE : GL_FALSE;
            attribute.type = static_cast<gl::ComponentType>(accessor->getComponentType());

            // A1.9: packaged MSFS files keep half-floats in SHORT/USHORT accessors and signed
            // components in BYTE accessors, all with normalized false. Hand those bit patterns
            // to GL as they are stored; everything else keeps the accessor's own type.
            if (m_assetOptimized && !accessor->isNormalized()) {
                const bool halfFloat = (name.rfind("TEXCOORD_", 0) == 0 || name == "COLOR_0")
                    && (accessor->getComponentType() == gltf::ComponentType::Short
                        || accessor->getComponentType() == gltf::ComponentType::UnsignedShort);
                const bool signedByte = (name == "NORMAL" || name == "TANGENT")
                    && accessor->getComponentType() == gltf::ComponentType::Byte;
                // The skinned 48-byte layout stores COLOR_0 as BYTE but the bytes are RGBA8
                // (all 0xFF for the added white), so this one is unsigned despite the accessor.
                const bool unsignedByteColor = name == "COLOR_0"
                    && accessor->getComponentType() == gltf::ComponentType::Byte;
                if (halfFloat) {
                    attribute.type = gl::ComponentType::HalfFloat;
                } else if (signedByte) {
                    attribute.normalized = GL_TRUE;
                } else if (unsignedByteColor) {
                    attribute.type = gl::ComponentType::UByte;
                    attribute.normalized = GL_TRUE;
                }
            }

            switch (attributeType) {
            case gltf::AttributeType::Scalar:
                attribute.dimension = 1;
                break;
            case gltf::AttributeType::Vec2:
                attribute.dimension = 2;
                break;
            case gltf::AttributeType::Vec3:
                attribute.dimension = 3;
                break;
            case gltf::AttributeType::Vec4:
                attribute.dimension = 4;
                break;
            default:
                break;
            }

            // Without a bufferView byteStride GL tightly packs attribute.dimension components.
            // When the packaged VEC4 normal is read as VEC3, keep the source element stride so
            // the fourth byte is skipped per vertex instead of shifting the following ones.
            if (attribute.stride == 0 && attributeType != accessor->getType()) {
                attribute.stride = static_cast<GLsizei>(
                    accessor->getByteLengthFrom(accessor->getComponentType(), accessor->getType(), false));
            }

            mesh->attributes.emplace_back(attribute);
        };

        mesh->attributes.clear();
        mesh->defines.clear();
        for (const gltf::Attribute& gltfAttribute : gltfPrimitive->getAttributes()) {
            addAttribute(gltfAttribute.getAccessor(), gltfAttribute.getName());
        }

        mesh->vertexCount = vertexCount;

        // A1.11: normal-mapped primitives without TANGENT get MikkTSpace tangents so the
        // shader's bitangent carries the UV mirroring sign. Existing TANGENT attributes and
        // materials without a normal map are left untouched. A2.2: the buffer is generated
        // once per loaded model and reused by later mesh updates; the key includes the
        // normal map's UV set.
        // Omitted normal maps do not need generated tangent frames. Retain authored
        // attributes, but avoid this CPU work and its extra GPU buffer for this open.
        // A7.1: anisotropic materials need them as well, also when the images were omitted.
        if (vertexCount > 0u && mesh->attributes.size() < 16u && gltfPrimitive->getMaterial() != nullptr
            && tangentFrameUvSet(*gltfPrimitive->getMaterial(), m_modelImagesOmitted) != kNoNormalMapUvSet) {
            const GeneratedTangentKey tangentKey { gltfPrimitive, generatedTangentUvSet(*gltfPrimitive, m_modelImagesOmitted) };
            auto tangentIt = m_generatedTangentBuffers.find(tangentKey);
            if (tangentIt == m_generatedTangentBuffers.end()) {
                auto prepared = m_preparedTangents.find(tangentKey);
                std::vector<glm::vec4> generated;
                if (prepared != m_preparedTangents.end()) {
                    generated = std::move(prepared->second);
                    m_preparedTangents.erase(prepared);
                } else {
                    generated = generateMikkTangents(*gltfPrimitive, m_modelImagesOmitted);
                }
                if (generated.size() == vertexCount) {
                    auto tangentBuffer = std::make_unique<gl::Buffer>(m_resourceManager.m_gl);
                    tangentBuffer->write(core::makeRange(
                        reinterpret_cast<const uint8_t*>(generated.data()),
                        generated.size() * sizeof(generated[0])));
                    tangentIt = m_generatedTangentBuffers.emplace(tangentKey, std::move(tangentBuffer)).first;
                }
            }
            if (tangentIt != m_generatedTangentBuffers.end()) {
                gl::Attribute tangentAttribute("a_tangent");
                tangentAttribute.type = gl::ComponentType::Float;
                tangentAttribute.dimension = 4;
                tangentAttribute.buffer = tangentIt->second.get();
                mesh->attributes.emplace_back(tangentAttribute);
                mesh->defines.define("HAS_TANGENT_VEC4");
            }
        }

        if (gltfPrimitive->getTargets().size() > 0) {
            size_t attributeCount = 0;
            std::unordered_map<std::string, gltf::Accessor*> attributes;
            mesh->defines.define("NUM_VERTICIES", vertexCount);
            mesh->defines.define("HAS_MORPH_TARGETS", 1);

            for (auto& target : gltfPrimitive->getTargets()) {
                attributeCount += target.accessorCount();
                attributes.merge(target.getAccessorMapping());
            }

            size_t maxTargetAttribute = attributes.size();
            size_t targetCount = gltfPrimitive->getTargets().size();
            if (targetCount * attributes.size() > m_maxTextureArrayLength) {
                targetCount = m_maxTextureArrayLength / attributes.size();
            }

            size_t attributeOffset = 0;
            std::unordered_map<std::string, size_t> attributeOffsets;
            for (auto& [attribute, _] : attributes) {
                mesh->defines.define("HAS_MORPH_TARGET_" + attribute, 1);
                mesh->defines.define("MORPH_TARGET_" + attribute + "_OFFSET", static_cast<int>(attributeOffset));
                attributeOffsets[attribute] = attributeOffset;
                attributeOffset += targetCount;
            }

            // Create texture
            if (attributeCount != mesh->morphTargetAttributeCount) {
                mesh->morphTargetAttributeCount = attributeCount;
                if (vertexCount <= m_max2DTextureSize) {
                    if (gltfPrimitive->getTargets().size() * attributeOffsets.size() > m_maxTextureArrayLength) {
                        core::logWarning("Morph targets exceed texture size limit. Only %u of %u are used", targetCount, gltfPrimitive->getTargets().size());
                    }
                    const uint32_t width = static_cast<uint32_t>(math::ceil(math::sqrt(vertexCount)));
                    const size_t singleTextureSize = static_cast<size_t>(math::pow(width, 2) * 4);
                    const size_t dataSize = singleTextureSize * targetCount * attributeOffsets.size();
                    std::vector<float> textureData(dataSize, 0.0f);
                    for (int i = 0; i < targetCount; ++i) {
                        const auto& target = gltfPrimitive->getTargets().at(i);
                        for (auto& [accessorName, offsetRef] : attributeOffsets) {
                            auto targetAccessorMap = target.getAccessorMapping();
                            if (targetAccessorMap.find(accessorName) != targetAccessorMap.end()) {
                                auto accessor = targetAccessorMap.at(accessorName);
                                size_t offset = offsetRef * singleTextureSize;
                                core::MemoryRange<float> accessorData;

                                core::Slice<float> accessorSliceData;
                                if (accessor->getSparse().has_value()) {
                                    if (!accessor->getSparseDataAs(accessorSliceData)) {
                                        core::logWarning("Could not generate morph target texture");
                                        return false;
                                    }
                                } else if (!accessor->getDataAs(accessorSliceData)) {
                                    core::logWarning("Could not generate morph target texture");
                                    return false;
                                }

                                std::vector<float> floatData;
                                if (accessor->getComponentType() != gltf::ComponentType::Float) {
                                    if (convert(*accessor, floatData, true) == false) {
                                        core::logWarning("Could not generate morph target texture");
                                        return false;
                                    }
                                    accessorData = core::makeMutableByteRange(floatData).viewAs<float>();

                                } else {
                                    accessorData = accessorSliceData.getMemoryRange();
                                }

                                const auto accessorType = accessor->getType();
                                switch (accessorType) {
                                case gltf::AttributeType::Vec2:
                                case gltf::AttributeType::Vec3: {
                                    // Add padding to fit vec2/vec3 into rgba
                                    size_t paddingOffset = 0;
                                    size_t accessorOffset = 0;
                                    const size_t componentCount = gltf::Accessor::getComponentCountFrom(accessorType);
                                    for (size_t j = 0; j < accessor->getCount(); ++j) {
                                        std::copy(accessorData.begin() + accessorOffset, accessorData.begin() + accessorOffset + componentCount, textureData.begin() + offset + paddingOffset);
                                        paddingOffset += 4;
                                        accessorOffset += componentCount;
                                    }
                                    break;
                                }
                                case gltf::AttributeType::Vec4:
                                    std::copy(accessorData.begin(), accessorData.end(), textureData.begin() + offset);
                                    break;
                                default:
                                    core::logWarning("Unsupported attribute type for morph targets");
                                    break;
                                }
                            }
                            ++offsetRef;
                        }
                    }
                    mesh->setMorphTargetTexture(width, textureData, targetCount * attributeOffsets.size());
                } else {
                    core::logWarning("Mesh of Morph targets too big. Cannot apply morphing.");
                }
            }
        }
        auto gltfWeightCount = static_cast<int>(std::max(gltfNode->weights ? gltfNode->weights->size() : 0, gltfMesh->weights ? gltfMesh->weights->size() : 0));
        if (gltfWeightCount > 128) {
            core::logWarning("This asset uses a high number of weights: %d", gltfWeightCount);
        }
        gltfWeightCount = std::max(gltfWeightCount, 1); // shader requires at least one item
        mesh->defines.define("WEIGHT_COUNT", gltfWeightCount);

        m_animationPointerTargets[{ gltfMesh, "weights" }].callback = KHRAnimationPointerTarget::Callback<std::vector<float>> {
            [mesh](const std::vector<float>& value) -> void {
                mesh->updateWeights(value);
            }
        };

        mesh->polygonMode = gl::PolygonMode::Fill;
        mesh->defines.define("DEBUG", static_cast<int>(RenderMode::None));

        if (nodeIt != m_transformHierarchyMapping.map.end()) {
            // from the spec: If the determinant of the transform is a negative value, the winding order of the mesh triangle faces should be reversed. This supports negative scales for mirroring geometry.
            // Mesh::effectiveFrontFace() applies the asset-optimized (clockwise) convention
            // on top of this, so a mirrored packaged mesh ends up counter-clockwise again.
            const float det = math::determinant(nodeIt->second->getWorldTransform());
            if (det < 0.f) {
                mesh->frontFace = gl::Winding::Clockwise;
            } else {
                mesh->frontFace = gl::Winding::CounterClockwise;
            }
        }

        mesh->bumpGeneration();

        return true;
    };

    m_meshMapping.createRendererObjectFromGltf = [this](std::tuple<const gltf::Node*, const gltf::Mesh*, const gltf::Primitive*> gltfTuple) -> renderer::Mesh* {
        auto mesh = new Mesh { m_resourceManager.m_gl };
        mesh->vertexShader = "primitive.vert";
        m_meshMapping.updateRendererobjectFromGltf(gltfTuple, mesh);
        return mesh;
    };
    m_meshMapping.destroyRendererobjectFromGltf = [this](std::tuple<const gltf::Node*, const gltf::Mesh*, const gltf::Primitive*> gltfTuple, renderer::Mesh* mesh) {
        auto [gltfNode, gltfMesh, gltfPrimitive] = gltfTuple;

        auto nodeIt = m_transformHierarchyMapping.map.find(gltfNode);
        if (nodeIt != m_transformHierarchyMapping.map.end()) {
            nodeIt->second->removeMesh(mesh);
        }

        // remove mesh from all skins that use it
        for (const auto& [_, skin] : m_skinMapping.map) {
            if (skin != nullptr) {
                skin->removeMesh(mesh);
            }
        }

        m_animationPointerTargets.erase({ gltfMesh, "weights" });

        delete mesh;
    };
    m_meshMapping.needsRecreation = [this](renderer::Mesh*, std::tuple<const gltf::Node*, const gltf::Mesh*, const gltf::Primitive*>) -> bool {
        return false;
    };
}

void GltfRenderLayer::updateMeshes(const core::Vector<gltf::Node*>& rootNodes)
{
    core::LinkedList<std::tuple<const gltf::Node*, const gltf::Mesh*, const gltf::Primitive*>> gltfTuples;
    // Meshes: in our renderer a mesh is a drawable set of vertices and attributes. It specifies the vertex shader.
    // the gltf correspondence would be 'primitive'
    std::function<void(const gltf::Node&)> gatherMeshRecursive;
    gatherMeshRecursive = [&](const gltf::Node& gltfNode) {
        const gltf::Mesh* gltfMesh = gltfNode.getMesh();
        if (gltfMesh != nullptr) {
            for (const gltf::Primitive& gltfPrimitive : gltfMesh->getPrimitives()) {
                gltfTuples.emplace_back(&gltfNode, gltfMesh, &gltfPrimitive);
            }
        }

        // recurse into children
        for (const gltf::Node* child : gltfNode.getChildren()) {
            gatherMeshRecursive(*child);
        }
    };

    // start the gathering process on all root nodes
    for (const gltf::Node* gltfNode : rootNodes) {
        gatherMeshRecursive(*gltfNode);
    }

    // A2.2: the generated tangents are cached for the loaded model (see
    // m_generatedTangentBuffers); updateMeshes only re-binds the mesh attributes. A model
    // load calls clearGeneratedTangents, and a normal-map UV-set change makes the cache key
    // miss. Rebuilding them here made every material edit regenerate all of them.
    m_meshMapping.update(gltfTuples);
}

void GltfRenderLayer::clearGeneratedTangents()
{
    m_generatedTangentBuffers.clear();
    m_preparedTangents.clear();
    m_materialNormalUvSets.clear();
}

void GltfRenderLayer::prepareGeneratedTangents(const gltf::Gltf& gltf)
{
    m_preparedTangents.clear();
    const bool imagesOmitted = gltf.imagesOmitted();
    struct Work {
        const gltf::Primitive* primitive;
        std::vector<glm::vec4> tangents;
    };
    std::vector<Work> work;
    uint64_t largestTemporary = 1;
    for (const auto& mesh : gltf.getMeshes()) for (const auto& primitive : mesh.getPrimitives()) {
        const auto* material = primitive.getMaterial();
        if (!material || tangentFrameUvSet(*material, imagesOmitted) == kNoNormalMapUvSet
            || gltf::GltfUtils::getAttributeAccessor(primitive, "TANGENT")) continue;
        work.push_back({&primitive, {}});
        if (const auto* positions = gltf::GltfUtils::getAttributeAccessor(primitive, "POSITION"))
            largestTemporary = std::max(largestTemporary, uint64_t(positions->getCount())*256);
    }
    core::loading::begin(core::loading::Tangents, static_cast<uint32_t>(work.size()));
    core::loading::Timer timer(core::loading::Tangents);
    const uint64_t freeRam = core::loading::state.availableRam;
    const size_t memoryWorkers = freeRam ? std::max<uint64_t>(1, freeRam/8/largestTemporary) : 1;
    const size_t workers = std::min({work.size(), memoryWorkers,
        size_t(std::max(1u, std::min(4u, std::thread::hardware_concurrency())))});
    std::atomic<size_t> next{0};
    // Futures die before captured work/next, including if starting a worker throws.
    std::vector<std::future<void>> futures;
    for (size_t i = 0; i < workers; ++i) futures.push_back(std::async(std::launch::async, [&] {
        for (size_t index = next++; index < work.size(); index = next++) {
            work[index].tangents = generateMikkTangents(*work[index].primitive, imagesOmitted);
            ++core::loading::state.completed;
        }
    }));
    for (auto& future : futures) future.get();
    for (auto& item : work)
        m_preparedTangents.emplace(GeneratedTangentKey{item.primitive, generatedTangentUvSet(*item.primitive, imagesOmitted)}, std::move(item.tangents));
}

