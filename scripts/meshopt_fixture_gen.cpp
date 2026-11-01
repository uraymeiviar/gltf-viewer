// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
//
// Dev-only fixture writer for scripts/validate_meshopt.py (target meshopt_fixture_gen, build/tools/<config>; built
// with Glider, never linked into it or shipped). It uses meshoptimizer's own encoders, from the same fetched source
// as the decoder Glider links, to write meshopt-compressed glTF files and, for each, the same model uncompressed.
//
// Usage: meshopt_fixture_gen <output folder>
//
// Files (all small, a few tens of KiB):
//   ext.glb / ext_plain.glb     EXT_meshopt_compression (vertex codec v0), fallback buffer without uri
//   khr.glb / khr_plain.glb     KHR_meshopt_compression (vertex codec v1, COLOR filter on COLOR_0)
//   ext_gltf.gltf + .bin        the EXT model as .gltf: compressed buffer in a .bin file, fallback buffer without uri
//   ext_corrupt.glb             the EXT model with the torus POSITION stream damaged (decode must fail)
//   ext_optional.gltf + 2 .bin  EXT_meshopt_compression used but not required: the fallback buffer has a uri with
//                               the uncompressed data (a loader without the extension reads it)
// Content: a torus (POSITION uint16 quantized with KHR_mesh_quantization, NORMAL int8 OCTAHEDRAL, TEXCOORD_0 uint16
// normalized, COLOR_0 uint8, uint16 indices in TRIANGLES mode) and an animated sphere (POSITION float EXPONENTIAL,
// NORMAL int16 OCTAHEDRAL, TEXCOORD_0 float, uint32 indices in INDICES mode; rotation int16 QUATERNION, translation
// float EXPONENTIAL, times float).
//
// The *_plain files hold exactly the bytes the decoder produces, in the same bufferView layout (no extension), so the
// compressed and plain files must render byte-identically. The tool itself checks that every unfiltered stream
// decodes back to the encoder's input bytes, and that filtered streams decode within their quantization step of the
// source floats (normals, quaternions, exponent-mantissa floats, colours); it exits 1 when any check fails.

#include <meshoptimizer.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;
int g_failures = 0;

void check(bool condition, const std::string& label)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", label.c_str());
        ++g_failures;
    }
}

enum class Mode { Attributes, Triangles, Indices };
enum class Filter { None, Octahedral, Quaternion, Exponential, Color };

const char* modeName(Mode mode) { return mode == Mode::Attributes ? "ATTRIBUTES" : mode == Mode::Triangles ? "TRIANGLES" : "INDICES"; }
const char* filterName(Filter filter)
{
    switch (filter) {
    case Filter::None: return "NONE";
    case Filter::Octahedral: return "OCTAHEDRAL";
    case Filter::Quaternion: return "QUATERNION";
    case Filter::Exponential: return "EXPONENTIAL";
    case Filter::Color: return "COLOR";
    }
    return "NONE";
}

// One bufferView: the bytes the encoder takes (after the encode filter) and the decoded bytes.
struct Stream {
    std::string name;
    Mode mode = Mode::Attributes;
    Filter filter = Filter::None;
    uint32_t stride = 0;
    uint32_t count = 0;
    std::vector<uint8_t> input; // encoder input (filtered bytes)
    std::vector<uint8_t> decoded; // decoder output = the plain file's bytes
    std::vector<uint8_t> encoded;
    bool vertexStride = false; // bufferView.byteStride written (vertex attributes)
    uint32_t target = 0;
};

void align4(std::vector<uint8_t>& bytes)
{
    while (bytes.size() % 4) bytes.push_back(0);
}

template <typename T>
T at(const std::vector<uint8_t>& bytes, size_t offset)
{
    T value;
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}

