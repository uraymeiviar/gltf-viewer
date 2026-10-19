// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "imgui/ResourcesPanel.h"
#include "imgui/AntiAliasing.h"
#include "imgui/ViewportInstance.h"
#include "renderViewInput/CameraActor.h"
#include "gltf/Gltf.h"
#include <vector>
#include <memory>

namespace ux3d::renderer {
class GltfRenderLayer;
}

namespace ux3d::imgui {

class SceneHierarchyPanel {
public:
    static void render(ux3d::gltf::Gltf& gltfModel,
                       ux3d::gltf::Scene* activeScene,
                       SelectionState& selection,
                       ux3d::renderer::GltfRenderLayer* gltfLayer,
                       ux3d::editor::CameraActor& cameraActor,
                       AntiAliasing& antiAliasing,
                       bool& isSceneDirty,
                       bool* pOpen = nullptr,
                       std::vector<std::unique_ptr<ViewportInstance>>* viewports = nullptr,
                       ViewportInstance** activeViewport = nullptr);

    static void requestScrollToSelectedNode();
    static void resetSearch();
    static void setSearchFilter(const std::string& filter);
};

} // namespace ux3d::imgui
