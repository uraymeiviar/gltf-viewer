// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "imgui/GroundSettings.h"
#include <glad/glad.h>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ux3d::imgui {

// A6.8 (docs/ASOBO_EXTENSIONS_PHASE_6.md): the FFT ocean's looping simulation (Tessendorf 2001, JONSWAP spectrum with
// the Horvath 2015 directional spread). Needs an OpenGL 4.3+ context (compute shaders); oceanAvailable() says so.
//
// Cascades: C patches of R x R texels, each holding one band of wave numbers [kLow, kLow x B) (the bands tile the
// spectrum from an eighth of the peak wave number up to 4 cm waves; B is limited so that the frames below sample the
// shortest wave of a band at about 7 frames per period). Every cascade's dispersion is quantised to a whole multiple
// of 2 pi m / T (m >= 1 per cascade, T the loop period), so the surface repeats exactly after T, and F frames of one
// sub-period T / m are computed once on the GPU (spectrum, two 1D inverse FFT passes, displacement + Jacobian, foam
// accumulation over two turns of the loop, mip chain by compute) into one RGBA16F texture array of C x F layers:
// (Dx, height, Dz, accumulated Jacobian) in metres. Rendering samples the two frames around the current time per
// cascade and interpolates. A settings change recomputes into a second array spread over frames (a GPU budget per
// frame, measured with timestamp queries) and swaps it in when complete; the old array stays until then.
//
// Live mode (A6.8b) skips the loop: one layer per cascade (R x R with mips) is recomputed every frame at the current
// time with the exact dispersion (the phase in double precision), foam accumulating from frame to frame; it allows 512^2
// at high quality (4 x 512^2 = 10.7 MiB of layers + 16 MiB of spectrum). Changing a setting rebuilds the live set at once
// (the CPU spectrum is the cost: a few ms at 128^2 up to about 100 ms at 4 x 512^2); a mode change drops the old set.
//
// Memory (the RGBA16F array with its mips, x 4/3): Low 2 x 128^2 x 32 frames = 10.7 MiB, Medium 3 x 256^2 x 48 = 96.0 MiB,
// High 4 x 256^2 x 64 = 170.7 MiB, plus the spectrum and FFT scratch (1.25 / 5 / 5 MiB); during a recompute the old
// and the new array both exist.
class OceanSimulation {
public:
    static constexpr int kMaxCascades = 4;
    static constexpr int kSlopeLods = 8;

    struct QualityInfo {
        int cascades;
        int resolution;
        int frames;
    };
    // live: the per-frame mode's layout (frames = 1; the resolution of high is 512).
    static QualityInfo qualityInfo(OceanQuality quality, bool live = false);

    struct Cascade {
        float patchSize = 1.0f;          // m
        float kLow = 0.0f, kHigh = 0.0f; // the band of wave numbers, rad/m
        int loopDivisor = 1;             // m: the cascade repeats every T / m
        // Mean square slope (both axes) the cascade loses when sampled at mip level 0..7 (0 at level 0).
        std::array<float, kSlopeLods> unresolvedSlopeVariance{};
        float slopeVariance = 0.0f;      // of the whole band
        float heightVariance = 0.0f;
    };

    // What the precomputed set depends on.
    struct Key {
        float windSpeed = 0.0f, windDirection = 0.0f, fetch = 0.0f, choppiness = 0.0f, swell = 0.0f, period = 0.0f;
        OceanQuality quality = OceanQuality::Medium;
        bool live = false; // the period does not matter then (it is 0 in the key)
        bool operator==(const Key&) const = default;
    };
    static Key keyOf(const OceanSettings& settings);

    struct Stats {
        double lastPrecomputeMs = 0.0;     // wall time of the last complete set (synchronous: one call; sliced: all frames)
        double lastPrecomputeGpuMs = 0.0;  // GPU time of the last complete set (sum of the measured slices)
        double lastSliceGpuMs = 0.0;       // GPU time of the latest measured slice
        double maxSliceGpuMs = 0.0;        // the largest slice of the last complete set
        std::vector<float> slicesGpuMs;    // the measured slices of the last complete set
        int lastSliceItems = 0;
        int lastPrecomputeFrames = 0;      // application frames the last set took (1 = synchronous)
        uint64_t sets = 0;
        uint64_t aborted = 0;
        // Live mode: the GPU time of the last measured per-frame step (all cascades: spectrum, FFTs, finalize, mips),
        // its running average and maximum, and how many steps ran.
        double liveGpuMs = 0.0;
        double liveGpuAverageMs = 0.0;
        double liveGpuMaxMs = 0.0;
        uint64_t liveSteps = 0;
        uint64_t liveMeasured = 0;
    };

    OceanSimulation() = default;
    ~OceanSimulation();
    OceanSimulation(const OceanSimulation&) = delete;
    OceanSimulation& operator=(const OceanSimulation&) = delete;

    // Brings the precomputed set up to date with the settings. synchronous: everything now (the first set, capture
    // runs); otherwise a pending recompute advances by about budgetMs of GPU time. Returns false on failure (logged once).
    bool update(const OceanSettings& settings, bool synchronous, double budgetMs);
    void release();

