// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  RenderPass.h
//  middleware
//
//  Created by Moritz Becher on 31.05.19.
//

#pragma once

#include "LogicalDevice.h"

namespace ux3d::gpu {
// Attachments

enum class AttachmentLoadOp : uint32_t {
    Load = 0u,
    Clear = 1u,
    DontCare = 2u,
};

enum class AttachmentStoreOp : uint32_t {
    Store = 0u,
    DontCare = 1u,
};

struct AttachmentDescription {
    enum class CreateFlagBits : uint32_t {
        None = 0,
        MayAlias = 0x00000001u,
    };

    using CreateFlag = core::Flag<CreateFlagBits>;

    CreateFlag flags;
    Format format;
    SampleCount samples;
    AttachmentLoadOp loadOp;
    AttachmentStoreOp storeOp;
    AttachmentLoadOp stencilLoadOp;
    AttachmentStoreOp stencilStoreOp;
    ImageLayout initialLayout;
    ImageLayout finalLayout;
};

// Subpass

struct AttachmentReference {
    uint32_t attachment;
    ImageLayout layout;
};

enum class SubpassDescriptionFlags : uint32_t {
};

struct SubpassDescription {
    SubpassDescriptionFlags flags;
    core::Vector<AttachmentReference> inputAttachments;
    core::Vector<AttachmentReference> colorAttachments;
    core::Vector<AttachmentReference> resolveAttachments;
    core::Vector<AttachmentReference> depthStencilAttachment;
    core::Vector<AttachmentReference> preserveAttachments;
};

// RenderPass

struct RenderPassCreateInfo {
    core::Vector<AttachmentDescription> attachments;
    core::Vector<SubpassDescription> subpasses;
};

struct RenderPass {
    Handle _handle = InvalidHandle;
    LogicalDevice _device;
};

GpuResult createRenderPass(
    const LogicalDevice& logicalDevice,
    const RenderPassCreateInfo& createInfo,
    RenderPass& outRenderPass,
    core::IAllocator* allocator);

void destroyRenderPass(RenderPass& renderPass);
}
