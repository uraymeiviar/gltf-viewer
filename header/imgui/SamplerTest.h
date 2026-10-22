// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "imgui/SelfCheckResult.h"
#include <functional>
#include <string>

namespace ux3d::renderer {
class Renderer;
}

namespace ux3d::renderer::gl {
class GL;
}

namespace ux3d::imgui {

SelfCheckResult runSamplerSelfCheck(
    ux3d::renderer::gl::GL* gl,
    bool forceFailure = false,
    const std::string& testCase = "all",
    const std::string& imageDumpPath = "",
    ux3d::renderer::Renderer* glRenderer = nullptr,
    std::function<bool(const std::string&)> reloadAndDraw = nullptr
);

} // namespace ux3d::imgui
