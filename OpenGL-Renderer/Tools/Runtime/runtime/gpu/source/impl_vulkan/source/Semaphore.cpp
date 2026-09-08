// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  Semaphore.cpp
//
//
//  Created by Moritz Becher on 13.06.19.
//

#include "gpu/Semaphore.h"

#include <vulkan/vulkan.h>

#include "gpu/impl_vulkan/Result.h"

ux3d::gpu::GpuResult ux3d::gpu::createSemaphore(const LogicalDevice& _device, Semaphore& _semaphore)
{
    VkSemaphoreCreateInfo vkCreateInfo {};
    vkCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkSemaphore vkSemaphore;
    const VulkanResult result = vkCreateSemaphore(reinterpret_cast<VkDevice>(_device._handle), &vkCreateInfo, nullptr,
        &vkSemaphore);
    if (result) {
        _semaphore._handle = reinterpret_cast<uintptr_t>(vkSemaphore);
        _semaphore._device = _device;
    }

    return result;
}

void ux3d::gpu::destroySemaphore(Semaphore& _semaphore)
{
    if (_semaphore._handle != InvalidHandle) {
        vkDestroySemaphore(reinterpret_cast<VkDevice>(_semaphore._device._handle),
            reinterpret_cast<VkSemaphore>(_semaphore._handle), nullptr);
        _semaphore._handle = InvalidHandle;
    }
}
