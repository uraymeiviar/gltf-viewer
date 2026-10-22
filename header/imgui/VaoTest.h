// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "imgui/SelfCheckResult.h"
#include <functional>
#include <string>

namespace ux3d::renderer {
class Renderer;
class CommandGenerator;
}

namespace ux3d::renderer::gl {
class GL;
}

namespace ux3d::imgui {

SelfCheckResult runVaoSelfCheck(
    ux3d::renderer::gl::GL* gl,
    bool forceFailure = false,
    const std::string& testCase = "all",
    ux3d::renderer::Renderer* glRenderer = nullptr,
    ux3d::renderer::CommandGenerator* commandGenerator = nullptr,
    std::function<bool(const std::string&)> reloadAndDraw = nullptr
);

} // namespace ux3d::imgui
