// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "renderer/gl/Enums.h"

namespace ux3d::renderer::gl {
struct UniformBinding {
    GLuint location = invalid;
    PrimitiveType type = PrimitiveType::Invalid;
};

struct UniformBlockBinding {
    GLuint blockIndex = invalid;
    PrimitiveType type = PrimitiveType::Invalid;
};
}
