// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  PipelineLayout.h
//  middleware
//
//  Created by Moritz Becher on 07.06.19.
//

#pragma once

#include "DescriptorSetLayout.h"
#include "LogicalDevice.h"

namespace ux3d::gpu {
// Pipeline

struct PipelineLayout {
    Handle _handle = InvalidHandle;
    LogicalDevice _device;
};

struct PushConstantRange {
    ShaderStageFlag stageFlags;
    uint32_t offset;
    uint32_t size;
};

struct PipelineLayoutCreateInfo {
    core::Vector<DescriptorSetLayout> setLayouts;
    core::Vector<PushConstantRange> pushConstantRanges;
};

GpuResult createPipelineLayout(
    const LogicalDevice& device,
    const PipelineLayoutCreateInfo& createInfo,
    PipelineLayout& pipelineLayout,
    core::IAllocator* allocator);

void destroyPipelineLayout(PipelineLayout& pipelineLayout);
} // !ux3d
