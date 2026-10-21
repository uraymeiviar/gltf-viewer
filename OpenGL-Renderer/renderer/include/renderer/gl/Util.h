// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "renderer/gl/Enums.h"
#include "renderer/gl/GL.h"

namespace ux3d::renderer::gl {
/// If the GL context has an error, print it and return false.
bool checkGLError(GL*, const char* message, bool printLog = true);
} // namespace ux3d::renderer::gl

// Diagnostics at command, uniform, sampler, texture-bind and state-setup
// granularity are expensive because glGetError may synchronize with the
// driver. Keep them in Debug and in explicitly requested diagnostic Release
// builds; coarse pass/resource checks call checkGLError directly and remain in
// every configuration.
#if defined(GLIDER_GL_DEBUG_CHECKS) || defined(_DEBUG) || !defined(NDEBUG)
#define GLIDER_HOT_GL_CHECK(glHandle, message) ::ux3d::renderer::gl::checkGLError((glHandle), (message))
#else
#define GLIDER_HOT_GL_CHECK(glHandle, message) ((void)0)
#endif

namespace ux3d::renderer::gl {

struct ShaderLocation {
    GLuint Location = invalid; // binding
    PrimitiveType Type = PrimitiveType::Invalid;
    GLint Size = 0u;
    GLuint BlockIndex = invalid; // uniform block index
    GLint Slot = -1; // texture unit
};

GLuint getByteSize(PrimitiveType);
GLint getComponentCount(PrimitiveType);
GLenum getComponentType(PrimitiveType);
}
