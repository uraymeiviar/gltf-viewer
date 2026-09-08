// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/gl/Buffer.h"
#include "core/Logger.h"
#include "renderer/gl/GL.h"
#include "renderer/gl/Util.h"

ux3d::renderer::gl::Buffer::Buffer(GL* gl)
    : m_gl(gl)
{
    m_gl->genBuffers(1, &handle);
}

ux3d::renderer::gl::Buffer::Buffer(Buffer&& other)
    : m_gl(other.m_gl)
    , handle(other.handle)
{
    other.handle = invalid;
}

ux3d::renderer::gl::Buffer::Buffer(GL* gl, const core::MemoryRange<const uint8_t>& data)
    : Buffer(gl)
{
    write(data);
}

ux3d::renderer::gl::Buffer::~Buffer()
{
    if (handle != invalid) {
        m_gl->deleteBuffers(1, &handle);
    }
}

size_t ux3d::renderer::gl::Buffer::getSize() const
{
    m_gl->bindBuffer(GL_ARRAY_BUFFER, handle);
    GLint size;
    m_gl->getBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);
    return static_cast<size_t>(size);
}

void ux3d::renderer::gl::Buffer::write(const core::MemoryRange<const uint8_t>& data)
{
    write(data.size(), data.data());
}

void ux3d::renderer::gl::Buffer::write(size_t size, const void* data)
{
    m_gl->bindBuffer(GL_ARRAY_BUFFER, handle);
    m_gl->bufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
}

void ux3d::renderer::gl::Buffer::bind(BufferTarget target) const
{
    m_gl->bindBuffer(static_cast<GLenum>(target), handle);
}
