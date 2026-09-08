// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Vector.h"

#include "LogicalDevice.h"

namespace ux3d::gpu {
struct DescriptorSetLayoutBinding {
    uint32_t binding;
    DescriptorType descriptorType;
    uint32_t descriptorCount;
    ShaderStageFlag stageFlags;
    // TODO: pImmutableSamplers
};

struct DescriptorSetLayoutCreateInfo {
    enum class CreateFlagBits : uint32_t {
        None = 0,
        PushDescriptor = 0x1,
        UpdateAfterBind = 0x2,
    };
    using CreateFlag = core::Flag<CreateFlagBits>;
    CreateFlag flags;
    core::Vector<DescriptorSetLayoutBinding> bindings;
};

struct DescriptorSetLayout {
    LogicalDevice _device;
    Handle _handle = InvalidHandle;
};

GpuResult
createDescriptorSetLayout(
    const LogicalDevice& _device,
    const DescriptorSetLayoutCreateInfo& _info,
    DescriptorSetLayout& _outLayout);

void destroyDescriptorSetLayout(
    DescriptorSetLayout& _layout);

}
