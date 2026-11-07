// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// CPU readers for glTF vertex attributes and triangles, shared by TextureMetrics (the Texture Distortion / Density
// views) and the Image Viewer's UV layout overlay. Normalized integer components are scaled as the spec says
// (UNSIGNED_BYTE / 255, UNSIGNED_SHORT / 65535, signed with the -1 clamp), packaged (ASOBO_asset_optimized)
// TEXCOORD_n in SHORT / USHORT without `normalized` are half floats, and triangles come from every triangle mode
// (TRIANGLES, TRIANGLE_STRIP, TRIANGLE_FAN; indexed or not) with the renderer's index slice (ASOBO_primitive).

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace ux3d::gltf {
class Accessor;
class Primitive;
} // namespace ux3d::gltf

namespace ux3d::imgui::mesh_attributes {

// One float array per accessor: an MSFS package shares vertex accessors between many primitives.
using Floats = std::shared_ptr<const std::vector<float>>;

// components: how many are read per element (a packaged VEC4 NORMAL-style padding is skipped by the stride).
// halfFloat: packaged TEXCOORD_n stored as half floats in SHORT / USHORT. Null when the data is unreadable.
Floats decode(const ux3d::gltf::Accessor& accessor, uint32_t components, bool halfFloat);

// True for a packaged file's TEXCOORD_n accessor stored as half floats (SHORT / USHORT, not normalized).
bool uvIsHalfFloat(const ux3d::gltf::Accessor& accessor, bool packaged);

class AccessorCache {
public:
    Floats read(const ux3d::gltf::Accessor* accessor, uint32_t components, bool halfFloat);

private:
    std::unordered_map<const ux3d::gltf::Accessor*, Floats> m_cache;
};

// The primitive's triangles in draw order with vertex indices into its accessors. indexCount / indexByteOffset /
// baseVertex: the drawn index slice (renderer::Mesh; the whole index accessor is count, 0, 0). Points and lines
// give no triangles. False when the indices are unreadable or reach past vertexCount.
bool readTriangles(const ux3d::gltf::Primitive& primitive, size_t indexCount, size_t indexByteOffset, int64_t baseVertex,
                   size_t vertexCount, std::vector<std::array<uint32_t, 3>>& triangles);

} // namespace ux3d::imgui::mesh_attributes
