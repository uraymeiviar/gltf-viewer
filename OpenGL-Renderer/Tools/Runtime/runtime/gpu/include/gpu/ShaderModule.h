// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  ShaderModule.h
//  middleware
//
//  Created by Moritz Becher on 31.05.19.
//

#pragma once

#include "LogicalDevice.h"

namespace ux3d::gpu {
struct ShaderModuleCreateInfo {
    core::Vector<uint32_t> code;
};

struct ShaderModule {
    Handle _handle = InvalidHandle;
    LogicalDevice _device;
};

GpuResult createShaderModule(
    const LogicalDevice& logicalDevice,
    const ShaderModuleCreateInfo& createInfo,
    ShaderModule& outShaderModule);

void destroyShaderModule(ShaderModule& shaderModule);

gpu::ShaderModule makeModule(
    const gpu::LogicalDevice& _device,
    const core::Vector<uint32_t>& code);
}
