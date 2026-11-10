// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "gltf/Gltf.h"
#include "imgui/SceneRevisions.h"
#include "gltf/Material.h"
#include "gltf/Texture.h"

namespace ux3d::renderer {
class GltfRenderLayer;
}

namespace ux3d::imgui {

struct SelectionState;

class MaterialInspector {
public:
    // Renders the full material inspector GUI
    // Returns true if any material property was modified
    static bool render(ux3d::gltf::Material* material,
                       ux3d::gltf::Gltf& gltfModel,
                       SceneRevisions& revisions,
                       SelectionState& selection,
                       ux3d::renderer::GltfRenderLayer* gltfLayer = nullptr);

    // A material extension's texture (ASOBO and other vendor extensions) as a read-only slot: the same preview,
    // texture button, UV set and UV layout rows as a core slot, without scale or strength.
    // normalMap: the image is a normal map (signed-square decoded in a packaged file).
    static void renderExtensionTextureSlot(const char* label,
                                           ux3d::gltf::Texture* texture,
                                           uint32_t texCoord,
                                           ux3d::gltf::Gltf& gltfModel,
                                           ux3d::gltf::Material* material,
                                           SelectionState& selection,
                                           ux3d::renderer::GltfRenderLayer* gltfLayer,
                                           bool normalMap);

private:
    static bool renderTextureSlot(const char* label,
                                  std::optional<ux3d::gltf::TextureInfo>& texInfo,
                                  ux3d::gltf::Gltf& gltfModel,
                                  ux3d::gltf::Material* material,
                                  SelectionState& selection,
                                  ux3d::renderer::GltfRenderLayer* gltfLayer,
                                  bool showScale = false,
                                  bool showStrength = false,
                                  bool normalMap = false);
};

} // namespace ux3d::imgui
