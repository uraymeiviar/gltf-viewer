// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/Framebuffer.h"
#include "imgui/Profiling.h"
#include <algorithm>
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

    const uint64_t pixels = static_cast<uint64_t>(m_width) * static_cast<uint64_t>(m_height);
    ux3d::imgui::profiling::countTextureCreation();
    // Resolve/single-sample RGBA8 color texture.
    ux3d::imgui::profiling::countTextureAllocation(pixels * 4ull);
    ux3d::imgui::profiling::countFramebufferCreation(m_samples > 1 ? 2u : 1u);
    ux3d::imgui::profiling::countRenderbufferCreation(m_samples > 1 ? 2u : 1u);
    if (m_samples > 1) {
        // Multisampled RGBA8 color + D24S8 depth/stencil renderbuffers. The
        // sample count multiplies renderbuffer storage, not texel count.
        ux3d::imgui::profiling::countRenderbufferAllocation(
            pixels * 4ull * static_cast<uint64_t>(m_samples) * 2ull);
    } else {
        // Single-sample D24S8 depth/stencil renderbuffer.
        ux3d::imgui::profiling::countRenderbufferAllocation(pixels * 4ull);
    }
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
    resolve();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::resolve()
{
    if (m_samples > 1 && m_msaaFbo && m_fbo) {
        // Hardware multisample resolve via blit to single-sample FBO
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_msaaFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
        glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
}

TransmissionTarget::~TransmissionTarget()
{
    destroy();
}

void TransmissionTarget::destroy()
{
    if (m_fbo) {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    if (m_texture) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
}

void TransmissionTarget::create()
{
    if (m_texture) {
        return;
    }
    GLint previousTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D); // allocates the mip chain
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));

    GLint previousFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFbo);
    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFbo));
}

void TransmissionTarget::capture(Framebuffer& source)
{
    if (!m_texture || !source.isValid()) {
        return;
    }
    // A multisampled image cannot be blitted with scaling, so resolve it first.
    source.resolve();
    glBindFramebuffer(GL_READ_FRAMEBUFFER, source.getResolveFbo());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
    glBlitFramebuffer(0, 0, source.getWidth(), source.getHeight(), 0, 0, kSize, kSize, GL_COLOR_BUFFER_BIT, GL_LINEAR);

    GLint previousTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));

    source.bind();
}

DecalBuffer::~DecalBuffer()
{
    release();
}

void DecalBuffer::destroy()
{
    if (m_accumulationFbo != 0) {
        glDeleteFramebuffers(1, &m_accumulationFbo);
        m_accumulationFbo = 0;
    }
    if (m_prepassFbo != 0) {
        glDeleteFramebuffers(1, &m_prepassFbo);
        m_prepassFbo = 0;
    }
    if (m_normalTexture != 0) {
        glDeleteTextures(1, &m_normalTexture);
        m_normalTexture = 0;
    }
    for (GLuint& texture : m_accumulationTextures) {
        if (texture != 0) {
            glDeleteTextures(1, &texture);
            texture = 0;
        }
    }
    if (m_depthTexture != 0) {
        glDeleteTextures(1, &m_depthTexture);
        m_depthTexture = 0;
    }
    m_width = 0;
    m_height = 0;
}

bool DecalBuffer::ensure(int width, int height)
{
    if (width < 1 || height < 1) {
        return false;
    }
    if (isValid()) {
        if (width == m_width && height == m_height) {
            return false;
        }
        destroy();
        create(width, height);
        ++m_resizeCount;
        ++m_allocationCount;
        return true;
    }
    create(width, height);
    ++m_allocationCount;
    return true;
}

void DecalBuffer::release()
{
    if (!isValid() && m_depthTexture == 0 && m_normalTexture == 0) {
        return;
    }
    destroy();
    ++m_releaseCount;
}