void encodeStream(Stream& stream, int vertexVersion)
{
    if (stream.mode == Mode::Attributes) {
        stream.encoded.resize(meshopt_encodeVertexBufferBound(stream.count, stream.stride));
        stream.encoded.resize(meshopt_encodeVertexBufferLevel(stream.encoded.data(), stream.encoded.size(), stream.input.data(),
            stream.count, stream.stride, 2, vertexVersion));
    } else {
        std::vector<unsigned int> indices(stream.count);
        for (uint32_t i = 0; i < stream.count; ++i) {
            indices[i] = stream.stride == 2 ? reinterpret_cast<const uint16_t*>(stream.input.data())[i]
                                            : reinterpret_cast<const uint32_t*>(stream.input.data())[i];
        }
        unsigned int vertexCount = 0;
        for (unsigned int index : indices) vertexCount = std::max(vertexCount, index + 1);
        if (stream.mode == Mode::Triangles) {
            stream.encoded.resize(meshopt_encodeIndexBufferBound(stream.count, vertexCount));
            stream.encoded.resize(meshopt_encodeIndexBuffer(stream.encoded.data(), stream.encoded.size(), indices.data(), stream.count));
        } else {
            stream.encoded.resize(meshopt_encodeIndexSequenceBound(stream.count, vertexCount));
            stream.encoded.resize(meshopt_encodeIndexSequence(stream.encoded.data(), stream.encoded.size(), indices.data(), stream.count));
        }
    }
    check(!stream.encoded.empty(), stream.name + ": encoded");
    // The reference decode, with the library's own decoder.
    stream.decoded.assign(size_t(stream.count) * stream.stride, 0);
    int status = 0;
    if (stream.mode == Mode::Attributes) {
        status = meshopt_decodeVertexBuffer(stream.decoded.data(), stream.count, stream.stride, stream.encoded.data(), stream.encoded.size());
        check(meshopt_decodeVertexVersion(stream.encoded.data(), stream.encoded.size()) == vertexVersion, stream.name + ": vertex codec version");
    } else if (stream.mode == Mode::Triangles) {
        status = meshopt_decodeIndexBuffer(stream.decoded.data(), stream.count, stream.stride, stream.encoded.data(), stream.encoded.size());
    } else {
        status = meshopt_decodeIndexSequence(stream.decoded.data(), stream.count, stream.stride, stream.encoded.data(), stream.encoded.size());
    }
    check(status == 0, stream.name + ": decodes");
    if (stream.mode == Mode::Triangles) {
        // The triangle codec keeps every triangle and its winding but may rotate its three indices.
        bool same = stream.decoded.size() == stream.input.size();
        auto index = [&](const std::vector<uint8_t>& bytes, size_t i) {
            return stream.stride == 2 ? uint32_t(at<uint16_t>(bytes, i * 2)) : at<uint32_t>(bytes, i * 4);
        };
        for (size_t tri = 0; same && tri < stream.count / 3; ++tri) {
            const uint32_t a = index(stream.input, tri * 3), b = index(stream.input, tri * 3 + 1), c = index(stream.input, tri * 3 + 2);
            const uint32_t x = index(stream.decoded, tri * 3), y = index(stream.decoded, tri * 3 + 1), z = index(stream.decoded, tri * 3 + 2);
            same = (x == a && y == b && z == c) || (x == b && y == c && z == a) || (x == c && y == a && z == b);
        }
        check(same, stream.name + ": the codec is lossless (same triangles and winding, rotations allowed)");
    } else {
        check(stream.decoded == stream.input, stream.name + ": the codec is lossless (decoded bytes = encoder input)");
    }
    switch (stream.filter) {
    case Filter::None: break;
    case Filter::Octahedral: meshopt_decodeFilterOct(stream.decoded.data(), stream.count, stream.stride); break;
    case Filter::Quaternion: meshopt_decodeFilterQuat(stream.decoded.data(), stream.count, stream.stride); break;
    case Filter::Exponential: meshopt_decodeFilterExp(stream.decoded.data(), stream.count, stream.stride); break;
    case Filter::Color: meshopt_decodeFilterColor(stream.decoded.data(), stream.count, stream.stride); break;
    }
}

template <typename T>
std::vector<uint8_t> bytesOf(const std::vector<T>& values)
{
    std::vector<uint8_t> bytes(values.size() * sizeof(T));
    std::memcpy(bytes.data(), values.data(), bytes.size());
    return bytes;
}


struct Geometry {
    std::vector<float> positions, normals, uvs, colors; // 3, 3, 2, 4 per vertex
    std::vector<uint32_t> indices;
    size_t vertices() const { return positions.size() / 3; }
};

