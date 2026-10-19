// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "gltf/Gltf.h"
#include "imgui/AnimationPlayer.h"
#include "core/Logger.h"
#include <imgui.h>
#include <string>
#include <vector>

namespace ux3d::renderer {
class GltfRenderLayer;
}

namespace ux3d::imgui {

struct LogEntry {
    std::string text;
    ImVec4 color;
    ux3d::core::LogLevel level = ux3d::core::LogLevel::Info;
};

class AntiAliasing;

class StatusBar {
public:
    static void render(ux3d::gltf::Gltf& gltfModel,
                       ux3d::renderer::GltfRenderLayer* gltfLayer,
                       AnimationPlayer& animationPlayer,
                       bool& isSceneDirty,
                       bool& showLogger,
                       bool& showValidation,
                       const std::vector<LogEntry>& consoleLogs,
                       AntiAliasing* antiAliasing = nullptr);
};

} // namespace ux3d::imgui
