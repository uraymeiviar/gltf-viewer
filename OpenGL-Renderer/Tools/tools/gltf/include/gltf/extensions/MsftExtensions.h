// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

// Microsoft vendor extensions Glider recognises besides MSFT_texture_dds (KhronosGroup/glTF extensions/2.0/Vendor):
//   * MSFT_lod (node or material): `ids` lists the lower levels of detail of the object that carries it (the
//     highest LOD); extras.MSFT_screencoverage holds the screen-coverage switch hints. Glider renders the highest LOD
//     (what the file draws; the lower LOD nodes are normally not in the scene) and lists the chain (Inspector).
//   * MSFT_packing_occlusionRoughnessMetallic (material): occlusionRoughnessMetallicTexture uses the core channel
//     layout (R occlusion, G roughness, B metallic), so the renderer binds it as the metallic-roughness and occlusion
//     texture of a material that has no core one (the spec makes it an alternative packing of the same data).
//     roughnessMetallicOcclusionTexture (R / G / B = roughness / metallic / occlusion) and the two-channel normalTexture
//     are recognised but not used.
//   * MSFT_packing_normalRoughnessMetallic (material): recognised only (its RG normal / B roughness / A metallic
//     texture would need its own shader decoding; the core textures are rendered).
// The loader keeps all three as unknown extensions (JSON text); these helpers read that text.

#include <string>
#include <vector>

namespace ux3d::gltf {
class GltfObject;

namespace msft {
inline constexpr const char* Lod = "MSFT_lod";
inline constexpr const char* PackingOrm = "MSFT_packing_occlusionRoughnessMetallic";
inline constexpr const char* PackingNrm = "MSFT_packing_normalRoughnessMetallic";

struct LodInfo {
    bool present = false;              // the object carries MSFT_lod
    std::vector<int> ids;              // lower LOD indices (nodes or materials), highest quality first
    std::vector<double> screenCoverage; // extras.MSFT_screencoverage (LOD0 first), may be empty
};
LodInfo lod(const GltfObject& object);

// A texture reference read from an unknown extension: textures[index], texCoord (-1: absent).
struct TextureRef {
    int index = -1;
    int texCoord = 0;
};
// MSFT_packing_occlusionRoughnessMetallic.occlusionRoughnessMetallicTexture of a material (index -1 when absent).
TextureRef packedOcclusionRoughnessMetallic(const GltfObject& material);
// The names of the texture properties a packing extension object carries (for the Inspector), e.g.
// "occlusionRoughnessMetallicTexture -> texture 4".
std::vector<std::string> packingSummary(const GltfObject& material);
}
}
