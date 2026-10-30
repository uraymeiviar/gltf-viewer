// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// SkyEnvironment (A6.2 / A6.3, declared in SkyEnvironment.h): the procedural sky LUTs, the source cube, the
// time-sliced IBL filter jobs and their GPU timing.

#include "imgui/SkyEnvironment.h"

namespace ux3d::imgui {

SkyRefineState SkyEnvironment::state() const
{
    if (!m_active || m_front == nullptr) return SkyRefineState::Inactive;
    if (m_job.active) return m_job.refine ? SkyRefineState::Refining : SkyRefineState::Updating;
    if (m_front->version != m_skyVersion) return SkyRefineState::Updating;
    return m_front->full ? SkyRefineState::Converged : SkyRefineState::WaitingToRefine;
}

float SkyEnvironment::progress() const
{
    if (!m_job.active || m_job.totalUnits <= 0.0) return state() == SkyRefineState::Converged ? 1.0f : 0.0f;
    return static_cast<float>(std::clamp(m_job.doneUnits / m_job.totalUnits, 0.0, 1.0));
}

SkyEnvironment::Result SkyEnvironment::update(const Inputs& in)
{
    m_frame = in.frame;
    pollTimers();
    if (!ensurePrograms()) return Result::Failed;
    const bool configChanged = !m_active || !(in.ibl == m_config);
    const bool atmosphereChanged = !m_active || !(in.atmosphere == m_atmosphere);
    const bool sunChanged = !m_active || in.sunDirection != m_sunDirection;
    GlState saved;
    saved.save();
    prepareState();
    m_atmosphere = in.atmosphere;
    m_sunDirection = in.sunDirection;
    if (configChanged) {
        // Activation or a new quality: everything is (re)built synchronously, like an HDR load.
        const auto start = profiling::Clock::now();
        abortJob();
        releaseSets();
        m_config = in.ibl;
        if (!ensureLuts() || !ensureBrdfLuts()) {
            saved.restore();
            return Result::Failed;
        }
        buildAtmosphereLuts();
        buildSkyViewLut();
        ++m_skyVersion;
        m_lastChangeFrame = m_frame;
        m_full = allocateSet(fullSpec());
        runSynchronous(m_full);
        m_front = &m_full;
        m_active = true;
        ++m_stats.synchronousBuilds;
        m_stats.lastSynchronousMs = profiling::elapsedMs(start);
        saved.restore();
        return Result::Reattach;
    }

    Result result = Result::None;
    // Every piece of time-sliced work is timed on its own (GL_TIMESTAMP after it), so the budget uses a measured
    // cost per kind of work (CostKey) and the frame's total extra GPU time is known.
    bool timing = false;
    double usedMs = 0.0;
    auto beginTiming = [&]() {
        if (!timing && in.timeSlicing) timing = beginFrameTimer();
    };
    if (atmosphereChanged || sunChanged) {
        beginTiming();
        if (atmosphereChanged) {
            buildAtmosphereLuts();
            usedMs += timedSegment(costKey(kKindAtmosphereLuts, 0, 0, 0), 1.0);
        }
        buildSkyViewLut();
        usedMs += timedSegment(costKey(kKindSkyViewLut, 0, 0, 0), 1.0);
        ++m_skyVersion;
        m_lastChangeFrame = m_frame;
    }

    if (!in.timeSlicing) {
        // Reference path: every change is filtered at the configured quality before this frame draws.
        abortJob();
        if (m_front != &m_full || m_full.version != m_skyVersion) {
            const auto start = profiling::Clock::now();
            runSynchronous(m_full);
            ++m_stats.synchronousBuilds;
            m_stats.lastSynchronousMs = profiling::elapsedMs(start);
            if (m_front != &m_full) {
                m_front = &m_full;
                ++m_stats.swaps;
            }
            result = Result::Retarget;
        }
        saved.restore();
        return result;
    }

    if (m_job.active && m_job.refine && m_job.version != m_skyVersion) {
        abortJob();
        ++m_stats.abortedRefines;
    }
    if (!m_job.active) {
        if (m_front->version != m_skyVersion) {
            // While the sky changes: reduced size, the source cube rendered at once from the current LUT.
            OutputSet& target = (m_front == &m_fast[0]) ? m_fast[1] : m_fast[0];
            beginTiming();
            startJob(target, fastSpec(), false);
            usedMs += timedSegment(costKey(kKindSourceAtomic, m_job.source->size, 0, 0), sourceUnits(m_job.source->size));
            ++m_stats.fastJobs;
        } else if (!m_front->full && m_frame - m_lastChangeFrame >= kSettleFrames) {
            // The sky is still: the configured quality, its source cube faces and mip chain as sliced work too
            // (the job is aborted if the sky changes before it completes).
            beginTiming();
            startJob(m_full, fullSpec(), true);
            ++m_stats.refineJobs;
        }
    }
    bool refineWork = false;
    if (m_job.active) {
        beginTiming();
        refineWork = m_job.refine;
        usedMs += runSlices(std::max(in.budgetMs, 0.05) - usedMs, false);
        if (m_job.next >= m_job.items.size()) {
            finishJob();
            result = Result::Retarget;
        }
    }
    if (timing) endFrameTimer(refineWork);
    saved.restore();
    return result;
}

void SkyEnvironment::release()
{
    abortJob();
    releaseSets();
    destroyTexture(m_sourceFast.cube);
    destroyTexture(m_sourceFull.cube);
    m_sourceFast = SourceCube();
    m_sourceFull = SourceCube();
    destroyTarget(m_transmittanceLut);
    destroyTarget(m_multiScatteringLut);
    destroyTarget(m_skyViewLut);
    destroyTexture(m_ggxLut);
    destroyTexture(m_charlieLut);
    destroyTexture(m_sheenLut);
    m_brdfLutSize = 0;
    m_brdfLutHalfFloat = false;
    for (GLuint* program : { &m_transmittanceProgram, &m_multiScatteringProgram, &m_skyViewProgram, &m_cubeProgram,
                             &m_filterProgram }) {
        if (*program) glDeleteProgram(*program);
        *program = 0;
    }
    m_programsFailed = false;
    if (m_fbo) glDeleteFramebuffers(1, &m_fbo);
    m_fbo = 0;
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    m_vao = 0;
    for (FrameTimer& timer : m_timers) {
        if (!timer.queries.empty()) glDeleteQueries(static_cast<GLsizei>(timer.queries.size()), timer.queries.data());
        timer = FrameTimer();
    }
    m_activeTimer = -1;
    m_active = false;
    m_front = nullptr;
    m_config = IblConfig();
}

uint64_t SkyEnvironment::memoryBytes() const
{
    uint64_t bytes = 0;
    for (const OutputSet* set : { &m_full, &m_fast[0], &m_fast[1] }) bytes += set->bytes;
    bytes += m_sourceFast.bytes + m_sourceFull.bytes;
    const uint64_t lutTexel = m_brdfLutHalfFloat ? 8 : 16;
    bytes += 2ull * m_brdfLutSize * m_brdfLutSize * lutTexel + 1ull * m_brdfLutSize * m_brdfLutSize * lutTexel;
    bytes += (256ull * 64 + 32ull * 32 + 192ull * 108) * 16;
    return bytes;
}

void SkyEnvironment::prepareState()
{
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_COLOR_LOGIC_OP);
    glDepthMask(GL_FALSE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    for (GLuint unit = 0; unit < 3; ++unit) glBindSampler(unit, 0);
}

uint64_t SkyEnvironment::costKey(int kind, uint32_t size, uint32_t sourceSize, uint32_t samples)
{
    return static_cast<uint64_t>(kind) | (static_cast<uint64_t>(size) << 4) | (static_cast<uint64_t>(sourceSize) << 20) |
           (static_cast<uint64_t>(samples == 1) << 36);
}

bool SkyEnvironment::isLutKey(uint64_t key)
{
    const int kind = static_cast<int>(key & 15u);
    return kind == kKindAtmosphereLuts || kind == kKindSkyViewLut;
}

double SkyEnvironment::msPerUnit(uint64_t key) const
{
    const auto it = m_costs.find(key);
    if (it != m_costs.end() && it->second.count > 0) return it->second.value();
    if (isLutKey(key)) return kLutGuessMs;
    double dearest = 0.0;
    for (const auto& [other, estimate] : m_costs) {
        if (!isLutKey(other) && estimate.count > 0) dearest = std::max(dearest, estimate.value());
    }
    return dearest > 0.0 ? dearest * kUnknownCostFactor : kInitialMsPerUnit;
}

std::string SkyEnvironment::loadShader(const std::string& path, int depth)
{
    const VfsFile file = EmbeddedVFS::instance().getFile(path);
    if (!file.isValid()) {
        std::fprintf(stderr, "Sky: %s is missing from the embedded resources\n", path.c_str());
        return std::string();
    }
    std::string text(file.asStringView());
    if (depth > 4) return text;
    // #include <name> (resolved in shaders/, the ShaderCache convention).
    std::string out;
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(start, end - start);
        const size_t include = line.find("#include");
        const size_t open = line.find_first_of("<\"", include == std::string::npos ? 0 : include);
        const size_t close = open == std::string::npos ? std::string::npos : line.find_first_of(">\"", open + 1);
        if (include != std::string::npos && line.find_first_not_of(" \t") == include && open != std::string::npos &&
            close != std::string::npos) {
            out += loadShader("shaders/" + line.substr(open + 1, close - open - 1), depth + 1);
            out += "\n";
        } else {
            out += line;
            out += "\n";
        }
        start = end + 1;
    }
    return out;
}

