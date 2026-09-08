// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "DescriptorSets.h"
#include "Framebuffer.h"
#include "Pipeline.h"
#include "RenderPass.h"
#include "Semaphore.h"

namespace ux3d::gpu {
// forward decls
struct CommandBuffer;
struct PipelineLayout;
struct Buffer;
struct Swapchain;

enum class SubpassContents : uint32_t {
    Inline = 0,
    SecondaryCommandBuffers = 1
};

struct RenderPassBeginInfo {
    RenderPass renderPass;
    Framebuffer framebuffer;
    Rect renderArea;
    core::Vector<ClearValue> clearValues;
};

void draw(
    const CommandBuffer& _CmdBuffer,
    uint32_t vertexCount,
    uint32_t instanceCount,
    uint32_t firstVertex,
    uint32_t firstInstance);

void drawIndexed(
    const CommandBuffer& _CmdBuffer,
    uint32_t indexCount,
    uint32_t instanceCount,
    uint32_t firstIndex,
    int32_t vertexOffset,
    uint32_t firstInstance);

void dispatch(
    const CommandBuffer& _commandBuffer,
    Extent3d _groupCount);

void bindPipeline(const CommandBuffer& _CmdBuffer, const Pipeline& pipeline, PipelineBindPoint bindPoint);

/// Binds the given buffer as a vertex buffer to the given binding number.
void bindVertexBuffer(
    const CommandBuffer& _CmdBuffer,
    uint32_t binding,
    const Buffer& _VertexBuffer,
    uint64_t _Offset);

// TODO: bindVertexBuffers with vector of Buffer and offsets

void bindIndexBuffer(
    const CommandBuffer& _CmdBuffer,
    const Buffer& _indexBuffer,
    uint64_t _byteOffset,
    IndexType _type);

void bindDescriptorSets(
    const CommandBuffer& _CmdBuffer,
    PipelineBindPoint _bindPoint,
    const PipelineLayout& _layout,
    uint32_t _firstSet,
    const core::Vector<DescriptorSet>& _descriptorSets,
    const core::Vector<uint32_t>& _dynamicOffsets);

struct BufferCopyRegion {
    size_t srcOffset;
    size_t dstOffset;
    size_t size;
};

void copyBuffer(
    const CommandBuffer& _CmdBuffer,
    const Buffer& _source,
    const Buffer& _destination,
    const core::Vector<BufferCopyRegion>& _regions);

struct ImageCopyRegion {
    ImageSubresourceLayers srcSubresource;
    Offset3d srcOffset;
    ImageSubresourceLayers dstSubresource;
    Offset3d dstOffset;
    Extent3d extent {};
};

void copyImage(
    const CommandBuffer& _commandBuffer,
    const Image& _srcImage,
    const Image& _dstImage,
    const ImageLayout& _srcLayout,
    const ImageLayout& _dstLayout,
    const core::Vector<ImageCopyRegion>& _regions);

void copyBufferToImage(
    const CommandBuffer& _commandBuffer,
    const Buffer& _srcBuffer,
    const Image& _dstImage,
    const Extent3d& _imageExtent,
    const ImageLayout& _imageLayout,
    const ImageSubresourceLayers& _imageSubresource,
    size_t _bufferOffset);

void imageLayoutTransition(
    const CommandBuffer& _commandBuffer,
    const Image& _image,
    const gpu::ImageSubresourceRange& subresourceRange,
    PipelineStage _srcStage,
    PipelineStage _dstStage,
    Access _srcAccess,
    Access _dstAccess,
    gpu::ImageLayout _oldLayout,
    gpu::ImageLayout _newLayout);

void globalBarrier(
    const CommandBuffer& _commandBuffer,
    PipelineStage _srcStage,
    PipelineStage _dstStage,
    Access _srcAccess,
    Access _dstAccess);

void nextSubpass(const CommandBuffer& _CmdBuffer, SubpassContents _contents);

void beginRenderPass(
    const CommandBuffer& _CmdBuffer,
    const RenderPassBeginInfo& _info,
    SubpassContents _contents);

void endRenderPass(const CommandBuffer& _CmdBuffer);

/// Copies the whole image (first mip, first array image, color aspect).
void copyImageWhole(
    const CommandBuffer& _commandBuffer,
    const gpu::Image& _srcImage,
    const gpu::Image& _dstImage,
    const gpu::ImageLayout& _srcLayout,
    const gpu::ImageLayout& _dstLayout,
    const gpu::Extent3d& _imageExtent);

struct SubmitInfo {
    core::Vector<std::tuple<Semaphore, PipelineStage>> waits;
    core::Vector<Semaphore> signalSemaphores;
    core::Vector<CommandBuffer> commandBuffers;
};

GpuResult submit(const Queue& queue, const SubmitInfo& info);

GpuResult present(
    const Queue& _queue,
    Swapchain& _swapchain,
    uint32_t _idx,
    const Semaphore& _waitSemaphore);

GpuResult waitQueueIdle(const Queue& _queue);

GpuResult waitDeviceIdle(const LogicalDevice& _device);

} // !ux3d
