#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dxgi1_4.h>
#include <psapi.h>
#endif

namespace ux3d::imgui {
struct MemoryInfo {
    uint64_t availableRam = 0, availableVram = 0, gpuBudget = 0, gpuUsage = 0;
    const char* gpuSource = "unknown";
    bool gpuKnown = false;
    uint64_t totalRam = 0, commitLimit = 0, commitAvailable = 0;
    uint64_t workingSet = 0, peakWorkingSet = 0, privateCommit = 0, peakPrivateCommit = 0;
    uint64_t processCpu100ns = 0;
    unsigned logicalProcessors = 1;
    uint64_t gpuCapacity = 0, sharedGpuUsage = 0, sharedGpuBudget = 0;
    bool sharedGpuKnown = false;
};

// Query the OS-supplied DXGI dynamically; no redistributable is needed.
inline MemoryInfo queryMemoryInfo()
{
    MemoryInfo info;
#ifdef _WIN32
    MEMORYSTATUSEX ram{};
    ram.dwLength = sizeof(ram);
    if (GlobalMemoryStatusEx(&ram)) {
        info.availableRam = ram.ullAvailPhys; info.totalRam = ram.ullTotalPhys;
        info.commitLimit = ram.ullTotalPageFile; info.commitAvailable = ram.ullAvailPageFile;
    }
    PROCESS_MEMORY_COUNTERS_EX process{};
    if (GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&process),sizeof(process))) {
        info.workingSet = process.WorkingSetSize; info.peakWorkingSet = process.PeakWorkingSetSize;
        info.privateCommit = process.PrivateUsage; info.peakPrivateCommit = process.PeakPagefileUsage;
    }
    FILETIME created{}, exited{}, kernel{}, user{};
    if (GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user))
        info.processCpu100ns = (uint64_t(kernel.dwHighDateTime)<<32 | kernel.dwLowDateTime) +
            (uint64_t(user.dwHighDateTime)<<32 | user.dwLowDateTime);
    SYSTEM_INFO system{}; GetSystemInfo(&system); info.logicalProcessors = std::max(1ul,system.dwNumberOfProcessors);
    static uint64_t capacity = 0;
    static IDXGIAdapter3* adapter = []() -> IDXGIAdapter3* {
        HMODULE module = LoadLibraryExW(L"dxgi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!module) return nullptr;
        using CreateFactory = HRESULT(WINAPI*)(REFIID, void**);
        auto create = reinterpret_cast<CreateFactory>(GetProcAddress(module, "CreateDXGIFactory1"));
        IDXGIFactory1* factory = nullptr;
        if (!create || FAILED(create(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) return nullptr;
        IDXGIAdapter3* best = nullptr;
        SIZE_T bestBytes = 0;
        const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const UINT vendorId = vendor && std::strstr(vendor, "NVIDIA") ? 0x10de :
            vendor && (std::strstr(vendor, "ATI") || std::strstr(vendor, "AMD")) ? 0x1002 :
            vendor && std::strstr(vendor, "Intel") ? 0x8086 : 0;
        for (UINT i = 0; ; ++i) {
            IDXGIAdapter1* candidate = nullptr;
            if (factory->EnumAdapters1(i, &candidate) == DXGI_ERROR_NOT_FOUND) break;
            if (!candidate) break;
            DXGI_ADAPTER_DESC1 desc{};
            candidate->GetDesc1(&desc);
            if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) && vendorId != 0 && desc.VendorId == vendorId
                && (!best || desc.DedicatedVideoMemory > bestBytes)) {
                IDXGIAdapter3* queried = nullptr;
                if (SUCCEEDED(candidate->QueryInterface(__uuidof(IDXGIAdapter3), reinterpret_cast<void**>(&queried)))) {
                    if (best) best->Release();
                    best = queried; bestBytes = desc.DedicatedVideoMemory;
                    capacity = desc.DedicatedVideoMemory;
                }
            }
            candidate->Release();
        }
        factory->Release();
        return best; // held for process lifetime; the system DLL remains loaded
    }();
    DXGI_QUERY_VIDEO_MEMORY_INFO video{};
    if (adapter && SUCCEEDED(adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &video))) {
        info.gpuBudget = video.Budget; info.gpuUsage = video.CurrentUsage;
        info.availableVram = video.Budget > video.CurrentUsage ? video.Budget-video.CurrentUsage : 0;
        info.gpuKnown = true; info.gpuSource = "DXGI local process budget";
        info.gpuCapacity = capacity;
        DXGI_QUERY_VIDEO_MEMORY_INFO shared{};
        if (SUCCEEDED(adapter->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL,&shared))) {
            info.sharedGpuKnown = true; info.sharedGpuUsage = shared.CurrentUsage; info.sharedGpuBudget = shared.Budget;
        }
    }
#endif
    if (!info.gpuKnown && glfwExtensionSupported("GL_NVX_gpu_memory_info")) {
        GLint available = 0, total = 0;
        glGetIntegerv(0x9049, &available);
        glGetIntegerv(0x9048, &total);
        info.availableVram = uint64_t(std::max(0, available))*1024;
        info.gpuBudget = uint64_t(std::max(0, total))*1024;
        info.gpuUsage = info.gpuBudget > info.availableVram ? info.gpuBudget-info.availableVram : 0;
        info.gpuKnown = true; info.gpuSource = "GL_NVX_gpu_memory_info (adapter)";
    } else if (!info.gpuKnown && glfwExtensionSupported("GL_ATI_meminfo")) {
        GLint free[4]{};
        glGetIntegerv(0x87FC, free);
        info.availableVram = uint64_t(std::max(0, free[0]))*1024;
        info.gpuKnown = true; info.gpuSource = "GL_ATI_meminfo (free texture pool)";
    }
    if (const char* cap = std::getenv("GLIDER_TEST_VRAM_MB")) {
        char* end = nullptr;
        const auto mb = std::strtoull(cap, &end, 10);
        if (end != cap && *end == 0 && mb > 0 && mb <= 1048576) {
            const uint64_t bytes = mb*1024*1024;
            info.gpuBudget = info.gpuKnown && info.gpuBudget ? std::min(bytes, info.gpuBudget) : bytes;
            info.availableVram = info.gpuBudget > info.gpuUsage ? info.gpuBudget-info.gpuUsage : 0;
            info.gpuKnown = true; info.gpuSource = "GLIDER_TEST_VRAM_MB cap";
        }
    }
    return info;
}
} // namespace ux3d::imgui
