// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  PipelineLayout.cpp
//  middleware
//
//  Created by Moritz Becher on 07.06.19.
//

#include "gpu/PipelineLayout.h"

#include <vulkan/vulkan.h>

#include "core/Allocator.h"
#include "core/HeapAllocator.h"
#include "core/LinearAllocator.h"
#include "core/Slice.h"

#include "gpu/impl_vulkan/Result.h"

ux3d::gpu::GpuResult ux3d::gpu::createPipelineLayout(const LogicalDevice& _device, const PipelineLayoutCreateInfo& _createInfo, PipelineLayout& _pipelineLayout, core::IAllocator* _allocator)
{
    core::Slice<VkDescriptorSetLayout> vkDescriptorSetLayouts;
    if (!vkDescriptorSetLayouts.allocate(_createInfo.setLayouts.size(), _allocator)) {
        return GpuError::OutOfHostMemory;
    }

    for (size_t i = 0u; i < vkDescriptorSetLayouts.size(); ++i) {
        vkDescriptorSetLayouts[i] = reinterpret_cast<VkDescriptorSetLayout>(_createInfo.setLayouts[i]._handle);
    }

    core::Slice<VkPushConstantRange> vkPushConstantRanges;
    if (!vkPushConstantRanges.allocate(_createInfo.pushConstantRanges.size(), _allocator)) {
        return GpuError::OutOfHostMemory;
    }

    for (size_t i = 0u; i < vkPushConstantRanges.size(); ++i) {
        vkPushConstantRanges[i].stageFlags = static_cast<VkShaderStageFlags>(_createInfo.pushConstantRanges[i].stageFlags);
        vkPushConstantRanges[i].offset = _createInfo.pushConstantRanges[i].offset;
        vkPushConstantRanges[i].size = _createInfo.pushConstantRanges[i].size;
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(vkDescriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = vkDescriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(vkPushConstantRanges.size());
    pipelineLayoutInfo.pPushConstantRanges = vkPushConstantRanges.data();

    VkPipelineLayout pipelineLayout;
    const VulkanResult result = vkCreatePipelineLayout(reinterpret_cast<VkDevice>(_device._handle), &pipelineLayoutInfo, nullptr, &pipelineLayout);
    if (result) {
        _pipelineLayout._handle = reinterpret_cast<Handle>(pipelineLayout);
        _pipelineLayout._device = _device;
    }

    return result;
}

void ux3d::gpu::destroyPipelineLayout(PipelineLayout& _pipelineLayout)
{
    if (_pipelineLayout._handle != InvalidHandle) {
        vkDestroyPipelineLayout(reinterpret_cast<VkDevice>(_pipelineLayout._device._handle), reinterpret_cast<VkPipelineLayout>(_pipelineLayout._handle), nullptr);
        _pipelineLayout._handle = InvalidHandle;
    }
}
