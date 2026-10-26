// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/Profiling.h"
#include "imgui/SceneRevisions.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numeric>

namespace ux3d::imgui::profiling {

bool g_countingEnabled = false;

FrameCounters& frameCounters()
{
    static FrameCounters counters;
    return counters;
}

void resetFrameCounters()
{
    frameCounters() = FrameCounters();
}

void setCountingEnabled(bool enabled)
{
    g_countingEnabled = enabled;
}

bool countingEnabled()
{
    return g_countingEnabled;
}

std::optional<uint64_t> internalFormatTexelBytes(GLenum internalFormat)
{
    switch (internalFormat) {
    // 8-bit single/dual/three/four-component color formats.
    case GL_R8:
    case GL_R8I:
    case GL_R8UI:
    case GL_R8_SNORM:
        return 1;
    case GL_RG8:
    case GL_RG8I:
    case GL_RG8UI:
    case GL_RG8_SNORM:
        return 2;
    case GL_RGB8:
    case GL_RGB8I:
    case GL_RGB8UI:
    case GL_RGB8_SNORM:
    case GL_SRGB8:
        return 3;
    case GL_RGBA8:
    case GL_RGBA8I:
    case GL_RGBA8UI:
    case GL_RGBA8_SNORM:
    case GL_SRGB8_ALPHA8:
    case GL_RGB10_A2:
    case GL_RGB10_A2UI:
        return 4;
    case GL_RGBA4:
    case GL_RGB5_A1:
    case GL_RGB565:
        return 2;

    // 16-bit and half-float color formats.
    case GL_R16:
    case GL_R16F:
    case GL_R16I:
    case GL_R16UI:
    case GL_R16_SNORM:
        return 2;
    case GL_RG16:
    case GL_RG16F:
    case GL_RG16I:
    case GL_RG16UI:
    case GL_RG16_SNORM:
        return 4;
    case GL_RGB16:
    case GL_RGB16F:
    case GL_RGB16I:
    case GL_RGB16UI:
    case GL_RGB16_SNORM:
        return 6;
    case GL_RGBA16:
    case GL_RGBA16F:
    case GL_RGBA16I:
    case GL_RGBA16UI:
    case GL_RGBA16_SNORM:
        return 8;

    // 32-bit and full-float color formats.
    case GL_R32F:
    case GL_R32I:
    case GL_R32UI:
        return 4;
    case GL_RG32F:
    case GL_RG32I:
    case GL_RG32UI:
        return 8;
    case GL_RGB32F:
    case GL_RGB32I:
    case GL_RGB32UI:
        return 12;
    case GL_RGBA32F:
    case GL_RGBA32I:
    case GL_RGBA32UI:
        return 16;

    // Packed / special color formats.
    case GL_R11F_G11F_B10F:
    case GL_RGB9_E5:
    case GL_RGB10:
        return 4;
    case GL_R3_G3_B2:
    case GL_RGBA2:
        return 1;

    // Depth and stencil formats used by framebuffers.
    case GL_DEPTH_COMPONENT16:
        return 2;
    case GL_DEPTH_COMPONENT24:
        return 3;
    case GL_DEPTH_COMPONENT32:
    case GL_DEPTH_COMPONENT32F:
    case GL_DEPTH24_STENCIL8:
        return 4;
    case GL_DEPTH32F_STENCIL8:
        return 8;
    case GL_STENCIL_INDEX1:
    case GL_STENCIL_INDEX4:
    case GL_STENCIL_INDEX8:
        return 1;
    case GL_STENCIL_INDEX16:
        return 2;

    // Unsized legacy/internal formats, compressed formats and anything not
    // listed above are intentionally unavailable instead of being guessed.
    default:
        return std::nullopt;
    }
}

std::optional<uint64_t> pixelTransferBytes(GLenum format, GLenum type)
{
    // Packed types describe a complete pixel, so their size does not depend on
    // the component count of the transfer format.
    switch (type) {
    case GL_UNSIGNED_BYTE_3_3_2:
    case GL_UNSIGNED_BYTE_2_3_3_REV:
        return 1;
    case GL_UNSIGNED_SHORT_5_6_5:
    case GL_UNSIGNED_SHORT_5_6_5_REV:
    case GL_UNSIGNED_SHORT_4_4_4_4:
    case GL_UNSIGNED_SHORT_4_4_4_4_REV:
    case GL_UNSIGNED_SHORT_5_5_5_1:
    case GL_UNSIGNED_SHORT_1_5_5_5_REV:
        return 2;
    case GL_UNSIGNED_INT_8_8_8_8:
    case GL_UNSIGNED_INT_8_8_8_8_REV:
    case GL_UNSIGNED_INT_10_10_10_2:
    case GL_UNSIGNED_INT_2_10_10_10_REV:
    case GL_UNSIGNED_INT_24_8:
    case GL_UNSIGNED_INT_10F_11F_11F_REV:
    case GL_UNSIGNED_INT_5_9_9_9_REV:
        return 4;
    case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
        return 8;
    default:
        break;
    }

    uint64_t components = 0;
    switch (format) {
    case GL_RED:
    case GL_RED_INTEGER:
    case GL_DEPTH_COMPONENT:
    case GL_STENCIL_INDEX:
        components = 1;
        break;
    case GL_RG:
    case GL_RG_INTEGER:
    case GL_DEPTH_STENCIL:
        components = 2;
        break;
    case GL_RGB:
    case GL_RGB_INTEGER:
    case GL_BGR:
    case GL_BGR_INTEGER:
        components = 3;
        break;
    case GL_RGBA:
    case GL_RGBA_INTEGER:
    case GL_BGRA:
    case GL_BGRA_INTEGER:
        components = 4;
        break;
    default:
        return std::nullopt;
    }

    uint64_t componentBytes = 0;
    switch (type) {
    case GL_UNSIGNED_BYTE:
    case GL_BYTE:
        componentBytes = 1;
        break;
    case GL_UNSIGNED_SHORT:
    case GL_SHORT:
    case GL_HALF_FLOAT:
        componentBytes = 2;
        break;
    case GL_UNSIGNED_INT:
    case GL_INT:
    case GL_FLOAT:
        componentBytes = 4;
        break;
    case GL_DOUBLE:
        componentBytes = 8;
        break;
    default:
        return std::nullopt;
    }

    return components * componentBytes;
}

StartupProfile& startupProfile()
{
    static StartupProfile profile;
    return profile;
}

namespace {

using nlohmann::json;

json countersToJson(const FrameCounters& counters)
{
    return json{
        { "glCalls", counters.glCalls },
        { "drawCalls", counters.drawCalls },
        { "rendererDrawCalls", counters.rendererDrawCalls },
        { "appDrawCalls", counters.appDrawCalls },
        { "vaoCreations", counters.vaoCreations },
        { "vaoDeletions", counters.vaoDeletions },
        { "bufferCreations", counters.bufferCreations },
        { "textureCreations", counters.textureCreations },
        { "framebufferCreations", counters.framebufferCreations },
        { "renderbufferCreations", counters.renderbufferCreations },
        { "shaderCompiles", counters.shaderCompiles },
        { "programLinks", counters.programLinks },
        { "samplerCreations", counters.samplerCreations },
        { "samplerDeletions", counters.samplerDeletions },
        { "samplerBinds", counters.samplerBinds },
        { "samplerParameterCalls", counters.samplerParameterCalls },
        { "texParameterCalls", counters.texParameterCalls },
        { "polygonModeCalls", counters.polygonModeCalls },
        { "cullFaceEnableCalls", counters.cullFaceEnableCalls },
        { "cullFaceSelectionCalls", counters.cullFaceSelectionCalls },
        { "frontFaceCalls", counters.frontFaceCalls },
        { "depthTestEnableCalls", counters.depthTestEnableCalls },
        { "depthFuncCalls", counters.depthFuncCalls },
        { "blendEnableCalls", counters.blendEnableCalls },
        { "blendFuncCalls", counters.blendFuncCalls },
        { "blendEquationCalls", counters.blendEquationCalls },
        { "bufferUploads", counters.bufferUploads },
        { "bufferUploadBytes", counters.bufferUploadBytes },
        { "bufferAllocations", counters.bufferAllocations },
        { "bufferAllocationBytes", counters.bufferAllocationBytes },
        { "textureUploads", counters.textureUploads },
        { "textureUploadBytes", counters.textureUploadBytes },
        { "textureUploadUnknownCalls", counters.textureUploadUnknownCalls },
        { "textureUploadBytesComplete", counters.textureUploadUnknownCalls == 0 },
        { "textureAllocations", counters.textureAllocations },
        { "textureAllocationBytes", counters.textureAllocationBytes },
        { "textureAllocationBytesEstimated", true },
        { "textureAllocationUnknownCalls", counters.textureAllocationUnknownCalls },
        { "textureAllocationBytesComplete", counters.textureAllocationUnknownCalls == 0 },
        { "renderbufferAllocations", counters.renderbufferAllocations },
        { "renderbufferAllocationBytes", counters.renderbufferAllocationBytes },
        { "renderbufferAllocationBytesEstimated", true },
        { "renderbufferAllocationUnknownCalls", counters.renderbufferAllocationUnknownCalls },
        { "renderbufferAllocationBytesComplete", counters.renderbufferAllocationUnknownCalls == 0 },
        { "dirtySyncs", counters.dirtySyncs },
        { "sceneChangeMask", counters.sceneChangeMask },
        { "sceneChanges", SceneRevisions::maskToString(static_cast<uint32_t>(counters.sceneChangeMask)) },
        { "revisionMarks", counters.revisionMarks },
        { "updateGltfCalls", counters.updateGltfCalls },
        { "updateSceneCalls", counters.updateSceneCalls },
        { "applyAnimationCalls", counters.applyAnimationCalls },
        { "cacheRebuilds", counters.cacheRebuilds },
        { "sceneBuilds", counters.sceneBuilds },
        { "sceneDrawables", counters.sceneDrawables },
        { "wireframeMaterialCopies", counters.wireframeMaterialCopies },
        { "sceneNodesVisited", counters.sceneNodesVisited },
        { "pickingBuilds", counters.pickingBuilds },
        { "commandGenerations", counters.commandGenerations },
        { "overlayDraws", counters.overlayDraws },
        { "boundsComputations", counters.boundsComputations },
        { "boundsCacheHits", counters.boundsCacheHits },
        { "overrideApplies", counters.overrideApplies },
        { "overrideSkips", counters.overrideSkips },
        { "programCacheHits", counters.programCacheHits },
        { "programCacheMisses", counters.programCacheMisses },
        { "transformNodesVisited", counters.transformNodesVisited },
        { "transformNodesRecomputed", counters.transformNodesRecomputed },
        { "aaPasses", counters.aaPasses },
        { "uiDrawLists", counters.uiDrawLists },
        { "uiVertices", counters.uiVertices },
        { "uiIndices", counters.uiIndices }
    };
}

json stagesToJson(const std::vector<StageSample>& stages)
{
    json result = json::array();
    for (const auto& stage : stages) {
        result.push_back(json{
            { "name", stage.name },
            { "ms", stage.ms }
        });
    }
    return result;
}

json frameToJson(const FrameSample& sample)
{
    return json{
        { "frame", sample.frame },
        { "firstFrame", sample.firstFrame },
        { "frameIntervalMs", sample.frameIntervalMs },
        { "cpuFrameMs", sample.cpuFrameMs },
        { "stages", stagesToJson(sample.stages) },
        { "counters", countersToJson(sample.counters) }
    };
}

json startupToJson(const StartupProfile& startup)
{
    return json{
        { "vfsDecompressMs", startup.vfsDecompressMs },
        { "environmentInitMs", startup.environmentInitMs },
        { "environmentFilterMs", startup.environmentFilterMs },
        { "environmentLoads", startup.environmentLoads },
        { "loadDecodeMs", startup.loadDecodeMs },
        { "rendererUploadMs", startup.rendererUploadMs },
        { "validationMs", startup.validationMs },
        { "validationReportedMs", startup.validationReportedMs },
        { "validationBytesRead", startup.validationBytesRead },
        { "peakWorkingSetAfterDecode", startup.peakWorkingSetAfterDecode },
        { "peakWorkingSetAfterUpload", startup.peakWorkingSetAfterUpload },
        { "decodeBudgetBytes", startup.decodeBudgetBytes },
        { "decodeBudgetWaits", startup.decodeBudgetWaits },
        { "decodePeakInFlightBytes", startup.decodePeakInFlightBytes },
        { "decodePeakInFlightCount", startup.decodePeakInFlightCount },
        { "totalStartupMs", startup.totalStartupMs },
        { "modelBytes", startup.modelBytes },
        { "environmentBytes", startup.environmentBytes },
        { "nodeCount", startup.nodeCount },
        { "meshCount", startup.meshCount },
        { "materialCount", startup.materialCount },
        { "imageCount", startup.imageCount },
        { "textureCount", startup.textureCount },
        { "stages", stagesToJson(startup.stages) },
        { "counters", countersToJson(startup.counters) }
    };
}

json captureToJson(const CaptureRecord& capture)
{
    return json{
        { "valid", capture.valid },
        { "firstFrame", capture.firstFrame },
        { "source", capture.source },
        { "region", capture.region },
        { "path", capture.path },
        { "frame", capture.frame },
        { "viewportId", capture.viewportId },
        { "width", capture.width },
        { "height", capture.height },
        { "timeSeconds", capture.timeSeconds },
        { "readbackMs", capture.readbackMs },
        { "writeMs", capture.writeMs },
        { "rgbaHash", capture.rgbaHash },
        { "fileBytes", capture.fileBytes },
        { "flipVertically", true }
    };
}

json gpuFrameToJson(const GpuFrameReport& frame)
{
    json passes = json::array();
    for (const GpuPassReport& pass : frame.passes) {
        passes.push_back(json{
            { "name", pass.name },
            { "available", pass.available },
            { "ms", pass.available ? json(pass.ms) : json(nullptr) }
        });
    }

    return json{
        { "sequence", frame.sequence },
        { "frame", frame.frame },
        { "viewport", frame.viewport },
        { "label", frame.label },
        { "passes", passes }
    };
}

json gpuToJson(const GpuReport& gpu)
{
    json completed = json::array();
    for (const GpuFrameReport& frame : gpu.completed) {
        completed.push_back(gpuFrameToJson(frame));
    }

    json pending = json::array();
    for (const GpuFrameReport& frame : gpu.pending) {
        pending.push_back(gpuFrameToJson(frame));
    }

    json completedRange = json(nullptr);
    if (!gpu.completed.empty()) {
        completedRange = json{
            { "first", gpu.completed.front().frame },
            { "last", gpu.completed.back().frame }
        };
    }

    json pendingRange = json(nullptr);
    if (!gpu.pending.empty()) {
        pendingRange = json{
            { "first", gpu.pending.front().frame },
            { "last", gpu.pending.back().frame }
        };
    }

    return json{
        { "enabled", gpu.enabled },
        { "method", gpu.method },
        { "cpuSubmissionStage", gpu.cpuSubmissionStage },
        { "pendingFrames", gpu.pendingFrames },
        { "unavailablePolls", gpu.unavailablePolls },
        { "droppedFrames", gpu.droppedFrames },
        { "retention", json{
            { "completedFrameLimit", gpu.completedFrameLimit },
            { "completedFramesRetained", gpu.completed.size() },
            { "completedFramesDropped", gpu.completedFramesDropped },
            { "completedFrameRange", completedRange },
            { "pendingFrameRange", pendingRange },
            { "strategy", "oldest completed samples are dropped after the limit" }
        } },
        { "completed", completed },
        { "pending", pending }
    };
}

json timingStatsToJson(const std::vector<double>& input)
{
    if (input.empty()) {
        return json{
            { "count", 0 },
            { "min", nullptr },
            { "median", nullptr },
            { "mean", nullptr },
            { "p95", nullptr },
            { "max", nullptr },
            { "stddev", nullptr }
        };
    }

    std::vector<double> values = input;
    std::sort(values.begin(), values.end());
    const double sum = std::accumulate(values.begin(), values.end(), 0.0);
    const double mean = sum / static_cast<double>(values.size());
    double varianceSum = 0.0;
    for (double value : values) {
        const double difference = value - mean;
        varianceSum += difference * difference;
    }
    const double stddev = std::sqrt(varianceSum / static_cast<double>(values.size()));

    const size_t midpoint = values.size() / 2;
    const double median = values.size() % 2 == 0
        ? (values[midpoint - 1] + values[midpoint]) * 0.5
        : values[midpoint];
    const size_t p95Index = std::min(values.size() - 1,
        static_cast<size_t>(std::ceil(0.95 * static_cast<double>(values.size()))) - 1);

    return json{
        { "count", values.size() },
        { "min", values.front() },
        { "median", median },
        { "mean", mean },
        { "p95", values[p95Index] },
        { "max", values.back() },
        { "stddev", stddev },
        { "definition", "population stddev; p95 nearest-rank" }
    };
}

bool writeJsonFile(const std::string& path, const json& document)
{
    std::error_code directoryError;
    const std::filesystem::path outputPath(path);
    if (outputPath.has_parent_path()) {
        std::filesystem::create_directories(outputPath.parent_path(), directoryError);
    }

    std::ofstream stream(outputPath, std::ios::binary | std::ios::trunc);
    if (!stream) {
        return false;
    }
    stream << document.dump(2) << '\n';
    stream.flush();
    const bool flushed = stream.good();
    stream.close();
    return flushed && !stream.fail();
}

} // namespace

bool writeProfileReport(const std::string& path, const ProfileReport& report)
{
    if (path.empty()) {
        return false;
    }

    json frames = json::array();
    for (const auto& sample : report.frames) {
        frames.push_back(frameToJson(sample));
    }

    json frameRange = json(nullptr);
    if (!report.frames.empty()) {
        frameRange = json{
            { "first", report.frames.front().frame },
            { "last", report.frames.back().frame }
        };
    }

    const json document{
        { "schema", "glider.profile.v1" },
        { "build", report.build },
        { "scenarioName", report.scenarioName },
        { "model", report.modelPath },
        { "isolated", report.isolated },
        { "instrumentationEnabled", report.instrumentationEnabled },
        { "vsyncRequested", report.vsyncRequested },
        { "gl", json{
            { "version", report.glVersion },
            { "vendor", report.gpuVendor },
            { "renderer", report.gpuRenderer }
        } },
        { "window", json{
            { "width", report.windowWidth },
            { "height", report.windowHeight }
        } },
        { "viewport", json{
            { "width", report.viewportWidth },
            { "height", report.viewportHeight },
            { "samples", report.viewportSamples },
            { "antiAliasing", report.antiAliasing }
        } },
        { "reportFrame", report.reportFrame },
        { "frameRetention", json{
            { "limit", report.frameSampleLimit },
            { "retained", report.frames.size() },
            { "dropped", report.frameSamplesDropped },
            { "range", frameRange },
            { "strategy", "oldest CPU frame samples are dropped after the limit" }
        } },
        { "counterScope", json{
            { "glCalls", "calls through the renderer gl::GL wrapper" },
            { "appDrawCalls", "explicitly counted direct application draws" },
            { "uploads", "renderer wrapper uploads plus explicitly counted application uploads" },
            { "textureAllocationBytes", "estimated from internal format, dimensions and mip level; excludes driver padding and mipmaps generated after upload" },
            { "renderbufferAllocationBytes", "estimated from internal format, dimensions and sample count" },
            { "textureUploadBytes", "estimated from transfer format, type and dimensions" },
            { "unknownFormatCalls", "counted separately; byte totals are lower bounds when a completeness flag is false" }
        } },
        { "startup", startupToJson(report.startup) },
        { "frames", frames },
        { "gpu", gpuToJson(report.gpu) },
        { "captureRequested", report.captureRequested },
        { "hasFirstFrameCapture", report.hasFirstFrameCapture },
        { "firstFrameCapture", captureToJson(report.firstFrameCapture) },
        { "capture", captureToJson(report.capture) }
    };

    return writeJsonFile(path, document);
}

bool writeBenchmarkReport(const std::string& path, const BenchmarkReport& report)
{
    if (path.empty()) {
        return false;
    }

    json measuredFrames = json::array();
    std::vector<double> frameIntervals;
    std::vector<double> cpuFrameTimes;
    frameIntervals.reserve(report.measuredFrames.size());
    cpuFrameTimes.reserve(report.measuredFrames.size());
    for (const FrameSample& sample : report.measuredFrames) {
        measuredFrames.push_back(frameToJson(sample));
        frameIntervals.push_back(sample.frameIntervalMs);
        cpuFrameTimes.push_back(sample.cpuFrameMs);
    }

    const json firstFrame = report.hasFirstFrame
        ? frameToJson(report.firstFrame)
        : json(nullptr);

    json frameRange = json(nullptr);
    if (!report.measuredFrames.empty()) {
        frameRange = json{
            { "first", report.measuredFrames.front().frame },
            { "last", report.measuredFrames.back().frame }
        };
    }

    const json document{
        { "schema", "glider.benchmark.v1" },
        { "build", report.build },
        { "scenarioName", report.scenarioName },
        { "model", report.modelPath },
        { "isolated", report.isolated },
        { "detailedInstrumentation", report.detailedInstrumentation },
        { "complete", report.measuredFrames.size() == report.measuredFramesRequested },
        { "warmupFrames", report.warmupFrames },
        { "measuredFramesRequested", report.measuredFramesRequested },
        { "measuredFramesRecorded", report.measuredFrames.size() },
        { "measuredFrameNumbers", frameRange },
        { "vsync", json{
            { "requested", report.vsyncRequested },
            { "control", "glfwSwapInterval" }
        } },
        { "gl", json{
            { "version", report.glVersion },
            { "vendor", report.gpuVendor },
            { "renderer", report.gpuRenderer }
        } },
        { "window", json{
            { "width", report.windowWidth },
            { "height", report.windowHeight }
        } },
        { "viewport", json{
            { "width", report.viewportWidth },
            { "height", report.viewportHeight },
            { "samples", report.viewportSamples },
            { "antiAliasing", report.antiAliasing }
        } },
        { "timing", json{
            { "source", "std::chrono::steady_clock at frame start and after UI submission" },
            { "frameIntervalMs", timingStatsToJson(frameIntervals) },
            { "cpuFrameMs", timingStatsToJson(cpuFrameTimes) }
        } },
        { "firstFrame", firstFrame },
        { "measuredFrames", measuredFrames },
        { "gpu", gpuToJson(report.gpu) }
    };

    return writeJsonFile(path, document);
}

} // namespace ux3d::imgui::profiling
