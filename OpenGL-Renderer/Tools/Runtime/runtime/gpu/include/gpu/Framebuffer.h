// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  Framebuffer.h
//  middleware
//
//  Created by Jim Eckerlein on 7.06.2019.
//

#pragma once

#include "core/Vector.h"

#include "ImageView.h"
#include "RenderPass.h"

namespace ux3d::gpu {
struct FramebufferCreateInfo {
    RenderPass renderPass;
    core::Vector<ImageView> attachments;
    uint32_t width {};
    uint32_t height {};
    uint32_t layers {};
};

struct Framebuffer {
    Handle _handle = InvalidHandle;
    LogicalDevice _device;
};

GpuResult createFramebuffer(
    const ux3d::gpu::LogicalDevice& device,
    const ux3d::gpu::FramebufferCreateInfo& createInfo,
    ux3d::gpu::Framebuffer& outFramebuffer,
    core::IAllocator* allocator);

void destroyFramebuffer(
    Framebuffer& framebuffer);
}
