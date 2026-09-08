// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <cstdint>

#include "core/String.h"
#include "core/Vector.h"

#include "Types.h"

namespace ux3d::gpu {
// forward decls:
struct Instance;

struct PhysicalDevice {
    Handle _handle = InvalidHandle;
};

GpuResult enumeratePhysicalDevices(
    const Instance& instance,
    core::Vector<PhysicalDevice>& outPhysicalDevices,
    core::IAllocator* allocator);

enum class PhysicalDeviceType : uint32_t {
    Other,
    IntegratedGPU,
    DiscreteGPU,
    VirtualGPU,
    CPU
};

struct PhysicalDeviceLimits {
    // TODO: fill with required fields
};

struct PhysicalDeviceFeatures {
    // TODO: fill with required fields
    bool graphicsSupport = false;
    bool computeSupport = false;
};

struct PhysicalDeviceProperties {
    uint32_t driverVersion;
    uint32_t vendorID;
    uint32_t deviceID;
    PhysicalDeviceType deviceType;
    core::string deviceName;
    PhysicalDeviceLimits limits;
    PhysicalDeviceFeatures features;
    bool hasSwapchainSupport = false;
};

GpuResult getPhysicalDeviceProperties(
    const PhysicalDevice& physicalDevice,
    PhysicalDeviceProperties& outPhysicalDeviceProperties,
    core::IAllocator* allocator);
}
