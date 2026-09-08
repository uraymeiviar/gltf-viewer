// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gpu/Commands.h"
#include "gpu/Buffer.h"
#include "gpu/CommandBuffer.h"
#include "gpu/PipelineLayout.h"
#include "gpu/Queue.h"
#include "gpu/Swapchain.h"

#include "core/Allocator.h"
#include "core/HeapAllocator.h"
#include "core/LinearAllocator.h"

#include <vulkan/vulkan.h>

#include "gpu/impl_vulkan/Result.h"

void ux3d::gpu::draw(
    const CommandBuffer& _Buffer,
    uint32_t vertexCount,
    uint32_t instanceCount,
    uint32_t firstVertex,
    uint32_t firstInstance)
{
    vkCmdDraw(reinterpret_cast<VkCommandBuffer>(_Buffer._handle), vertexCount, instanceCount, firstVertex,
        firstInstance);
}

void ux3d::gpu::drawIndexed(
    const CommandBuffer& _Buffer,
    uint32_t indexCount,
    uint32_t instanceCount,
    uint32_t firstIndex,
    int32_t vertexOffset,
    uint32_t firstInstance)
{
    vkCmdDrawIndexed(reinterpret_cast<VkCommandBuffer>(_Buffer._handle), indexCount, instanceCount, firstIndex,
        vertexOffset, firstInstance);
}

void ux3d::gpu::dispatch(
    const CommandBuffer& _commandBuffer,
    gpu::Extent3d _groupCount)
{
    vkCmdDispatch(
        reinterpret_cast<VkCommandBuffer>(_commandBuffer._handle),
        _groupCount.width,
        _groupCount.height,
        _groupCount.depth);
}

void ux3d::gpu::bindPipeline(const CommandBuffer& _CmdBuffer, const Pipeline& pipeline, PipelineBindPoint bindPoint)
{
    vkCmdBindPipeline(
        reinterpret_cast<VkCommandBuffer>(_CmdBuffer._handle),
        VkPipelineBindPoint(bindPoint),
        reinterpret_cast<VkPipeline>(pipeline._handle));
}

void ux3d::gpu::bindVertexBuffer(
    const CommandBuffer& _CmdBuffer,
    uint32_t binding,
    const Buffer& _VertexBuffer,
    uint64_t _Offset)
{
    const VkBuffer buffer = reinterpret_cast<VkBuffer>(_VertexBuffer._handle);
    vkCmdBindVertexBuffers(reinterpret_cast<VkCommandBuffer>(_CmdBuffer._handle), binding, 1u, &buffer, &_Offset);
}

void ux3d::gpu::bindIndexBuffer(
    const CommandBuffer& _CmdBuffer,
    const Buffer& _indexBuffer,
    const uint64_t _byteOffset,
    const IndexType _type)
{
    vkCmdBindIndexBuffer(reinterpret_cast<VkCommandBuffer>(_CmdBuffer._handle),
        reinterpret_cast<VkBuffer>(_indexBuffer._handle), _byteOffset, static_cast<VkIndexType>(_type));
}

void ux3d::gpu::bindDescriptorSets(
    const CommandBuffer& _CmdBuffer,
    const PipelineBindPoint _bindPoint,
    const PipelineLayout& _layout,
    const uint32_t _firstSet,
    const core::Vector<DescriptorSet>& _descriptorSets,
    const core::Vector<uint32_t>& _dynamicOffsets)
{
    static_assert(sizeof(DescriptorSet) == sizeof(VkDescriptorSet),
        "_descriptorSets value type does not match vulkan handle size"); // check if reinterpret_cast<const VkDescriptorSet*>(_descriptorSets.data()) is valid
    vkCmdBindDescriptorSets(
        reinterpret_cast<VkCommandBuffer>(_CmdBuffer._handle),
        static_cast<VkPipelineBindPoint>(_bindPoint),
        reinterpret_cast<VkPipelineLayout>(_layout._handle),
        _firstSet,
        static_cast<uint32_t>(_descriptorSets.size()),
        reinterpret_cast<const VkDescriptorSet*>(_descriptorSets.data()),
        static_cast<uint32_t>(_dynamicOffsets.size()),
        _dynamicOffsets.data());
}