Geometry torus(int segments, int rings, float major, float minor)
{
    Geometry g;
    for (int i = 0; i <= segments; ++i) {
        const double u = 2 * kPi * i / segments;
        for (int j = 0; j <= rings; ++j) {
            const double v = 2 * kPi * j / rings;
            const double cx = std::cos(u), cz = std::sin(u);
            const double nx = std::cos(v) * cx, ny = std::sin(v), nz = std::cos(v) * cz;
            g.positions.insert(g.positions.end(), { float(major * cx + minor * nx), float(minor * ny), float(major * cz + minor * nz) });
            g.normals.insert(g.normals.end(), { float(nx), float(ny), float(nz) });
            g.uvs.insert(g.uvs.end(), { float(double(i) / segments), float(double(j) / rings) });
            g.colors.insert(g.colors.end(), { float(0.5 + 0.5 * std::cos(u)), float(0.5 + 0.5 * std::sin(v)), float(0.5 + 0.5 * std::sin(u)), 1.0f });
        }
    }
    for (int i = 0; i < segments; ++i) {
        for (int j = 0; j < rings; ++j) {
            const uint32_t a = i * (rings + 1) + j, b = (i + 1) * (rings + 1) + j;
            g.indices.insert(g.indices.end(), { a, a + 1, b, b, a + 1, b + 1 });
        }
    }
    return g;
}

Geometry sphere(int segments, int rings, float radius)
{
    Geometry g;
    for (int j = 0; j <= rings; ++j) {
        const double v = kPi * j / rings;
        for (int i = 0; i <= segments; ++i) {
            const double u = 2 * kPi * i / segments;
            const double nx = std::sin(v) * std::cos(u), ny = std::cos(v), nz = std::sin(v) * std::sin(u);
            g.positions.insert(g.positions.end(), { float(radius * nx), float(radius * ny), float(radius * nz) });
            g.normals.insert(g.normals.end(), { float(nx), float(ny), float(nz) });
            g.uvs.insert(g.uvs.end(), { float(double(i) / segments), float(double(j) / rings) });
        }
    }
    for (int j = 0; j < rings; ++j) {
        for (int i = 0; i < segments; ++i) {
            const uint32_t a = j * (segments + 1) + i, b = (j + 1) * (segments + 1) + i;
            g.indices.insert(g.indices.end(), { a, a + 1, b, a + 1, b + 1, b }); // counter-clockwise from outside
        }
    }
    return g;
}

struct Model {
    std::vector<Stream> streams;
    nlohmann::json accessors = nlohmann::json::array();
    float torusOrigin[3] {};
    float torusScale = 1;
};

uint32_t addStream(Model& model, Stream stream, int vertexVersion)
{
    encodeStream(stream, vertexVersion);
    model.streams.push_back(std::move(stream));
    return uint32_t(model.streams.size() - 1);
}

void addAccessor(Model& model, uint32_t view, int componentType, uint32_t count, const char* type, bool normalized,
    const nlohmann::json& min = nullptr, const nlohmann::json& max = nullptr)
{
    nlohmann::json accessor = { { "bufferView", view }, { "componentType", componentType }, { "count", count }, { "type", type } };
    if (normalized) accessor["normalized"] = true;
    if (!min.is_null()) accessor["min"] = min;
    if (!max.is_null()) accessor["max"] = max;
    model.accessors.push_back(accessor);
}

// Float min/max of `components` floats per element at stride `stride` in decoded bytes.
std::pair<nlohmann::json, nlohmann::json> floatBounds(const Stream& s, int components)
{
    std::vector<float> lo(components, 1e30f), hi(components, -1e30f);
    for (uint32_t i = 0; i < s.count; ++i) {
        for (int c = 0; c < components; ++c) {
            const float value = at<float>(s.decoded, size_t(i) * s.stride + c * 4);
            lo[c] = std::min(lo[c], value);
            hi[c] = std::max(hi[c], value);
        }
    }
    return { lo, hi };
}

