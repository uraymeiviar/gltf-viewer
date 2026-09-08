// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  Buffer.cpp
//  middleware
//
//  Created by Moritz Becher on 23.05.19.
//

#include "gpu/Buffer.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "core/HeapAllocator.h"
#include "core/LinearAllocator.h"

#include "gpu/impl_vulkan/Result.h"

namespace ux3d::gpu {
struct VkBufferCreateInfoData {
    core::Vector<uint32_t> vkQueueFamilyIndices;
};

VulkanResult convert(
    const BufferCreateInfo& _info,
    VkBufferCreateInfo& _outVkInfo,
    VkBufferCreateInfoData& _data,
    core::IAllocator* _allocator)
{
    if (!_data.vkQueueFamilyIndices.reserve(_info.queues.size(), _allocator)) {
        return GpuError::OutOfHostMemory;
    }

    for (const auto& queue : _info.queues) {
        (void)_data.vkQueueFamilyIndices.emplace_back(queue._queueFamilyIndex);
    }

    _outVkInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    _outVkInfo.pNext = nullptr;
    _outVkInfo.flags = static_cast<VkBufferCreateFlags>(_info.flags);
    _outVkInfo.size = static_cast<VkDeviceSize>(_info.size);
    _outVkInfo.usage = static_cast<VkBufferUsageFlags>(_info.usage);
    _outVkInfo.sharingMode = static_cast<VkSharingMode>(_info.sharingMode);
    _outVkInfo.queueFamilyIndexCount = static_cast<uint32_t>(_data.vkQueueFamilyIndices.size());
    _outVkInfo.pQueueFamilyIndices = _data.vkQueueFamilyIndices.data();

    return VulkanResult::Success;
}
}

ux3d::gpu::GpuResult ux3d::gpu::createBuffer(
    GPUAllocator& _gpuAllocator,
    const BufferCreateInfo& _createInfo,
    Buffer& _outBuffer,
    const AllocationCreateInfo& _allocationCreateInfo,
    core::IAllocator* _allocator)
{
    if (_outBuffer._handle != InvalidHandle) {
        return GpuError::InvalidHandle;
    }

    auto vmaAllocator = reinterpret_cast<VmaAllocator>(_gpuAllocator._handle);

    VkBufferCreateInfoData vkData;
    VkBufferCreateInfo vkBufferCreateInfo {};

    VulkanResult result = convert(_createInfo, vkBufferCreateInfo, vkData, _allocator);

    if (!result) {
        return std::move(result);
    }

    VmaAllocationInfo vmaAllocationInfo {};

    // TODO: fill vmaAllocationCreateInfo
    VmaAllocationCreateInfo vmaAllocationCreateInfo {};
    vmaAllocationCreateInfo.usage = static_cast<VmaMemoryUsage>(_allocationCreateInfo.usage);

    VkBuffer vkBuffer;
    VmaAllocation vmaAllocation;

    result = vmaCreateBuffer(vmaAllocator, &vkBufferCreateInfo, &vmaAllocationCreateInfo, &vkBuffer, &vmaAllocation,
        &vmaAllocationInfo);

    if (result) {
        _outBuffer._handle = reinterpret_cast<Handle>(vkBuffer);
        _outBuffer._gpuAllocator = _gpuAllocator;
        _outBuffer._gpuAllocation._handle = reinterpret_cast<Handle>(vmaAllocation);
    }

    return std::move(result);
}

void ux3d::gpu::destroyBuffer(Buffer& _buffer)
{
    if (_buffer._handle != InvalidHandle) {
        vmaDestroyBuffer(reinterpret_cast<VmaAllocator>(_buffer._gpuAllocator._handle),
            reinterpret_cast<VkBuffer>(_buffer._handle),
            reinterpret_cast<VmaAllocation>(_buffer._gpuAllocation._handle));
        _buffer._handle = InvalidHandle;
        _buffer._gpuAllocation._handle = InvalidHandle;
        _buffer._gpuAllocator._handle = InvalidHandle;
    }
}
