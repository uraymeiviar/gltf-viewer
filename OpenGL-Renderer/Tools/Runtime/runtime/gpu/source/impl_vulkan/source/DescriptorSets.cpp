// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  DescriptorSets.cpp
//  middleware
//
//  Created by Moritz Becher on 13.06.19.
//

#include "core/HeapAllocator.h"
#include "core/LinearAllocator.h"
#include "core/Slice.h"

#include "gpu/DescriptorSets.h"
#include "gpu/LogicalDevice.h"
#include "gpu/impl_vulkan/Result.h"

#include <vulkan/vulkan.h>

namespace ux3d::gpu {
struct VkDescriptorSetAllocateInfoData {
    core::Vector<VkDescriptorSetLayout> vkDescriptorSets;
};

VulkanResult convert(
    const DescriptorSetAllocateInfo& _allocInfo,
    VkDescriptorSetAllocateInfo& _vkAllocInfo,
    VkDescriptorSetAllocateInfoData& _data,
    core::IAllocator* _allocator)
{
    _vkAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    _vkAllocInfo.descriptorPool = reinterpret_cast<VkDescriptorPool>(_allocInfo.descriptorPool._handle);

    if (!_data.vkDescriptorSets.reserve(_allocInfo.setLayouts.size(), _allocator)) {
        return GpuError::OutOfHostMemory;
    }

    for (const auto& setLayout : _allocInfo.setLayouts) {
        (void)_data.vkDescriptorSets.emplace_back(reinterpret_cast<VkDescriptorSetLayout>(setLayout._handle));
    }

    _vkAllocInfo.descriptorSetCount = static_cast<uint32_t>(_data.vkDescriptorSets.size());
    _vkAllocInfo.pSetLayouts = _data.vkDescriptorSets.data();

    return GpuResult::Success;
}
}

ux3d::gpu::GpuResult ux3d::gpu::allocateDescriptorSets(
    const LogicalDevice& _device,
    const DescriptorSetAllocateInfo& _allocationInfo,
    core::Vector<DescriptorSet>& _descriptorSets,
    core::IAllocator* _allocator)
{
    VkDescriptorSetAllocateInfoData vkData;
    VkDescriptorSetAllocateInfo vkAllocInfo {};

    VulkanResult result = convert(_allocationInfo, vkAllocInfo, vkData, _allocator);

    if (!result) {
        return std::move(result);
    }

    core::Slice<VkDescriptorSet> vkDescriptorSets;

    if (!vkDescriptorSets.allocate(vkAllocInfo.descriptorSetCount, _allocator)) {
        return GpuError::OutOfHostMemory;
    }

    result = vkAllocateDescriptorSets(reinterpret_cast<VkDevice>(_device._handle), &vkAllocInfo,
        vkDescriptorSets.data());

    if (!result) {
        return std::move(result);
    }

    if (!_descriptorSets.reserve(vkDescriptorSets.size(), _allocator)) {
        return GpuError::OutOfHostMemory;
    }

    for (const VkDescriptorSet& vkDescriptorSet : vkDescriptorSets) {
        _descriptorSets.emplace_back()->_handle = reinterpret_cast<Handle>(vkDescriptorSet);
    }

    return std::move(result);
}

ux3d::gpu::GpuResult ux3d::gpu::updateDescriptorSets(
    const LogicalDevice& _device,
    const core::Vector<WriteDescriptorSet>& _writeSets,
    const core::Vector<CopyDescriptorSet>& _copySets,
    core::IAllocator* _allocator)
{
    core::Vector<VkWriteDescriptorSet> writeSets(_allocator);

    if (!writeSets.reserve(_writeSets.size(), _allocator)) {
        return GpuError::OutOfHostMemory;
    }

    size_t uImageCount = 0u;
    size_t uBufferCount = 0u;

    for (const auto& writeSet : _writeSets) {
        uImageCount += writeSet.images.size();
        uBufferCount += writeSet.buffers.size();
    }

    core::Vector<VkDescriptorImageInfo> vkimages;
    core::Vector<VkDescriptorBufferInfo> vkbuffers;

    if (!vkimages.reserve(uImageCount, _allocator) || !vkbuffers.reserve(uBufferCount, _allocator)) {
        return GpuError::OutOfHostMemory;
    }

    for (const WriteDescriptorSet& writeSet : _writeSets) {
        VkWriteDescriptorSet* pSet = writeSets.emplace_back();
        pSet->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        pSet->pNext = nullptr;

        pSet->dstSet = reinterpret_cast<VkDescriptorSet>(writeSet.dstSet._handle);
        pSet->dstBinding = writeSet.dstBinding;
        pSet->descriptorType = static_cast<VkDescriptorType>(writeSet.descriptorType);
        pSet->dstArrayElement = writeSet.dstArrayElement;
        pSet->pTexelBufferView = nullptr; // TODO: implement texbuffers

        if (!writeSet.images.empty()) {
            pSet->pImageInfo = &vkimages.data()[vkimages.size()];
            for (const DescriptorImageInfo& image : writeSet.images) {
                VkDescriptorImageInfo* pVkImage = vkimages.emplace_back();
                pVkImage->imageLayout = static_cast<VkImageLayout>(image.imageLayout);
                pVkImage->imageView = reinterpret_cast<VkImageView>(image.imageView._handle);
                pVkImage->sampler = reinterpret_cast<VkSampler>(image.sampler._handle);
            }
            pSet->descriptorCount = static_cast<uint32_t>(vkimages.size());
        } else if (!writeSet.buffers.empty()) {
            pSet->pBufferInfo = &vkbuffers.data()[vkbuffers.size()];
            for (const DescriptorBufferInfo& buffer : writeSet.buffers) {
                VkDescriptorBufferInfo* pVkBuffer = vkbuffers.emplace_back();
                pVkBuffer->buffer = reinterpret_cast<VkBuffer>(buffer.buffer._handle);
                pVkBuffer->offset = buffer.offset;
                pVkBuffer->range = buffer.range;
            }
            // TODO: This does not work with multiple descriptors in one binding.
            pSet->descriptorCount = static_cast<uint32_t>(vkbuffers.size());
        }
    }

    core::Vector<VkCopyDescriptorSet> copySets;

    if (!copySets.reserve(_copySets.size(), _allocator)) {
        return GpuError::OutOfHostMemory;
    }

    for (const CopyDescriptorSet& copy : _copySets) {
        VkCopyDescriptorSet* pSet = copySets.emplace_back();
        pSet->sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET;
        pSet->pNext = nullptr;

        pSet->srcSet = reinterpret_cast<VkDescriptorSet>(copy.srcSet._handle);
        pSet->srcBinding = copy.srcBinding;
        pSet->srcArrayElement = copy.srcArrayElement;
        pSet->dstSet = reinterpret_cast<VkDescriptorSet>(copy.dstSet._handle);
        pSet->dstBinding = copy.dstBinding;
        pSet->dstArrayElement = copy.dstArrayElement;
        pSet->descriptorCount = copy.descriptorCount;
    }

    vkUpdateDescriptorSets(reinterpret_cast<VkDevice>(_device._handle),
        static_cast<uint32_t>(writeSets.size()), writeSets.data(),
        static_cast<uint32_t>(copySets.size()), copySets.data());

    return GpuResult::Success;
}
