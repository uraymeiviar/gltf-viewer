// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Vector.h"

#include "PhysicalDevice.h"
#include "Queue.h"
#include "Types.h"

namespace ux3d::gpu {
struct LogicalDevice {
    Handle _handle = InvalidHandle;
};

struct LogicalDeviceCreateInfo {
    PhysicalDeviceFeatures usedFeatures;
    bool needsSwapchainSupport = true; // TODO: a better name would probably be the inverse: isHeadless
};

GpuResult createLogicalDevice(
    const PhysicalDevice& physicalDevice,
    const LogicalDeviceCreateInfo& logicalDeviceCreateInfo,
    const core::Vector<QueueCreateInfo>& queueInfos,
    LogicalDevice& outLogicalDevice,
    core::Vector<Queue>& outQueues,
    core::IAllocator* allocator);

void destroyLogicalDevice(LogicalDevice& logicalDevice);
}
