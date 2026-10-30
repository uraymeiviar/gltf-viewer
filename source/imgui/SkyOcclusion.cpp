// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// SkyOcclusionVolume (A3.9, declared in SkyOcclusion.h): the time-sliced bake of the sky-visibility volume from
// orthographic caster depth maps, and its GPU resources.

#include "imgui/SkyOcclusion.h"

namespace ux3d::imgui {

uint64_t SkyOcclusionVolume::jobBytes() const
{
    if (!m_job.active) return 0;
    const uint64_t cells = static_cast<uint64_t>(m_job.bounds.dims.x) * m_job.bounds.dims.y * m_job.bounds.dims.z;
    return cells * 16u + static_cast<uint64_t>(m_job.spec.mapSize) * m_job.spec.mapSize * kBatch * 2u;
}

SkyOcclusionVolume::Bounds SkyOcclusionVolume::fitBounds(const glm::vec3& lo, const glm::vec3& hi, const SkyOcclusionSpec& spec)
{
    Bounds b;
    const glm::vec3 extent = glm::max(hi - lo, glm::vec3(0.0f));
    const float largest = std::max({ extent.x, extent.y, extent.z, 1.0e-4f });
    b.cell = largest / static_cast<float>(std::max(spec.maxCells - 2 * kPaddingCells - 1, 1));
    for (int axis = 0; axis < 3; ++axis) {
        const int inner = static_cast<int>(std::ceil(extent[axis] / b.cell)) + 1;
        b.dims[axis] = std::clamp(inner + 2 * kPaddingCells, 1 + 2 * kPaddingCells, spec.maxCells);
    }
    const glm::vec3 centre = 0.5f * (lo + hi);
    b.origin = centre - 0.5f * glm::vec3(b.dims) * b.cell;
    return b;
}

void SkyOcclusionVolume::startJob(uint64_t key, const Bounds& bounds, SkyOcclusionQuality quality)
{
    if (m_job.active) ++m_stats.aborted;
    releaseJobTargets();
    m_job = Job();
    m_job.active = true;
    m_job.serial = ++m_nextSerial;
    m_job.key = key;
    m_job.bounds = bounds;
    m_job.quality = quality;
    m_job.spec = skyOcclusionSpec(quality);
    m_job.started = std::chrono::steady_clock::now();
}

void SkyOcclusionVolume::cancelJob()
{
    if (m_job.active) ++m_stats.aborted;
    releaseJobTargets();
    m_job = Job();
}

SkyOcclusionVolume::DirectionMatrices SkyOcclusionVolume::directionMatrices(const Bounds& b, const glm::vec3& towardsSky, int mapSize)
{
    const glm::vec3 L = glm::normalize(towardsSky);
    const glm::vec3 upReference = std::abs(L.y) < 0.99f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
    const glm::vec3 X = glm::normalize(glm::cross(upReference, L));
    const glm::vec3 Y = glm::cross(L, X);
    glm::vec3 lo(std::numeric_limits<float>::max());
    glm::vec3 hi(-std::numeric_limits<float>::max());
    for (int corner = 0; corner < 8; ++corner) {
        const glm::vec3 p = b.origin + glm::vec3((corner & 1) ? b.dims.x : 0, (corner & 2) ? b.dims.y : 0,
                                                 (corner & 4) ? b.dims.z : 0) * b.cell;
        const glm::vec3 q(glm::dot(p, X), glm::dot(p, Y), glm::dot(p, L));
        lo = glm::min(lo, q);
        hi = glm::max(hi, q);
    }
    const float margin = 0.5f * b.cell;
    const float halfX = 0.5f * (hi.x - lo.x) + margin;
    const float halfY = 0.5f * (hi.y - lo.y) + margin;
    const float cx = 0.5f * (hi.x + lo.x);
    const float cy = 0.5f * (hi.y + lo.y);
    const float top = hi.z + margin;
    const float range = (hi.z - lo.z) + 2.0f * margin;
    DirectionMatrices m;
    // world -> light view: x' = dot(p, X) - cx, y' = dot(p, Y) - cy, z' = dot(p, L) - top (<= 0 below the top).
    m.view[0] = glm::vec4(X.x, Y.x, L.x, 0.0f);
    m.view[1] = glm::vec4(X.y, Y.y, L.y, 0.0f);
    m.view[2] = glm::vec4(X.z, Y.z, L.z, 0.0f);
    m.view[3] = glm::vec4(-cx, -cy, -top, 1.0f);
    m.projection = glm::mat4(1.0f);
    m.projection[0][0] = 1.0f / halfX;
    m.projection[1][1] = 1.0f / halfY;
    m.projection[2][2] = -2.0f / range;
    m.projection[3][2] = -1.0f;
    glm::mat4 bias(0.5f);
    bias[3] = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
    m.textureFromWorld = bias * m.projection * m.view;
    m.depthRange = range;
    m.texelWorld = 2.0f * std::max(halfX, halfY) / static_cast<float>(mapSize);
    return m;
}

bool SkyOcclusionVolume::runFrame(const std::function<void(int layer, const DirectionMatrices&)>& renderDepth)
{
    if (!m_job.active) return false;
    if (!ensurePrograms() || !ensureJobTargets()) {
        cancelJob();
        return false;
    }
    const auto frameStart = std::chrono::steady_clock::now();
    pollTimers();
    const int timer = beginTimer();
    const int end = std::min(m_job.next + m_job.spec.directionsPerFrame, m_job.spec.directions);
    while (m_job.next < end) {
        const int count = std::min(kBatch, end - m_job.next);
        std::array<DirectionMatrices, kBatch> matrices{};
        std::array<glm::vec3, kBatch> directions{};
        for (int layer = 0; layer < count; ++layer) {
            directions[static_cast<size_t>(layer)] = skyOcclusionDirection(m_job.next + layer, m_job.spec.directions);
            matrices[static_cast<size_t>(layer)] =
                directionMatrices(m_job.bounds, directions[static_cast<size_t>(layer)], m_job.spec.mapSize);
            bindDepthLayer(layer, true);
            renderDepth(layer, matrices[static_cast<size_t>(layer)]);
        }
        accumulate(count, matrices, directions);
        m_job.next += count;
    }
    ++m_job.frames;
    bool completed = false;
    double finalizeMs = 0.0;
    if (m_job.next >= m_job.spec.directions) {
        const auto finalizeStart = std::chrono::steady_clock::now();
        completed = finalize();
        finalizeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - finalizeStart).count();
    }
    restoreState();
    if (timer >= 0) endTimer(timer, m_job.serial);
    const double frameMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - frameStart).count();
    m_job.cpuMs += frameMs;
    m_job.maxFrameCpuMs = std::max(m_job.maxFrameCpuMs, frameMs);
    if (m_job.next >= m_job.spec.directions) {
        if (completed) {
            m_stats.bakes++;
            m_stats.lastFrames = m_job.frames;
            m_stats.lastCpuMs = m_job.cpuMs;
            m_stats.lastMaxFrameCpuMs = m_job.maxFrameCpuMs;
            m_stats.lastFinalizeCpuMs = finalizeMs;
            m_stats.lastGpuMs = -1.0;
            m_stats.lastMaxFrameGpuMs = -1.0;
            m_completedSerial = m_job.serial;
            m_completedGpuMs = 0.0;
            m_completedMaxFrameGpuMs = 0.0;
            m_completedPendingTimers = 0;
            for (const TimerPair& pair : m_timers) {
                if (pair.pending && pair.serial == m_completedSerial) ++m_completedPendingTimers;
            }
            m_completedGpuMs += m_job.gpuMsSettled;
            m_completedMaxFrameGpuMs = std::max(m_completedMaxFrameGpuMs, m_job.maxFrameGpuMsSettled);
            if (m_completedPendingTimers == 0) {
                m_stats.lastGpuMs = m_completedGpuMs;
                m_stats.lastMaxFrameGpuMs = m_completedMaxFrameGpuMs;
            }
        }
        releaseJobTargets();
        m_job = Job();
    }
    return completed;
}

