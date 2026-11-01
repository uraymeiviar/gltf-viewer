// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "Enums.h"

#include <string>

namespace ux3d::renderer::gl {
class Buffer;

struct Attribute {
    Attribute(const std::string& name)
        : name(name)
    {
    }
    const std::string name;
    GLboolean normalized = GL_FALSE;
    GLsizei stride = 0;
    GLsizei offset = 0;
    Buffer* buffer = nullptr;
    GLint dimension = 3;
    ComponentType type = ComponentType::Float;
    // EXT_mesh_gpu_instancing: a per-instance stream (glVertexAttribDivisor) and, for one column of a matrix
    // attribute, the column's location after the attribute's own (a mat4 occupies four locations).
    GLuint divisor = 0;
    GLuint locationOffset = 0;
};
}