void ux3d::gpu::copyBuffer(
    const CommandBuffer& _CmdBuffer,
    const Buffer& _source,
    const Buffer& _destination,
    const core::Vector<BufferCopyRegion>& _regions)
{
    static_assert(sizeof(BufferCopyRegion) == sizeof(VkBufferCopy));
    // TODO: check if reinterpret_cast<const VkDescriptorSet*>(_descriptorSets.data()) is valid
    vkCmdCopyBuffer(reinterpret_cast<VkCommandBuffer>(_CmdBuffer._handle), reinterpret_cast<VkBuffer>(_source._handle),
        reinterpret_cast<VkBuffer>(_destination._handle), static_cast<uint32_t>(_regions.size()),
        reinterpret_cast<const VkBufferCopy*>(_regions.data()));
}

void ux3d::gpu::copyImage(
    const CommandBuffer& _commandBuffer,
    const Image& _srcImage,
    const Image& _dstImage,
    const ImageLayout& _srcLayout,
    const ImageLayout& _dstLayout,
    const core::Vector<ImageCopyRegion>& _regions)
{
    static_assert(sizeof(ImageCopyRegion) == sizeof(VkImageCopy));
    vkCmdCopyImage(
        reinterpret_cast<VkCommandBuffer>(_commandBuffer._handle),
        reinterpret_cast<VkImage>(_srcImage._handle),
        static_cast<VkImageLayout>(_srcLayout),
        reinterpret_cast<VkImage>(_dstImage._handle),
        static_cast<VkImageLayout>(_dstLayout),
        static_cast<uint32_t>(_regions.size()),
        reinterpret_cast<const VkImageCopy*>(_regions.data()));
}

void ux3d::gpu::copyBufferToImage(
    const CommandBuffer& _commandBuffer,
    const Buffer& _srcBuffer,
    const Image& _dstImage,
    const Extent3d& _imageExtent,
    const ImageLayout& _imageLayout,
    const ImageSubresourceLayers& _imageSubresource,
    size_t _bufferOffset)
{
    VkBufferImageCopy copy {};
    copy.bufferRowLength = 0;
    copy.bufferImageHeight = 0;
    copy.bufferOffset = _bufferOffset;
    copy.imageSubresource.aspectMask = _imageSubresource.aspectMask;
    copy.imageSubresource.layerCount = _imageSubresource.layerCount;
    copy.imageSubresource.mipLevel = _imageSubresource.mip;
    copy.imageSubresource.baseArrayLayer = _imageSubresource.baseLayer;
    copy.imageExtent.width = _imageExtent.width;
    copy.imageExtent.height = _imageExtent.height;
    copy.imageExtent.depth = _imageExtent.depth;

    vkCmdCopyBufferToImage(
        reinterpret_cast<VkCommandBuffer>(_commandBuffer._handle),
        reinterpret_cast<VkBuffer>(_srcBuffer._handle),
        reinterpret_cast<VkImage>(_dstImage._handle),
        static_cast<VkImageLayout>(_imageLayout),
        1, &copy);
}

void ux3d::gpu::imageLayoutTransition(
    const CommandBuffer& _commandBuffer,
    const Image& _image,
    const gpu::ImageSubresourceRange& subresourceRange,
    PipelineStage _srcStage,
    PipelineStage _dstStage,
    Access _srcAccess,
    Access _dstAccess,
    gpu::ImageLayout _oldLayout,
    gpu::ImageLayout _newLayout)
{
    VkImageMemoryBarrier imageBarrier {};
    imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageBarrier.image = reinterpret_cast<VkImage>(_image._handle);
    imageBarrier.oldLayout = static_cast<VkImageLayout>(_oldLayout);
    imageBarrier.newLayout = static_cast<VkImageLayout>(_newLayout);
    imageBarrier.srcAccessMask = static_cast<VkAccessFlags>(_srcAccess);
    imageBarrier.dstAccessMask = static_cast<VkAccessFlags>(_dstAccess);
    imageBarrier.subresourceRange.aspectMask = subresourceRange.aspectMask;
    imageBarrier.subresourceRange.baseMipLevel = subresourceRange.baseMip;
    imageBarrier.subresourceRange.levelCount = subresourceRange.mipCount;
    imageBarrier.subresourceRange.baseArrayLayer = subresourceRange.baseLayer;
    imageBarrier.subresourceRange.layerCount = subresourceRange.layerCount;
    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    vkCmdPipelineBarrier(
        reinterpret_cast<VkCommandBuffer>(_commandBuffer._handle),
        static_cast<VkPipelineStageFlagBits>(_srcStage),
        static_cast<VkPipelineStageFlagBits>(_dstStage),
        VK_DEPENDENCY_BY_REGION_BIT,
        0, nullptr,
        0, nullptr,
        1, &imageBarrier);
}

