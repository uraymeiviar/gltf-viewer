// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  DescriptorPool.h
//  middleware
//
//  Created by Moritz Becher on 13.06.19.
//

#pragma once

#include "core/Vector.h"

#include "LogicalDevice.h"

namespace ux3d::gpu {
// DescriptorPoolSize that determines how many descriptors of each type are required
struct DescriptorPoolSize {
    DescriptorType type;
    uint32_t descriptorCount;
};

// Create the descriptor pool
struct DescriptorPoolCreateInfo {
    enum class CreateFlagBits : uint32_t {
        None = 0,
        FreeDescriptorSet = 0x00000001,
        UpdateAfterBind = 0x00000002,
    };
    using CreateFlag = core::Flag<CreateFlagBits>;

    CreateFlag flags;
    uint32_t maxSets;
    core::Vector<DescriptorPoolSize> poolSizes;
};

struct DescriptorPool {
    Handle _handle {};
    LogicalDevice _device;
};

GpuResult createDescriptorPool(
    const LogicalDevice& device,
    const DescriptorPoolCreateInfo& descriptorPoolInfo,
    DescriptorPool& descriptorPool,
    core::IAllocator* allocator);

void destroyDescriptorPool(DescriptorPool& descriptorPool);
}
