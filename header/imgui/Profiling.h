// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <glad/glad.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ux3d::imgui::profiling {

using Clock = std::chrono::steady_clock;

// Retained samples are bounded so a long interactive profiling session cannot
// grow without limit. The reports state the limit, the retained range and the
// number of oldest samples that were dropped.
inline constexpr uint64_t kMaxRetainedFrameSamples = 512;

// ---------------------------------------------------------------------------
// Frame counters
//
// The GL counters cover calls made through the renderer's gl::GL wrapper
// (GladGL). Direct application GL calls are counted explicitly at the few
// places where the viewer issues them (grid, bounding boxes, AA, framebuffers).
// The report states this scope instead of pretending to be a driver-level
// trace.
// ---------------------------------------------------------------------------

struct FrameCounters {
    uint64_t glCalls = 0;                 // calls through the renderer gl::GL wrapper
    uint64_t drawCalls = 0;               // renderer + explicitly counted app draws
    uint64_t rendererDrawCalls = 0;
    uint64_t appDrawCalls = 0;

    uint64_t vaoCreations = 0;
    uint64_t vaoDeletions = 0;
    uint64_t bufferCreations = 0;
    uint64_t textureCreations = 0;
    uint64_t framebufferCreations = 0;
    uint64_t renderbufferCreations = 0;
    uint64_t shaderCompiles = 0;
    uint64_t programLinks = 0;
    uint64_t samplerCreations = 0;
    uint64_t samplerDeletions = 0;
    uint64_t samplerBinds = 0;
    uint64_t samplerParameterCalls = 0;
    uint64_t texParameterCalls = 0;
    uint64_t polygonModeCalls = 0;

    uint64_t bufferUploads = 0;
    uint64_t bufferUploadBytes = 0;
    uint64_t bufferAllocations = 0;
    uint64_t bufferAllocationBytes = 0;
    uint64_t textureUploads = 0;
    uint64_t textureUploadBytes = 0;
    uint64_t textureUploadUnknownCalls = 0;
    uint64_t textureAllocations = 0;
    uint64_t textureAllocationBytes = 0;
    uint64_t textureAllocationUnknownCalls = 0;
    uint64_t renderbufferAllocations = 0;
    uint64_t renderbufferAllocationBytes = 0;
    uint64_t renderbufferAllocationUnknownCalls = 0;

    uint64_t dirtySyncs = 0;
    uint64_t updateGltfCalls = 0;
    uint64_t updateSceneCalls = 0;
    uint64_t applyAnimationCalls = 0;
    uint64_t cacheRebuilds = 0;
    uint64_t sceneBuilds = 0;
    uint64_t sceneDrawables = 0;
    uint64_t sceneNodesVisited = 0;
    uint64_t pickingBuilds = 0;
    uint64_t commandGenerations = 0;
    uint64_t overlayDraws = 0;
    uint64_t aaPasses = 0;

    uint64_t uiDrawLists = 0;
    uint64_t uiVertices = 0;
    uint64_t uiIndices = 0;
};

extern bool g_countingEnabled;

FrameCounters& frameCounters();
void resetFrameCounters();
void setCountingEnabled(bool enabled);
bool countingEnabled();

// Storage estimate for one texel of an internal format. Returns nullopt for
// unsized or unsupported formats, which are reported as unknown calls instead
// of being silently charged as RGBA8.
std::optional<uint64_t> internalFormatTexelBytes(GLenum internalFormat);

// Bytes per pixel described by a client transfer format/type pair. Packed
// types are handled explicitly; unsupported pairs return nullopt.
std::optional<uint64_t> pixelTransferBytes(GLenum format, GLenum type);

inline void countGlCall()
{
    if (g_countingEnabled) {
        ++frameCounters().glCalls;
    }
}

inline void countRendererDrawCall()
{
    if (g_countingEnabled) {
        auto& counters = frameCounters();
        ++counters.drawCalls;
        ++counters.rendererDrawCalls;
    }
}

inline void countAppDrawCall()
{
    if (g_countingEnabled) {
        auto& counters = frameCounters();
        ++counters.drawCalls;
        ++counters.appDrawCalls;
    }
}

inline void countVaoCreation(uint64_t count = 1)
{
    if (g_countingEnabled) frameCounters().vaoCreations += count;
}

inline void countVaoDeletion(uint64_t count = 1)
{
    if (g_countingEnabled) frameCounters().vaoDeletions += count;
}

inline void countBufferCreation(uint64_t count = 1)
{
    if (g_countingEnabled) frameCounters().bufferCreations += count;
}