GLuint SkyEnvironment::compile(GLenum type, const std::string& source, const char* label)
{
    GLuint shader = glCreateShader(type);
    const char* text = source.c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Sky shader compile error (%s): %s\n", label, log);
        glDeleteShader(shader);
        return 0;
    }
    profiling::countShaderCompile();
    return shader;
}

GLuint SkyEnvironment::link(const std::string& vertexSource, const std::string& fragmentSource, const char* label)
{
    // The prefix ShaderCache gives every shader, so the IBL filter compiles exactly as in IBLSampler.
    static const std::string kPrefix = "#version 410\nprecision highp float;\nprecision highp int;\n";
    const GLuint vs = compile(GL_VERTEX_SHADER, kPrefix + vertexSource, label);
    const GLuint fs = compile(GL_FRAGMENT_SHADER, kPrefix + fragmentSource, label);
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
        char log[4096];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Sky program link error (%s): %s\n", label, log);
        glDeleteProgram(program);
        return 0;
    }
    profiling::countProgramLink();
    return program;
}

bool SkyEnvironment::ensurePrograms()
{
    if (m_filterProgram && m_cubeProgram && m_vao && m_fbo) return true;
    if (m_programsFailed) return false;
    const std::string fullscreen = loadShader("shaders/ibl_sampler/ibl_fullscreen.vert");
    const std::string filter = loadShader("shaders/ibl_sampler/ibl_filtering.frag");
    const std::string passes = loadShader("shaders/sky_passes.frag");
    if (fullscreen.empty() || filter.empty() || passes.empty()) {
        m_programsFailed = true;
        return false;
    }
    auto passProgram = [&](const char* define) {
        return link(fullscreen, std::string("#define SKY_PASSES\n#define ") + define + "\n" + passes, define);
    };
    m_transmittanceProgram = passProgram("PASS_TRANSMITTANCE");
    m_multiScatteringProgram = passProgram("PASS_MULTISCATTER");
    m_skyViewProgram = passProgram("PASS_SKYVIEW");
    m_cubeProgram = passProgram("PASS_CUBE");
    m_filterProgram = link(fullscreen, filter, "ibl_filtering");
    if (!m_transmittanceProgram || !m_multiScatteringProgram || !m_skyViewProgram || !m_cubeProgram || !m_filterProgram) {
        m_programsFailed = true;
        return false;
    }
    glGenVertexArrays(1, &m_vao);
    glGenFramebuffers(1, &m_fbo);
    return true;
}

