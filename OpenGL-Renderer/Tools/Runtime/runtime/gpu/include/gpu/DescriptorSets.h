// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  DescriptorSets.h
//  middleware
//
//  Created by Moritz Becher on 13.06.19.
//

#pragma once

#include "core/Vector.h"

#include "BufferView.h"
#include "DescriptorPool.h"
#include "DescriptorSetLayout.h"
#include "ImageView.h"
#include "Sampler.h"

namespace ux3d::gpu {
struct LogicalDevice;

struct DescriptorSetAllocateInfo {
    DescriptorPool descriptorPool;
    core::Vector<DescriptorSetLayout> setLayouts;
};

struct DescriptorSet {
    Handle _handle = InvalidHandle;
};

GpuResult allocateDescriptorSets(
    const LogicalDevice& device,
    const DescriptorSetAllocateInfo& allocationInfo,
    core::Vector<DescriptorSet>& descriptorSets,
    core::IAllocator* allocator);

struct DescriptorImageInfo {
    Sampler sampler;
    ImageView imageView;
    ImageLayout imageLayout {};
};

struct DescriptorBufferInfo {
    Buffer buffer;
    uint64_t offset {};
    uint64_t range { WholeSize };
};

struct WriteDescriptorSet {
    DescriptorSet dstSet;
    uint32_t dstBinding = 0u;
    uint32_t dstArrayElement = 0u;

    // Derive from pImageInfo, pBufferInfo and pTexelBufferView until we support extensions:
    // uint32_t dstDescriptorCount = 0u;

    DescriptorType descriptorType {};

    core::Vector<DescriptorImageInfo> images;
    core::Vector<DescriptorBufferInfo> buffers;
    core::Vector<BufferView> texelBuffers;
};

struct CopyDescriptorSet {
    DescriptorSet srcSet;
    uint32_t srcBinding {};
    uint32_t srcArrayElement {};
    DescriptorSet dstSet;
    uint32_t dstBinding {};
    uint32_t dstArrayElement {};
    uint32_t descriptorCount {};
};

GpuResult updateDescriptorSets(
    const LogicalDevice& _device,
    const core::Vector<WriteDescriptorSet>& _writeSets,
    const core::Vector<CopyDescriptorSet>& _copySets,
    core::IAllocator* _allocator);

} // !ux3d
