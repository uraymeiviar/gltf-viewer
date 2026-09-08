// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "Buffer.h"
#include "LogicalDevice.h"

namespace ux3d::gpu {
struct BufferViewCreateInfo {
    enum class CreateFlagBits : uint32_t {
        None = 0,
    };
    using CreateFlag = core::Flag<CreateFlagBits>;

    CreateFlag flags;
    Buffer buffer;
    Format format;
    size_t offset;
    size_t range;
};

struct BufferView {
    Handle _handle = InvalidHandle;
    LogicalDevice _device;
};

GpuResult createBufferView(
    const LogicalDevice& device,
    const BufferViewCreateInfo& createInfo,
    BufferView& _outBufferView);

void destroyBufferView(
    BufferView& _BufferView);
}