void SkyOcclusionVolume::pollTimers()
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
        if (m_job.active && pair.serial == m_job.serial) {
            m_job.gpuMsSettled += ms;
            m_job.maxFrameGpuMsSettled = std::max(m_job.maxFrameGpuMsSettled, ms);
        } else if (pair.serial == m_completedSerial && m_completedPendingTimers > 0) {
            m_completedGpuMs += ms;
            m_completedMaxFrameGpuMs = std::max(m_completedMaxFrameGpuMs, ms);
            if (--m_completedPendingTimers == 0) {
                m_stats.lastGpuMs = m_completedGpuMs;
                m_stats.lastMaxFrameGpuMs = m_completedMaxFrameGpuMs;
            }
        }
    }
}

void SkyOcclusionVolume::release()
{
    cancelJobSilently();
    destroyVolume(m_front);
    for (Program* program : { &m_accumulate, &m_finalize }) {
        if (program->id) glDeleteProgram(program->id);
        *program = Program();
    }
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    m_vao = 0;
    if (m_sliceFbo) glDeleteFramebuffers(1, &m_sliceFbo);
    m_sliceFbo = 0;
    for (TimerPair& pair : m_timers) {
        if (pair.queries[0]) glDeleteQueries(2, pair.queries);
        pair = TimerPair();
    }
    m_programsFailed = false;
}

