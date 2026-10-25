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
    // Resolves the multisampled image into the color texture; no-op when single-sampled.
    // Leaves the read/draw framebuffer bindings changed.
    void resolve();

    GLuint getColorTexture() const { return m_colorTexture; }
    GLuint getFbo() const { return m_samples > 1 ? m_msaaFbo : m_fbo; }
    GLuint getResolveFbo() const { return m_fbo; }
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

// Mipmapped copy of the opaque scene that KHR_materials_transmission samples. It has a
// fixed size (the shader samples it in normalized screen coordinates) and exists before
// any material is built, so transmissive materials always reference a valid texture.
class TransmissionTarget {
public:
    static constexpr int kSize = 1024;

    TransmissionTarget() = default;
    ~TransmissionTarget();

    TransmissionTarget(const TransmissionTarget&) = delete;
    TransmissionTarget& operator=(const TransmissionTarget&) = delete;

    void create();
    void destroy();
    // Copies source's current image into the texture and rebuilds its mipmaps, then
    // binds source again for drawing.
    void capture(Framebuffer& source);

    GLuint getTexture() const { return m_texture; }

private:
    GLuint m_texture = 0;
    GLuint m_fbo = 0;
};

// A2.1: the one decal buffer shared by every viewport (docs/ASOBO_EXTENSIONS_PHASE_2.md
// section 2). It is created on first use, grows to the largest render-scale resolution a
// viewport needs, and is released when no viewport needs it or the model is unloaded.
// Layout: DEPTH_COMPONENT32F depth, RGB10_A2 G0 (world-space geometric normal, n*0.5+0.5)
// and the six accumulation targets A0-A5. Prepass = depth + G0; accumulation = A0-A5 only,
// sampling the depth texture for the decal shader's tolerant depth test. Reports 33 bytes
// per pixel.
class DecalBuffer {
public:
    static constexpr int kAccumulationTargetCount = 6;
    static constexpr uint64_t kBytesPerPixel = 33; // 4 depth + 4 G0 + 4+8+8+2+2+1

    DecalBuffer() = default;
    ~DecalBuffer();

    DecalBuffer(const DecalBuffer&) = delete;
    DecalBuffer& operator=(const DecalBuffer&) = delete;

    // Creates the buffer, or grows it when width/height exceed the current size.
    // Returns true when the GL objects changed.
    bool ensure(int width, int height);
    void release();

    bool isValid() const { return m_prepassFbo != 0 && m_accumulationFbo != 0; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    uint64_t getMemoryBytes() const
    {
        return static_cast<uint64_t>(m_width) * static_cast<uint64_t>(m_height) * kBytesPerPixel;
    }
    uint64_t getAllocationCount() const { return m_allocationCount; }
    uint64_t getResizeCount() const { return m_resizeCount; }
    uint64_t getReleaseCount() const { return m_releaseCount; }

    GLuint getDepthTexture() const { return m_depthTexture; }
    GLuint getNormalTexture() const { return m_normalTexture; }
    GLuint getAccumulationTexture(int index) const
    {
        return index >= 0 && index < kAccumulationTargetCount ? m_accumulationTextures[index] : 0;
    }
    GLuint getPrepassFbo() const { return m_prepassFbo; }
    GLuint getAccumulationFbo() const { return m_accumulationFbo; }

    // Clears the prepass depth/G0 and every accumulation target to the design's values.
    // Leaves the framebuffer binding at 0; callers rebind the viewport target afterwards.
    void clearTargets();
    // Binds the prepass FBO and its viewport (depth + G0). The shared buffer can be larger
    // than the current viewport, so the viewport is set to the caller's render size and
    // only that sub-rectangle is drawn.
    void bindPrepass(int viewportWidth, int viewportHeight);
    // Binds the accumulation FBO and its viewport (A0-A5). It has no depth attachment; the
    // prepass depth texture is sampled by the decal shader instead.
    void bindAccumulation(int viewportWidth, int viewportHeight);

private:
    void create(int width, int height);
    void destroy();

    int m_width = 0;
    int m_height = 0;

    GLuint m_depthTexture = 0;
    GLuint m_normalTexture = 0;
    GLuint m_accumulationTextures[kAccumulationTargetCount] = {};
    GLuint m_prepassFbo = 0;
    GLuint m_accumulationFbo = 0;

    uint64_t m_allocationCount = 0;
    uint64_t m_resizeCount = 0;
    uint64_t m_releaseCount = 0;
};

} // namespace ux3d::imgui
