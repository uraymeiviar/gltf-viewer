// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  ShaderModule.cpp
//  middleware
//
//  Created by Moritz Becher on 31.05.19.
//

#include "gpu/ShaderModule.h"

#include "core/Allocator.h"
#include "core/HeapAllocator.h"
#include "core/LinearAllocator.h"

#include <vulkan/vulkan.h>

#include "gpu/impl_vulkan/Result.h"

ux3d::gpu::GpuResult ux3d::gpu::createShaderModule(
    const LogicalDevice& _logicalDevice,
    const ShaderModuleCreateInfo& _createInfo,
    ShaderModule& _outShaderModule)
{
    if (_outShaderModule._handle != InvalidHandle) {
        return GpuError::InvalidHandle;
    }

    VkShaderModuleCreateInfo vkCreateInfo = {};
    vkCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vkCreateInfo.codeSize = _createInfo.code.size() * sizeof(uint32_t);
    vkCreateInfo.pCode = reinterpret_cast<uint32_t*>(_createInfo.code.data());

    VkShaderModule vkShaderModule;
    const VulkanResult result = vkCreateShaderModule(reinterpret_cast<VkDevice>(_logicalDevice._handle), &vkCreateInfo,
        nullptr, &vkShaderModule);
    if (result) {
        _outShaderModule._handle = reinterpret_cast<Handle>(vkShaderModule);
        _outShaderModule._device = _logicalDevice;
    }

    return result;
}

void ux3d::gpu::destroyShaderModule(ShaderModule& _shaderModule)
{
    if (_shaderModule._handle != InvalidHandle) {
        vkDestroyShaderModule(reinterpret_cast<VkDevice>(_shaderModule._device._handle),
            reinterpret_cast<VkShaderModule>(_shaderModule._handle), nullptr);
        _shaderModule._handle = InvalidHandle;
    }
}

ux3d::gpu::ShaderModule
ux3d::gpu::makeModule(
    const gpu::LogicalDevice& _device,
    const core::Vector<uint32_t>& code)
{
    gpu::ShaderModuleCreateInfo info {};
    info.code.reserve(code.size(), core::HeapAllocator::defaultInstance());
    info.code.assign(code.begin(), code.end(), core::HeapAllocator::defaultInstance());
    gpu::ShaderModule module;
    gpu::createShaderModule(_device, info, module);
    return module;
}