void ux3d::gpu::nextSubpass(const CommandBuffer& _CmdBuffer, const SubpassContents _contents)
{
    vkCmdNextSubpass(reinterpret_cast<VkCommandBuffer>(_CmdBuffer._handle), static_cast<VkSubpassContents>(_contents));
}

void ux3d::gpu::beginRenderPass(
    const CommandBuffer& _CmdBuffer,
    const RenderPassBeginInfo& _info,
    SubpassContents _contents)
{
    VkRenderPassBeginInfo vkinfo {};
    static_assert(sizeof(VkClearValue) == sizeof(ClearValue), "Incompatible clear value type");

    vkinfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    vkinfo.pNext = nullptr;

    vkinfo.renderArea.extent.height = _info.renderArea.extent.height;
    vkinfo.renderArea.extent.width = _info.renderArea.extent.width;
    vkinfo.renderArea.offset.x = _info.renderArea.offset.x;
    vkinfo.renderArea.offset.y = _info.renderArea.offset.y;

    vkinfo.framebuffer = reinterpret_cast<VkFramebuffer>(_info.framebuffer._handle);
    vkinfo.renderPass = reinterpret_cast<VkRenderPass>(_info.renderPass._handle);
    vkinfo.clearValueCount = static_cast<uint32_t>(_info.clearValues.size());
    vkinfo.pClearValues = reinterpret_cast<const VkClearValue*>(_info.clearValues.data());

    vkCmdBeginRenderPass(reinterpret_cast<VkCommandBuffer>(_CmdBuffer._handle), &vkinfo,
        static_cast<VkSubpassContents>(_contents));
}

void ux3d::gpu::endRenderPass(const CommandBuffer& _CmdBuffer)
{
    vkCmdEndRenderPass(reinterpret_cast<VkCommandBuffer>(_CmdBuffer._handle));
}

ux3d::gpu::GpuResult
ux3d::gpu::submit(const Queue& queue, const ux3d::gpu::SubmitInfo& info)
{
    VkSubmitInfo vkInfo {};
    vkInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    core::Vector<VkCommandBuffer> cmds;
    if (false == cmds.reserve(info.commandBuffers.size(), core::HeapAllocator::defaultInstance())) {
        return GpuError::OutOfHostMemory;
    }
    for (const auto& cmd : info.commandBuffers) {
        (void)(cmds.push_back(reinterpret_cast<VkCommandBuffer>(cmd._handle)));
    }
    vkInfo.commandBufferCount = cmds.size();
    vkInfo.pCommandBuffers = cmds.data();

    core::Vector<VkSemaphore> waitSemaphores;
    core::Vector<VkPipelineStageFlags> waitStages;
    if (false == waitSemaphores.reserve(info.waits.size(), core::HeapAllocator::defaultInstance())) {
        return GpuError::OutOfHostMemory;
    }
    if (false == waitStages.reserve(info.waits.size(), core::HeapAllocator::defaultInstance())) {
        return GpuError::OutOfHostMemory;
    }
    for (const auto& [sem, stage] : info.waits) {
        (void)(waitSemaphores.push_back(reinterpret_cast<VkSemaphore>(sem._handle)));
        (void)(waitStages.push_back(static_cast<VkPipelineStageFlags>(stage)));
    }
    vkInfo.waitSemaphoreCount = waitSemaphores.size();
    vkInfo.pWaitSemaphores = waitSemaphores.data();
    vkInfo.pWaitDstStageMask = waitStages.data();

    core::Vector<VkSemaphore> signalSemaphores;
    if (false == signalSemaphores.reserve(info.signalSemaphores.size(), core::HeapAllocator::defaultInstance())) {
        return GpuError::OutOfHostMemory;
    }
    for (const auto& sem : info.signalSemaphores) {
        (void)(signalSemaphores.push_back(reinterpret_cast<VkSemaphore>(sem._handle)));
    }
    vkInfo.signalSemaphoreCount = signalSemaphores.size();
    vkInfo.pSignalSemaphores = signalSemaphores.data();

    const auto result = vkQueueSubmit(
        reinterpret_cast<VkQueue>(queue._handle),
        1,
        &vkInfo,
        VK_NULL_HANDLE);

    if (result == VK_SUCCESS) {
        return GpuResult::Success;
    } else {
        return static_cast<GpuError>(result);
    }
}

