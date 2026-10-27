// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/gl/Buffer.h"
#include "core/Logger.h"
#include "core/LoadProgress.h"
#include <stdexcept>
#include <algorithm>
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
    , residentBytes(other.residentBytes)
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
    core::loading::checkpoint();
    core::loading::Timer timer(core::loading::Upload);
    m_gl->bindBuffer(GL_ARRAY_BUFFER, handle);
    const bool chunked = core::loading::state.yield && data && size > 8u*1024*1024;
    m_gl->bufferData(GL_ARRAY_BUFFER, size, chunked ? nullptr : data, GL_STATIC_DRAW);
    if (m_gl->getError() != GL_NO_ERROR) {
        ++core::loading::state.allocationFailures;
        throw std::bad_alloc();
    }
    if (chunked) {
        constexpr size_t chunk = 8u*1024*1024;
        for (size_t offset = 0; offset < size; offset += chunk) {
            core::loading::checkpoint();
            m_gl->bindBuffer(GL_ARRAY_BUFFER, handle);
            m_gl->bufferSubData(GL_ARRAY_BUFFER, offset, std::min(chunk, size-offset), static_cast<const uint8_t*>(data)+offset);
            if (m_gl->getError() != GL_NO_ERROR) { ++core::loading::state.allocationFailures; throw std::bad_alloc(); }
        }
    }
    ++core::loading::state.completed;
    residentBytes = size;
}

void ux3d::renderer::gl::Buffer::bind(BufferTarget target) const
{
    m_gl->bindBuffer(static_cast<GLenum>(target), handle);
}
