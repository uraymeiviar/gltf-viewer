// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/MemoryRange.h"
#include "renderer/gl/Enums.h"

namespace ux3d::renderer::gl {
class GL;

class Buffer {
public:
    Buffer(GL*);
    Buffer(const Buffer&) = default;
    Buffer(Buffer&&);
    Buffer(GL* gl, const core::MemoryRange<const uint8_t>& data);
    ~Buffer();

    Buffer& operator=(Buffer&&);

    void write(size_t size, const void* data);
    void write(const core::MemoryRange<const uint8_t>& data);

    size_t getSize() const;

    void bind(BufferTarget target) const;

    GLuint handle = invalid;
    uint64_t residentBytes = 0;

private:
    GL* m_gl;
};
}
