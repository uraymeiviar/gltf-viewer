#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>

// Shared by the CPU importer and the GL owner. No UI or GL calls on importer threads.
namespace ux3d::core::loading {
using Clock = std::chrono::steady_clock;
enum Stage { Json, Buffers, Images, Upload, Mips, Ibl, Validation, Shaders, StageCount };
inline constexpr const char* names[] = { "JSON parse", "Buffer read", "Image decode", "GPU upload",
    "Mip generation", "IBL filtering", "Validation", "Shader compile" };
struct State {
    std::mutex mutex;
    std::array<double, StageCount> milliseconds{};
    std::atomic<int> stage{Json};
    std::atomic<uint32_t> completed{0}, total{0}, textureLimit{0}, reductions{0};
    std::atomic<uint64_t> estimatedTextureBytes{0};
    std::atomic<uint64_t> textureBudget{0}, peakGpuUsage{0}, availableVram{0}, availableRam{0};
    std::atomic<uint32_t> maxTextureSize{16384}, progressFrames{0}, allocationFailures{0};
    std::atomic<bool> memoryAware{false};
    // True when the active quality profile asks for unlimited textures. The
    // estimate-based budget must then leave the image dimensions untouched.
    std::atomic<bool> unlimitedTextures{false};
    // True while a preset change re-decodes and re-uploads the retained images.
    std::atomic<bool> textureRefresh{false};
    std::atomic<uint32_t> requestedTextureLimit{0};
    // Installed and invoked exclusively by the GL thread, between complete work items.
    std::function<void()> yield;
};
inline State state;
inline void reset()
{
    std::lock_guard lock(state.mutex);
    state.milliseconds.fill(0);
    state.stage = Json; state.completed = 0; state.total = 0;
    state.reductions = 0; state.estimatedTextureBytes = 0;
    state.progressFrames = 0; state.allocationFailures = 0; state.peakGpuUsage = 0;
    state.unlimitedTextures = false; state.textureRefresh = false;
}
inline void begin(Stage stage, uint32_t total = 0)
{
    state.stage = stage; state.completed = 0; state.total = total;
}
struct Timer {
    Stage stage;
    Clock::time_point start = Clock::now();
    explicit Timer(Stage value) : stage(value) {}
    ~Timer() {
        const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        std::lock_guard lock(state.mutex);
        state.milliseconds[stage] += ms;
    }
};
inline void checkpoint() { if (state.yield) state.yield(); }
} // namespace ux3d::core::loading
