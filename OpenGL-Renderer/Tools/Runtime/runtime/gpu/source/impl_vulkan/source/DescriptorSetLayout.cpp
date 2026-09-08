// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gpu/DescriptorSetLayout.h"
#include "core/HeapAllocator.h"
#include "gpu/impl_vulkan/Result.h"

#include <vulkan/vulkan.h>

ux3d::gpu::GpuResult
ux3d::gpu::createDescriptorSetLayout(
    const LogicalDevice& _device,
    const DescriptorSetLayoutCreateInfo& _info,
    DescriptorSetLayout& _outLayout)
{
    core::Vector<VkDescriptorSetLayoutBinding> bindings;
    if (false == bindings.reserve(_info.bindings.size(), core::HeapAllocator::defaultInstance())) {
        return GpuError::OutOfHostMemory;
    }
    for (auto const& binding : _info.bindings) {
        (void)bindings.emplace_back(VkDescriptorSetLayoutBinding {
            binding.binding,
            static_cast<VkDescriptorType>(binding.descriptorType),
            binding.descriptorCount,
            static_cast<VkShaderStageFlags>(binding.stageFlags),
            nullptr });
    }

    VkDescriptorSetLayoutCreateInfo info {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        {},
        _info.flags,
        static_cast<VkDescriptorSetLayoutCreateFlags>(bindings.size()),
        bindings.data()
    };

    VulkanResult const r = vkCreateDescriptorSetLayout(
        reinterpret_cast<VkDevice>(_device._handle),
        &info,
        nullptr,
        reinterpret_cast<VkDescriptorSetLayout*>(&_outLayout._handle));
    _outLayout._device = _device;
    return r;
}

void ux3d::gpu::destroyDescriptorSetLayout(
    DescriptorSetLayout& _layout)
{
    vkDestroyDescriptorSetLayout(
        reinterpret_cast<VkDevice>(_layout._device._handle),
        reinterpret_cast<VkDescriptorSetLayout>(_layout._handle),
        nullptr);
}
