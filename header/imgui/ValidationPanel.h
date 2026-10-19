// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "gltf/Gltf.h"
#include "imgui/GltfValidator.h"
#include "imgui/ResourcesPanel.h"
#include "renderViewInput/CameraActor.h"
#include <string>

namespace ux3d::renderer {
class GltfRenderLayer;
}

namespace ux3d::imgui {

class ValidationPanel {
public:
    static void render(bool* pOpen,
                      const std::string& currentFilePath,
                      ux3d::gltf::Gltf& gltfModel,
                      ux3d::imgui::SelectionState& selection,
                      ux3d::editor::CameraActor& cameraActor,
                      ux3d::renderer::GltfRenderLayer* gltfLayer);

    // Quick status indicator for status bar
    static void renderStatusBarIndicator(bool* pOpenValidationPanel);

    // Request tab focus on next render
    static void requestFocus();
};

} // namespace ux3d::imgui
