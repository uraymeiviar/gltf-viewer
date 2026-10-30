// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "imgui/EmbeddedVFS.h"
#include "imgui/NightSky.h"
#include "imgui/Profiling.h"
#include "imgui/SkyAtmosphere.h"

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ux3d::imgui {

// A6.2 / A6.3: the procedural sky and the dynamic environment lighting built from it
// (docs/ASOBO_EXTENSIONS_PHASE_6.md). Implementation in source/imgui/SkyEnvironment.cpp.
//
// Sky (A6.2): three small LUTs (sky_passes.frag; model and units in SkyAtmosphere.h): transmittance (256 x 64) and
// multiple scattering (32 x 32), rebuilt when the atmosphere parameters change, and the sky view (192 x 108), rebuilt
// when the sun moves. The background samples the sky-view LUT directly (cubemap.frag, PROCEDURAL_SKY), so it follows
// the sun the same frame.
//
// Environment lighting (A6.3): the sky is rendered into a source cube (PASS_CUBE), mipmapped, and filtered with the
// IBL sampler's own shader (ibl_filtering.frag: Lambertian irradiance, GGX mips, Charlie sheen mips, BRDF LUTs) into
// a set of output cubes. The renderer's EnvironmentMap always points at the *front* set; work goes into another set,
// which becomes the front only once every pass of the job is complete (never a half-updated environment).
//  * Activation, a quality change, or time slicing off: one synchronous filter at the configured quality (as an HDR
//    load does).
//  * The sky changed (time of day, sun, atmosphere): a *fast* job (cube 128, irradiance 32, 128 GGX / 256 Lambertian /
//    32 sheen samples, or the configured values when smaller) renders its source cube from the current LUT at once and
//    then runs its filter passes in row bands, as many per frame as the GPU budget allows (budgetMs, default 2.5 ms,
//    from a per-unit cost measured with GL_TIMESTAMP queries); when complete it is swapped in. While the sky keeps
//    changing, the next fast job starts from the then-current LUT.
//  * The sky has been still for kSettleFrames frames and the front is a fast set: a *refine* job at the configured
//    quality runs the same way and is swapped in when complete (aborted if the sky changes meanwhile).
// GGX mip 0 (roughness 0) uses one sample: the configured count gives the same value (every sample is the same
// direction). Every other pass uses the configured (or fast) counts unchanged.
class SkyEnvironment {
public:
    // IBL quality the refined cubes use (the effective render-quality settings).
    struct IblConfig {
        uint32_t cubeSize = 1024;
        uint32_t lambertianSize = 0; // 0 = cubeSize
        uint32_t lutSize = 1024;
        bool halfFloat = false;
        uint32_t ggxSamples = 1024;
        uint32_t lambertianSamples = 2048;
        uint32_t sheenSamples = 64;
        float lodBias = 0.0f;
        bool operator==(const IblConfig&) const = default;
    };

    struct Inputs {
        AtmosphereParameters atmosphere;
        glm::vec3 sunDirection = glm::vec3(0.0f, 1.0f, 0.0f); // world, unit
        IblConfig ibl;
        bool timeSlicing = true;
        double budgetMs = 2.5;
        uint64_t frame = 0;
    };

    enum class Result {
        None,      // the front cubes are unchanged
        Reattach,  // new front cubes and LUTs (activation, quality change): rebuild the environment map
        Retarget,  // the front cubes were swapped: point the environment map at them
        Failed,    // programs or targets could not be created
    };

    struct Stats {
        uint64_t synchronousBuilds = 0;
        double lastSynchronousMs = 0.0;   // CPU wall time of the last synchronous build (GPU work queued + LUTs)
        uint64_t fastJobs = 0, refineJobs = 0, abortedRefines = 0, swaps = 0, incompleteSwaps = 0;
        uint64_t lutAtmosphereBuilds = 0, lutSkyViewBuilds = 0;
        uint64_t slicedFrames = 0;        // frames whose time-sliced sky work has a GPU time
        uint64_t overBudgetFrames = 0;    // ... above kStatedBudgetMs
        double gpuMsLast = 0.0, gpuMsMax = 0.0, gpuMsSum = 0.0;
        uint64_t refineFrames = 0;        // time-sliced frames that ran refinement work
        double refineGpuMsSum = 0.0;      // their GPU time (all refinements so far)
        double msPerMegaUnit = 0.0;       // the dearest measured filter cost (GPU ms per 1e6 sample-texels)
        double skyViewLutMs = 0.0;        // measured GPU time of one sky-view LUT build
        double atmosphereLutsMs = 0.0;    // ... of the transmittance + multiple-scattering LUTs
        double fastSourceMs = 0.0;        // ... of a fast job's source cube (6 faces + mips)
        // Half-float range encoding (chooseRadianceScale): sky-view LUT readbacks and their CPU wall time (the
        // readback waits for the LUT build).
        uint64_t radianceScaleReadbacks = 0;
        double radianceScaleMsLast = 0.0, radianceScaleMsMax = 0.0, radianceScaleMsSum = 0.0;
    };

    static constexpr uint32_t kFastCubeSize = 128;
    static constexpr uint32_t kFastLambertianSize = 32;
    static constexpr uint32_t kFastGgxSamples = 128;
    static constexpr uint32_t kFastLambertianSamples = 256;
    static constexpr uint32_t kFastSheenSamples = 32;
    static constexpr uint32_t kLowestMipLevel = 4; // IBLSampler's InitizalizationParameters::lowestMipLevel
    static constexpr uint64_t kSettleFrames = 10;
    static constexpr double kStatedBudgetMs = 4.0; // the documented per-frame limit (budgetMs aims below it)

    SkyEnvironment() = default;
    ~SkyEnvironment() { release(); }
    SkyEnvironment(const SkyEnvironment&) = delete;
    SkyEnvironment& operator=(const SkyEnvironment&) = delete;

    bool active() const { return m_active; }
    // A job is running or due: keep frames coming (the idle wait would stall it).
    bool busy() const { return m_active && (m_job.active || (m_front != nullptr && !m_front->full)); }
    GLuint skyViewLut() const { return m_skyViewLut.texture; }
    GLuint transmittanceLut() const { return m_transmittanceLut.texture; }
    GLuint frontLambertian() const { return m_front ? m_front->lambertian : 0; }
    GLuint frontGgx() const { return m_front ? m_front->ggx : 0; }
    GLuint frontSheen() const { return m_front ? m_front->sheen : 0; }
    // The renderer's environment intensity for the front cubes: 1 / the range-encoding scale they were filtered with
    // (exactly 1 with 32-bit cubes). Always the completed front set's, never the job in flight.
    float frontRadianceDecode() const { return m_front ? 1.0f / m_front->radianceScale : 1.0f; }
    uint32_t frontMipCount() const { return m_front ? m_front->mipLevels : 0; }
    uint32_t frontSize() const { return m_front ? m_front->size : 0; }
    GLuint ggxLut() const { return m_ggxLut; }
    GLuint charlieLut() const { return m_charlieLut; }
    GLuint sheenLut() const { return m_sheenLut; }
    const Stats& stats() const { return m_stats; }
    uint64_t skyVersion() const { return m_skyVersion; }
    uint64_t frontVersion() const { return m_front ? m_front->version : 0; }
    const IblConfig& config() const { return m_config; }

    SkyRefineState state() const;

    float progress() const;

    Result update(const Inputs& in);

    // Frees every GL object (the renderer's environment map must stop using the cubes first).
    void release();

    // GPU memory of the resident cubes, LUTs and source cubes (texel payload, mips included).
    uint64_t memoryBytes() const;

private:
    struct Target {
        GLuint texture = 0;
        int width = 0;
        int height = 0;
    };

    struct OutputSet {
        GLuint lambertian = 0, ggx = 0, sheen = 0;
        uint32_t size = 0, lambertianSize = 0, mipLevels = 0;
        bool halfFloat = false;
        bool full = false;
        float radianceScale = 1.0f; // stored radiance / physical radiance (half-float range encoding)
        uint64_t version = 0;
        uint64_t bytes = 0;
    };

    struct SetSpec {
        uint32_t size = 0, lambertianSize = 0;
        uint32_t ggxSamples = 0, lambertianSamples = 0, sheenSamples = 0;
        bool full = false;
    };

    struct SourceCube {
        float radianceScale = 1.0f; // chosen at job start from the sky-view LUT (chooseRadianceScale)
        GLuint cube = 0;
        uint32_t size = 0;
        uint64_t bytes = 0;
    };

    // One piece of a job: a filter pass over a face (and mip) of an output cube, drawn in row bands, or (refine jobs)
    // one face of the source cube or its mip chain.
    struct WorkItem {
        int distribution = 0; // kKindLambertian, kKindGgx, kKindCharlie, kKindSourceFace, kKindSourceMips
        float roughness = 0.0f;
        uint32_t mip = 0;
        uint32_t face = 0;
        GLuint target = 0;
        uint32_t size = 0;
        uint32_t samples = 0;
    };

    struct Job {
        bool active = false;
        bool refine = false;
        OutputSet* target = nullptr;
        SourceCube* source = nullptr;
        uint64_t version = 0;
        std::vector<WorkItem> items;
        size_t next = 0;
        uint32_t nextRow = 0;
        double totalUnits = 0.0;
        double doneUnits = 0.0;
    };

    // One frame's GL_TIMESTAMP queries: queries[0] before the first piece of work, queries[i + 1] after piece i.
    struct FrameTimer {
        struct Segment {
            uint64_t key = 0;
            double units = 0.0;
        };
        std::vector<GLuint> queries;
        std::vector<Segment> segments;
        bool pending = false;
        bool refine = false;
    };

    // The cost per unit of one kind of work: the largest of its last measurements (the budget follows the dearest).
    struct CostEstimate {
        std::array<double, 8> window{};
        size_t count = 0;
        double value() const
        {
            double worst = 0.0;
            for (size_t i = 0; i < std::min(count, window.size()); ++i) worst = std::max(worst, window[i]);
            return worst;
        }
    };

    // GL state the passes change, restored afterwards (the renderer and ImGui keep their own caches).
    struct GlState {
        GLint framebuffer = 0, program = 0, vao = 0, activeTexture = GL_TEXTURE0;
        GLint viewport[4] = { 0, 0, 0, 0 };
        GLint scissorBox[4] = { 0, 0, 0, 0 };
        GLint texture2d[3] = { 0, 0, 0 };
        GLint textureCube[3] = { 0, 0, 0 };
        GLint sampler[3] = { 0, 0, 0 };
        GLint polygonMode[2] = { GL_FILL, GL_FILL };
        GLboolean colorMask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
        GLboolean depthMask = GL_TRUE;
        GLboolean scissor = GL_FALSE, blend = GL_FALSE, depth = GL_FALSE, cull = GL_FALSE, stencil = GL_FALSE;
        GLboolean colorLogic = GL_FALSE;

        void save()
        {
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
            glGetIntegerv(GL_CURRENT_PROGRAM, &program);
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
            glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
            glGetIntegerv(GL_VIEWPORT, viewport);
            glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
            for (int i = 0; i < 3; ++i) {
                glActiveTexture(GL_TEXTURE0 + i);
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture2d[i]);
                glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &textureCube[i]);
                glGetIntegerv(GL_SAMPLER_BINDING, &sampler[i]);
            }
            glActiveTexture(static_cast<GLenum>(activeTexture));
            glGetIntegerv(GL_POLYGON_MODE, polygonMode);
            glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
            glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
            scissor = glIsEnabled(GL_SCISSOR_TEST);
            blend = glIsEnabled(GL_BLEND);
            depth = glIsEnabled(GL_DEPTH_TEST);
            cull = glIsEnabled(GL_CULL_FACE);
            stencil = glIsEnabled(GL_STENCIL_TEST);
            colorLogic = glIsEnabled(GL_COLOR_LOGIC_OP);
        }

        static void setEnabled(GLenum cap, GLboolean on)
        {
            if (on) glEnable(cap);
            else glDisable(cap);
        }

        void restore() const
        {
            glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(framebuffer));
            glUseProgram(static_cast<GLuint>(program));
            glBindVertexArray(static_cast<GLuint>(vao));
            for (int i = 0; i < 3; ++i) {
                glActiveTexture(GL_TEXTURE0 + i);
                glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture2d[i]));
                glBindTexture(GL_TEXTURE_CUBE_MAP, static_cast<GLuint>(textureCube[i]));
                glBindSampler(static_cast<GLuint>(i), static_cast<GLuint>(sampler[i]));
            }
            glActiveTexture(static_cast<GLenum>(activeTexture));
            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            glScissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);
            glPolygonMode(GL_FRONT_AND_BACK, static_cast<GLenum>(polygonMode[0]));
            glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
            glDepthMask(depthMask);
            setEnabled(GL_SCISSOR_TEST, scissor);
            setEnabled(GL_BLEND, blend);
            setEnabled(GL_DEPTH_TEST, depth);
            setEnabled(GL_CULL_FACE, cull);
            setEnabled(GL_STENCIL_TEST, stencil);
            setEnabled(GL_COLOR_LOGIC_OP, colorLogic);
        }
    };

    // Kinds of work, each with its own measured cost (CostKey: kind, target size, source size, one-sample flag).
    // Units: one filter sample at one texel (the source passes: one texel; the LUTs: one build).
    static constexpr int kKindLambertian = 0, kKindGgx = 1, kKindCharlie = 2, kKindSourceFace = 3, kKindSourceMips = 4,
                         kKindAtmosphereLuts = 5, kKindSkyViewLut = 6, kKindSourceAtomic = 7;
    static constexpr double kDrawUnits = 2.0e4;          // fixed cost of one draw (state, FBO attachment)
    static constexpr double kInitialMsPerUnit = 2.0e-6;  // until anything is measured (0.5 G units per second)
    static constexpr double kUnknownCostFactor = 8.0;    // an unmeasured kind: 8 x the dearest measured one
    static constexpr double kLutGuessMs = 0.5;           // an unmeasured LUT build

    static void prepareState();

    static uint64_t costKey(int kind, uint32_t size, uint32_t sourceSize, uint32_t samples);

    static bool isLutKey(uint64_t key);

    // Measured GPU milliseconds per unit of a kind of work, or a deliberately high guess while it is unmeasured.
    double msPerUnit(uint64_t key) const;

    // ---- programs ----

    static std::string loadShader(const std::string& path, int depth = 0);

    static GLuint compile(GLenum type, const std::string& source, const char* label);

    static GLuint link(const std::string& vertexSource, const std::string& fragmentSource, const char* label);

    bool ensurePrograms();

    // ---- textures ----

    static void destroyTexture(GLuint& texture);

    static void destroyTarget(Target& target);

    static bool createLut(Target& target, int width, int height);

    bool ensureLuts();

    // IBLSampler::createCubemapTexture: levels 0 .. floor(log2(size)) + 1 when mipmapped.
    static GLuint createCube(uint32_t size, bool mipmaps, GLint internalFormat, uint64_t& bytes, uint32_t maxLevel);

    static uint32_t mipLevelsFor(uint32_t size);

    SetSpec fullSpec() const;

    SetSpec fastSpec() const;

    OutputSet allocateSet(const SetSpec& spec) const;

    static void destroySet(OutputSet& set);

    // Makes `set` match `spec` (reallocated only when the sizes or the format differ).
    void ensureSet(OutputSet& set, const SetSpec& spec);

    void releaseSets();

    SourceCube& ensureSource(bool full, uint32_t size);

    // ---- passes ----

    void drawFullscreen();

    void setAtmosphereUniforms(GLuint program) const;

    void renderLut(GLuint program, const Target& target);

    void buildAtmosphereLuts();

    void buildSkyViewLut();

    // Half-float output cubes (RGBA16F: smallest subnormal 6e-8, smallest normal 6.1e-5) cannot hold the night sky's
    // radiance (the airglow is 1.6e-8 environment units at the zenith: it rounds to zero). With 16-bit cubes the source
    // cube (RGBA32F) is therefore written as radiance x S, S a power of two from 2^20 down until the sky-view LUT's
    // brightest channel x S is at most 32000 (bilinear LUT reads and the positive, normalised IBL filters cannot exceed
    // it; half of the 65504 range stays as headroom), and the renderer decodes the front set with 1 / S
    // (frontRadianceDecode, the environment intensity). 32-bit cubes keep S = 1: nothing is read back or changed.
    float chooseRadianceScale();

    // One face of the source cube from the current sky-view LUT.
    void renderSourceFace(const SourceCube& source, GLint face);

    static void buildSourceMips(const SourceCube& source);

    // The sky into the source cube (all faces) and its mip chain, at once.
    void renderSourceCube(const SourceCube& source);

    static double sourceUnits(uint32_t size) { return 6.0 * size * size * 3.0 + 7.0 * kDrawUnits; }

    // One band of rows of one filter pass (ibl_filtering.frag with IBLSampler::applyFilter's uniforms).
    void drawFilter(const WorkItem& item, const SourceCube& source, uint32_t row, uint32_t rows);

    bool ensureBrdfLuts();

    // ---- jobs ----

    static double rowUnits(const WorkItem& item)
    {
        return static_cast<double>(std::max(1u, item.size)) * item.samples;
    }

    static double itemUnits(const WorkItem& item);

    static uint64_t itemKey(const WorkItem& item, uint32_t sourceSize)
    {
        return costKey(item.distribution, item.size, sourceSize, item.samples);
    }

    std::vector<WorkItem> buildItems(const OutputSet& set, const SetSpec& spec, bool sourceItems, const SourceCube& source) const;

    // Prepares `target` for the current sky and queues the job. Fast jobs render their source cube from the current
    // sky-view LUT here, at once (the sky changes every frame while they run); refine jobs (sliceSource) render it as
    // their first work items (the sky is still; a change aborts them).
    void startJob(OutputSet& target, const SetSpec& spec, bool refine, bool sliceSource = true);

    // Runs work until `budgetMs` (estimated from the measured costs) is spent, at least one piece; everything when
    // `all`. Returns the estimated milliseconds spent.
    double runSlices(double budgetMs, bool all);

    void finishJob();

    void abortJob()
    {
        m_job = Job();
    }

    // A whole filter at once into `target` (synchronous paths).
    void runSynchronous(OutputSet& target);

    // ---- GPU timing (GL_TIMESTAMP after every piece of time-sliced work, read once available) ----

    bool beginFrameTimer();

    // Closes a piece of work that ran since the previous timestamp; returns its estimated milliseconds.
    double timedSegment(uint64_t key, double units);

    void endFrameTimer(bool refine);

    void pollTimers();

    bool m_active = false;
    bool m_programsFailed = false;
    IblConfig m_config;
    AtmosphereParameters m_atmosphere;
    glm::vec3 m_sunDirection = glm::vec3(0.0f, 1.0f, 0.0f);
    uint64_t m_skyVersion = 0;
    uint64_t m_frame = 0;
    uint64_t m_lastChangeFrame = 0;

    GLuint m_transmittanceProgram = 0, m_multiScatteringProgram = 0, m_skyViewProgram = 0, m_cubeProgram = 0;
    GLuint m_filterProgram = 0;
    GLuint m_vao = 0, m_fbo = 0;
    Target m_transmittanceLut, m_multiScatteringLut, m_skyViewLut;
    GLuint m_ggxLut = 0, m_charlieLut = 0, m_sheenLut = 0;
    uint32_t m_brdfLutSize = 0;
    bool m_brdfLutHalfFloat = false;

    OutputSet m_full;
    std::array<OutputSet, 2> m_fast{};
    OutputSet* m_front = nullptr;
    SourceCube m_sourceFast, m_sourceFull;
    Job m_job;

    std::array<FrameTimer, 8> m_timers{};
    size_t m_nextTimer = 0;
    int m_activeTimer = -1;
    std::unordered_map<uint64_t, CostEstimate> m_costs;
    Stats m_stats;
};

} // namespace ux3d::imgui
