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
    bool ensureTargets(int width, int height, bool halfResolution)
    {
        const int aoWidth = halfResolution ? (width + 1) / 2 : width;
        const int aoHeight = halfResolution ? (height + 1) / 2 : height;
        if (m_result.texture != 0 && m_width == width && m_height == height && m_half == halfResolution) {
            return true;
        }
        releaseTargets();
        m_width = width;
        m_height = height;
        m_half = halfResolution;
        m_aoWidth = aoWidth;
        m_aoHeight = aoHeight;
        bool ok = createTarget(m_raw, aoWidth, aoHeight) && createTarget(m_result, width, height);
        if (ok && halfResolution) ok = createTarget(m_denoised, aoWidth, aoHeight);
        if (!ok) releaseTargets();
        return ok;
    }

    // The prepass target used when no decal buffer is active: DEPTH_COMPONENT32F depth and the RGB10_A2 normal,
    // exactly the decal buffer's prepass layout.
    bool ensurePrepassTarget(int width, int height)
    {
        if (m_prepassFbo != 0 && m_prepassWidth == width && m_prepassHeight == height) return true;
        releasePrepassTarget();
        m_prepassWidth = width;
        m_prepassHeight = height;
        glGenTextures(1, &m_prepassDepth);
        glBindTexture(GL_TEXTURE_2D, m_prepassDepth);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        setNearest();
        glGenTextures(1, &m_prepassNormal);
        glBindTexture(GL_TEXTURE_2D, m_prepassNormal);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2, width, height, 0, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, nullptr);
        setNearest();
        glBindTexture(GL_TEXTURE_2D, 0);
        GLint previous = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
        glGenFramebuffers(1, &m_prepassFbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_prepassFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_prepassDepth, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_prepassNormal, 0);
        const GLenum drawBuffer = GL_COLOR_ATTACHMENT0;
        glDrawBuffers(1, &drawBuffer);
        const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
        profiling::countTextureCreation(2);
        if (!complete) releasePrepassTarget();
        return complete;
    }

    // Binds and clears the own prepass target (depth 1, normal 0) for a width x height draw.
    void bindPrepass(int width, int height)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_prepassFbo);
        glViewport(0, 0, width, height);
        glDepthMask(GL_TRUE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearDepth(1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    GLuint getPrepassDepth() const { return m_prepassDepth; }
    GLuint getPrepassNormal() const { return m_prepassNormal; }
    bool hasPrepassTarget() const { return m_prepassFbo != 0; }

    // Runs the AO passes (targets from ensureTargets). Leaves the framebuffer binding, the viewport and the program
    // changed; the caller restores its render target. Returns false when a program or target is missing.
    bool compute(const Params& params)
    {
        if (!ensurePrograms() || m_result.texture == 0 || params.width != m_width || params.height != m_height ||
            params.depthTexture == 0 || params.normalTexture == 0) {
            // Whatever the shading commands read this frame then means "no occlusion".
            if (m_result.fbo != 0) clearTarget(m_result);
            m_lastComputed = false;
            return false;
        }
        pollTimer();
        const bool timed = beginTimer();

        GLint previousVao = 0;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_COLOR_LOGIC_OP);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glDepthMask(GL_FALSE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glBindVertexArray(m_vao);
        for (GLuint unit = 0; unit < 3; ++unit) glBindSampler(unit, 0);

        const glm::mat4 inverseProjection = glm::inverse(params.projection);
        const glm::mat3 viewRotation = glm::mat3(params.view);
        const int scale = m_half ? 2 : 1;

        // 1. GTAO at the AO resolution.
        glBindFramebuffer(GL_FRAMEBUFFER, m_raw.fbo);
        glViewport(0, 0, m_aoWidth, m_aoHeight);
        glUseProgram(m_gtao.id);
        bindTexture(0, params.depthTexture);
        bindTexture(1, params.normalTexture);
        glUniform1i(m_gtao.loc("u_Depth"), 0);
        glUniform1i(m_gtao.loc("u_Normal"), 1);
        glUniform2i(m_gtao.loc("u_FullSize"), m_width, m_height);
        glUniform2i(m_gtao.loc("u_AoSize"), m_aoWidth, m_aoHeight);
        glUniform1i(m_gtao.loc("u_AoScale"), scale);
        glUniformMatrix4fv(m_gtao.loc("u_InvProjection"), 1, GL_FALSE, glm::value_ptr(inverseProjection));
        glUniformMatrix3fv(m_gtao.loc("u_ViewRotation"), 1, GL_FALSE, glm::value_ptr(viewRotation));
        glUniform1f(m_gtao.loc("u_ProjScaleY"), params.projection[1][1]);
        glUniform1i(m_gtao.loc("u_Orthographic"), params.orthographic ? 1 : 0);
        glUniform1f(m_gtao.loc("u_Radius"), params.radius);
        glUniform1f(m_gtao.loc("u_Intensity"), params.intensity);
        glUniform1i(m_gtao.loc("u_SliceCount"), params.sliceCount);
        glUniform1i(m_gtao.loc("u_StepCount"), params.stepCount);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // 2. Depth-aware denoise (into the result at full resolution, into the half target otherwise).
        const Target& denoiseTarget = m_half ? m_denoised : m_result;
        glBindFramebuffer(GL_FRAMEBUFFER, denoiseTarget.fbo);
        glViewport(0, 0, m_aoWidth, m_aoHeight);
        glUseProgram(m_denoise.id);
        bindTexture(0, m_raw.texture);
        glUniform1i(m_denoise.loc("u_Ao"), 0);
        glUniform2i(m_denoise.loc("u_FullSize"), m_width, m_height);
        glUniform2i(m_denoise.loc("u_AoSize"), m_aoWidth, m_aoHeight);
        glUniform1i(m_denoise.loc("u_AoScale"), scale);
        glUniform1f(m_denoise.loc("u_DepthTolerance"), kDenoiseDepthTolerance);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // 3. Half resolution: depth-aware upsample to the full-resolution result.
        if (m_half) {
            glBindFramebuffer(GL_FRAMEBUFFER, m_result.fbo);
            glViewport(0, 0, m_width, m_height);
            glUseProgram(m_upsample.id);
            bindTexture(0, m_denoised.texture);
            bindTexture(1, params.depthTexture);
            glUniform1i(m_upsample.loc("u_Ao"), 0);
            glUniform1i(m_upsample.loc("u_Depth"), 1);
            glUniform2i(m_upsample.loc("u_FullSize"), m_width, m_height);
            glUniform2i(m_upsample.loc("u_AoSize"), m_aoWidth, m_aoHeight);
            glUniform1i(m_upsample.loc("u_AoScale"), scale);
            glUniformMatrix4fv(m_upsample.loc("u_InvProjection"), 1, GL_FALSE, glm::value_ptr(inverseProjection));
            glUniform1f(m_upsample.loc("u_DepthTolerance"), kUpsampleDepthTolerance);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }

        for (GLuint unit = 0; unit < 2; ++unit) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(static_cast<GLuint>(previousVao));
        glUseProgram(0);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        if (timed) endTimer();
        m_lastComputed = true;
        ++m_computeCount;
        return true;
    }

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
    uint64_t getMemoryBytes() const
    {
        const uint64_t ao = static_cast<uint64_t>(m_aoWidth) * static_cast<uint64_t>(m_aoHeight) * 8u;
        const uint64_t full = static_cast<uint64_t>(m_width) * static_cast<uint64_t>(m_height) * 8u;
        const uint64_t prepass = static_cast<uint64_t>(m_prepassWidth) * static_cast<uint64_t>(m_prepassHeight) * 8u;
        return (m_raw.texture ? ao : 0) + (m_denoised.texture ? ao : 0) + (m_result.texture ? full : 0) +
               (m_prepassFbo ? prepass : 0);
    }

    void releaseTargets()
    {
        destroyTarget(m_raw);
        destroyTarget(m_denoised);
        destroyTarget(m_result);
        m_width = m_height = m_aoWidth = m_aoHeight = 0;
        m_lastComputed = false;
    }

    void releasePrepassTarget()
    {
        if (m_prepassFbo) glDeleteFramebuffers(1, &m_prepassFbo);
        if (m_prepassDepth) glDeleteTextures(1, &m_prepassDepth);
        if (m_prepassNormal) glDeleteTextures(1, &m_prepassNormal);
        m_prepassFbo = m_prepassDepth = m_prepassNormal = 0;
        m_prepassWidth = m_prepassHeight = 0;
    }

    // Everything, programs and timer queries included.
    void release()
    {
        releaseTargets();
        releasePrepassTarget();
        for (Program* program : { &m_gtao, &m_denoise, &m_upsample }) {
            if (program->id) glDeleteProgram(program->id);
            *program = Program();
        }
        if (m_vao) glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
        for (TimerPair& pair : m_timers) {
            if (pair.queries[0]) glDeleteQueries(2, pair.queries);
            pair = TimerPair();
        }
        m_programsFailed = false;
    }

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

    static void setNearest()
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    }

    static void bindTexture(GLuint unit, GLuint texture)
    {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, texture);
    }

    static bool createTarget(Target& target, int width, int height)
    {
        glGenTextures(1, &target.texture);
        glBindTexture(GL_TEXTURE_2D, target.texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, width, height, 0, GL_RG, GL_FLOAT, nullptr);
        setNearest();
        glBindTexture(GL_TEXTURE_2D, 0);
        GLint previous = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
        glGenFramebuffers(1, &target.fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.texture, 0);
        const GLenum drawBuffer = GL_COLOR_ATTACHMENT0;
        glDrawBuffers(1, &drawBuffer);
        const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        if (complete) {
            const GLfloat open[4] = { 1.0f, -1.0f, 0.0f, 1.0f };
            glClearBufferfv(GL_COLOR, 0, open);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
        profiling::countTextureCreation(1);
        return complete;
    }

    // Visibility 1 and no surface everywhere (read as "no occlusion").
    static void clearTarget(const Target& target)
    {
        GLint previous = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
        glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
        const GLfloat open[4] = { 1.0f, -1.0f, 0.0f, 1.0f };
        glClearBufferfv(GL_COLOR, 0, open);
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
    }

    static void destroyTarget(Target& target)
    {
        if (target.fbo) glDeleteFramebuffers(1, &target.fbo);
        if (target.texture) glDeleteTextures(1, &target.texture);
        target = Target();
    }

    static GLuint compile(GLenum type, const std::string& source)
    {
        GLuint shader = glCreateShader(type);
        const char* text = source.c_str();
        glShaderSource(shader, 1, &text, nullptr);
        glCompileShader(shader);
        GLint ok = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[2048];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            std::fprintf(stderr, "SSAO shader compile error: %s\n", log);
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    static GLuint link(const std::string& fragmentBody, const char* passDefine)
    {
        static const char* kVertex = "#version 410 core\n"
                                     "void main() {\n"
                                     "    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);\n"
                                     "    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
                                     "}\n";
        const GLuint vs = compile(GL_VERTEX_SHADER, kVertex);
        const GLuint fs = compile(GL_FRAGMENT_SHADER, std::string("#version 410 core\n#define ") + passDefine + "\n" +
                                                          fragmentBody);
        if (!vs || !fs) {
            if (vs) glDeleteShader(vs);
            if (fs) glDeleteShader(fs);
            return 0;
        }
        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        glDeleteShader(vs);
        glDeleteShader(fs);
        GLint ok = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[2048];
            glGetProgramInfoLog(program, sizeof(log), nullptr, log);
            std::fprintf(stderr, "SSAO program link error: %s\n", log);
            glDeleteProgram(program);
            return 0;
        }
        profiling::countShaderCompile();
        profiling::countShaderCompile();
        profiling::countProgramLink();
        return program;
    }

    bool ensurePrograms()
    {
        if (m_gtao.id && m_denoise.id && m_upsample.id && m_vao) return true;
        if (m_programsFailed) return false;
        const VfsFile file = EmbeddedVFS::instance().getFile("shaders/ssao_passes.frag");
        if (!file.isValid()) {
            m_programsFailed = true;
            std::fprintf(stderr, "SSAO: shaders/ssao_passes.frag is missing from the embedded resources\n");
            return false;
        }
        const std::string body(file.asStringView());
        m_gtao.id = link(body, "PASS_GTAO");
        m_denoise.id = link(body, "PASS_DENOISE");
        m_upsample.id = link(body, "PASS_UPSAMPLE");
        if (!m_vao) {
            glGenVertexArrays(1, &m_vao);
            profiling::countVaoCreation();
        }
        if (!m_gtao.id || !m_denoise.id || !m_upsample.id) {
            m_programsFailed = true;
            return false;
        }
        return true;
    }

    // GL_TIMESTAMP pairs (allowed inside the renderer's GL_TIME_ELAPSED pass queries), read only once available.
    void pollTimer()
    {
        for (TimerPair& pair : m_timers) {
            if (!pair.pending) continue;
            GLint available = 0;
            glGetQueryObjectiv(pair.queries[1], GL_QUERY_RESULT_AVAILABLE, &available);
            if (!available) continue;
            GLuint64 start = 0;
            GLuint64 end = 0;
            glGetQueryObjectui64v(pair.queries[0], GL_QUERY_RESULT, &start);
            glGetQueryObjectui64v(pair.queries[1], GL_QUERY_RESULT, &end);
            pair.pending = false;
            const double ms = end > start ? static_cast<double>(end - start) * 1.0e-6 : 0.0;
            m_lastGpuMs = ms;
            m_averageGpuMs = m_averageGpuMs < 0.0 ? ms : m_averageGpuMs * 0.9 + ms * 0.1;
        }
    }

    bool beginTimer()
    {
        for (size_t i = 0; i < m_timers.size(); ++i) {
            TimerPair& pair = m_timers[(m_nextTimer + i) % m_timers.size()];
            if (pair.pending) continue;
            if (!pair.queries[0]) glGenQueries(2, pair.queries);
            glQueryCounter(pair.queries[0], GL_TIMESTAMP);
            m_activeTimer = static_cast<int>((m_nextTimer + i) % m_timers.size());
            m_nextTimer = (static_cast<size_t>(m_activeTimer) + 1) % m_timers.size();
            return true;
        }
        return false;
    }

    void endTimer()
    {
        if (m_activeTimer < 0) return;
        TimerPair& pair = m_timers[static_cast<size_t>(m_activeTimer)];
        glQueryCounter(pair.queries[1], GL_TIMESTAMP);
        pair.pending = true;
        m_activeTimer = -1;
    }

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