Model buildModel(bool khr)
{
    const int version = khr ? 1 : 0;
    meshopt_encodeVertexVersion(version);
    meshopt_encodeIndexVersion(1);
    Model model;

    // --- torus: quantized, cache-optimised triangle list
    Geometry t = torus(48, 24, 1.0f, 0.35f);
    meshopt_optimizeVertexCache(t.indices.data(), t.indices.data(), t.indices.size(), t.vertices());
    const uint32_t tv = uint32_t(t.vertices());
    float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
    for (size_t i = 0; i < tv; ++i)
        for (int c = 0; c < 3; ++c) {
            lo[c] = std::min(lo[c], t.positions[i * 3 + c]);
            hi[c] = std::max(hi[c], t.positions[i * 3 + c]);
        }
    const float extent = std::max({ hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2] });
    std::memcpy(model.torusOrigin, lo, sizeof(lo));
    model.torusScale = extent / 65535.0f;
    {
        std::vector<uint16_t> q(tv * 4, 0);
        uint16_t qmin[3] = { 65535, 65535, 65535 }, qmax[3] = { 0, 0, 0 };
        for (uint32_t i = 0; i < tv; ++i)
            for (int c = 0; c < 3; ++c) {
                q[i * 4 + c] = uint16_t(std::lround((t.positions[i * 3 + c] - lo[c]) / extent * 65535.0f));
                qmin[c] = std::min(qmin[c], q[i * 4 + c]);
                qmax[c] = std::max(qmax[c], q[i * 4 + c]);
            }
        Stream s { "torus POSITION", Mode::Attributes, Filter::None, 8, tv, bytesOf(q) };
        s.vertexStride = true;
        s.target = 34962;
        const uint32_t view = addStream(model, s, version);
        addAccessor(model, view, 5123, tv, "VEC3", false, { qmin[0], qmin[1], qmin[2] }, { qmax[0], qmax[1], qmax[2] });
    }
    {
        std::vector<float> n4(tv * 4);
        for (uint32_t i = 0; i < tv; ++i) {
            n4[i * 4 + 0] = t.normals[i * 3 + 0];
            n4[i * 4 + 1] = t.normals[i * 3 + 1];
            n4[i * 4 + 2] = t.normals[i * 3 + 2];
            n4[i * 4 + 3] = 1.0f;
        }
        Stream s { "torus NORMAL", Mode::Attributes, Filter::Octahedral, 4, tv };
        s.input.resize(size_t(tv) * 4);
        meshopt_encodeFilterOct(s.input.data(), tv, 4, 8, n4.data());
        s.vertexStride = true;
        s.target = 34962;
        const uint32_t view = addStream(model, s, version);
        const Stream& d = model.streams[view];
        double worst = 0;
        for (uint32_t i = 0; i < tv; ++i)
            for (int c = 0; c < 3; ++c)
                worst = std::max(worst, std::abs(at<int8_t>(d.decoded, i * 4 + c) / 127.0 - t.normals[i * 3 + c]));
        check(worst < 0.03, "torus NORMAL: OCTAHEDRAL decode within 0.03 of the source normals (" + std::to_string(worst) + ")");
        addAccessor(model, view, 5120, tv, "VEC3", true);
    }
    {
        std::vector<uint16_t> uv(tv * 2);
        for (uint32_t i = 0; i < tv * 2; ++i) uv[i] = uint16_t(std::lround(t.uvs[i] * 65535.0f));
        Stream s { "torus TEXCOORD_0", Mode::Attributes, Filter::None, 4, tv, bytesOf(uv) };
        s.vertexStride = true;
        s.target = 34962;
        addAccessor(model, addStream(model, s, version), 5123, tv, "VEC2", true);
    }
    {
        Stream s { "torus COLOR_0", Mode::Attributes, khr ? Filter::Color : Filter::None, 4, tv };
        s.input.resize(size_t(tv) * 4);
        if (khr) {
            meshopt_encodeFilterColor(s.input.data(), tv, 4, 8, t.colors.data());
        } else {
            for (uint32_t i = 0; i < tv * 4; ++i) s.input[i] = uint8_t(std::lround(t.colors[i] * 255.0f));
        }
        s.vertexStride = true;
        s.target = 34962;
        const uint32_t view = addStream(model, s, version);
        const Stream& d = model.streams[view];
        double worst = 0;
        for (uint32_t i = 0; i < tv * 4; ++i) worst = std::max(worst, std::abs(d.decoded[i] / 255.0 - t.colors[i]));
        check(worst < 0.02, std::string("torus COLOR_0: ") + (khr ? "COLOR filter" : "unfiltered") + " decode within 0.02 (" + std::to_string(worst) + ")");
        addAccessor(model, view, 5121, tv, "VEC4", true);
    }
    {
        std::vector<uint16_t> idx(t.indices.begin(), t.indices.end());
        Stream s { "torus indices", Mode::Triangles, Filter::None, 2, uint32_t(idx.size()), bytesOf(idx) };
        s.target = 34963;
        addAccessor(model, addStream(model, s, version), 5123, uint32_t(idx.size()), "SCALAR", false);
    }

    // --- sphere: float positions (EXPONENTIAL), int16 octahedral normals, index sequence
    Geometry sp = sphere(32, 16, 0.6f);
    const uint32_t sv = uint32_t(sp.vertices());
    {
        Stream s { "sphere POSITION", Mode::Attributes, Filter::Exponential, 12, sv };
        s.input.resize(size_t(sv) * 12);
        meshopt_encodeFilterExp(s.input.data(), sv, 12, 15, sp.positions.data(), meshopt_EncodeExpSharedVector);
        s.vertexStride = true;
        s.target = 34962;
        const uint32_t view = addStream(model, s, version);
        const Stream& d = model.streams[view];
        double worst = 0;
        for (uint32_t i = 0; i < sv * 3; ++i) worst = std::max(worst, double(std::abs(at<float>(d.decoded, i * 4) - sp.positions[i])));
        check(worst < 1e-3, "sphere POSITION: EXPONENTIAL decode within 1e-3 (" + std::to_string(worst) + ")");
        auto [mn, mx] = floatBounds(d, 3);
        addAccessor(model, view, 5126, sv, "VEC3", false, mn, mx);
    }
    {
        std::vector<float> n4(sv * 4);
        for (uint32_t i = 0; i < sv; ++i) {
            n4[i * 4 + 0] = sp.normals[i * 3 + 0];
            n4[i * 4 + 1] = sp.normals[i * 3 + 1];
            n4[i * 4 + 2] = sp.normals[i * 3 + 2];
            n4[i * 4 + 3] = 1.0f;
        }
        Stream s { "sphere NORMAL", Mode::Attributes, Filter::Octahedral, 8, sv };
        s.input.resize(size_t(sv) * 8);
        meshopt_encodeFilterOct(s.input.data(), sv, 8, 12, n4.data());
        s.vertexStride = true;
        s.target = 34962;
        const uint32_t view = addStream(model, s, version);
        const Stream& d = model.streams[view];
        double worst = 0;
        for (uint32_t i = 0; i < sv; ++i)
            for (int c = 0; c < 3; ++c)
                worst = std::max(worst, std::abs(at<int16_t>(d.decoded, i * 8 + c * 2) / 32767.0 - sp.normals[i * 3 + c]));
        check(worst < 0.003, "sphere NORMAL: 16-bit OCTAHEDRAL decode within 0.003 (" + std::to_string(worst) + ")");
        addAccessor(model, view, 5122, sv, "VEC3", true);
    }
    {
        Stream s { "sphere TEXCOORD_0", Mode::Attributes, Filter::None, 8, sv, bytesOf(sp.uvs) };
        s.vertexStride = true;
        s.target = 34962;
        addAccessor(model, addStream(model, s, version), 5126, sv, "VEC2", false);
    }
    {
        Stream s { "sphere indices", Mode::Indices, Filter::None, 4, uint32_t(sp.indices.size()), bytesOf(sp.indices) };
        s.target = 34963;
        addAccessor(model, addStream(model, s, version), 5125, uint32_t(sp.indices.size()), "SCALAR", false);
    }

    // --- animation of the sphere node: times, rotation (QUATERNION), translation (EXPONENTIAL)
    const uint32_t keys = 9;
    std::vector<float> times(keys), rotations(keys * 4), translations(keys * 3);
    for (uint32_t k = 0; k < keys; ++k) {
        times[k] = 0.25f * k;
        const double angle = 2 * kPi * k / (keys - 1) * 0.5; // half a turn about Y and an X tilt
        const double tilt = 0.4 * std::sin(2 * kPi * k / (keys - 1));
        // q = qy(angle) * qx(tilt)
        const double cy = std::cos(angle / 2), sy = std::sin(angle / 2), cx = std::cos(tilt / 2), sx = std::sin(tilt / 2);
        rotations[k * 4 + 0] = float(cy * sx);
        rotations[k * 4 + 1] = float(sy * cx);
        rotations[k * 4 + 2] = float(-sy * sx);
        rotations[k * 4 + 3] = float(cy * cx);
        translations[k * 3 + 0] = 1.6f;
        translations[k * 3 + 1] = float(0.3 * std::sin(2 * kPi * k / (keys - 1)));
        translations[k * 3 + 2] = 0.0f;
    }
    {
        Stream s { "animation input", Mode::Attributes, Filter::None, 4, keys, bytesOf(times) };
        addAccessor(model, addStream(model, s, version), 5126, keys, "SCALAR", false, { times.front() }, { times.back() });
    }
    {
        Stream s { "animation rotation", Mode::Attributes, Filter::Quaternion, 8, keys };
        s.input.resize(size_t(keys) * 8);
        meshopt_encodeFilterQuat(s.input.data(), keys, 8, 12, rotations.data());
        const uint32_t view = addStream(model, s, version);
        const Stream& d = model.streams[view];
        double worst = 0;
        for (uint32_t k = 0; k < keys; ++k) {
            double dot = 0;
            for (int c = 0; c < 4; ++c) dot += at<int16_t>(d.decoded, k * 8 + c * 2) / 32767.0 * rotations[k * 4 + c];
            worst = std::max(worst, 1.0 - std::abs(dot));
        }
        check(worst < 1e-4, "animation rotation: QUATERNION decode within 1 - |dot| < 1e-4 (" + std::to_string(worst) + ")");
        addAccessor(model, view, 5122, keys, "VEC4", true);
    }
    {
        Stream s { "animation translation", Mode::Attributes, Filter::Exponential, 12, keys };
        s.input.resize(size_t(keys) * 12);
        meshopt_encodeFilterExp(s.input.data(), keys, 12, 20, translations.data(), meshopt_EncodeExpSeparate);
        const uint32_t view = addStream(model, s, version);
        const Stream& d = model.streams[view];
        double worst = 0;
        for (uint32_t i = 0; i < keys * 3; ++i) worst = std::max(worst, double(std::abs(at<float>(d.decoded, i * 4) - translations[i])));
        check(worst < 1e-4, "animation translation: EXPONENTIAL decode within 1e-4 (" + std::to_string(worst) + ")");
        addAccessor(model, view, 5126, keys, "VEC3", false);
    }
    return model;
}

