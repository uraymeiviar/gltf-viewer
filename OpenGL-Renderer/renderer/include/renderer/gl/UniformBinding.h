// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "renderer/gl/Enums.h"

namespace ux3d::renderer::gl {
// P8.1b/c: uniform buffer binding points of the shaders' per-frame blocks
// (camera_block.glsl, environment_block.glsl). Other blocks are still bound at their
// block index, which stays far below these.
constexpr GLuint kCameraBlockBinding = 12;
constexpr GLuint kEnvironmentBlockBinding = 13;

struct UniformBinding {
    GLuint location = invalid;
    PrimitiveType type = PrimitiveType::Invalid;
    // What this program location already holds (uniform values are program state):
    // the revision of the last committed Uniform value, or the texture unit of the last
    // committed sampler. A commit of the same value is skipped.
    mutable uint64_t committedRevision = 0;
    mutable GLint committedUnit = -1;
};

struct UniformBlockBinding {
    GLuint blockIndex = invalid;
    PrimitiveType type = PrimitiveType::Invalid;
};
}
