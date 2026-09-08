// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
// Created by jim on 9/23/19.
//

#include "gpu/Sampler.h"
#include "gpu/impl_vulkan/Result.h"

#include <vulkan/vulkan.h>

ux3d::gpu::GpuResult
ux3d::gpu::createSampler(
    const ux3d::gpu::LogicalDevice& _device,
    const ux3d::gpu::SamplerCreateInfo& _info,
    ux3d::gpu::Sampler& _outSampler)
{
    VkSamplerCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.flags = _info.flags;
    info.magFilter = static_cast<VkFilter>(_info.magFilter);
    info.minFilter = static_cast<VkFilter>(_info.minFilter);
    info.mipmapMode = static_cast<VkSamplerMipmapMode>(_info.mipmapMode);
    info.addressModeU = static_cast<VkSamplerAddressMode>(_info.addressModeU);
    info.addressModeV = static_cast<VkSamplerAddressMode>(_info.addressModeW);
    info.addressModeW = static_cast<VkSamplerAddressMode>(_info.addressModeV);
    info.mipLodBias = _info.mipLodBias;
    info.anisotropyEnable = _info.anisotropyEnable;
    info.maxAnisotropy = _info.maxAnisotropy;
    info.compareEnable = _info.compareEnable;
    info.compareOp = static_cast<VkCompareOp>(_info.compareOp);
    info.minLod = _info.minLod;
    info.maxLod = _info.maxLod;
    info.borderColor = static_cast<VkBorderColor>(_info.borderColor);
    info.unnormalizedCoordinates = _info.unnormalizedCoordinates;

    const VulkanResult r = vkCreateSampler(
        reinterpret_cast<VkDevice>(_device._handle),
        &info,
        nullptr,
        reinterpret_cast<VkSampler*>(&_outSampler._handle));
    _outSampler._device = _device;
    return r;
}

void ux3d::gpu::destroySampler(
    ux3d::gpu::Sampler& _sampler)
{
    vkDestroySampler(
        reinterpret_cast<VkDevice>(_sampler._handle),
        reinterpret_cast<VkSampler>(_sampler._handle),
        nullptr);
}
