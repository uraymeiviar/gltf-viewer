// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  RenderPass.cpp
//
//  Created by Moritz Becher on 31.05.19.
//

#include "gpu/RenderPass.h"

#include "core/Slice.h"

#include <vulkan/vulkan.h>

#include "gpu/impl_vulkan/Result.h"

#include "core/Allocator.h"
#include "core/HeapAllocator.h"
#include "core/LinearAllocator.h"

ux3d::gpu::GpuResult ux3d::gpu::createRenderPass(const LogicalDevice& _logicalDevice, const RenderPassCreateInfo& _createInfo, RenderPass& _outRenderPass, core::IAllocator* _allocator)
{
    if (_outRenderPass._handle != InvalidHandle) {
        return GpuError::InvalidHandle;
    }

    core::Slice<VkAttachmentDescription> vkAttachments;

    if (vkAttachments.allocate(_createInfo.attachments.size(), _allocator) == false) {
        return GpuError::OutOfHostMemory;
    }

    for (uint32_t i = 0u; i < vkAttachments.size(); ++i) {
        vkAttachments[i] = {
            {},
            static_cast<VkFormat>(_createInfo.attachments[i].format),
            static_cast<VkSampleCountFlagBits>(_createInfo.attachments[i].samples),
            static_cast<VkAttachmentLoadOp>(_createInfo.attachments[i].loadOp),
            static_cast<VkAttachmentStoreOp>(_createInfo.attachments[i].storeOp),
            static_cast<VkAttachmentLoadOp>(_createInfo.attachments[i].stencilLoadOp),
            static_cast<VkAttachmentStoreOp>(_createInfo.attachments[i].stencilStoreOp),
            static_cast<VkImageLayout>(_createInfo.attachments[i].initialLayout),
            static_cast<VkImageLayout>(_createInfo.attachments[i].finalLayout)
        };
    }

    core::Slice<VkSubpassDescription> vkSubpasses;
    if (vkSubpasses.allocate(_createInfo.subpasses.size(), _allocator) == false) {
        return GpuError::OutOfHostMemory;
    }

    for (uint32_t i = 0u; i < vkSubpasses.size(); ++i) {
        const SubpassDescription& subpass = _createInfo.subpasses[i];
        vkSubpasses[i].flags = static_cast<VkSubpassDescriptionFlags>(subpass.flags);
        vkSubpasses[i].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        vkSubpasses[i].inputAttachmentCount = static_cast<uint32_t>(subpass.inputAttachments.size());
        // we can just cast the element type directly as it has the exact same layout as its vulkan sibling
        vkSubpasses[i].pInputAttachments = reinterpret_cast<VkAttachmentReference*>(subpass.inputAttachments.data());
        vkSubpasses[i].colorAttachmentCount = static_cast<uint32_t>(subpass.colorAttachments.size());
        vkSubpasses[i].pColorAttachments = reinterpret_cast<VkAttachmentReference*>(subpass.colorAttachments.data());
        vkSubpasses[i].pResolveAttachments = reinterpret_cast<VkAttachmentReference*>(subpass.resolveAttachments.data());
        vkSubpasses[i].pDepthStencilAttachment = reinterpret_cast<VkAttachmentReference*>(subpass.depthStencilAttachment.data());
        vkSubpasses[i].preserveAttachmentCount = static_cast<uint32_t>(subpass.preserveAttachments.size());
        vkSubpasses[i].pPreserveAttachments = reinterpret_cast<uint32_t*>(subpass.preserveAttachments.data());
    }

    VkRenderPassCreateInfo vkRenderPassInfo = {};
    vkRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    vkRenderPassInfo.attachmentCount = static_cast<uint32_t>(vkAttachments.size());
    vkRenderPassInfo.pAttachments = vkAttachments.data();
    vkRenderPassInfo.subpassCount = static_cast<uint32_t>(vkSubpasses.size());
    vkRenderPassInfo.pSubpasses = vkSubpasses.data();

    VkRenderPass vkRenderPass;
    const VulkanResult result = vkCreateRenderPass(reinterpret_cast<VkDevice>(_logicalDevice._handle), &vkRenderPassInfo, nullptr, &vkRenderPass);
    if (result) {
        _outRenderPass._handle = reinterpret_cast<Handle>(vkRenderPass);
        _outRenderPass._device = _logicalDevice;
    }

    return result;
}

void ux3d::gpu::destroyRenderPass(RenderPass& _renderPass)
{
    if (_renderPass._handle != InvalidHandle) {
        vkDestroyRenderPass(reinterpret_cast<VkDevice>(_renderPass._device._handle), reinterpret_cast<VkRenderPass>(_renderPass._handle), nullptr);
        _renderPass._handle = InvalidHandle;
    }
}
