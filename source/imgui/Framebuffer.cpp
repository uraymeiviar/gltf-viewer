// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/Framebuffer.h"
#include <cstdio>
#include <utility>

namespace ux3d::imgui {

Framebuffer::Framebuffer() = default;

Framebuffer::~Framebuffer()
{
    cleanup();
}

Framebuffer::Framebuffer(Framebuffer&& other) noexcept
    : m_fbo(other.m_fbo)
    , m_colorTexture(other.m_colorTexture)
    , m_depthRbo(other.m_depthRbo)
    , m_msaaFbo(other.m_msaaFbo)
    , m_msaaColorRbo(other.m_msaaColorRbo)
    , m_msaaDepthRbo(other.m_msaaDepthRbo)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_samples(other.m_samples)
{
    other.m_fbo = 0;
    other.m_colorTexture = 0;
    other.m_depthRbo = 0;
    other.m_msaaFbo = 0;
    other.m_msaaColorRbo = 0;
    other.m_msaaDepthRbo = 0;
    other.m_width = 0;
    other.m_height = 0;
    other.m_samples = 1;
}

Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept
{
    if (this != &other) {
        cleanup();
        m_fbo = other.m_fbo;
        m_colorTexture = other.m_colorTexture;
        m_depthRbo = other.m_depthRbo;
        m_msaaFbo = other.m_msaaFbo;
        m_msaaColorRbo = other.m_msaaColorRbo;
        m_msaaDepthRbo = other.m_msaaDepthRbo;
        m_width = other.m_width;
        m_height = other.m_height;
        m_samples = other.m_samples;

        other.m_fbo = 0;
        other.m_colorTexture = 0;
        other.m_depthRbo = 0;
        other.m_msaaFbo = 0;
        other.m_msaaColorRbo = 0;
        other.m_msaaDepthRbo = 0;
        other.m_width = 0;
        other.m_height = 0;
        other.m_samples = 1;
    }
    return *this;
}

void Framebuffer::cleanup()
{
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    if (m_colorTexture) {
        glDeleteTextures(1, &m_colorTexture);
        m_colorTexture = 0;
    }
    if (m_depthRbo) {
        glDeleteRenderbuffers(1, &m_depthRbo);
        m_depthRbo = 0;
    }
    if (m_msaaFbo) {
        glDeleteFramebuffers(1, &m_msaaFbo);
        m_msaaFbo = 0;
    }
    if (m_msaaColorRbo) {
        glDeleteRenderbuffers(1, &m_msaaColorRbo);
        m_msaaColorRbo = 0;
    }
    if (m_msaaDepthRbo) {
        glDeleteRenderbuffers(1, &m_msaaDepthRbo);
        m_msaaDepthRbo = 0;
    }
    m_width = 0;
    m_height = 0;
    m_samples = 1;
}

void Framebuffer::resize(int width, int height, int samples)
{
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    if (samples < 1) samples = 1;

    // Clamp samples to GPU capability
    GLint maxSamples = 1;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    if (samples > maxSamples) samples = maxSamples;

    if (m_width == width && m_height == height && m_samples == samples && m_fbo != 0) {
        return;
    }

    cleanup();

    m_width = width;
    m_height = height;
    m_samples = samples;

    // 1. Always create the resolve / single-sampled texture & FBO
    glGenTextures(1, &m_colorTexture);
    glBindTexture(GL_TEXTURE_2D, m_colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTexture, 0);

    if (m_samples <= 1) {
        // Single-sample depth/stencil renderbuffer
        glGenRenderbuffers(1, &m_depthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, m_depthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_width, m_height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_depthRbo);
    } else {
        // 2. Create MSAA FBO with multisampled color & depth renderbuffers
        glGenFramebuffers(1, &m_msaaFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_msaaFbo);

        glGenRenderbuffers(1, &m_msaaColorRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, m_msaaColorRbo);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples, GL_RGBA8, m_width, m_height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_msaaColorRbo);

        glGenRenderbuffers(1, &m_msaaDepthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, m_msaaDepthRbo);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples, GL_DEPTH24_STENCIL8, m_width, m_height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_msaaDepthRbo);

        GLenum msaaStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (msaaStatus != GL_FRAMEBUFFER_COMPLETE) {
            std::fprintf(stderr, "MSAA Framebuffer is not complete! Status: 0x%X\n", msaaStatus);
        }
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "Framebuffer is not complete! Status: 0x%X\n", status);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::bind()
{
    if (m_samples > 1 && m_msaaFbo) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_msaaFbo);
    } else if (m_fbo) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    }
    glViewport(0, 0, m_width, m_height);
}

void Framebuffer::unbind()
{
    if (m_samples > 1 && m_msaaFbo && m_fbo) {
        // Hardware multisample resolve via blit to single-sample FBO
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_msaaFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
        glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace ux3d::imgui