inline void countTextureCreation(uint64_t count = 1)
{
    if (g_countingEnabled) frameCounters().textureCreations += count;
}

inline void countFramebufferCreation(uint64_t count = 1)
{
    if (g_countingEnabled) frameCounters().framebufferCreations += count;
}

inline void countRenderbufferCreation(uint64_t count = 1)
{
    if (g_countingEnabled) frameCounters().renderbufferCreations += count;
}

inline void countShaderCompile()
{
    if (g_countingEnabled) ++frameCounters().shaderCompiles;
}

inline void countProgramLink()
{
    if (g_countingEnabled) ++frameCounters().programLinks;
}

inline void countSamplerCreation(uint64_t count = 1)
{
    if (g_countingEnabled) frameCounters().samplerCreations += count;
}

inline void countSamplerDeletion(uint64_t count = 1)
{
    if (g_countingEnabled) frameCounters().samplerDeletions += count;
}

inline void countSamplerBind(uint64_t count = 1)
{
    if (g_countingEnabled) frameCounters().samplerBinds += count;
}

inline void countSamplerParameter(uint64_t count = 1)
{
    if (g_countingEnabled) frameCounters().samplerParameterCalls += count;
}

inline void countTexParameter(uint64_t count = 1)
{
    if (g_countingEnabled) frameCounters().texParameterCalls += count;
}

inline void countPolygonModeCall(uint64_t count = 1)
{
    if (g_countingEnabled) frameCounters().polygonModeCalls += count;
}

inline void countBufferUpload(uint64_t bytes)
{
    if (g_countingEnabled) {
        auto& counters = frameCounters();
        ++counters.bufferUploads;
        counters.bufferUploadBytes += bytes;
    }
}

inline void countBufferAllocation(uint64_t bytes)
{
    if (g_countingEnabled) {
        auto& counters = frameCounters();
        ++counters.bufferAllocations;
        counters.bufferAllocationBytes += bytes;
    }
}

inline void countTextureUpload(uint64_t bytes)
{
    if (g_countingEnabled) {
        auto& counters = frameCounters();
        ++counters.textureUploads;
        counters.textureUploadBytes += bytes;
    }
}

inline void countTextureUploadUnknown()
{
    if (g_countingEnabled) {
        auto& counters = frameCounters();
        ++counters.textureUploads;
        ++counters.textureUploadUnknownCalls;
    }
}

inline void countTextureAllocation(uint64_t bytes)
{
    if (g_countingEnabled) {
        auto& counters = frameCounters();
        ++counters.textureAllocations;
        counters.textureAllocationBytes += bytes;
    }
}

inline void countTextureAllocationUnknown()
{
    if (g_countingEnabled) {
        auto& counters = frameCounters();
        ++counters.textureAllocations;
        ++counters.textureAllocationUnknownCalls;
    }
}

inline void countRenderbufferAllocation(uint64_t bytes)
{
    if (g_countingEnabled) {
        auto& counters = frameCounters();
        ++counters.renderbufferAllocations;
        counters.renderbufferAllocationBytes += bytes;
    }
}

inline void countRenderbufferAllocationUnknown()
{
    if (g_countingEnabled) {
        auto& counters = frameCounters();
        ++counters.renderbufferAllocations;
        ++counters.renderbufferAllocationUnknownCalls;
    }
}

inline void countDirtySync()
{
    if (g_countingEnabled) ++frameCounters().dirtySyncs;
}

inline void countCacheRebuild()
{
    if (g_countingEnabled) ++frameCounters().cacheRebuilds;
}

inline void countSceneBuild(uint64_t drawables)
{
    if (g_countingEnabled) {
        auto& counters = frameCounters();
        ++counters.sceneBuilds;
        counters.sceneDrawables += drawables;
    }
}

inline void countSceneNodesVisited(uint64_t nodes)
{
    if (g_countingEnabled) frameCounters().sceneNodesVisited += nodes;
}

inline void countPickingBuild()
{
    if (g_countingEnabled) ++frameCounters().pickingBuilds;
}

inline void countCommandGeneration()
{
    if (g_countingEnabled) ++frameCounters().commandGenerations;
}

inline void countOverlayDraw()
{
    if (g_countingEnabled) ++frameCounters().overlayDraws;
}

inline void countAntiAliasingPass()
{
    if (g_countingEnabled) ++frameCounters().aaPasses;
}

inline void countUiDrawData(uint64_t drawLists, uint64_t vertices, uint64_t indices)
{
    if (g_countingEnabled) {
        auto& counters = frameCounters();
        counters.uiDrawLists += drawLists;
        counters.uiVertices += vertices;
        counters.uiIndices += indices;
    }
}

