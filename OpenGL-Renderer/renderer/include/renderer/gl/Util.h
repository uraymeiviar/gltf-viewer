// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "renderer/gl/Enums.h"
#include "renderer/gl/GL.h"

namespace ux3d::renderer::gl {
/// If the GL context has an error, print it and return false.
bool checkGLError(GL*, const char* message, bool printLog = true);

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
