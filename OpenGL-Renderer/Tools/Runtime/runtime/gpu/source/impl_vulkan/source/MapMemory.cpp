// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  Memory.cpp
//  middleware
//
//  Created by Jim Eckerlein on 7.06.2019.
//

#include "gpu/MapMemory.h"
#include "gpu//Buffer.h"
#include "gpu/Allocator.h"
#include "gpu/Image.h"

#include "gpu/impl_vulkan/Result.h"

#include <vk_mem_alloc.h>

ux3d::gpu::GpuResult ux3d::gpu::mapMemory(const GPUAllocator& _gpuAllocator, const GPUAllocation& _gpuAllocation, void*& _pOutMappedData)
{
    if (_gpuAllocator._handle == InvalidHandle || _gpuAllocation._handle == InvalidHandle) {
        return GpuError::InvalidHandle;
    }

    _pOutMappedData = nullptr;

    VmaAllocationInfo vmaAllocationInfo {};
    vmaGetAllocationInfo(
        reinterpret_cast<VmaAllocator>(_gpuAllocator._handle),
        reinterpret_cast<VmaAllocation>(_gpuAllocation._handle),
        &vmaAllocationInfo);

    VkMemoryPropertyFlags vkMemoryFlags {};
    vmaGetMemoryTypeProperties(
        reinterpret_cast<VmaAllocator>(_gpuAllocator._handle),
        vmaAllocationInfo.memoryType,
        &vkMemoryFlags);

    if ((vkMemoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 0) {
        // Memory is not visible from the CPU and is therefore not mappable.
        return GpuError::MemoryMapFailed;
    }

    const VulkanResult result = vmaMapMemory(
        reinterpret_cast<VmaAllocator>(_gpuAllocator._handle),
        reinterpret_cast<VmaAllocation>(_gpuAllocation._handle),
        &_pOutMappedData);

    return result;
}

ux3d::gpu::GpuResult ux3d::gpu::unmapMemory(const GPUAllocator& _gpuAllocator, const GPUAllocation& _gpuAllocation)
{
    if (_gpuAllocator._handle == InvalidHandle || _gpuAllocation._handle == InvalidHandle) {
        return GpuError::InvalidHandle;
    }

    vmaUnmapMemory(
        reinterpret_cast<VmaAllocator>(_gpuAllocator._handle),
        reinterpret_cast<VmaAllocation>(_gpuAllocation._handle));

    return GpuResult::Success;
}

ux3d::gpu::GpuResult ux3d::gpu::mapMemory(const Buffer& _buffer, void*& _outMappedData)
{
    return mapMemory(_buffer._gpuAllocator, _buffer._gpuAllocation, _outMappedData);
}

ux3d::gpu::GpuResult ux3d::gpu::unmapMemory(const Buffer& _buffer)
{
    return unmapMemory(_buffer._gpuAllocator, _buffer._gpuAllocation);
}

ux3d::gpu::GpuResult ux3d::gpu::mapMemory(const Image& _image, void*& _outMappedData)
{
    return mapMemory(_image._gpuAllocator, _image._gpuAllocation, _outMappedData);
}

ux3d::gpu::GpuResult ux3d::gpu::unmapMemory(const Image& _image)
{
    return unmapMemory(_image._gpuAllocator, _image._gpuAllocation);
}