void SkyEnvironment::destroyTexture(GLuint& texture)
{
    if (texture) glDeleteTextures(1, &texture);
    texture = 0;
}

void SkyEnvironment::destroyTarget(Target& target)
{
    destroyTexture(target.texture);
    target = Target();
}

bool SkyEnvironment::createLut(Target& target, int width, int height)
{
    if (target.texture) return true;
    glGenTextures(1, &target.texture);
    glBindTexture(GL_TEXTURE_2D, target.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    target.width = width;
    target.height = height;
    profiling::countTextureCreation(1);
    return true;
}

bool SkyEnvironment::ensureLuts()
{
    return createLut(m_transmittanceLut, 256, 64) && createLut(m_multiScatteringLut, 32, 32) &&
           createLut(m_skyViewLut, 192, 108);
}

GLuint SkyEnvironment::createCube(uint32_t size, bool mipmaps, GLint internalFormat, uint64_t& bytes, uint32_t maxLevel)
{
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
    const GLint levels = mipmaps ? static_cast<GLint>(std::floor(std::log2(static_cast<double>(size)))) + 1 : 0;
    const uint64_t texelBytes = internalFormat == GL_RGBA16F ? 8 : 16;
    for (GLint level = 0; level <= levels; ++level) {
        const GLint s = std::max<GLint>(1, static_cast<GLint>(size) >> level);
        for (GLenum face = 0; face < 6; ++face) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level, internalFormat, s, s, 0, GL_RGBA, GL_FLOAT, nullptr);
        }
        bytes += 6ull * static_cast<uint64_t>(s) * static_cast<uint64_t>(s) * texelBytes;
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, mipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, mipmaps ? static_cast<GLint>(maxLevel) : 0);
    profiling::countTextureCreation(1);
    return texture;
}

uint32_t SkyEnvironment::mipLevelsFor(uint32_t size)
{
    // IBLSampler::getMipmapLevels: floor(log2(size)) + 1 - lowestMipLevel (the filter writes mips 0..that).
    const int levels = static_cast<int>(std::floor(std::log2(static_cast<double>(std::max(size, 1u))))) + 1 -
                       static_cast<int>(kLowestMipLevel);
    return static_cast<uint32_t>(std::max(levels, 2));
}

SkyEnvironment::SetSpec SkyEnvironment::fullSpec() const
{
    SetSpec spec;
    spec.size = std::max(m_config.cubeSize, 32u);
    spec.lambertianSize = m_config.lambertianSize == 0 ? spec.size : std::min(m_config.lambertianSize, spec.size);
    spec.ggxSamples = m_config.ggxSamples;
    spec.lambertianSamples = m_config.lambertianSamples;
    spec.sheenSamples = m_config.sheenSamples;
    spec.full = true;
    return spec;
}

