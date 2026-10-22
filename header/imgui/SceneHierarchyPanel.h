// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "imgui/ResourcesPanel.h"
#include "imgui/SceneRevisions.h"
#include "imgui/SelfCheckResult.h"
#include "imgui/AntiAliasing.h"
#include "imgui/ViewportInstance.h"
#include "renderViewInput/CameraActor.h"
#include "gltf/Gltf.h"
#include <vector>
#include <memory>
#include <cstdint>

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
                       SceneRevisions& revisions,
                       bool* pOpen = nullptr,
                       std::vector<std::unique_ptr<ViewportInstance>>* viewports = nullptr,
                       ViewportInstance** activeViewport = nullptr);

    static void requestScrollToSelectedNode();
    static void resetSearch();
    static void setSearchFilter(const std::string& filter);

    static uint64_t rowModelBuildCount();
    static void resetRowModelBuildCount();

    // Focused P2.2 warm-cache invalidation check used by the command-line
    // regression run. It calls the same cache implementation used by render().
    static SelfCheckResult runCacheInvalidationSelfCheck(bool forceFailure = false);
};

} // namespace ux3d::imgui