// ---------------------------------------------------------------------------
// CPU samples
// ---------------------------------------------------------------------------

inline double milliseconds(Clock::duration duration)
{
    return std::chrono::duration<double, std::milli>(duration).count();
}

inline double elapsedMs(Clock::time_point start, Clock::time_point end = Clock::now())
{
    return milliseconds(end - start);
}

struct StageSample {
    std::string name;
    double ms = 0.0;
};

struct FrameSample {
    uint64_t frame = 0;
    double frameIntervalMs = 0.0;
    double cpuFrameMs = 0.0;
    bool firstFrame = false;
    std::vector<StageSample> stages;
    FrameCounters counters;
};

struct StartupProfile {
    double vfsDecompressMs = 0.0;
    double environmentInitMs = 0.0;
    double environmentFilterMs = 0.0;
    double loadDecodeMs = 0.0;
    double rendererUploadMs = 0.0;
    double validationMs = 0.0;
    double validationReportedMs = 0.0;
    double totalStartupMs = 0.0;

    uint64_t modelBytes = 0;
    uint64_t environmentBytes = 0;
    uint64_t nodeCount = 0;
    uint64_t meshCount = 0;
    uint64_t materialCount = 0;
    uint64_t imageCount = 0;
    uint64_t textureCount = 0;
    uint64_t environmentLoads = 0;

    std::vector<StageSample> stages;
    FrameCounters counters;
};

StartupProfile& startupProfile();

struct CaptureRecord {
    bool valid = false;
    bool firstFrame = false;
    std::string source;
    std::string region;
    std::string path;
    uint64_t frame = 0;
    uint32_t viewportId = 0;
    int width = 0;
    int height = 0;
    double timeSeconds = 0.0;
    double readbackMs = 0.0;
    double writeMs = 0.0;
    uint64_t rgbaHash = 0;
    uint64_t fileBytes = 0;
};

struct GpuPassReport {
    std::string name;
    bool available = false;
    double ms = 0.0;
};

struct GpuFrameReport {
    uint64_t sequence = 0;
    uint64_t frame = 0;
    uint32_t viewport = 0;
    std::string label;
    std::vector<GpuPassReport> passes;
};

struct GpuReport {
    bool enabled = false;
    std::string method;
    std::string cpuSubmissionStage;
    uint64_t pendingFrames = 0;
    uint64_t unavailablePolls = 0;
    uint64_t droppedFrames = 0;
    uint64_t completedFrameLimit = 0;
    uint64_t completedFramesDropped = 0;
    std::vector<GpuFrameReport> completed;
    std::vector<GpuFrameReport> pending;
};

struct ProfileReport {
    std::string scenarioName;
    std::string modelPath;
    std::string build;
    std::string gpuVendor;
    std::string gpuRenderer;
    std::string glVersion;

    bool isolated = false;
    bool instrumentationEnabled = false;
    bool vsyncRequested = true;

    int windowWidth = 0;
    int windowHeight = 0;
    int viewportWidth = 0;
    int viewportHeight = 0;
    int viewportSamples = 0;
    std::string antiAliasing;

    StartupProfile startup;
    uint64_t reportFrame = 0;
    uint64_t frameSampleLimit = 0;
    uint64_t frameSamplesDropped = 0;
    std::vector<FrameSample> frames;
    GpuReport gpu;
    bool captureRequested = false;
    bool hasFirstFrameCapture = false;
    CaptureRecord firstFrameCapture;
    CaptureRecord capture;
};

struct BenchmarkReport {
    std::string scenarioName;
    std::string modelPath;
    std::string build;
    std::string gpuVendor;
    std::string gpuRenderer;
    std::string glVersion;

    bool isolated = false;
    bool vsyncRequested = true;
    bool detailedInstrumentation = false;

    int windowWidth = 0;
    int windowHeight = 0;
    int viewportWidth = 0;
    int viewportHeight = 0;
    int viewportSamples = 0;
    std::string antiAliasing;

    uint64_t warmupFrames = 0;
    uint64_t measuredFramesRequested = 0;
    bool hasFirstFrame = false;
    FrameSample firstFrame;
    std::vector<FrameSample> measuredFrames;
    GpuReport gpu;
};

bool writeProfileReport(const std::string& path, const ProfileReport& report);
bool writeBenchmarkReport(const std::string& path, const BenchmarkReport& report);

} // namespace ux3d::imgui::profiling