SkyEnvironment::SetSpec SkyEnvironment::fastSpec() const
{
    const SetSpec full = fullSpec();
    SetSpec spec;
    spec.size = std::min(kFastCubeSize, full.size);
    spec.lambertianSize = std::min(kFastLambertianSize, std::min(full.lambertianSize, spec.size));
    spec.ggxSamples = std::min(kFastGgxSamples, full.ggxSamples);
    spec.lambertianSamples = std::min(kFastLambertianSamples, full.lambertianSamples);
    spec.sheenSamples = std::min(kFastSheenSamples, full.sheenSamples);
    spec.full = spec.size == full.size && spec.lambertianSize == full.lambertianSize &&
                spec.ggxSamples == full.ggxSamples && spec.lambertianSamples == full.lambertianSamples &&
                spec.sheenSamples == full.sheenSamples;
    return spec;
}

SkyEnvironment::OutputSet SkyEnvironment::allocateSet(const SetSpec& spec) const
{
    OutputSet set;
    set.size = spec.size;
    set.lambertianSize = spec.lambertianSize;
    set.mipLevels = mipLevelsFor(spec.size);
    set.halfFloat = m_config.halfFloat;
    set.full = spec.full;
    const GLint format = m_config.halfFloat ? GL_RGBA16F : GL_RGBA32F;
    set.lambertian = createCube(spec.lambertianSize, false, format, set.bytes, 0);
    set.ggx = createCube(spec.size, true, format, set.bytes, set.mipLevels);
    set.sheen = createCube(spec.size, true, format, set.bytes, set.mipLevels);
    return set;
}

void SkyEnvironment::destroySet(OutputSet& set)
{
    destroyTexture(set.lambertian);
    destroyTexture(set.ggx);
    destroyTexture(set.sheen);
    set = OutputSet();
}

void SkyEnvironment::ensureSet(OutputSet& set, const SetSpec& spec)
{
    if (set.ggx != 0 && set.size == spec.size && set.lambertianSize == spec.lambertianSize &&
        set.halfFloat == m_config.halfFloat) {
        set.full = spec.full;
        return;
    }
    destroySet(set);
    set = allocateSet(spec);
}

void SkyEnvironment::releaseSets()
{
    destroySet(m_full);
    destroySet(m_fast[0]);
    destroySet(m_fast[1]);
    m_front = nullptr;
}

SkyEnvironment::SourceCube& SkyEnvironment::ensureSource(bool full, uint32_t size)
{
    SourceCube& source = full ? m_sourceFull : m_sourceFast;
    if (source.cube != 0 && source.size == size) return source;
    destroyTexture(source.cube);
    source = SourceCube();
    source.size = size;
    source.cube = createCube(size, true, GL_RGBA32F, source.bytes, 1000);
    return source;
}

void SkyEnvironment::drawFullscreen()
{
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void SkyEnvironment::setAtmosphereUniforms(GLuint program) const
{
    const AtmosphereParameters& p = m_atmosphere;
    const float mie = std::max(p.mieDensity, 0.0f);
    const glm::vec3 rayleigh = sky::rayleighScattering();
    const glm::vec3 mieScattering = sky::mieScattering() * mie;
    const glm::vec3 mieExtinction = sky::mieExtinction() * mie;
    const glm::vec3 ozone = p.ozone ? sky::ozoneAbsorption() : glm::vec3(0.0f);
    glUniform4f(glGetUniformLocation(program, "u_AtmosphereRadii"), static_cast<float>(sky::kBottomRadiusKm),
                static_cast<float>(sky::kTopRadiusKm), static_cast<float>(sky::kBottomRadiusKm + sky::kObserverAltitudeKm), 0.0f);
    glUniform4f(glGetUniformLocation(program, "u_Rayleigh"), rayleigh.r, rayleigh.g, rayleigh.b, sky::kRayleighScaleHeightKm);
    glUniform4f(glGetUniformLocation(program, "u_MieScattering"), mieScattering.r, mieScattering.g, mieScattering.b,
                sky::kMieScaleHeightKm);
    glUniform4f(glGetUniformLocation(program, "u_MieExtinction"), mieExtinction.r, mieExtinction.g, mieExtinction.b,
                sky::kMiePhaseG);
    glUniform4f(glGetUniformLocation(program, "u_OzoneAbsorption"), ozone.r, ozone.g, ozone.b, sky::kOzonePeakKm);
    glUniform4f(glGetUniformLocation(program, "u_GroundAlbedo"), p.groundAlbedo.r, p.groundAlbedo.g, p.groundAlbedo.b,
                sky::kOzoneHalfWidthKm);
    glUniform3f(glGetUniformLocation(program, "u_SunDirection"), m_sunDirection.x, m_sunDirection.y, m_sunDirection.z);
    // A6.6 night sky glow, per unit top-of-atmosphere sun illuminance like everything in the LUTs.
    const glm::vec3 airglow = night::airglowRadiance(p.airglow) / sky::kSolarIlluminance;
    glUniform4f(glGetUniformLocation(program, "u_Airglow"), airglow.r, airglow.g, airglow.b, night::kAirglowLayerKm);
    glUniform1i(glGetUniformLocation(program, "u_TransmittanceLUT"), 0);
    glUniform1i(glGetUniformLocation(program, "u_MultiScatteringLUT"), 1);
    glUniform1i(glGetUniformLocation(program, "u_SkyViewLUT"), 2);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_transmittanceLut.texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_multiScatteringLut.texture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_skyViewLut.texture);
    glActiveTexture(GL_TEXTURE0);
}

