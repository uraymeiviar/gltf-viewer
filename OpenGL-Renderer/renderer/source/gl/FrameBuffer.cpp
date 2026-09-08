// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/gl/FrameBuffer.h"
#include "core/Logger.h"
#include "renderer/gl/Texture.h"

ux3d::renderer::gl::FrameBuffer::FrameBuffer(GL* gl)
    : m_gl(gl)
{
}

ux3d::renderer::gl::FrameBuffer::~FrameBuffer()
{
    if (m_frameBuffer != invalid) {
        m_gl->deleteFramebuffers(1, &m_frameBuffer);
        m_frameBuffer = invalid;
    }
}

bool ux3d::renderer::gl::FrameBuffer::initialize(bool _backBuffer, GLenum _target)
{
    if (m_frameBuffer == invalid) {
        if (_backBuffer) {
            m_frameBuffer = 0u;
        } else {
            m_gl->genFramebuffers(1, &m_frameBuffer);
        }
    }

    m_target = _target;

    return m_frameBuffer != invalid;
}

auto ux3d::renderer::gl::FrameBuffer::addColorTarget(uint32_t _Width, uint32_t _Height, Format _Format, ComponentType _PixelFormat, InternalFormat _InternalFormat, uint32_t _index) -> Texture&
{
    ColorAttachment& attachment = m_colorAttachments.emplace_back(ColorAttachment { Texture(m_gl, TextureTarget::Texture2d), 0 });
    attachment.slot = _index == UINT32_MAX ? static_cast<uint32_t>(m_colorAttachments.size() - 1) : _index;
    attachment.texture.write(_Width, _Height, _Format, _PixelFormat, nullptr, _InternalFormat);
    return attachment.texture;
}

auto ux3d::renderer::gl::FrameBuffer::setDepthStencilTarget(uint32_t _Width, uint32_t _Height, Format _Format, ComponentType _PixelFormat, InternalFormat _InternalFormat) -> Texture&
{
    m_depthStencilTarget.emplace(m_gl, TextureTarget::Texture2d);
    m_depthStencilTarget->write(_Width, _Height, _Format, _PixelFormat, nullptr, _InternalFormat);
    return *m_depthStencilTarget;
}

bool ux3d::renderer::gl::FrameBuffer::bind() const
{
    m_gl->bindFramebuffer(m_target, m_frameBuffer);

    bool validAttachment = m_frameBuffer == 0; // Its the backbuffer --> attachments not specified by us

    core::Vector<GLenum> m_Buffers;
    if (m_Buffers.reserve(m_colorAttachments.size() == false)) {
        return false;
    }

    // Bind color attachments.
    for (const auto& [tex, slot] : m_colorAttachments) {
        tex.bind();

        validAttachment = true;

        const GLenum colorAttachment = GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(slot);
        (void)m_Buffers.emplace_back(colorAttachment);

        m_gl->framebufferTexture2D(m_target, colorAttachment, GL_TEXTURE_2D, tex.handle, 0);
    }

    // Bind depth stencil attachment, if any.
    if (m_depthStencilTarget) {
        m_depthStencilTarget->bind();
        validAttachment = true;
        m_gl->framebufferTexture2D(m_target, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_depthStencilTarget->handle, 0);
    }

    if (validAttachment == false) {
        core::logError("No attachment bound to framebuffer %u", m_frameBuffer);
    }

    if (m_Buffers.empty() == false) {
        m_gl->drawBuffers(static_cast<GLsizei>(m_Buffers.size()), m_Buffers.data());
    }

    return core::debugAssert(m_gl->checkFramebufferStatus(m_target) == GL_FRAMEBUFFER_COMPLETE, "Cannot complete framebuffer");
}
