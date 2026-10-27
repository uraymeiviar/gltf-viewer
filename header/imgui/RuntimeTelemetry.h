#pragma once

#include "imgui/MemoryInfo.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <string>

namespace ux3d::imgui {
inline std::string memoryText(uint64_t bytes)
{
    if (bytes < 1024) return std::to_string(bytes) + " B";
    char text[64];
    const double divisor = bytes >= (1ull<<30) ? double(1ull<<30) : bytes >= (1ull<<20) ? double(1ull<<20) : 1024.;
    std::snprintf(text,sizeof(text),"%.2f %s",bytes/divisor,bytes >= (1ull<<30) ? "GiB" : bytes >= (1ull<<20) ? "MiB" : "KiB");
    return text;
}
struct RuntimeTelemetry {
    MemoryInfo memory;
    double fps = 0, renderFps = 0, cpuMs = 0, gpuMs = 0, cpuPercent = 0, p95CpuMs = 0, maxCpuMs = 0;
    uint64_t peakGpuUsage = 0, frames = 0, renderedFrames = 0;
    bool lastFrameRendered = false;
    std::array<double,240> cpuHistory{};
    size_t historyCount = 0, historyIndex = 0;
    std::chrono::steady_clock::time_point sampled{}, fpsStart{};
    uint64_t lastCpu = 0, fpsFrames = 0, fpsRenders = 0;

    static RuntimeTelemetry& get() { static RuntimeTelemetry value; return value; }
    void record(double frameCpuMs, double frameGpuMs, bool rendered)
    {
        const auto now = std::chrono::steady_clock::now();
        const bool first = fpsStart.time_since_epoch().count() == 0;
        if (first) fpsStart = now;
        ++frames; if (!first) ++fpsFrames; if (rendered) { ++renderedFrames; if (!first) ++fpsRenders; }
        lastFrameRendered = rendered;
        cpuMs = frameCpuMs; gpuMs = frameGpuMs;
        cpuHistory[historyIndex++ % cpuHistory.size()] = frameCpuMs;
        historyCount = std::min(cpuHistory.size(),historyCount+1);
        const double elapsed = std::chrono::duration<double>(now-fpsStart).count();
        if (elapsed >= .05) {
            fps = fpsFrames/elapsed; renderFps = fpsRenders/elapsed;
        }
        if (elapsed >= .5) {
            fpsFrames = fpsRenders = 0; fpsStart = now;
        }
        if (sampled.time_since_epoch().count() == 0 || now-sampled >= std::chrono::milliseconds(500)) {
            auto next = queryMemoryInfo();
            if (sampled.time_since_epoch().count() && next.processCpu100ns >= lastCpu)
                cpuPercent = (next.processCpu100ns-lastCpu)/1e7/std::chrono::duration<double>(now-sampled).count()/next.logicalProcessors*100;
            lastCpu = next.processCpu100ns; memory = next; sampled = now;
            peakGpuUsage = std::max(peakGpuUsage,memory.gpuUsage);
            auto sorted = cpuHistory;
            std::sort(sorted.begin(),sorted.begin()+historyCount);
            if (historyCount) { p95CpuMs = sorted[(historyCount-1)*95/100]; maxCpuMs = sorted[historyCount-1]; }
        }
    }
};
} // namespace ux3d::imgui
