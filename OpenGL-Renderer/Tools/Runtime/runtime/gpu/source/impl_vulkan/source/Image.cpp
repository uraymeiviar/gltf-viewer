// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  Image.cpp
//  middleware
//
//  Created by Jim Eckerlein on 7.06.2019.
//

#include "gpu/Image.h"
#include "gpu/Buffer.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "core/Allocator.h"
#include "core/HeapAllocator.h"
#include "core/LinearAllocator.h"
#include "core/Logger.h"
#include "core/Slice.h"

#include "gpu/impl_vulkan/Result.h"

namespace ux3d::gpu {
/// Stores all data-slices required for Vulkan.
/// Lives only while creating a new Vulkan image, dropped after creation.

struct VkImageCreateInfoData {
    core::Slice<uint32_t> vkQueueFamilyIndices;
};

VulkanResult convert(
    const ImageCreateInfo& _info,
    VkImageCreateInfo& _outCreateInfo,
    VkImageCreateInfoData& _createInfoData,
    core::IAllocator* _Allocator)
{
    if (!_createInfoData.vkQueueFamilyIndices.assign(_info.queueFamilyIndices.begin(),
            _info.queueFamilyIndices.end(), _Allocator)) {
        return GpuError::OutOfHostMemory;
    }

    // typedef struct VkImageCreateInfo {
    //     VkStructureType          sType;
    //     const void*              pNext;
    //     VkImageCreateFlags       flags;
    //     VkImageType              imageType;
    //     VkFormat                 format;
    //     VkExtent3D               extent;
    //     uint32_t                 mipLevels;
    //     uint32_t                 arrayLayers;
    //     VkSampleCountFlagBits    samples;
    //     VkImageTiling            tiling;
    //     VkImageUsageFlags        usage;
    //     VkSharingMode            sharingMode;
    //     uint32_t                 queueFamilyIndexCount;
    //     const uint32_t*          pQueueFamilyIndices;
    //     VkImageLayout            initialLayout;
    // } VkImageCreateInfo;

    _outCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    _outCreateInfo.pNext = nullptr;
    _outCreateInfo.flags = static_cast<VkImageCreateFlags>(_info.flags);
    _outCreateInfo.imageType = static_cast<VkImageType>(_info.type);
    _outCreateInfo.format = static_cast<VkFormat>(_info.format);
    _outCreateInfo.extent.width = _info.extent.width;
    _outCreateInfo.extent.height = _info.extent.height;
    _outCreateInfo.extent.depth = _info.extent.depth;
    _outCreateInfo.mipLevels = _info.mipLevels;
    _outCreateInfo.arrayLayers = _info.arrayLayers;
    _outCreateInfo.samples = static_cast<VkSampleCountFlagBits>(_info.sampleCount);
    _outCreateInfo.tiling = static_cast<VkImageTiling>(_info.tiling);
    _outCreateInfo.usage = static_cast<VkImageUsageFlags>(_info.usage);
    _outCreateInfo.sharingMode = static_cast<VkSharingMode>(_info.sharingMode);
    _outCreateInfo.queueFamilyIndexCount = static_cast<uint32_t>(_createInfoData.vkQueueFamilyIndices.size());
    _outCreateInfo.pQueueFamilyIndices = _createInfoData.vkQueueFamilyIndices.data();
    _outCreateInfo.initialLayout = static_cast<VkImageLayout>(_info.initialLayout);

    return GpuResult::Success;
}
}

ux3d::gpu::GpuResult ux3d::gpu::createImage(
    ux3d::gpu::GPUAllocator& _gpuAllocator,
    const ux3d::gpu::ImageCreateInfo& _createInfo,
    ux3d::gpu::Image& _outImage,
    const ux3d::gpu::AllocationCreateInfo& _allocationCreateInfo,
    core::IAllocator* _allocator)
{
    auto vmaAllocator = reinterpret_cast<VmaAllocator>(_gpuAllocator._handle);
    VkImageCreateInfoData createData;
    VkImageCreateInfo vkImageCreateInfo {};

    VulkanResult result = convert(_createInfo, vkImageCreateInfo, createData, _allocator);

    if (!result) {
        return std::move(result);
    }

    VmaAllocationInfo vmaAllocationInfo {};

    // TODO: fill vmaAllocationCreateInfo
    VmaAllocationCreateInfo vmaAllocationCreateInfo {};
    vmaAllocationCreateInfo.usage = static_cast<VmaMemoryUsage>(_allocationCreateInfo.usage);

    VkImage vkImage;
    VmaAllocation vmaAllocation;
    result = vmaCreateImage(vmaAllocator, &vkImageCreateInfo, &vmaAllocationCreateInfo, &vkImage,
        &vmaAllocation, &vmaAllocationInfo);
    if (result) {
        _outImage._handle = reinterpret_cast<Handle>(vkImage);
        _outImage._gpuAllocator = _gpuAllocator;
        _outImage._gpuAllocation._handle = reinterpret_cast<Handle>(vmaAllocation);
    } else {
        core::logWarning("could not create vulkan image");
    }

    return std::move(result);
}

void ux3d::gpu::destroyImage(ux3d::gpu::Image& _image)
{
    if (_image._handle != InvalidHandle) {
        vmaDestroyImage(
            reinterpret_cast<VmaAllocator>(_image._gpuAllocator._handle),
            reinterpret_cast<VkImage>(_image._handle),
            reinterpret_cast<VmaAllocation>(_image._gpuAllocation._handle));

        _image._handle = InvalidHandle;
    }
}
