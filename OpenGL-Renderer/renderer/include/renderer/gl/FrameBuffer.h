// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "GL.h"
#include "Texture.h"

#include <list>
#include <optional>

namespace ux3d::renderer::gl {
class FrameBuffer {
public:
    static constexpr uint32_t glColorAttachmentCount = 16u;

    FrameBuffer(GL* gl);
    ~FrameBuffer();

    bool initialize(bool _BackBuffer, GLenum target = GL_FRAMEBUFFER);

    // calls creates and allocates a new rendertarget at _index,
    Texture& addColorTarget(uint32_t _Width, uint32_t _Height, Format _Format = Format::RGB, ComponentType _PixelFormat = ComponentType::Float, InternalFormat _InternalFormat = InternalFormat::Identity, uint32_t _index = UINT32_MAX);

    // currently not supported:
    //Texture* setDepthTarget(Texture* _pDepthTarget);
    //Texture* setStencilTarget(Texture* _pStencilTarget);
    Texture& setDepthStencilTarget(uint32_t _Width, uint32_t _Height, const Format _Format = Format::DS, const ComponentType _PixelFormat = ComponentType::UInt24D8S, const InternalFormat _InternalFormat = InternalFormat::D24S8);

    bool bind() const;

    GLuint getFramebuffer() const { return m_frameBuffer; }

private:
    struct ColorAttachment {
        Texture texture;
        uint32_t slot;
    };

    GL* m_gl;
    std::list<ColorAttachment> m_colorAttachments;
    std::optional<Texture> m_depthStencilTarget;

    GLuint m_frameBuffer = invalid;
    GLenum m_target = GL_FRAMEBUFFER;
};
}
