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
};
}