    bool ready() const { return m_front.texture != 0; }
    bool live() const { return m_front.live; }
    // Live mode: brings the layers to the ocean time (no-op for a precomputed set, or when the time did not change).
    void stepLive(double time);
    bool busy() const { return m_job.active; }
    float progress() const;
    GLuint texture() const { return m_front.texture; }
    uint64_t version() const { return m_front.version; }
    const Key& key() const { return m_front.key; }
    int cascadeCount() const { return m_front.cascades; }
    int resolution() const { return m_front.resolution; }
    int frames() const { return m_front.frames; }
    const Cascade& cascade(int index) const { return m_front.cascade[static_cast<size_t>(index)]; }
    // Mean square slope of the waves shorter than the finest cascade (Cox and Munk 1954 total minus the resolved part).
    float tailSlopeVariance() const { return m_front.tailSlopeVariance; }
    float significantWaveHeight() const { return m_front.significantWaveHeight; }
    // Mean square slope (both axes) of everything the cascades resolve; the foam threshold reads it.
    float resolvedSlopeVariance() const { return m_front.resolvedSlopeVariance; }
    uint64_t memoryBytes() const;
    const Stats& stats() const { return m_stats; }

    // Fractional frame position of a cascade at time t (seconds): [0, frames).
    double framePosition(int cascade, double time) const;

private:
    struct Set {
        GLuint texture = 0;
        GLuint spectrum = 0; // live sets keep their own h0 array
        bool live = false;
        Key key;
        int cascades = 0, resolution = 0, frames = 0, levels = 0;
        std::array<Cascade, kMaxCascades> cascade{};
        float tailSlopeVariance = 0.0f;
        float significantWaveHeight = 0.0f;
        float resolvedSlopeVariance = 0.0f;
        uint64_t version = 0;
        uint64_t bytes = 0;
    };
    struct Job {
        bool active = false;
        Set set;
        int nextItem = 0;
        int itemCount = 0;
        double wallMs = 0.0;
        double gpuMs = 0.0;
        double maxSliceMs = 0.0;
        double lastSliceUnits = 0.0;
        std::vector<float> slices;
        int appFrames = 0;
        uint64_t serial = 0;
    };
    struct SliceTimer {
        GLuint queries[2] = { 0, 0 };
        bool pending = false;
        bool live = false;
        double units = 0.0;
        uint64_t job = 0;
    };

    bool ensurePrograms(int resolution, bool live);
    void releasePrograms();
    bool startJob(const Key& key);
    void runItem(int item);
    // One cascade's frame: spectrum, row and column FFT, finalize into the array layer. Live: the exact dispersion at
    // time, foam accumulating by recovery per second x dt (reset: starting from the raw Jacobian).
    void computeLayer(const Set& set, int cascade, int layer, int frame, double time, bool accumulate, float recovery, bool reset);
    double itemUnits(int item) const;
    void pollTimers(bool wait);
    void finishJob();
    void abortJob();
    static void destroy(Set& set);

    Set m_front;
    Job m_job;
    Stats m_stats;
    uint64_t m_nextVersion = 1;
    bool m_failed = false;
    Key m_failedKey;

    // Programs (built for one resolution) and the job's spectrum / scratch textures.
    int m_programResolution = 0;
    bool m_programLive = false;
    GLuint m_spectrumProgram = 0, m_fftProgram = 0, m_finalizeProgram = 0, m_foamProgram = 0, m_mipProgram = 0;
    GLuint m_spectrumTexture = 0; // RGBA32F 2D array, one layer per cascade: h0(k), conj(h0(-k))
    GLuint m_scratchTexture = 0;  // RGBA32F R x R: two complex fields
    int m_scratchResolution = 0;
    int m_spectrumResolution = 0;
    // Live mode: the time of the last step and whether the layers hold a valid previous state (foam accumulation).
    double m_liveTime = 0.0;
    bool m_liveValid = false;

    // Timestamp queries around each frame's slice (read back one or more frames later).
    std::array<SliceTimer, 4> m_timers{};
    double m_costPerUnitMs = 0.0;
    uint64_t m_jobSerial = 0;
    uint64_t m_finishedSerial = 0;
};

// The choppiness the foam's Jacobian is evaluated with: the surface's own (Tessendorf's lambda), at least 0.5, so a
// sea without horizontal displacement still has whitecaps.
inline float oceanFoamChoppiness(float choppiness)
{
    return choppiness > 0.5f ? choppiness : 0.5f;
}

// The foam's threshold on the summed Jacobian J = 1 + sum over the cascades of (J_c - 1) (ocean.frag): foam where J is
// below 1 - z sigma, ramping over 0.35 sigma. J's compression part is about Gaussian with the standard deviation
// sigma = lambda_f sqrt(slope variance the footprint resolves) (first order: the trace of the displacement gradient;
// the shader evaluates sigma per pixel), so the threshold 1 - z sigma gives the whitecap coverage of Monahan and
// O'Muircheartaigh (1980) at the wind speed, scaled by the foam amount (0 = none, 0.5 = Monahan, 1 = four times), at
// every distance. coverage is that target; sigma is the full-resolution value (for the state report).
struct OceanFoamParameters {
    double sigma = 0.0;
    double coverage = 0.0;
    double z = 1.0e4;     // P(Z > z) = coverage for a standard normal Z
    double lambda = 0.5;  // oceanFoamChoppiness
};
OceanFoamParameters oceanFoamParameters(const OceanSettings& ocean, float choppiness, float windSpeed, float resolvedSlopeVariance);

// What the settings panel shows of the active viewport's ocean (Viewports.cpp fills it each frame): the recompute
// progress, the memory and, in live mode, the measured per-frame GPU time.
struct OceanUiStatus {
    bool ready = false;
    bool live = false;
    bool busy = false;
    float progress = 1.0f;
    uint64_t memoryBytes = 0;
    double liveGpuMs = 0.0;
    float significantWaveHeight = 0.0f;
};
OceanUiStatus& oceanUiStatus();

// A6.8: whether the ocean can run (an OpenGL 4.3+ context; GLIDER_TEST_NO_COMPUTE=1 forces "no" as a test hook).
bool oceanAvailable();

} // namespace ux3d::imgui