void SkyOcclusionVolume::cancelJobSilently()
{
    releaseJobTargets();
    m_job = Job();
}

void SkyOcclusionVolume::destroyVolume(Volume& volume)
{
    if (volume.texture) glDeleteTextures(1, &volume.texture);
    volume = Volume();
}

bool SkyOcclusionVolume::ensureJobTargets()
{
    if (m_accumulator && m_depthArray && m_depthFbo) return true;
    releaseJobTargets();
    const Bounds& b = m_job.bounds;
    const int size = m_job.spec.mapSize;
    glGenTextures(1, &m_accumulator);
    glBindTexture(GL_TEXTURE_3D, m_accumulator);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, b.dims.x, b.dims.y, b.dims.z, 0, GL_RGBA, GL_FLOAT, nullptr);
    setNearest3D();
    glBindTexture(GL_TEXTURE_3D, 0);
    glGenTextures(1, &m_depthArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_depthArray);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT16, size, size, kBatch, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT,
                 nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glGenFramebuffers(1, &m_depthFbo);
    profiling::countTextureCreation(2);
    // Accumulator cleared to zero, slice by slice.
    GLint previous = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    if (!m_sliceFbo) glGenFramebuffers(1, &m_sliceFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_sliceFbo);
    bool complete = true;
    const GLfloat zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    for (int z = 0; z < b.dims.z && complete; ++z) {
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_accumulator, 0, z);
        const GLenum drawBuffer = GL_COLOR_ATTACHMENT0;
        glDrawBuffers(1, &drawBuffer);
        complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        if (complete) glClearBufferfv(GL_COLOR, 0, zero);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, m_depthFbo);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_depthArray, 0, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    complete = complete && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
    if (!complete) {
        releaseJobTargets();
        return false;
    }
    return true;
}

void SkyOcclusionVolume::releaseJobTargets()
{
    if (m_accumulator) glDeleteTextures(1, &m_accumulator);
    if (m_depthArray) glDeleteTextures(1, &m_depthArray);
    if (m_depthFbo) glDeleteFramebuffers(1, &m_depthFbo);
    m_accumulator = m_depthArray = m_depthFbo = 0;
}

void SkyOcclusionVolume::bindDepthLayer(int layer, bool clear)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_depthFbo);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_depthArray, 0, layer);
    glViewport(0, 0, m_job.spec.mapSize, m_job.spec.mapSize);
    if (clear) {
        glDepthMask(GL_TRUE);
        glClearDepth(1.0);
        glClear(GL_DEPTH_BUFFER_BIT);
    }
}

void SkyOcclusionVolume::prepareFullscreenState()
{
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_COLOR_LOGIC_OP);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBindVertexArray(m_vao);
    for (GLuint unit = 0; unit < 2; ++unit) glBindSampler(unit, 0);
}

void SkyOcclusionVolume::restoreState()
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    glBindTexture(GL_TEXTURE_3D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(0);
    glUseProgram(0);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void SkyOcclusionVolume::accumulate(int count, const std::array<DirectionMatrices, kBatch>& matrices, const std::array<glm::vec3, kBatch>& directions)
{
    const Bounds& b = m_job.bounds;
    prepareFullscreenState();
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE);
    glUseProgram(m_accumulate.id);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_depthArray);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(m_accumulate.loc("u_DepthMaps"), 1);
    glUniform1i(m_accumulate.loc("u_Count"), count);
    glUniform3fv(m_accumulate.loc("u_Origin"), 1, glm::value_ptr(b.origin));
    glUniform1f(m_accumulate.loc("u_Cell"), b.cell);
    std::array<glm::mat4, kBatch> textureFromWorld{};
    std::array<glm::vec4, kBatch> directionBias{};
    for (int i = 0; i < count; ++i) {
        textureFromWorld[static_cast<size_t>(i)] = matrices[static_cast<size_t>(i)].textureFromWorld;
        // A cell must lie this far in front of the outermost surface (texture depth units): a tenth of a cell, at
        // least half a depth-map texel.
        const float biasWorld = std::max(0.1f * b.cell, 0.5f * matrices[static_cast<size_t>(i)].texelWorld);
        directionBias[static_cast<size_t>(i)] =
            glm::vec4(directions[static_cast<size_t>(i)], biasWorld / matrices[static_cast<size_t>(i)].depthRange);
    }
    glUniformMatrix4fv(m_accumulate.loc("u_TextureFromWorld"), kBatch, GL_FALSE, glm::value_ptr(textureFromWorld[0]));
    glUniform4fv(m_accumulate.loc("u_DirectionBias"), kBatch, glm::value_ptr(directionBias[0]));
    const GLint sliceLocation = m_accumulate.loc("u_Slice");
    glBindFramebuffer(GL_FRAMEBUFFER, m_sliceFbo);
    glViewport(0, 0, b.dims.x, b.dims.y);
    for (int z = 0; z < b.dims.z; ++z) {
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_accumulator, 0, z);
        glUniform1i(sliceLocation, z);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glDisable(GL_BLEND);
}

