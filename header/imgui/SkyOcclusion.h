// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "imgui/EmbeddedVFS.h"
#include "imgui/Profiling.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace ux3d::imgui {

// A3.9: sky occlusion inside the model (docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.9).
//
// The model's own geometry hides part of the sky from points inside it (a cockpit, a wheel well). A sky-visibility
// volume over the model's bounds stores, per cell, the first two moments of the visible directions:
//   a = (1/N) sum_visible 1 (the visible fraction of the sphere) and m = (1/N) sum_visible d,
// i.e. an L1 spherical-harmonic projection of the visibility V(d). From them pbr.frag (sky_occlusion.glsl) gets the
// cosine-weighted visibility around a normal n from two readings blended by how cap-like the visible set is: the L1
// value a + 2 m.n (exact for a half-space, 25% low for a small opening) and a visibility cone (axis m, solid angle
// 4 pi a) lit like a spherical cap light (exact for one opening or one blocker); the same cone gives the specular
// lobes' visibility.
//
// Bake (time-sliced, never per frame): N directions on a spherical Fibonacci lattice; for each, an orthographic
// depth map of the casters fitted to the volume (rendered by the application with its renderer and a caster layer),
// then a fragment pass over every slice of the volume that tests each cell centre against the maps of the batch
// (hardware depth compare, 2 x 2 PCF) and adds (v, v d) into an RGBA32F accumulator. A cell counts as visible in a
// direction only when it lies at least a small bias in front of the outermost surface towards the sky, so cells on or
// inside closed geometry see nothing. The last frame turns the sums into the RGBA16F volume: (a, m), or a = -1 for a
// cell without a single visible direction (inside a solid), which the lookup leaves out of its interpolation. The
// finished volume replaces the previous one only when it is complete; a half-baked volume is never used.
//
// Every pass is a fragment shader (#version 410), so it works on every 4.1+ context.
enum class SkyOcclusionQuality : int {
    Low = 0,    // 64 directions, up to 64 cells per axis, 512 px maps
    Medium = 1, // 128 directions, up to 96 cells, 768 px maps (default)
    High = 2,   // 256 directions, up to 128 cells, 1024 px maps
};

struct SkyOcclusionSpec {
    int directions = 128;
    int maxCells = 96;
    int mapSize = 768;
    int directionsPerFrame = 16; // a whole number of depth-map batches (kBatch)
};

inline SkyOcclusionSpec skyOcclusionSpec(SkyOcclusionQuality quality)
{
    switch (quality) {
    case SkyOcclusionQuality::Low: return { 64, 64, 512, 16 };
    case SkyOcclusionQuality::High: return { 256, 128, 1024, 32 };
    case SkyOcclusionQuality::Medium: break;
    }
    return { 128, 96, 768, 16 };
}

inline const char* skyOcclusionQualityName(SkyOcclusionQuality quality)
{
    return quality == SkyOcclusionQuality::Low ? "low" : quality == SkyOcclusionQuality::High ? "high" : "medium";
}

// Unit direction i of n on the spherical Fibonacci lattice (z-uniform, golden-angle azimuth). Fixed: every bake of the
// same model and quality is identical.
inline glm::vec3 skyOcclusionDirection(int i, int n)
{
    const double z = 1.0 - 2.0 * (static_cast<double>(i) + 0.5) / static_cast<double>(n);
    const double r = std::sqrt(std::max(0.0, 1.0 - z * z));
    const double phi = static_cast<double>(i) * 3.14159265358979323846 * (3.0 - std::sqrt(5.0));
    // glTF is Y-up: the lattice's pole axis is Y.
    return glm::vec3(static_cast<float>(r * std::cos(phi)), static_cast<float>(z), static_cast<float>(r * std::sin(phi)));
}

class SkyOcclusionVolume {
public:
    static constexpr int kBatch = 8;          // depth maps per accumulation pass (array layers)
    static constexpr int kPaddingCells = 2;   // empty cells around the model's bounds
    // Normal offset of the lookup, in cells (sky_occlusion.glsl): keeps the interpolation on the visible side of the
    // surface for walls at least this thick.
    static constexpr float kNormalOffsetCells = 1.0f;
    // Visibility at or above this fraction of an open half-space counts as fully open. It absorbs the direction
    // lattice's discretisation error (a half-space reads 1 +- 0.05 with 64 directions, +- 0.034 with 128, +- 0.024
    // with 256; measured over 2000 random orientations) and the bias that blocks a few grazing directions next to a
    // surface, so open and convex surfaces read exactly 1.
    static constexpr float kOpenFraction = 0.95f;

    struct Bounds {
        glm::vec3 origin{ 0.0f }; // world position of the volume's minimum corner
        float cell = 1.0f;        // cell edge (world units)
        glm::ivec3 dims{ 0 };
    };

    struct Stats {
        uint64_t bakes = 0;          // completed bakes
        uint64_t aborted = 0;        // jobs restarted before completion
        int lastFrames = 0;          // frames of the last completed bake
        double lastCpuMs = 0.0;      // CPU (submission) time of the last completed bake
        double lastGpuMs = -1.0;     // GPU time of the last completed bake (timestamps; -1 until available)
        double lastMaxFrameCpuMs = 0.0;
        double lastMaxFrameGpuMs = -1.0;
        double lastFinalizeCpuMs = 0.0;
        size_t lastCasters = 0;
    };

    SkyOcclusionVolume() = default;
    ~SkyOcclusionVolume() { release(); }
    SkyOcclusionVolume(const SkyOcclusionVolume&) = delete;
    SkyOcclusionVolume& operator=(const SkyOcclusionVolume&) = delete;

