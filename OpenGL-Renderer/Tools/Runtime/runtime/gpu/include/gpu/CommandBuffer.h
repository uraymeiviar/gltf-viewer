// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Vector.h"

#include "Framebuffer.h"
#include "RenderPass.h"

#include <optional>

namespace ux3d::gpu {
struct CommandPoolCreateInfo {
    enum class CreateFlagBits : uint32_t {
        None = 0,

        // Transient CommandBuffer specifies that command buffers allocated from the pool will be short-lived, meaning that they will be reset or freed in a relatively short timeframe. This flag may be used by the implementation to control memory allocation behavior within the pool.
        Transient = 0x00000001,

        // Reset CommandBuffer allows any command buffer allocated from a pool to be individually reset to the initial state
        Reset = 0x00000002,

        // Specifies that command buffers allocated from the pool are protected command buffers
        //ProtectedMemory = 0x00000004,
    };

    using CreateFlag = core::Flag<CreateFlagBits>;

    CreateFlag commandBufferFlags;
    uint32_t queueFamilyIndex;
};

struct CommandPool {
    Handle _handle = InvalidHandle;
    LogicalDevice device;
};

GpuResult createCommandPool(
    const LogicalDevice& logicalDevice,
    const CommandPoolCreateInfo& createInfo,
    CommandPool& outPool);

GpuResult destroyCommandPool(CommandPool& pool);

GpuResult resetCommandPool(const CommandPool& pool, bool releaseResources);

// VkCommandBufferLevel
enum class CommandBufferType : uint32_t {
    Primary = 0,
    Secondary = 1
};

struct CommandBuffer {
    CommandBufferType type = CommandBufferType::Primary;
    CommandPool pool;
    Handle _handle = InvalidHandle;
};

GpuResult
createCommandBuffer(const CommandPool& _Pool, CommandBufferType _Type, CommandBuffer& _OutCMDBuffer);

GpuResult createCommandBuffers(
    const CommandPool& _Pool,
    CommandBufferType _Type,
    uint32_t _Count,
    core::Vector<CommandBuffer>& _OutCMDBuffers,
    core::IAllocator* _Allocator);

GpuResult destroyCommandBuffer(CommandBuffer& _CommandBuffer);

GpuResult executeCommandBuffer(const CommandBuffer& _PrimaryCommands);

GpuResult executeCommandBuffers(
    const CommandBuffer& _PrimaryCommands,
    const core::Vector<CommandBuffer>& _SecondaryCommands,
    core::IAllocator* _Allocator);

// Reset the CommandBuffer to the initial state
// _ReleaseResources will return resources to the owning pool
GpuResult resetCommandBuffer(const CommandBuffer& _CommandBuffer, bool _ReleaseResources);

// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#vkBeginCommandBuffer
struct CommandBufferBeginInfo {
    enum class UsageFlagBits : uint32_t {
        None = 0,
        OneTimeSubmit = 0x00000001,
        RenderPassContinue = 0x00000002,
        SimultaneousUse = 0x00000004
    };

    using UsageFlag = core::Flag<UsageFlagBits>;

    struct InheritanceInfo {
        enum class QueryControlFlagBits : uint32_t {
            None = 0,
            Percise = 0x00000001
        };

        using QueryControlFlag = core::Flag<QueryControlFlagBits>;

        enum class QueryPipelineStatisticsFlagBits : uint32_t {
            InputAssemblyVertices = 0x00000001,
            InputAssemblyPrimitives = 0x00000002,
            VertexShaderInvocations = 0x00000004,
            GeometryShaderInvocations = 0x00000008,
            GeometryShaderPrimitives = 0x00000010,
            ClippingInvocations = 0x00000020,
            ClippingPrimitives = 0x00000040,
            FragmentShaderInvocations = 0x00000080,
            TesselationControlShaderPatches = 0x00000100,
            TesselationEvaluationShaderInvocations = 0x00000200,
            ComputeShaderInvocations = 0x00000400
        };

        using QueryPipelineStatisticsFlag = core::Flag<QueryPipelineStatisticsFlagBits>;

        RenderPass renderpass;
        Framebuffer framebuffer; // It can be VK_NULL_HANDLE if the framebuffer is not known,
        // or if the VkCommandBuffer will not be executed within a render pass instance.
        uint32_t subpass; // subpass index
        bool occlusionQueryEnabled = false; // TODO: check for reasonable default

        QueryControlFlag queryFlags;
        QueryPipelineStatisticsFlag pipelineStatistics;
    };

    UsageFlag flags;

    std::optional<InheritanceInfo> inheritanceInfo; // ignored if this is a primary commadnbuffer
};

void beginCommandBuffer(const CommandBuffer& _CmdBuffer, const CommandBufferBeginInfo& _info);

GpuResult endCommandBuffer(const CommandBuffer& _CmdBuffer);
} // !ux3d
