// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <glad/glad.h>

#include <array>
#include <cstdint>

namespace ux3d::imgui {

// A3.2: one viewport's cascaded sun shadow map: a GL_DEPTH_COMPONENT32F 2D array texture (one layer per
// cascade) and one depth-only framebuffer per layer. Memory: size * size * cascades * 4 bytes (2048 x 3:
// 48 MiB, 4096 x 4: 256 MiB). Released when the sun or its shadows are off and when the viewport closes.
class SunShadowMap {
public:
    SunShadowMap() = default;
    ~SunShadowMap() { release(); }
    SunShadowMap(const SunShadowMap&) = delete;
    SunShadowMap& operator=(const SunShadowMap&) = delete;

    // (Re)allocates for size x size x layers; returns false when the framebuffers are incomplete.
    bool ensure(int size, int layers)
    {
        if (m_texture != 0 && m_size == size && m_layers == layers) {
            return true;
        }
        release();
        glGenTextures(1, &m_texture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_texture);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, size, size, layers, 0, GL_DEPTH_COMPONENT,
                     GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        m_size = size;
        m_layers = layers;
        GLint previous = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
        bool complete = true;
        for (int layer = 0; layer < layers; ++layer) {
            glGenFramebuffers(1, &m_framebuffers[static_cast<size_t>(layer)]);
            glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffers[static_cast<size_t>(layer)]);
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_texture, 0, layer);
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
            complete = complete && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
        if (!complete) {
            release();
        }
        return complete;
    }

    // Binds a cascade's layer as the depth target and sets the viewport; clear sets its depth to 1.
    void bindLayer(int layer, bool clear) const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffers[static_cast<size_t>(layer)]);
        glViewport(0, 0, m_size, m_size);
        if (clear) {
            glDepthMask(GL_TRUE);
            glClearDepth(1.0);
            glClear(GL_DEPTH_BUFFER_BIT);
        }
    }

    void release()
    {
        for (GLuint& framebuffer : m_framebuffers) {
            if (framebuffer != 0) {
                glDeleteFramebuffers(1, &framebuffer);
                framebuffer = 0;
            }
        }
        if (m_texture != 0) {
            glDeleteTextures(1, &m_texture);
            m_texture = 0;
        }
        m_size = 0;
        m_layers = 0;
    }

    bool isValid() const { return m_texture != 0; }
    GLuint texture() const { return m_texture; }
    int size() const { return m_size; }
    int layers() const { return m_layers; }
    uint64_t bytes() const { return static_cast<uint64_t>(m_size) * static_cast<uint64_t>(m_size) * static_cast<uint64_t>(m_layers) * 4u; }

private:
    GLuint m_texture = 0;
    std::array<GLuint, 4> m_framebuffers {};
    int m_size = 0;
    int m_layers = 0;
};

} // namespace ux3d::imgui
