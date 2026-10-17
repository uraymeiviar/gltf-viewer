// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <glad/glad.h>
#include <cstdint>

namespace ux3d::imgui {

class Framebuffer {
public:
    Framebuffer();
    ~Framebuffer();

    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;

    void resize(int width, int height, int samples = 1);
    void bind();
    void unbind();

    GLuint getColorTexture() const { return m_colorTexture; }
    GLuint getFbo() const { return m_samples > 1 ? m_msaaFbo : m_fbo; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    int getSamples() const { return m_samples; }
    bool isValid() const { return m_fbo != 0 && m_colorTexture != 0; }

private:
    void cleanup();

    GLuint m_fbo = 0;              // Resolve / Single-sample FBO
    GLuint m_colorTexture = 0;      // Resolved color texture (for ImGui::Image)
    GLuint m_depthRbo = 0;          // Single-sample depth/stencil RBO (when samples <= 1)

    GLuint m_msaaFbo = 0;          // Multisampled FBO (when samples > 1)
    GLuint m_msaaColorRbo = 0;     // Multisampled color RBO
    GLuint m_msaaDepthRbo = 0;     // Multisampled depth/stencil RBO

    int m_width = 0;
    int m_height = 0;
    int m_samples = 1;
};

} // namespace ux3d::imgui