    // The volume pbr.frag reads (0 until the first bake completes).
    GLuint frontTexture() const { return m_front.texture; }
    const Bounds& frontBounds() const { return m_front.bounds; }
    uint64_t frontKey() const { return m_front.key; }
    SkyOcclusionQuality frontQuality() const { return m_front.quality; }
    bool ready() const { return m_front.texture != 0; }
    bool busy() const { return m_job.active; }
    uint64_t jobKey() const { return m_job.key; }
    int jobDirectionsDone() const { return m_job.next; }
    int jobDirections() const { return m_job.spec.directions; }
    const Bounds& jobBounds() const { return m_job.bounds; }
    const Stats& stats() const { return m_stats; }
    bool failed() const { return m_programsFailed; }

    uint64_t frontBytes() const
    {
        return m_front.texture ? static_cast<uint64_t>(m_front.bounds.dims.x) * m_front.bounds.dims.y * m_front.bounds.dims.z * 8u : 0u;
    }
    // Temporary memory while a bake runs: the RGBA32F accumulator and the depth-map array.
    uint64_t jobBytes() const;

    // The volume for world bounds [lo, hi]: cubic cells, at most spec.maxCells per axis padding included.
    static Bounds fitBounds(const glm::vec3& lo, const glm::vec3& hi, const SkyOcclusionSpec& spec);

    // Starts (or restarts) a bake. The current front volume stays in use until the new one is complete.
    void startJob(uint64_t key, const Bounds& bounds, SkyOcclusionQuality quality);

    void cancelJob();

    // Drops the front volume (the model it was baked for is gone).
    void dropFront()
    {
        destroyVolume(m_front);
    }

    // The orthographic projection of direction d for the job's bounds (view: world -> light view; projection: light view
    // -> clip; textureFromWorld: world -> depth-map texture space, z = window depth).
    struct DirectionMatrices {
        glm::mat4 view{ 1.0f };
        glm::mat4 projection{ 1.0f };
        glm::mat4 textureFromWorld{ 1.0f };
        float depthRange = 1.0f;
        float texelWorld = 1.0f;
    };

    static DirectionMatrices directionMatrices(const Bounds& b, const glm::vec3& towardsSky, int mapSize);

    // One frame of the running job. renderDepth(layer, matrices) draws the casters into the bound depth layer (the
    // volume binds and clears it before the call; the call may rebind it). Returns true when the job completed this
    // frame (the new volume is in front). Leaves the framebuffer binding, viewport and program changed.
    bool runFrame(const std::function<void(int layer, const DirectionMatrices&)>& renderDepth);

    // Collects finished GPU timestamps (also while no job runs, for the last bake's total).
    void pollTimers();

    void release();

    bool hasAnything() const { return m_front.texture != 0 || m_job.active || m_vao != 0; }

private:
    struct Volume {
        GLuint texture = 0;
        Bounds bounds;
        uint64_t key = 0;
        SkyOcclusionQuality quality = SkyOcclusionQuality::Medium;
    };
    struct Job {
        bool active = false;
        uint64_t key = 0;
        uint64_t serial = 0;
        Bounds bounds;
        SkyOcclusionQuality quality = SkyOcclusionQuality::Medium;
        SkyOcclusionSpec spec;
        int next = 0;
        int frames = 0;
        double cpuMs = 0.0;
        double maxFrameCpuMs = 0.0;
        double gpuMsSettled = 0.0;
        double maxFrameGpuMsSettled = 0.0;
        std::chrono::steady_clock::time_point started;
    };
    struct Program {
        GLuint id = 0;
        GLint loc(const char* name) const { return glGetUniformLocation(id, name); }
    };
    struct TimerPair {
        GLuint queries[2] = { 0, 0 };
        bool pending = false;
        uint64_t serial = 0;
    };

    void cancelJobSilently();

    static void destroyVolume(Volume& volume);

    bool ensureJobTargets();

    void releaseJobTargets();

public:
    // Binds depth layer `layer` of the batch array as the depth target (viewport = the map); clear sets depth 1.
    void bindDepthLayer(int layer, bool clear);

private:
    void prepareFullscreenState();

    void restoreState();

    // Adds the batch's (v, v d) to every cell of the accumulator.
    void accumulate(int count, const std::array<DirectionMatrices, kBatch>& matrices, const std::array<glm::vec3, kBatch>& directions);

    // Accumulator -> a new RGBA16F volume; swaps it in front. False when the target cannot be created.
    bool finalize();

    static void setNearest3D();

    static GLuint compile(GLenum type, const std::string& source);

    static GLuint link(const std::string& fragmentBody, const char* passDefine);

    bool ensurePrograms();

    // GL_TIMESTAMP pairs around each bake frame (allowed inside the renderer's GL_TIME_ELAPSED queries).
    int beginTimer();

    void endTimer(int index, uint64_t serial);

    Volume m_front;
    Job m_job;
    uint64_t m_nextSerial = 0;
    GLuint m_accumulator = 0;
    GLuint m_depthArray = 0;
    GLuint m_depthFbo = 0;
    GLuint m_sliceFbo = 0;
    Program m_accumulate;
    Program m_finalize;
    GLuint m_vao = 0;
    bool m_programsFailed = false;
    std::array<TimerPair, 48> m_timers{};
    Stats m_stats;
    uint64_t m_completedSerial = ~0ull;
    double m_completedGpuMs = 0.0;
    double m_completedMaxFrameGpuMs = 0.0;
    int m_completedPendingTimers = 0;
};

} // namespace ux3d::imgui
