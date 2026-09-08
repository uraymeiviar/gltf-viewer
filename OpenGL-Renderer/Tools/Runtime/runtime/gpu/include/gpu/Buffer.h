// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  Buffer.h
//  middleware
//
//  Created by Moritz Becher on 23.05.19.
//

#pragma once

#include "core/Vector.h"

#include "Allocator.h"
#include "Queue.h"

namespace ux3d::gpu {
struct BufferCreateInfo {
    enum class CreateFlagBits : uint32_t {
        None = 0,
        SparseBinding = 0x00000001,
        SparseResidency = 0x00000002,
        SparseAliased = 0x00000004,
        Protected = 0x00000008,
        DeviceAdressCaptureReplay = 0x00000010,
    };
    using CreateFlag = core::Flag<CreateFlagBits>;

    enum class UsageFlagBits : uint32_t {
        None = 0,
        TransferSource = 0x00000001,
        TransferDestination = 0x00000002,
        UniformTexelBuffer = 0x00000004,
        StorageTexelBuffer = 0x00000008,
        UniformBuffer = 0x00000010,
        StorageBuffer = 0x00000020,
        IndexBuffer = 0x00000040,
        VertexBuffer = 0x00000080,
        IndirectBuffer = 0x00000100,
    };
    using UsageFlag = core::Flag<UsageFlagBits>;

    CreateFlag flags;
    size_t size;
    UsageFlag usage;
    SharingMode sharingMode;
    core::Vector<Queue> queues;
};

struct Buffer {
    Handle _handle = InvalidHandle;
    GPUAllocator _gpuAllocator {};
    GPUAllocation _gpuAllocation {};
};

enum class MemoryUsage : uint32_t {
    GPUOnly = 1,
    CPUOnly = 2,
    CPUtoGPU = 3,
    GPUtoCPU = 4,
};

struct AllocationCreateInfo {
    MemoryUsage usage;
};

GpuResult createBuffer(
    GPUAllocator& gpuAllocator,
    const BufferCreateInfo& createInfo,
    Buffer& outBuffer,
    const AllocationCreateInfo& allocationCreateInfo,
    core::IAllocator* allocator);

void destroyBuffer(Buffer& buffer);
}