ux3d::gpu::GpuResult
ux3d::gpu::present(
    const Queue& _queue,
    Swapchain& _swapchain,
    const uint32_t _idx,
    const Semaphore& _waitSemaphore)
{
    VkPresentInfoKHR info {
        VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr,
        1, reinterpret_cast<VkSemaphore const*>(&_waitSemaphore._handle),
        1, reinterpret_cast<VkSwapchainKHR*>(&_swapchain._handle), &_idx,
        nullptr
    };

    const VulkanResult result = vkQueuePresentKHR(
        reinterpret_cast<VkQueue>(_queue._handle),
        &info);

    if (!result) {
        core::log(core::LogLevel::Fatal, "Cannot present on queue");
    }

    return result;
}

ux3d::gpu::GpuResult
ux3d::gpu::waitQueueIdle(const Queue& _queue)
{
    return VulkanResult { vkQueueWaitIdle(reinterpret_cast<VkQueue>(_queue._handle)) };
}

ux3d::gpu::GpuResult
ux3d::gpu::waitDeviceIdle(const LogicalDevice& _device)
{
    return VulkanResult { vkDeviceWaitIdle(reinterpret_cast<VkDevice>(_device._handle)) };
}

void ux3d::gpu::copyImageWhole(
    const CommandBuffer& _commandBuffer,
    const gpu::Image& _srcImage,
    const gpu::Image& _dstImage,
    const gpu::ImageLayout& _srcLayout,
    const gpu::ImageLayout& _dstLayout,
    const gpu::Extent3d& _imageExtent)
{
    VkImageCopy copy {};
    copy.extent.width = _imageExtent.width;
    copy.extent.height = _imageExtent.height;
    copy.extent.depth = _imageExtent.depth;
    copy.srcSubresource.layerCount = 1;
    copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.dstSubresource.layerCount = 1;
    copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    vkCmdCopyImage(
        reinterpret_cast<VkCommandBuffer>(_commandBuffer._handle),
        reinterpret_cast<VkImage>(_srcImage._handle),
        static_cast<VkImageLayout>(_srcLayout),
        reinterpret_cast<VkImage>(_dstImage._handle),
        static_cast<VkImageLayout>(_dstLayout),
        1, &copy);
}

void ux3d::gpu::globalBarrier(
    const CommandBuffer& _commandBuffer,
    PipelineStage _srcStage,
    PipelineStage _dstStage,
    Access _srcAccess,
    Access _dstAccess)
{
    VkMemoryBarrier barrier {};
    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = static_cast<VkAccessFlags>(_srcAccess);
    barrier.dstAccessMask = static_cast<VkAccessFlags>(_dstAccess);

    vkCmdPipelineBarrier(
        reinterpret_cast<VkCommandBuffer>(_commandBuffer._handle),
        static_cast<VkPipelineStageFlagBits>(_srcStage),
        static_cast<VkPipelineStageFlagBits>(_dstStage),
        VK_DEPENDENCY_BY_REGION_BIT,
        1, &barrier,
        0, nullptr,
        0, nullptr);
}