bool SkyOcclusionVolume::finalize()
{
    const Bounds& b = m_job.bounds;
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_3D, texture);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA16F, b.dims.x, b.dims.y, b.dims.z, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
    setNearest3D();
    glBindTexture(GL_TEXTURE_3D, 0);
    profiling::countTextureCreation(1);
    prepareFullscreenState();
    glDisable(GL_BLEND);
    glUseProgram(m_finalize.id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, m_accumulator);
    glUniform1i(m_finalize.loc("u_Accumulator"), 0);
    glUniform1f(m_finalize.loc("u_InvDirections"), 1.0f / static_cast<float>(m_job.spec.directions));
    // Fewer than half a visible direction (the PCF gives fractions): the cell is inside a solid.
    glUniform1f(m_finalize.loc("u_MinVisible"), 0.5f / static_cast<float>(m_job.spec.directions));
    const GLint sliceLocation = m_finalize.loc("u_Slice");
    glBindFramebuffer(GL_FRAMEBUFFER, m_sliceFbo);
    glViewport(0, 0, b.dims.x, b.dims.y);
    bool complete = true;
    for (int z = 0; z < b.dims.z && complete; ++z) {
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0, z);
        if (z == 0) complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        if (!complete) break;
        glUniform1i(sliceLocation, z);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    if (!complete) {
        glDeleteTextures(1, &texture);
        return false;
    }
    destroyVolume(m_front);
    m_front.texture = texture;
    m_front.bounds = b;
    m_front.key = m_job.key;
    m_front.quality = m_job.quality;
    return true;
}

void SkyOcclusionVolume::setNearest3D()
{
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0);
}

GLuint SkyOcclusionVolume::compile(GLenum type, const std::string& source)
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
        std::fprintf(stderr, "Sky occlusion shader compile error: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint SkyOcclusionVolume::link(const std::string& fragmentBody, const char* passDefine)
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
        std::fprintf(stderr, "Sky occlusion program link error: %s\n", log);
        glDeleteProgram(program);
        return 0;
    }
    profiling::countShaderCompile();
    profiling::countShaderCompile();
    profiling::countProgramLink();
    return program;
}

bool SkyOcclusionVolume::ensurePrograms()
{
    if (m_accumulate.id && m_finalize.id && m_vao) return true;
    if (m_programsFailed) return false;
    const VfsFile file = EmbeddedVFS::instance().getFile("shaders/sky_occlusion_bake.frag");
    if (!file.isValid()) {
        m_programsFailed = true;
        std::fprintf(stderr, "Sky occlusion: shaders/sky_occlusion_bake.frag is missing from the embedded resources\n");
        return false;
    }
    const std::string body(file.asStringView());
    m_accumulate.id = link(body, "PASS_ACCUMULATE");
    m_finalize.id = link(body, "PASS_FINALIZE");
    if (!m_vao) {
        glGenVertexArrays(1, &m_vao);
        profiling::countVaoCreation();
    }
    if (!m_accumulate.id || !m_finalize.id) {
        m_programsFailed = true;
        return false;
    }
    return true;
}

int SkyOcclusionVolume::beginTimer()
{
    for (size_t i = 0; i < m_timers.size(); ++i) {
        TimerPair& pair = m_timers[i];
        if (pair.pending) continue;
        if (!pair.queries[0]) glGenQueries(2, pair.queries);
        glQueryCounter(pair.queries[0], GL_TIMESTAMP);
        return static_cast<int>(i);
    }
    return -1;
}

void SkyOcclusionVolume::endTimer(int index, uint64_t serial)
{
    TimerPair& pair = m_timers[static_cast<size_t>(index)];
    glQueryCounter(pair.queries[1], GL_TIMESTAMP);
    pair.pending = true;
    pair.serial = serial;
}

} // namespace ux3d::imgui
