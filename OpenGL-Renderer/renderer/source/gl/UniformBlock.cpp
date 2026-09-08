// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/gl/UniformBlock.h"
#include "renderer/gl/UniformBinding.h"
#include "renderer/gl/Util.h"

using namespace ux3d;
using namespace ux3d::renderer;

ux3d::renderer::gl::UniformBlock::UniformBlock(GL* gl, const std::string& name)
    : name(name)
    , m_buffer(gl)
{
}

void ux3d::renderer::gl::UniformBlock::write(size_t size, const void* data)
{
    m_buffer.write(size, data);
}

void ux3d::renderer::gl::UniformBlock::commit(GL* gl, const UniformBlockBinding& binding) const
{
    gl->bindBufferBase(GL_UNIFORM_BUFFER, binding.blockIndex, m_buffer.handle);
    checkGLError(gl, "Failed to bind uniform block");
}
