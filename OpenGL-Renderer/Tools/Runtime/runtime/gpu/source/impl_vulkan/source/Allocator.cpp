// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  Allocator.cpp
//
//  Created by Moritz Becher on 23.05.19.
//

#include "gpu/Allocator.h"
#include "gpu/LogicalDevice.h"
#include "gpu/PhysicalDevice.h"

#include "gpu/impl_vulkan/Result.h"

#include <vk_mem_alloc.h>

ux3d::gpu::GpuResult ux3d::gpu::createGPUAllocator(
    const PhysicalDevice& _PhysicalDevice,
    const LogicalDevice& _LogicalDevice,
    GPUAllocator& _OutGPUAllocator)
{
    if (_OutGPUAllocator._handle != InvalidHandle) {
        return GpuError::InvalidHandle;
    }

    VmaAllocatorCreateInfo vmaCreateInfo {};
    vmaCreateInfo.physicalDevice = reinterpret_cast<VkPhysicalDevice>(_PhysicalDevice._handle);
    vmaCreateInfo.device = reinterpret_cast<VkDevice>(_LogicalDevice._handle);

    VmaAllocator vmaAllocator;
    const VulkanResult result = vmaCreateAllocator(&vmaCreateInfo, &vmaAllocator);

    if (result) {
        _OutGPUAllocator._handle = reinterpret_cast<Handle>(vmaAllocator);
    }

    return result;
}

void ux3d::gpu::destroyGPUAllocator(GPUAllocator& _gpuAllocator)
{
    if (_gpuAllocator._handle != InvalidHandle) {
        vmaDestroyAllocator(reinterpret_cast<VmaAllocator>(_gpuAllocator._handle));
        _gpuAllocator._handle = InvalidHandle;
    }
}
