// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "gltf/Gltf.h"
#include "imgui/GltfValidator.h"
#include "imgui/ResourcesPanel.h"
#include "imgui/SelfCheckResult.h"
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

    // Deterministic test controls used by command-line screenshot checks.
    static void setSearchFilter(const std::string& search);
    static void setSeverityFilter(bool errors, bool warnings, bool infos);

    // Reproducible P2.2 cache check and microbenchmark for the issue filter.
    static SelfCheckResult runCacheSelfCheck(bool forceFailure = false);

    // Focused warm-cache check: replace the report with an equal-length report
    // containing different issues and severities while filters stay unchanged.
    static SelfCheckResult runReportRevisionSelfCheck(bool forceFailure = false);
};

} // namespace ux3d::imgui
