// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "imgui/SelfCheckResult.h"
#include <string>
#include <functional>

namespace ux3d::renderer {
class Renderer;
class CommandGenerator;
namespace gl {
class GL;
}
}

namespace ux3d::imgui {

SelfCheckResult runDepthSelfCheck(
    ux3d::renderer::gl::GL* gl,
    bool forceFailure,
    const std::string& testCase,
    ux3d::renderer::Renderer* glRenderer,
    ux3d::renderer::CommandGenerator* commandGenerator,
    std::function<bool(const std::string&)> reloadAndDraw
);

} // namespace ux3d::imgui
