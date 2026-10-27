// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/GlCaps.h"

#include <glad/glad.h>

namespace ux3d::imgui {

namespace {
GlCaps s_caps;
}

void initGlCaps()
{
    GlCaps caps;
    GLint major = 0;
    GLint minor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    // Drain a possible error from a context that does not know these queries.
    while (glGetError() != GL_NO_ERROR) {
    }
    caps.major = major;
    caps.minor = minor;
    caps.compute = (major > 4 || (major == 4 && minor >= 3)) && GLAD_GL_VERSION_4_3 != 0 &&
                   glDispatchCompute != nullptr;
    if (caps.compute) {
        GLint value = 0;
        glGetIntegerv(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, &value);
        caps.maxComputeWorkGroupInvocations = value;
        for (GLuint i = 0; i < 3; ++i) {
            value = 0;
            glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, i, &value);
            caps.maxComputeWorkGroupSize[i] = value;
            value = 0;
            glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, i, &value);
            caps.maxComputeWorkGroupCount[i] = value;
        }
    }
    s_caps = caps;
}

const GlCaps& glCaps()
{
    return s_caps;
}

} // namespace ux3d::imgui
