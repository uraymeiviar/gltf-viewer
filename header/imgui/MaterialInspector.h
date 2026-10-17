// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "gltf/Gltf.h"
#include "gltf/Material.h"
#include "gltf/Texture.h"

namespace ux3d::imgui {

class MaterialInspector {
public:
    // Renders the full material inspector GUI
    // Returns true if any material property was modified
    static bool render(ux3d::gltf::Material* material, ux3d::gltf::Gltf& gltfModel, bool& isSceneDirty);

private:
    static bool renderTextureSlot(const char* label,
                                 std::optional<ux3d::gltf::TextureInfo>& texInfo,
                                 ux3d::gltf::Gltf& gltfModel,
                                 bool showScale = false,
                                 bool showStrength = false);
};

} // namespace ux3d::imgui