void SkyEnvironment::renderLut(GLuint program, const Target& target)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.texture, 0);
    const GLenum drawBuffer = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &drawBuffer);
    glViewport(0, 0, target.width, target.height);
    glUseProgram(program);
    // The pass samples the other LUTs; unbind the one it writes so no feedback loop exists.
    setAtmosphereUniforms(program);
    for (GLuint unit = 0; unit < 3; ++unit) {
        const GLuint bound = unit == 0 ? m_transmittanceLut.texture : unit == 1 ? m_multiScatteringLut.texture : m_skyViewLut.texture;
        if (bound == target.texture) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }
    glActiveTexture(GL_TEXTURE0);
    glUniform2f(glGetUniformLocation(program, "u_TargetSize"), static_cast<float>(target.width), static_cast<float>(target.height));
    drawFullscreen();
}

void SkyEnvironment::buildAtmosphereLuts()
{
    renderLut(m_transmittanceProgram, m_transmittanceLut);
    renderLut(m_multiScatteringProgram, m_multiScatteringLut);
    ++m_stats.lutAtmosphereBuilds;
}

void SkyEnvironment::buildSkyViewLut()
{
    renderLut(m_skyViewProgram, m_skyViewLut);
    ++m_stats.lutSkyViewBuilds;
}

float SkyEnvironment::chooseRadianceScale()
{
    if (!m_config.halfFloat || m_skyViewLut.texture == 0) return 1.0f;
    const auto start = profiling::Clock::now();
    std::vector<float> pixels(static_cast<size_t>(m_skyViewLut.width) * m_skyViewLut.height * 4);
    GLint packBuffer = 0;
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &packBuffer);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    GLint packAlignment = 4;
    glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glActiveTexture(GL_TEXTURE0);
    GLint bound = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);
    glBindTexture(GL_TEXTURE_2D, m_skyViewLut.texture);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(bound));
    glPixelStorei(GL_PACK_ALIGNMENT, packAlignment);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(packBuffer));
    float peak = 0.0f;
    bool finite = true;
    for (size_t i = 0; i < pixels.size(); i += 4) {
        for (size_t c = 0; c < 3; ++c) {
            const float value = pixels[i + c];
            if (!std::isfinite(value)) finite = false;
            else peak = std::max(peak, value * sky::kSolarIlluminance);
        }
    }
    float scale = 1048576.0f; // 2^20
    if (!finite) scale = 1.0f;
    while (scale > 1.0f && peak * scale > 32000.0f) scale *= 0.5f;
    const double ms = profiling::elapsedMs(start);
    ++m_stats.radianceScaleReadbacks;
    m_stats.radianceScaleMsLast = ms;
    m_stats.radianceScaleMsMax = std::max(m_stats.radianceScaleMsMax, ms);
    m_stats.radianceScaleMsSum += ms;
    return scale;
}

void SkyEnvironment::renderSourceFace(const SourceCube& source, GLint face)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    const GLenum drawBuffer = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &drawBuffer);
    glUseProgram(m_cubeProgram);
    setAtmosphereUniforms(m_cubeProgram);
    // 32-bit cubes: exactly kSolarIlluminance (S = 1).
    glUniform1f(glGetUniformLocation(m_cubeProgram, "u_RadianceScale"), sky::kSolarIlluminance * source.radianceScale);
    glUniform2f(glGetUniformLocation(m_cubeProgram, "u_TargetSize"), static_cast<float>(source.size), static_cast<float>(source.size));
    glViewport(0, 0, static_cast<GLsizei>(source.size), static_cast<GLsizei>(source.size));
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, source.cube, 0);
    glUniform1i(glGetUniformLocation(m_cubeProgram, "u_Face"), face);
    drawFullscreen();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
}

void SkyEnvironment::buildSourceMips(const SourceCube& source)
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, source.cube);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
}

void SkyEnvironment::renderSourceCube(const SourceCube& source)
{
    for (GLint face = 0; face < 6; ++face) renderSourceFace(source, face);
    buildSourceMips(source);
}

