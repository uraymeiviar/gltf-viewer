// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  Framebuffer.cpp
//  middleware
//
//  Created by Jim Eckerlein on 7.06.2019.
//

#include "gpu/Framebuffer.h"

#include <vulkan/vulkan.h>

#include "core/HeapAllocator.h"
#include "core/LinearAllocator.h"
#include "core/Logger.h"
#include "core/Slice.h"

#include "gpu/impl_vulkan/Result.h"

namespace ux3d::gpu {
/// Stores all data-slices required for Vulkan.
/// Lives only while creating a new Vulkan framebuffer, dropped after creation.

struct FramebufferCreateData {
    core::Slice<VkImageView> vkAttachments;

    explicit FramebufferCreateData(core::IAllocator* _Allocator)
        : vkAttachments { _Allocator }
    {
    }
};

VulkanResult convert(
    const FramebufferCreateInfo& _info, VkFramebufferCreateInfo& _outVkInfo,
    FramebufferCreateData& _outCreateData,
    core::IAllocator* _allocator)
{
    if (!_outCreateData.vkAttachments.allocate(_info.attachments.size(), _allocator)) {
        return GpuError::OutOfHostMemory;
    }

    for (size_t i = 0u; i < _info.attachments.size(); ++i) {
        _outCreateData.vkAttachments[i] = reinterpret_cast<VkImageView>(_info.attachments[i]._handle);
    }

    // typedef struct VkFramebufferCreateInfo {
    //     VkStructureType             sType;
    //     const void*                 pNext;
    //     VkFramebufferCreateFlags    flags;
    //     VkRenderPass                renderPass;
    //     uint32_t                    attachmentCount;
    //     const VkImageView*          pAttachments;
    //     uint32_t                    width;
    //     uint32_t                    height;
    //     uint32_t                    layers;
    // } VkFramebufferCreateInfo;

    _outVkInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    _outVkInfo.pNext = nullptr;
    _outVkInfo.flags = 0x0;
    _outVkInfo.renderPass = reinterpret_cast<VkRenderPass>(_info.renderPass._handle);
    _outVkInfo.attachmentCount = static_cast<uint32_t>(_outCreateData.vkAttachments.size());
    _outVkInfo.pAttachments = _outCreateData.vkAttachments.data();
    _outVkInfo.width = _info.width;
    _outVkInfo.height = _info.height;
    _outVkInfo.layers = _info.layers;

    return GpuResult::Success;
}
}

ux3d::gpu::GpuResult ux3d::gpu::createFramebuffer(
    const ux3d::gpu::LogicalDevice& _device,
    const ux3d::gpu::FramebufferCreateInfo& _createInfo,
    ux3d::gpu::Framebuffer& _outFramebuffer,
    core::IAllocator* _allocator)
{
    FramebufferCreateData createData { _allocator };

    VkFramebufferCreateInfo vkFramebufferCreateInfo {};
    VulkanResult result = convert(_createInfo, vkFramebufferCreateInfo, createData, _allocator);

    if (!result) {
        return std::move(result);
    }

    VkFramebuffer vkFramebuffer;
    result = vkCreateFramebuffer(
        reinterpret_cast<VkDevice>(_device._handle),
        &vkFramebufferCreateInfo,
        nullptr,
        &vkFramebuffer);

    if (result) {
        _outFramebuffer._handle = reinterpret_cast<Handle>(vkFramebuffer);
        _outFramebuffer._device = _device;
    }

    return std::move(result);
}

void ux3d::gpu::destroyFramebuffer(Framebuffer& _framebuffer)
{
    if (_framebuffer._device._handle != InvalidHandle) {
        vkDestroyFramebuffer(
            reinterpret_cast<VkDevice>(_framebuffer._device._handle),
            reinterpret_cast<VkFramebuffer>(_framebuffer._handle),
            nullptr);
        _framebuffer._handle = InvalidHandle;
    }
}
