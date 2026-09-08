// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "Types.h"

namespace ux3d::gpu {
struct PhysicalDevice;
struct LogicalDevice;

/// Allocator for GPU memory.
/// One allocator is required per logical device.
struct GPUAllocator {
    Handle _handle = InvalidHandle;
};

ux3d::gpu::GpuResult createGPUAllocator(
    const PhysicalDevice& physicalDevice,
    const LogicalDevice& logicalDevice,
    GPUAllocator& outGPUAllocator);

void destroyGPUAllocator(GPUAllocator& gpuAllocator);

/// Represents a single allocation of the GPU, made by the GPUAllocator.
struct GPUAllocation {
    Handle _handle = InvalidHandle;
};
}
