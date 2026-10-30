// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "imgui/EmbeddedVFS.h"
#include "imgui/Profiling.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

namespace ux3d::imgui {

// A3.5: one viewport's screen-space ambient occlusion (GTAO; docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.5).
//
// Order inside the viewport's forward draw: the A2.1 decal prepass draws every OPAQUE / MASK drawable's depth and
// world-space geometric normal (into the shared decal buffer when decals are active, otherwise into this object's
// own prepass target), then compute() runs from the prepass's post callback, before the opaque pass: GTAO at full
// or half resolution, a depth-aware 5 x 5 denoise and, at half resolution, a depth-aware upsample. pbr.frag reads
// the full-resolution result (RG32F: visibility, linear view depth) at its own pixel (ssao.glsl).
//
// Everything is allocated on first use and released while the viewport's SSAO is off, so a viewport without SSAO
// owns nothing and runs nothing. The passes are fragment shaders (#version 410): no compute, any 4.1+ context.
class AmbientOcclusion {
public:
    struct Params {
        GLuint depthTexture = 0;  // prepass depth (DEPTH_COMPONENT32F), origin at the render target's origin
        GLuint normalTexture = 0; // prepass world normal (RGB10_A2, n * 0.5 + 0.5)
        int width = 0;            // render size (full resolution)
        int height = 0;
        glm::mat4 projection{ 1.0f }; // the projection the prepass was drawn with (TAA-jittered)
        glm::mat4 view{ 1.0f };
        bool orthographic = false;
        float radius = 0.5f;
        float intensity = 1.0f;
        int sliceCount = 3;
        int stepCount = 6;
    };

    AmbientOcclusion() = default;
    ~AmbientOcclusion() { release(); }
    AmbientOcclusion(const AmbientOcclusion&) = delete;
    AmbientOcclusion& operator=(const AmbientOcclusion&) = delete;

    // The result target for a width x height render; call before the draw that computes it, so the texture handle
    // the shading commands bind is the one compute() fills. Returns false when the targets are incomplete.
    bool ensureTargets(int width, int height, bool halfResolution);

    // The prepass target used when no decal buffer is active: DEPTH_COMPONENT32F depth and the RGB10_A2 normal,
    // exactly the decal buffer's prepass layout.
    bool ensurePrepassTarget(int width, int height);

    // Binds and clears the own prepass target (depth 1, normal 0) for a width x height draw.
    void bindPrepass(int width, int height);

    GLuint getPrepassDepth() const { return m_prepassDepth; }
    GLuint getPrepassNormal() const { return m_prepassNormal; }
    bool hasPrepassTarget() const { return m_prepassFbo != 0; }

    // Runs the AO passes (targets from ensureTargets). Leaves the framebuffer binding, the viewport and the program
    // changed; the caller restores its render target. Returns false when a program or target is missing.
    bool compute(const Params& params);

    GLuint getResultTexture() const { return m_result.texture; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    int getAoWidth() const { return m_aoWidth; }
    int getAoHeight() const { return m_aoHeight; }
    bool isHalfResolution() const { return m_half; }
    bool lastComputed() const { return m_lastComputed; }
    uint64_t getComputeCount() const { return m_computeCount; }
    // GPU time of the AO passes (GTAO + denoise + upsample) of the latest frame whose timestamps are available, and an
    // exponential average over frames; negative before the first result.
    double getLastGpuMs() const { return m_lastGpuMs; }
    double getAverageGpuMs() const { return m_averageGpuMs; }
    uint64_t getMemoryBytes() const;

    void releaseTargets();

    void releasePrepassTarget();

    // Everything, programs and timer queries included.
    void release();

    bool hasAnything() const { return m_result.texture != 0 || m_prepassFbo != 0 || m_vao != 0; }

private:
    static constexpr float kDenoiseDepthTolerance = 0.01f;  // 1% of the view depth off the centre's plane
    static constexpr float kUpsampleDepthTolerance = 0.02f; // 2% of the view depth

    struct Target {
        GLuint texture = 0;
        GLuint fbo = 0;
    };
    struct Program {
        GLuint id = 0;
        GLint loc(const char* name) const { return glGetUniformLocation(id, name); }
    };
    struct TimerPair {
        GLuint queries[2] = { 0, 0 };
        bool pending = false;
    };

    static void setNearest();

    static void bindTexture(GLuint unit, GLuint texture);

    static bool createTarget(Target& target, int width, int height);

    // Visibility 1 and no surface everywhere (read as "no occlusion").
    static void clearTarget(const Target& target);

    static void destroyTarget(Target& target);

    static GLuint compile(GLenum type, const std::string& source);

    static GLuint link(const std::string& fragmentBody, const char* passDefine);

    bool ensurePrograms();

    // GL_TIMESTAMP pairs (allowed inside the renderer's GL_TIME_ELAPSED pass queries), read only once available.
    void pollTimer();

    bool beginTimer();

    void endTimer();

    Target m_raw;
    Target m_denoised;
    Target m_result;
    int m_width = 0;
    int m_height = 0;
    int m_aoWidth = 0;
    int m_aoHeight = 0;
    bool m_half = false;

    GLuint m_prepassFbo = 0;
    GLuint m_prepassDepth = 0;
    GLuint m_prepassNormal = 0;
    int m_prepassWidth = 0;
    int m_prepassHeight = 0;

    Program m_gtao;
    Program m_denoise;
    Program m_upsample;
    GLuint m_vao = 0;
    bool m_programsFailed = false;

    std::array<TimerPair, 4> m_timers{};
    size_t m_nextTimer = 0;
    int m_activeTimer = -1;
    double m_lastGpuMs = -1.0;
    double m_averageGpuMs = -1.0;
    bool m_lastComputed = false;
    uint64_t m_computeCount = 0;
};

} // namespace ux3d::imgui
