// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gpu/CommandBuffer.h"

#include "core/HeapAllocator.h"
#include "core/LinearAllocator.h"

#include <vulkan/vulkan.h>

#include "gpu/impl_vulkan/Result.h"

namespace ux3d::gpu {
void convert(const CommandPoolCreateInfo& _in, VkCommandPoolCreateInfo& _out)
{
    _out.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    _out.pNext = nullptr;
    _out.queueFamilyIndex = _in.queueFamilyIndex;
    _out.flags = VkCommandPoolCreateFlags(_in.commandBufferFlags);
}

GpuResult createCommandPool(
    const LogicalDevice& _logicalDevice,
    const CommandPoolCreateInfo& _createInfo,
    CommandPool& _outPool)
{
    if (_outPool._handle != InvalidHandle) {
        return GpuError::InvalidHandle;
    }

    VkCommandPoolCreateInfo vkInfo {};

    convert(_createInfo, vkInfo);

    VkCommandPool vkPool {};
    const VulkanResult result = vkCreateCommandPool(reinterpret_cast<VkDevice>(_logicalDevice._handle), &vkInfo,
        nullptr, &vkPool);

    if (result) {
        _outPool._handle = reinterpret_cast<Handle>(vkPool);
        _outPool.device = _logicalDevice;
    }

    return result;
}

GpuResult destroyCommandPool(CommandPool& _pool)
{
    if (_pool._handle != InvalidHandle && _pool.device._handle != InvalidHandle) {
        vkDestroyCommandPool(reinterpret_cast<VkDevice>(_pool.device._handle),
            reinterpret_cast<VkCommandPool>(_pool._handle), nullptr);
        _pool._handle = InvalidHandle;
        _pool.device._handle = InvalidHandle;

        return GpuResult::Success;
    }

    return GpuError::InvalidHandle;
}

GpuResult resetCommandPool(const CommandPool& _pool, const bool _releaseResources)
{
    if (_pool._handle != InvalidHandle && _pool.device._handle != InvalidHandle) {
        return VulkanResult(vkResetCommandPool(reinterpret_cast<VkDevice>(_pool.device._handle),
            reinterpret_cast<VkCommandPool>(_pool._handle),
            _releaseResources ? VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT : 0));
    }

    return GpuError::InvalidHandle;
}

GpuResult
createCommandBuffer(const CommandPool& _pool, const CommandBufferType _type, CommandBuffer& _outCMDBuffer)
{
    VkCommandBufferAllocateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.pNext = nullptr;
    info.commandBufferCount = 1u;
    info.level = static_cast<VkCommandBufferLevel>(_type);
    info.commandPool = reinterpret_cast<VkCommandPool>(_pool._handle);

    VkCommandBuffer cmd {};
    const VulkanResult result = vkAllocateCommandBuffers(reinterpret_cast<VkDevice>(_pool.device._handle),
        &info, &cmd);

    if (result) {
        _outCMDBuffer.type = _type;
        _outCMDBuffer._handle = reinterpret_cast<Handle>(cmd);
        _outCMDBuffer.pool = _pool;
    }

    return result;
}

GpuResult destroyCommandBuffer(CommandBuffer& _commandBuffer)
{
    if (_commandBuffer._handle != InvalidHandle && _commandBuffer.pool._handle != InvalidHandle) {
        const auto cmd = reinterpret_cast<VkCommandBuffer>(_commandBuffer._handle);
        vkFreeCommandBuffers(reinterpret_cast<VkDevice>(_commandBuffer.pool.device._handle),
            reinterpret_cast<VkCommandPool>(_commandBuffer.pool._handle), 1u, &cmd);

        return GpuResult::Success;
    }

    return GpuError::InvalidHandle;
}

GpuResult executeCommandBuffer(const CommandBuffer& _commandBuffer)
{
    if (_commandBuffer._handle == InvalidHandle) {
        return GpuError::InvalidHandle;
    }

    if (_commandBuffer.type != CommandBufferType::Primary) {
        return GpuError::InvalidHandle;
    }

    const auto cmd = reinterpret_cast<VkCommandBuffer>(_commandBuffer._handle);

    vkCmdExecuteCommands(cmd, 1u, nullptr);

    return GpuResult::Success;
}

GpuResult resetCommandBuffer(const CommandBuffer& _CommandBuffer, const bool _ReleaseResources)
{
    if (_CommandBuffer._handle == InvalidHandle) {
        return GpuError::InvalidHandle;
    }

    return VulkanResult(vkResetCommandBuffer(reinterpret_cast<VkCommandBuffer>(_CommandBuffer._handle),
        _ReleaseResources ? VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT : 0));
}

void beginCommandBuffer(const CommandBuffer& _CmdBuffer, const CommandBufferBeginInfo& _info)
{
    VkCommandBufferBeginInfo vkinfo {};

    vkinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkinfo.pNext = nullptr;
    vkinfo.flags = static_cast<VkCommandBufferUsageFlags>(_info.flags);
    vkinfo.pInheritanceInfo = nullptr;

    VkCommandBufferInheritanceInfo vkinheritance {};

    if (_CmdBuffer.type == CommandBufferType::Secondary && _info.inheritanceInfo.has_value()) {
        vkinheritance.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
        vkinheritance.pNext = nullptr;
        vkinheritance.framebuffer = reinterpret_cast<VkFramebuffer>(_info.inheritanceInfo->framebuffer._handle);
        vkinheritance.renderPass = reinterpret_cast<VkRenderPass>(_info.inheritanceInfo->renderpass._handle);
        vkinheritance.subpass = _info.inheritanceInfo->subpass;
        vkinheritance.occlusionQueryEnable = _info.inheritanceInfo->occlusionQueryEnabled ? VK_TRUE : VK_FALSE;
        vkinheritance.queryFlags = static_cast<VkQueryControlFlags>(_info.inheritanceInfo->queryFlags);
        vkinheritance.pipelineStatistics = static_cast<VkQueryPipelineStatisticFlags>(_info.inheritanceInfo->pipelineStatistics);

        vkinfo.pInheritanceInfo = &vkinheritance;
    }

    vkBeginCommandBuffer(reinterpret_cast<VkCommandBuffer>(_CmdBuffer._handle), &vkinfo);
}

ux3d::gpu::GpuResult endCommandBuffer(const CommandBuffer& _CmdBuffer)
{
    return GpuError(vkEndCommandBuffer(reinterpret_cast<VkCommandBuffer>(_CmdBuffer._handle)));
}
} // !ux3d

