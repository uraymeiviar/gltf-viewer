// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// AmbientOcclusion (A3.5, declared in AmbientOcclusion.h): the GTAO pass, its denoise / upsample and targets.

#include "imgui/AmbientOcclusion.h"

namespace ux3d::imgui {

bool AmbientOcclusion::ensureTargets(int width, int height, bool halfResolution)
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

bool AmbientOcclusion::ensurePrepassTarget(int width, int height)
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

void AmbientOcclusion::bindPrepass(int width, int height)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_prepassFbo);
    glViewport(0, 0, width, height);
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

bool AmbientOcclusion::compute(const Params& params)
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

uint64_t AmbientOcclusion::getMemoryBytes() const
{
    const uint64_t ao = static_cast<uint64_t>(m_aoWidth) * static_cast<uint64_t>(m_aoHeight) * 8u;
    const uint64_t full = static_cast<uint64_t>(m_width) * static_cast<uint64_t>(m_height) * 8u;
    const uint64_t prepass = static_cast<uint64_t>(m_prepassWidth) * static_cast<uint64_t>(m_prepassHeight) * 8u;
    return (m_raw.texture ? ao : 0) + (m_denoised.texture ? ao : 0) + (m_result.texture ? full : 0) +
           (m_prepassFbo ? prepass : 0);
}

void AmbientOcclusion::releaseTargets()
{
    destroyTarget(m_raw);
    destroyTarget(m_denoised);
    destroyTarget(m_result);
    m_width = m_height = m_aoWidth = m_aoHeight = 0;
    m_lastComputed = false;
}

void AmbientOcclusion::releasePrepassTarget()
{
    if (m_prepassFbo) glDeleteFramebuffers(1, &m_prepassFbo);
    if (m_prepassDepth) glDeleteTextures(1, &m_prepassDepth);
    if (m_prepassNormal) glDeleteTextures(1, &m_prepassNormal);
    m_prepassFbo = m_prepassDepth = m_prepassNormal = 0;
    m_prepassWidth = m_prepassHeight = 0;
}

void AmbientOcclusion::release()
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

void AmbientOcclusion::setNearest()
{
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
}

void AmbientOcclusion::bindTexture(GLuint unit, GLuint texture)
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, texture);
}

bool AmbientOcclusion::createTarget(Target& target, int width, int height)
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

void AmbientOcclusion::clearTarget(const Target& target)
{
    GLint previous = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
    const GLfloat open[4] = { 1.0f, -1.0f, 0.0f, 1.0f };
    glClearBufferfv(GL_COLOR, 0, open);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
}

void AmbientOcclusion::destroyTarget(Target& target)
{
    if (target.fbo) glDeleteFramebuffers(1, &target.fbo);
    if (target.texture) glDeleteTextures(1, &target.texture);
    target = Target();
}

GLuint AmbientOcclusion::compile(GLenum type, const std::string& source)
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

GLuint AmbientOcclusion::link(const std::string& fragmentBody, const char* passDefine)
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

bool AmbientOcclusion::ensurePrograms()
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

void AmbientOcclusion::pollTimer()
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

bool AmbientOcclusion::beginTimer()
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

void AmbientOcclusion::endTimer()
{
    if (m_activeTimer < 0) return;
    TimerPair& pair = m_timers[static_cast<size_t>(m_activeTimer)];
    glQueryCounter(pair.queries[1], GL_TIMESTAMP);
    pair.pending = true;
    m_activeTimer = -1;
}

} // namespace ux3d::imgui
