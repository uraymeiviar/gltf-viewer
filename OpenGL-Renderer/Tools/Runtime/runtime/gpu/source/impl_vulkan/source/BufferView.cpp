// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gpu/BufferView.h"

#include <vulkan/vulkan.h>

#include "gpu/impl_vulkan/Result.h"

ux3d::gpu::GpuResult
ux3d::gpu::createBufferView(
    const LogicalDevice& _device,
    const BufferViewCreateInfo& _createInfo,
    ux3d::gpu::BufferView& _outBufferView)
{
    VkBufferViewCreateInfo vkInfo {};
    vkInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    vkInfo.flags = _createInfo.flags;
    vkInfo.buffer = reinterpret_cast<VkBuffer>(_createInfo.buffer._handle);
    vkInfo.format = static_cast<VkFormat>(_createInfo.format);
    vkInfo.offset = static_cast<VkDeviceSize>(_createInfo.offset);
    vkInfo.range = static_cast<VkDeviceSize>(_createInfo.range);

    VkBufferView vkBufferView;

    const VulkanResult result = vkCreateBufferView(
        reinterpret_cast<VkDevice>(_device._handle),
        &vkInfo,
        nullptr,
        &vkBufferView);

    if (result) {
        _outBufferView._handle = reinterpret_cast<Handle>(vkBufferView);
        _outBufferView._device = _device;
    }

    return result;
}

void ux3d::gpu::destroyBufferView(ux3d::gpu::BufferView& _BufferView)
{
    if (_BufferView._handle == InvalidHandle) {
        return;
    }

    vkDestroyBufferView(
        reinterpret_cast<VkDevice>(_BufferView._device._handle),
        reinterpret_cast<VkBufferView>(_BufferView._handle),
        nullptr);

    _BufferView._handle = InvalidHandle;
}