ux3d::gpu::GpuResult ux3d::gpu::createCommandBuffers(
    const CommandPool& _pool,
    const CommandBufferType _type,
    const uint32_t _count,
    core::Vector<CommandBuffer>& _outCMDBuffers,
    core::IAllocator* _allocator)
{
    if (_pool._handle == InvalidHandle || _pool.device._handle == InvalidHandle) {
        return GpuError::InvalidHandle;
    }

    core::Vector<VkCommandBuffer> cmds;

    if (!cmds.resize(_count, _allocator)) {
        return GpuError::OutOfHostMemory;
    }

    VkCommandBufferAllocateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.pNext = nullptr;
    info.commandBufferCount = _count;
    info.level = static_cast<VkCommandBufferLevel>(_type);
    info.commandPool = reinterpret_cast<VkCommandPool>(_pool._handle);

    const VulkanResult result = vkAllocateCommandBuffers(reinterpret_cast<VkDevice>(_pool.device._handle), &info,
        cmds.data());

    if (!result) {
        return result;
    }

    for (const VkCommandBuffer& cmd : cmds) {
        CommandBuffer* pBuffer = _outCMDBuffers.emplace_back();
        if (pBuffer == nullptr) {
            return GpuError::OutOfHostMemory;
        }

        pBuffer->type = _type;
        pBuffer->_handle = reinterpret_cast<Handle>(cmd);
        pBuffer->pool = _pool;
    }

    return result;
}

ux3d::gpu::GpuResult ux3d::gpu::executeCommandBuffers(
    const CommandBuffer& _primaryCommands,
    const core::Vector<CommandBuffer>& _secondaryCommands,
    core::IAllocator* _allocator)
{
    if (_primaryCommands._handle == InvalidHandle) {
        return GpuError::InvalidHandle;
    }

    if (_primaryCommands.type != CommandBufferType::Primary) {
        return GpuError::InvalidHandle;
    }

    const auto primary = reinterpret_cast<VkCommandBuffer>(_primaryCommands._handle);

    core::Vector<VkCommandBuffer> secondarys; // could use slice here

    if (!secondarys.reserve(_secondaryCommands.size(), _allocator)) {
        return GpuError::OutOfHostMemory;
    }

    for (const CommandBuffer& cmd : _secondaryCommands) {
        if (cmd._handle != InvalidHandle || cmd.type == CommandBufferType::Secondary) {
            (void)secondarys.emplace_back(reinterpret_cast<VkCommandBuffer>(cmd._handle));
        }
    }

    // https://vulkan.lunarg.com/doc/view/1.0.26.0/linux/vkspec.chunked/ch05s06.html
    vkCmdExecuteCommands(primary, static_cast<uint32_t>(secondarys.size()), secondarys.data());

    return GpuResult::Success;
}
