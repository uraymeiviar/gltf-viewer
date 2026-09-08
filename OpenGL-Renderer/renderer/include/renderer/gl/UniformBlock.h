// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "Buffer.h"

#include <string>

namespace ux3d::renderer::gl {
struct UniformBlockBinding;

class UniformBlock {
public:
    UniformBlock(GL* gl, const std::string& name);

    // TODO: Uniform blocks should *not* be copyable, because all the underlying GL objects are shared!
    // TODO: It is currently only used to implement mesh copies when drawing the outline.
    UniformBlock(const UniformBlock&) = default;

    UniformBlock(UniformBlock&&) = default;
    UniformBlock& operator=(UniformBlock&&) = default;

    void write(size_t byteLength, const void* data);
    void commit(GL* gl, const UniformBlockBinding& binding) const;

    std::string name;

private:
    Buffer m_buffer;
};
}