void SkyEnvironment::drawFilter(const WorkItem& item, const SourceCube& source, uint32_t row, uint32_t rows)
{
    const uint32_t size = std::max(1u, item.size);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + item.face, item.target,
                           static_cast<GLint>(item.mip));
    const GLenum drawBuffer = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &drawBuffer);
    glViewport(0, 0, static_cast<GLsizei>(size), static_cast<GLsizei>(size));
    const bool band = row != 0 || rows < size;
    if (band) {
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, static_cast<GLint>(row), static_cast<GLsizei>(size), static_cast<GLsizei>(rows));
    }
    glUseProgram(m_filterProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, source.cube);
    glUniform1i(glGetUniformLocation(m_filterProgram, "u_cubemapTexture"), 0);
    glUniform1i(glGetUniformLocation(m_filterProgram, "uCubeMap"), 0);
    glUniform1f(glGetUniformLocation(m_filterProgram, "u_roughness"), item.roughness);
    glUniform1i(glGetUniformLocation(m_filterProgram, "u_sampleCount"), static_cast<GLint>(item.samples));
    glUniform1i(glGetUniformLocation(m_filterProgram, "u_width"), static_cast<GLint>(source.size));
    glUniform1f(glGetUniformLocation(m_filterProgram, "u_lodBias"), m_config.lodBias);
    glUniform1i(glGetUniformLocation(m_filterProgram, "u_distribution"), item.distribution);
    glUniform1i(glGetUniformLocation(m_filterProgram, "u_currentFace"), static_cast<GLint>(item.face));
    glUniform1i(glGetUniformLocation(m_filterProgram, "u_isGeneratingLUT"), 0);
    glUniform1f(glGetUniformLocation(m_filterProgram, "u_maxValue"), m_config.halfFloat ? 65504.0f : 3.0e38f);
    drawFullscreen();
    if (band) glDisable(GL_SCISSOR_TEST);
}

bool SkyEnvironment::ensureBrdfLuts()
{
    if (m_ggxLut != 0 && m_brdfLutSize == m_config.lutSize && m_brdfLutHalfFloat == m_config.halfFloat) return true;
    destroyTexture(m_ggxLut);
    destroyTexture(m_charlieLut);
    destroyTexture(m_sheenLut);
    m_brdfLutSize = std::max(m_config.lutSize, 1u);
    m_brdfLutHalfFloat = m_config.halfFloat;
    const GLint format = m_config.halfFloat ? GL_RGBA16F : GL_RGBA32F;
    auto create = [&](GLuint& texture) {
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, format, static_cast<GLsizei>(m_brdfLutSize), static_cast<GLsizei>(m_brdfLutSize), 0,
                     GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        profiling::countTextureCreation(1);
    };
    create(m_ggxLut);
    create(m_charlieLut);
    create(m_sheenLut);
    // IBLSampler::sampleLut: 512 samples, distributions 1 (GGX) and 2 (Charlie). The sheen E LUT is allocated but
    // never written there; it is cleared to zero here.
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    const GLenum drawBuffer = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(1, &drawBuffer);
    glViewport(0, 0, static_cast<GLsizei>(m_brdfLutSize), static_cast<GLsizei>(m_brdfLutSize));
    glUseProgram(m_filterProgram);
    glUniform1i(glGetUniformLocation(m_filterProgram, "u_cubemapTexture"), 0);
    glUniform1f(glGetUniformLocation(m_filterProgram, "u_roughness"), 0.0f);
    glUniform1i(glGetUniformLocation(m_filterProgram, "u_sampleCount"), 512);
    glUniform1i(glGetUniformLocation(m_filterProgram, "u_width"), static_cast<GLint>(m_config.cubeSize));
    glUniform1f(glGetUniformLocation(m_filterProgram, "u_lodBias"), m_config.lodBias);
    glUniform1i(glGetUniformLocation(m_filterProgram, "u_currentFace"), 0);
    glUniform1i(glGetUniformLocation(m_filterProgram, "u_isGeneratingLUT"), 1);
    glUniform1f(glGetUniformLocation(m_filterProgram, "u_maxValue"), m_config.halfFloat ? 65504.0f : 3.0e38f);
    for (const auto& [distribution, texture] : { std::pair<int, GLuint>{ 1, m_ggxLut }, std::pair<int, GLuint>{ 2, m_charlieLut } }) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        glUniform1i(glGetUniformLocation(m_filterProgram, "u_distribution"), distribution);
        drawFullscreen();
    }
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_sheenLut, 0);
    const GLfloat zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    glClearBufferfv(GL_COLOR, 0, zero);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_UNSUPPORTED;
}

double SkyEnvironment::itemUnits(const WorkItem& item)
{
    if (item.distribution == kKindSourceFace) return static_cast<double>(item.size) * item.size + kDrawUnits;
    if (item.distribution == kKindSourceMips) return 6.0 * item.size * item.size * 2.0 + kDrawUnits;
    return rowUnits(item) * std::max(1u, item.size) + kDrawUnits;
}

