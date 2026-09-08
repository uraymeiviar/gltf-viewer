// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ApplicationInfo.h"
#include "Types.h"

#include "core/Vector.h"

namespace ux3d::gpu {
enum class BackendType : uint32_t {
    Undefined,
    Vulkan
};

struct Instance {
    BackendType backendType = BackendType::Vulkan;
    Handle _handle = InvalidHandle;
};

// Instance Creation

struct InstanceCreateInfo {
    ApplicationInfo applicationInfo;
#ifdef NDEBUG
    bool enableValidation = false;
#else
    bool enableValidation = true;
#endif
    // TODO: Does not make sense, because we have to specify the extension appropriate for the current
    //  windowing system such as X11 anyway. Because the session is platform agnostic, it won't check
    //  which exact extension is required. This convenient feature of adding the VK_KHR_surface extension
    //  is of no use then.
    bool surfaceSupport = true;

    core::Vector<core::string> requiredExtensions;
};

GpuResult createInstance(const InstanceCreateInfo& info, Instance& outInstance, core::IAllocator* allocator);

void destroyInstance(Instance& instance);
}
