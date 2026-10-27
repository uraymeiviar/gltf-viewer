// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "gltf/Gltf.h"
#include "imgui/SceneRevisions.h"
#include "imgui/ResourcesPanel.h"
#include "imgui/ViewportInstance.h"
#include <vector>
#include <memory>

namespace ux3d::renderer {
class GltfRenderLayer;
class CommandGenerator;
}

namespace ux3d::imgui {

class AnimationPlayer;

class ElementInspector {
public:
    static void render(SelectionState& selection,
                       ux3d::gltf::Gltf& gltfModel,
                       SceneRevisions& revisions,
                       AnimationPlayer* animationPlayer = nullptr,
                       ux3d::renderer::GltfRenderLayer* gltfLayer = nullptr,
                       std::vector<std::unique_ptr<ViewportInstance>>* viewports = nullptr,
                       ux3d::renderer::CommandGenerator* commandGenerator = nullptr);

    static void clearPreviewCache();

    // Returns a cached preview texture for an image, or 0 when no preview can
    // be built. Shared by the image preview and the material texture slots.
    static unsigned int getImagePreviewTextureId(const ux3d::gltf::Image* image,
                                                 ux3d::renderer::GltfRenderLayer* gltfLayer,
                                                 bool signedSquareNormal = false);
};

} // namespace ux3d::imgui
