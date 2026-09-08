// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  DescriptorPool.cpp
//  middleware
//
//  Created by Moritz Becher on 13.06.19.
//

#include "gpu/DescriptorPool.h"

#include <vulkan/vulkan.h>

#include "core/HeapAllocator.h"
#include "core/LinearAllocator.h"
#include "core/Slice.h"

#include "gpu/impl_vulkan/Result.h"

namespace ux3d::gpu {
void convert(const DescriptorPoolSize& _poolSize, VkDescriptorPoolSize& _vkPoolSize)
{
    _vkPoolSize.type = static_cast<VkDescriptorType>(_poolSize.type);
    _vkPoolSize.descriptorCount = _poolSize.descriptorCount;
}

struct VkDescriptorPoolCreateInfoData {
    core::Slice<VkDescriptorPoolSize> vkDescriptorPoolSizes;
};

VulkanResult convert(
    const DescriptorPoolCreateInfo _descriptorPoolInfo,
    VkDescriptorPoolCreateInfo& _vkDescriptorPoolInfo,
    VkDescriptorPoolCreateInfoData& _data,
    core::IAllocator* _allocator)
{
    _vkDescriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    _vkDescriptorPoolInfo.flags = static_cast<VkDescriptorPoolCreateFlags>(_descriptorPoolInfo.flags);
    _vkDescriptorPoolInfo.maxSets = _descriptorPoolInfo.maxSets;

    if (!_data.vkDescriptorPoolSizes.allocate(_descriptorPoolInfo.poolSizes.size(), _allocator)) {
        return GpuError::OutOfHostMemory;
    }

    for (size_t i = 0u; i < _data.vkDescriptorPoolSizes.size(); ++i) {
        convert(_descriptorPoolInfo.poolSizes[i], _data.vkDescriptorPoolSizes[i]);
    }

    _vkDescriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(_data.vkDescriptorPoolSizes.size());
    _vkDescriptorPoolInfo.pPoolSizes = _data.vkDescriptorPoolSizes.data();

    return GpuResult::Success;
}
}

ux3d::gpu::GpuResult ux3d::gpu::createDescriptorPool(
    const LogicalDevice& _device,
    const DescriptorPoolCreateInfo& _descriptorPoolInfo,
    DescriptorPool& _descriptorPool,
    core::IAllocator* _allocator)
{
    VkDescriptorPoolCreateInfoData vkData;
    VkDescriptorPoolCreateInfo vkCreateInfo {};
    VulkanResult result = convert(_descriptorPoolInfo, vkCreateInfo, vkData, _allocator);

    if (!result) {
        return std::move(result);
    }

    VkDescriptorPool vkDescriptorPool;
    result = vkCreateDescriptorPool(reinterpret_cast<VkDevice>(_device._handle), &vkCreateInfo, nullptr,
        &vkDescriptorPool);

    if (result) {
        _descriptorPool._handle = reinterpret_cast<Handle>(vkDescriptorPool);
        _descriptorPool._device = _device;
    }

    return std::move(result);
}

void ux3d::gpu::destroyDescriptorPool(DescriptorPool& _descriptorPool)
{
    vkDestroyDescriptorPool(reinterpret_cast<VkDevice>(_descriptorPool._device._handle),
        reinterpret_cast<VkDescriptorPool>(_descriptorPool._handle), nullptr);
}
