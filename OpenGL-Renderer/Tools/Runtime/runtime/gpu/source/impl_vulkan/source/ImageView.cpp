// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  ImageView.cpp
//  middleware
//
//  Created by Jim Eckerlein on 7.06.2019.
//

#include <vulkan/vulkan.h>

#include "gpu/ImageView.h"
#include "gpu/impl_vulkan/Result.h"

namespace ux3d::gpu {
void convert(const ImageViewCreateInfo& _info, VkImageViewCreateInfo& _vkInfo)
{
    _vkInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    _vkInfo.pNext = nullptr;
    _vkInfo.flags = static_cast<VkImageViewCreateFlags>(_info.flags);
    _vkInfo.image = reinterpret_cast<VkImage>(_info.image._handle);
    _vkInfo.viewType = static_cast<VkImageViewType>(_info.viewType);
    _vkInfo.format = static_cast<VkFormat>(_info.format);
    _vkInfo.components.r = static_cast<VkComponentSwizzle>(_info.componentMapping.r);
    _vkInfo.components.g = static_cast<VkComponentSwizzle>(_info.componentMapping.g);
    _vkInfo.components.b = static_cast<VkComponentSwizzle>(_info.componentMapping.b);
    _vkInfo.components.a = static_cast<VkComponentSwizzle>(_info.componentMapping.a);
    _vkInfo.subresourceRange.aspectMask = static_cast<VkImageAspectFlags>(_info.subresourceRange.aspectMask);
    _vkInfo.subresourceRange.baseArrayLayer = _info.subresourceRange.baseLayer;
    _vkInfo.subresourceRange.layerCount = _info.subresourceRange.layerCount;
    _vkInfo.subresourceRange.baseMipLevel = _info.subresourceRange.baseMip;
    _vkInfo.subresourceRange.levelCount = _info.subresourceRange.mipCount;
}
}

ux3d::gpu::GpuResult ux3d::gpu::createImageView(
    const LogicalDevice& _device,
    const ImageViewCreateInfo& _createInfo,
    ImageView& _outImageView)
{
    VkImageViewCreateInfo vkInfo {};
    convert(_createInfo, vkInfo);
    VkImageView vkImageView;

    const VulkanResult result = vkCreateImageView(
        reinterpret_cast<VkDevice>(_device._handle),
        &vkInfo,
        nullptr,
        &vkImageView);

    if (result) {
        _outImageView._handle = reinterpret_cast<Handle>(vkImageView);
        _outImageView._device = _device;
    }

    return result;
}

void ux3d::gpu::destroyImageView(
    ImageView& _imageView)
{
    if (_imageView._handle != InvalidHandle) {
        vkDestroyImageView(
            reinterpret_cast<VkDevice>(_imageView._device._handle),
            reinterpret_cast<VkImageView>(_imageView._handle),
            nullptr);

        _imageView._handle = InvalidHandle;
    }
}