std::vector<SkyEnvironment::WorkItem> SkyEnvironment::buildItems(const OutputSet& set, const SetSpec& spec, bool sourceItems, const SourceCube& source) const
{
    std::vector<WorkItem> items;
    if (sourceItems) {
        for (uint32_t face = 0; face < 6; ++face) items.push_back({ kKindSourceFace, 0.0f, 0, face, source.cube, source.size, 1 });
        items.push_back({ kKindSourceMips, 0.0f, 0, 0, source.cube, source.size, 1 });
    }
    for (uint32_t face = 0; face < 6; ++face) {
        items.push_back({ kKindLambertian, 0.0f, 0, face, set.lambertian, set.lambertianSize, spec.lambertianSamples });
    }
    const uint32_t levels = set.mipLevels;
    for (int distribution : { kKindGgx, kKindCharlie }) {
        const GLuint target = distribution == kKindGgx ? set.ggx : set.sheen;
        const uint32_t samples = distribution == kKindGgx ? spec.ggxSamples : spec.sheenSamples;
        for (uint32_t mip = 0; mip <= levels; ++mip) {
            const float roughness = static_cast<float>(mip) / static_cast<float>(levels - 1u);
            // GGX roughness 0: every importance sample is the normal itself, so one sample is the same value.
            const uint32_t count = (distribution == kKindGgx && mip == 0) ? 1u : samples;
            for (uint32_t face = 0; face < 6; ++face) {
                items.push_back({ distribution, roughness, mip, face, target, std::max(1u, set.size >> mip), count });
            }
        }
    }
    return items;
}

void SkyEnvironment::startJob(OutputSet& target, const SetSpec& spec, bool refine, bool sliceSource)
{
    ensureSet(target, spec);
    SourceCube& source = ensureSource(spec.full, spec.size);
    // The LUT is final for this job (a sky change aborts refine jobs; fast jobs render their source now).
    source.radianceScale = chooseRadianceScale();
    const bool sourceItems = refine && sliceSource;
    if (!sourceItems) renderSourceCube(source);
    m_job = Job();
    m_job.active = true;
    m_job.refine = refine;
    m_job.target = &target;
    m_job.source = &source;
    m_job.version = m_skyVersion;
    m_job.items = buildItems(target, spec, sourceItems, source);
    for (const WorkItem& item : m_job.items) m_job.totalUnits += itemUnits(item);
}

double SkyEnvironment::runSlices(double budgetMs, bool all)
{
    double usedMs = 0.0;
    bool drew = false;
    while (m_job.active && m_job.next < m_job.items.size()) {
        const WorkItem& item = m_job.items[m_job.next];
        const uint64_t key = itemKey(item, m_job.source->size);
        const double cost = msPerUnit(key);
        if (item.distribution == kKindSourceFace || item.distribution == kKindSourceMips) {
            const double units = itemUnits(item);
            if (!all && drew && usedMs + cost * units > budgetMs) break;
            if (item.distribution == kKindSourceFace) renderSourceFace(*m_job.source, static_cast<GLint>(item.face));
            else buildSourceMips(*m_job.source);
            usedMs += timedSegment(key, units);
            m_job.doneUnits += units;
            drew = true;
            ++m_job.next;
            m_job.nextRow = 0;
            continue;
        }
        const uint32_t size = std::max(1u, item.size);
        const double unitsPerRow = rowUnits(item);
        const uint32_t rowsLeft = size - m_job.nextRow;
        uint32_t rows = rowsLeft;
        if (!all) {
            const double room = budgetMs - usedMs - cost * kDrawUnits;
            const double rowMs = cost * unitsPerRow;
            if (drew && room < rowMs) break;
            rows = static_cast<uint32_t>(std::clamp(std::floor(room / std::max(rowMs, 1.0e-12)), 1.0, static_cast<double>(rowsLeft)));
        }
        drawFilter(item, *m_job.source, m_job.nextRow, rows);
        const double units = unitsPerRow * rows + kDrawUnits;
        usedMs += timedSegment(key, units);
        m_job.doneUnits += units;
        drew = true;
        m_job.nextRow += rows;
        if (m_job.nextRow >= size) {
            ++m_job.next;
            m_job.nextRow = 0;
        }
    }
    return usedMs;
}

void SkyEnvironment::finishJob()
{
    if (m_job.next < m_job.items.size()) ++m_stats.incompleteSwaps; // never: only complete jobs are swapped in
    OutputSet* target = m_job.target;
    target->version = m_job.version;
    target->radianceScale = m_job.source->radianceScale;
    m_front = target;
    ++m_stats.swaps;
    const bool fullSource = m_job.source == &m_sourceFull;
    m_job = Job();
    if (fullSource) {
        // Like IBLSampler, the configured-size source cube is not kept once filtered (the next refinement
        // creates it again as its first work item).
        destroyTexture(m_sourceFull.cube);
        m_sourceFull = SourceCube();
    }
}

