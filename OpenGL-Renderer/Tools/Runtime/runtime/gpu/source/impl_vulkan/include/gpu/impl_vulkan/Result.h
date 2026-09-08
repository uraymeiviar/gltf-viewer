// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "gpu/Types.h"

#include <vulkan/vulkan.h>

namespace ux3d::gpu {
struct VulkanResult : public GpuResult {
    using GpuResult::GpuResult;

    VulkanResult(const VkResult _result)
    {
        derived = static_cast<ux3d::gpu::GpuError>(_result);
    }
};
}