enum class Variant { Compressed, Plain, Optional };

// The glTF JSON plus the binary buffers (buffer 0 first; GLB stores buffer 0 in its BIN chunk).
struct Output {
    nlohmann::json json;
    std::vector<std::vector<uint8_t>> buffers;
};

Output assemble(const Model& model, bool khr, Variant variant, bool corrupt)
{
    const std::string ext = khr ? "KHR_meshopt_compression" : "EXT_meshopt_compression";
    // The buffer bytes: plain = decoded streams, compressed = encoded streams, each 4-byte aligned.
    std::vector<uint8_t> plain, compressed;
    for (const Stream& s : model.streams) {
        align4(plain);
        align4(compressed);
        if (variant != Variant::Plain) {
            std::vector<uint8_t> encoded = s.encoded;
            if (corrupt && s.name == "torus POSITION") {
                encoded[0] = 0x00; // invalid vertex codec header (0xA0 | version expected)
                for (size_t i = 1; i < encoded.size(); i += 3) encoded[i] ^= 0x5a;
            }
            compressed.insert(compressed.end(), encoded.begin(), encoded.end());
        }
        plain.insert(plain.end(), s.decoded.begin(), s.decoded.end());
    }
    align4(plain);
    align4(compressed);

    nlohmann::json used = { "KHR_mesh_quantization" }, required = { "KHR_mesh_quantization" };
    Output out;
    if (variant == Variant::Plain) {
        out.json["buffers"] = { { { "byteLength", plain.size() } } };
        out.buffers = { plain };
    } else {
        used.push_back(ext);
        if (variant == Variant::Compressed) required.push_back(ext);
        nlohmann::json fallback = { { "byteLength", plain.size() }, { "extensions", { { ext, { { "fallback", true } } } } } };
        out.json["buffers"] = { { { "byteLength", compressed.size() } }, fallback };
        out.buffers = { compressed };
        if (variant == Variant::Optional) out.buffers.push_back(plain); // uncompressed data for loaders without it
    }
    out.json["asset"] = { { "version", "2.0" }, { "generator", "Glider meshopt_fixture_gen (meshoptimizer " + std::to_string(MESHOPTIMIZER_VERSION) + ")" } };
    out.json["extensionsUsed"] = used;
    out.json["extensionsRequired"] = required;
    out.json["accessors"] = model.accessors;
    out.json["materials"] = {
        { { "name", "torus" }, { "pbrMetallicRoughness", { { "baseColorFactor", { 1, 1, 1, 1 } }, { "metallicFactor", 0.0 }, { "roughnessFactor", 0.45 } } } },
        { { "name", "sphere" }, { "pbrMetallicRoughness", { { "baseColorFactor", { 0.9, 0.55, 0.2, 1 } }, { "metallicFactor", 0.6 }, { "roughnessFactor", 0.3 } } } },
    };
    out.json["meshes"] = {
        { { "name", "torus" }, { "primitives", { { { "attributes", { { "POSITION", 0 }, { "NORMAL", 1 }, { "TEXCOORD_0", 2 }, { "COLOR_0", 3 } } }, { "indices", 4 }, { "material", 0 } } } } },
        { { "name", "sphere" }, { "primitives", { { { "attributes", { { "POSITION", 5 }, { "NORMAL", 6 }, { "TEXCOORD_0", 7 } } }, { "indices", 8 }, { "material", 1 } } } } },
    };
    out.json["nodes"] = {
        { { "name", "torus_root" }, { "translation", { -1.6, 0.0, 0.0 } }, { "rotation", { 0.3826834, 0.0, 0.0, 0.9238795 } }, { "children", { 1 } } },
        { { "name", "torus_dequantize" }, { "mesh", 0 }, { "translation", { model.torusOrigin[0], model.torusOrigin[1], model.torusOrigin[2] } },
            { "scale", { model.torusScale, model.torusScale, model.torusScale } } },
        { { "name", "sphere" }, { "mesh", 1 }, { "translation", { 1.6, 0.0, 0.0 } } },
    };
    out.json["animations"] = { { { "name", "sphere_motion" },
        { "samplers", { { { "input", 9 }, { "output", 10 }, { "interpolation", "LINEAR" } }, { { "input", 9 }, { "output", 11 }, { "interpolation", "LINEAR" } } } },
        { "channels", { { { "sampler", 0 }, { "target", { { "node", 2 }, { "path", "rotation" } } } }, { { "sampler", 1 }, { "target", { { "node", 2 }, { "path", "translation" } } } } } } } };
    out.json["scenes"] = { { { "name", "meshopt" }, { "nodes", { 0, 2 } } } };
    out.json["scene"] = 0;
    // The same layout as the bytes above: one bufferView per stream.
    out.json["bufferViews"] = nlohmann::json::array();
    {
        size_t plainOffset = 0, compressedOffset = 0;
        for (const Stream& s : model.streams) {
            plainOffset = (plainOffset + 3) & ~size_t(3);
            compressedOffset = (compressedOffset + 3) & ~size_t(3);
            nlohmann::json view = { { "buffer", variant == Variant::Plain ? 0 : 1 }, { "byteOffset", plainOffset }, { "byteLength", s.decoded.size() } };
            if (s.vertexStride) view["byteStride"] = s.stride;
            if (s.target) view["target"] = s.target;
            if (variant != Variant::Plain) {
                nlohmann::json meshopt = { { "buffer", 0 }, { "byteOffset", compressedOffset }, { "byteLength", s.encoded.size() },
                    { "byteStride", s.stride }, { "count", s.count }, { "mode", modeName(s.mode) } };
                if (s.filter != Filter::None) meshopt["filter"] = filterName(s.filter);
                view["extensions"] = { { ext, meshopt } };
            }
            out.json["bufferViews"].push_back(view);
            plainOffset += s.decoded.size();
            compressedOffset += s.encoded.size();
        }
    }
    return out;
}