void SkyEnvironment::runSynchronous(OutputSet& target)
{
    startJob(target, fullSpec(), true, false);
    runSlices(0.0, true);
    finishJob();
}

bool SkyEnvironment::beginFrameTimer()
{
    for (size_t i = 0; i < m_timers.size(); ++i) {
        const size_t index = (m_nextTimer + i) % m_timers.size();
        FrameTimer& timer = m_timers[index];
        if (timer.pending) continue;
        timer.segments.clear();
        if (timer.queries.empty()) {
            timer.queries.resize(1);
            glGenQueries(1, timer.queries.data());
        }
        glQueryCounter(timer.queries[0], GL_TIMESTAMP);
        m_activeTimer = static_cast<int>(index);
        m_nextTimer = (index + 1) % m_timers.size();
        return true;
    }
    return false;
}

double SkyEnvironment::timedSegment(uint64_t key, double units)
{
    if (m_activeTimer >= 0) {
        FrameTimer& timer = m_timers[static_cast<size_t>(m_activeTimer)];
        const size_t index = timer.segments.size() + 1;
        if (timer.queries.size() <= index) {
            const size_t previous = timer.queries.size();
            timer.queries.resize(index + 1);
            glGenQueries(static_cast<GLsizei>(timer.queries.size() - previous), timer.queries.data() + previous);
        }
        glQueryCounter(timer.queries[index], GL_TIMESTAMP);
        timer.segments.push_back({ key, units });
    }
    return msPerUnit(key) * units;
}

void SkyEnvironment::endFrameTimer(bool refine)
{
    if (m_activeTimer < 0) return;
    FrameTimer& timer = m_timers[static_cast<size_t>(m_activeTimer)];
    timer.pending = !timer.segments.empty();
    timer.refine = refine;
    m_activeTimer = -1;
}

void SkyEnvironment::pollTimers()
{
    for (FrameTimer& timer : m_timers) {
        if (!timer.pending) continue;
        const size_t last = timer.segments.size();
        GLint available = 0;
        glGetQueryObjectiv(timer.queries[last], GL_QUERY_RESULT_AVAILABLE, &available);
        if (!available) continue;
        std::vector<GLuint64> stamps(last + 1, 0);
        for (size_t i = 0; i <= last; ++i) glGetQueryObjectui64v(timer.queries[i], GL_QUERY_RESULT, &stamps[i]);
        timer.pending = false;
        const double ms = stamps[last] > stamps[0] ? static_cast<double>(stamps[last] - stamps[0]) * 1.0e-6 : 0.0;
        m_stats.gpuMsLast = ms;
        m_stats.gpuMsMax = std::max(m_stats.gpuMsMax, ms);
        m_stats.gpuMsSum += ms;
        ++m_stats.slicedFrames;
        if (ms > kStatedBudgetMs) ++m_stats.overBudgetFrames;
        if (timer.refine) {
            ++m_stats.refineFrames;
            m_stats.refineGpuMsSum += ms;
        }
        // Cost per unit of each kind of work in this frame (summed over its pieces).
        std::vector<std::pair<uint64_t, std::pair<double, double>>> perKey;
        for (size_t i = 0; i < last; ++i) {
            const double segmentMs = stamps[i + 1] > stamps[i] ? static_cast<double>(stamps[i + 1] - stamps[i]) * 1.0e-6 : 0.0;
            const uint64_t key = timer.segments[i].key;
            auto it = std::find_if(perKey.begin(), perKey.end(), [key](const auto& entry) { return entry.first == key; });
            if (it == perKey.end()) {
                perKey.push_back({ key, { segmentMs, timer.segments[i].units } });
            } else {
                it->second.first += segmentMs;
                it->second.second += timer.segments[i].units;
            }
        }
        for (const auto& [key, value] : perKey) {
            if (value.second <= 0.0) continue;
            CostEstimate& estimate = m_costs[key];
            estimate.window[estimate.count % estimate.window.size()] = value.first / value.second;
            ++estimate.count;
        }
        double dearest = 0.0;
        for (const auto& [key, estimate] : m_costs) {
            if (!isLutKey(key) && estimate.count > 0) dearest = std::max(dearest, estimate.value());
        }
        m_stats.msPerMegaUnit = dearest * 1.0e6;
        auto measured = [this](uint64_t key, double units) {
            const auto it = m_costs.find(key);
            return it != m_costs.end() && it->second.count > 0 ? it->second.value() * units : 0.0;
        };
        m_stats.skyViewLutMs = measured(costKey(kKindSkyViewLut, 0, 0, 0), 1.0);
        m_stats.atmosphereLutsMs = measured(costKey(kKindAtmosphereLuts, 0, 0, 0), 1.0);
        const uint32_t fastSize = fastSpec().size;
        m_stats.fastSourceMs = measured(costKey(kKindSourceAtomic, fastSize, 0, 0), sourceUnits(fastSize));
    }
}

} // namespace ux3d::imgui
