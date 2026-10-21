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

struct LogSeverityCounts {
    size_t errors = 0;
    size_t warnings = 0;
    size_t info = 0;
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
                       const LogSeverityCounts& logCounts,
                       AntiAliasing* antiAliasing = nullptr);
};

} // namespace ux3d::imgui