void DecalBuffer::create(int width, int height)
{
    m_width = width;
    m_height = height;

    // Depth: one 32F texture shared by both framebuffers.
    glGenTextures(1, &m_depthTexture);
    glBindTexture(GL_TEXTURE_2D, m_depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // G0: world-space geometric normal, n * 0.5 + 0.5.
    glGenTextures(1, &m_normalTexture);
    glBindTexture(GL_TEXTURE_2D, m_normalTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2, width, height, 0, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    // A0-A5 in the design table's formats and order.
    const GLint internalFormats[DecalBuffer::kAccumulationTargetCount] = {
        GL_RGBA8, GL_RGBA16F, GL_RGBA16F, GL_RG8, GL_RG8, GL_R8,
    };
    const GLenum formats[DecalBuffer::kAccumulationTargetCount] = {
        GL_RGBA, GL_RGBA, GL_RGBA, GL_RG, GL_RG, GL_RED,
    };
    const GLenum types[DecalBuffer::kAccumulationTargetCount] = {
        GL_UNSIGNED_BYTE, GL_FLOAT, GL_FLOAT, GL_UNSIGNED_BYTE, GL_UNSIGNED_BYTE, GL_UNSIGNED_BYTE,
    };
    for (int i = 0; i < DecalBuffer::kAccumulationTargetCount; ++i) {
        glGenTextures(1, &m_accumulationTextures[i]);
        glBindTexture(GL_TEXTURE_2D, m_accumulationTextures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormats[i], width, height, 0, formats[i], types[i], nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    // Prepass: depth + G0.
    glGenFramebuffers(1, &m_prepassFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_prepassFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTexture, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_normalTexture, 0);
    const GLenum prepassDrawBuffer = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &prepassDrawBuffer);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "Decal prepass framebuffer is not complete! Status: 0x%X\n", status);
    }

    // Accumulation: the same depth texture + A0-A5. Depth is never written here.
    glGenFramebuffers(1, &m_accumulationFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_accumulationFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_depthTexture, 0);
    GLenum accumulationDrawBuffers[DecalBuffer::kAccumulationTargetCount];
    for (int i = 0; i < DecalBuffer::kAccumulationTargetCount; ++i) {
        const GLenum attachment = static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i);
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, m_accumulationTextures[i], 0);
        accumulationDrawBuffers[i] = attachment;
    }
    glDrawBuffers(DecalBuffer::kAccumulationTargetCount, accumulationDrawBuffers);
    status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "Decal accumulation framebuffer is not complete! Status: 0x%X\n", status);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ux3d::imgui::profiling::countFramebufferCreation(2);
    ux3d::imgui::profiling::countTextureCreation(2 + DecalBuffer::kAccumulationTargetCount);
    ux3d::imgui::profiling::countTextureAllocation(getMemoryBytes());
}

void DecalBuffer::clearTargets()
{
    if (!isValid()) {
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_prepassFbo);
    glViewport(0, 0, m_width, m_height);
    glDepthMask(GL_TRUE);
    const GLfloat one = 1.0f;
    glClearBufferfv(GL_DEPTH, 0, &one);
    const GLfloat zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    glClearBufferfv(GL_COLOR, 0, zero);

    // The accumulation pass never writes depth.
    glBindFramebuffer(GL_FRAMEBUFFER, m_accumulationFbo);
    glViewport(0, 0, m_width, m_height);
    glDepthMask(GL_FALSE);
    const GLfloat clearValues[DecalBuffer::kAccumulationTargetCount][4] = {
        { 0.0f, 0.0f, 0.0f, 1.0f }, // A0 base colour V, T
        { 0.0f, 0.0f, 0.0f, 1.0f }, // A1 normal V, T
        { 0.0f, 0.0f, 0.0f, 1.0f }, // A2 emissive V, T
        { 0.0f, 1.0f, 0.0f, 0.0f }, // A3 metallic V, T
        { 0.0f, 1.0f, 0.0f, 0.0f }, // A4 roughness V, T
        { 1.0f, 0.0f, 0.0f, 0.0f }, // A5 occlusion product
    };
    for (int i = 0; i < DecalBuffer::kAccumulationTargetCount; ++i) {
        glClearBufferfv(GL_COLOR, i, clearValues[i]);
    }
    glDepthMask(GL_TRUE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void DecalBuffer::bindPrepass(int viewportWidth, int viewportHeight)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_prepassFbo);
    glViewport(0, 0, viewportWidth, viewportHeight);
}

void DecalBuffer::bindAccumulation(int viewportWidth, int viewportHeight)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_accumulationFbo);
    glViewport(0, 0, viewportWidth, viewportHeight);
    glDepthMask(GL_FALSE);
}

} // namespace ux3d::imgui