bool writeFile(const std::filesystem::path& path, const std::vector<uint8_t>& bytes)
{
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    file.close();
    check(bool(file), "write " + path.string());
    return bool(file);
}

void put32(std::vector<uint8_t>& out, uint32_t value)
{
    for (int i = 0; i < 4; ++i) out.push_back(uint8_t(value >> (8 * i)));
}

size_t writeGlb(const std::filesystem::path& path, Output output)
{
    std::string json = output.json.dump();
    while (json.size() % 4) json.push_back(' ');
    std::vector<uint8_t> bin = output.buffers[0];
    while (bin.size() % 4) bin.push_back(0);
    std::vector<uint8_t> glb;
    put32(glb, 0x46546C67);
    put32(glb, 2);
    put32(glb, uint32_t(12 + 8 + json.size() + 8 + bin.size()));
    put32(glb, uint32_t(json.size()));
    put32(glb, 0x4E4F534A);
    glb.insert(glb.end(), json.begin(), json.end());
    put32(glb, uint32_t(bin.size()));
    put32(glb, 0x004E4942);
    glb.insert(glb.end(), bin.begin(), bin.end());
    writeFile(path, glb);
    return glb.size();
}

size_t writeGltf(const std::filesystem::path& path, Output output)
{
    size_t size = 0;
    for (size_t i = 0; i < output.buffers.size(); ++i) {
        const std::string bin = path.stem().string() + (i == 0 ? ".bin" : "_fallback.bin");
        output.json["buffers"][i]["uri"] = bin;
        writeFile(path.parent_path() / bin, output.buffers[i]);
        size += output.buffers[i].size();
    }
    const std::string json = output.json.dump(1, '\t');
    writeFile(path, std::vector<uint8_t>(json.begin(), json.end()));
    return json.size() + size;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::fprintf(stderr, "usage: meshopt_fixture_gen <output folder>\n");
        return 2;
    }
    const std::filesystem::path folder = argv[1];
    std::error_code error;
    std::filesystem::create_directories(folder, error);
    if (error) {
        std::fprintf(stderr, "cannot create %s\n", folder.string().c_str());
        return 2;
    }
    nlohmann::json summary;
    summary["meshoptimizer"] = MESHOPTIMIZER_VERSION;
    for (const bool khr : { false, true }) {
        const Model model = buildModel(khr);
        const std::string prefix = khr ? "khr" : "ext";
        size_t encoded = 0, decoded = 0;
        for (const Stream& s : model.streams) {
            encoded += s.encoded.size();
            decoded += s.decoded.size();
        }
        summary[prefix] = { { "streams", model.streams.size() }, { "encodedBytes", encoded }, { "decodedBytes", decoded },
            { "glb", writeGlb(folder / (prefix + ".glb"), assemble(model, khr, Variant::Compressed, false)) },
            { "plainGlb", writeGlb(folder / (prefix + "_plain.glb"), assemble(model, khr, Variant::Plain, false)) } };
        if (!khr) {
            summary["ext_gltf"] = writeGltf(folder / "ext_gltf.gltf", assemble(model, khr, Variant::Compressed, false));
            summary["ext_corrupt"] = writeGlb(folder / "ext_corrupt.glb", assemble(model, khr, Variant::Compressed, true));
            summary["ext_optional"] = writeGltf(folder / "ext_optional.gltf", assemble(model, khr, Variant::Optional, false));
        }
    }
    summary["failures"] = g_failures;
    std::printf("%s\n", summary.dump().c_str());
    return g_failures == 0 ? 0 : 1;
}
