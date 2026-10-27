// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

namespace ux3d::imgui {

// Capabilities of the OpenGL context that was actually obtained. Glider asks for 4.6 core and
// falls back to 4.5, 4.3 and finally 4.1 (see Application::run). Code that needs compute
// shaders, SSBOs or image load/store must test glCaps().compute first.
struct GlCaps {
    int major = 0;
    int minor = 0;
    // True for a 4.3+ context: compute shaders, SSBOs and image load/store are available.
    bool compute = false;
    int maxComputeWorkGroupInvocations = 0; // GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, 0 below 4.3
    int maxComputeWorkGroupSize[3] = { 0, 0, 0 };
    int maxComputeWorkGroupCount[3] = { 0, 0, 0 };
};

// Queries the current context (after the GL loader ran) and stores the result.
void initGlCaps();

const GlCaps& glCaps();

} // namespace ux3d::imgui
