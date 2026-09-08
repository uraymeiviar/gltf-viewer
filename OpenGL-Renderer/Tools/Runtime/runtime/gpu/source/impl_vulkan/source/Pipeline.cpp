// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  Pipeline.cpp
//
//  Created by Moritz Becher on 24.05.19.
//

#include "gpu/Pipeline.h"

#include "core/Allocator.h"
#include "core/HeapAllocator.h"
#include "core/LinearAllocator.h"

#include "core/Slice.h"

#include <vulkan/vulkan.h>

#include "gpu/impl_vulkan/Result.h"

namespace ux3d::gpu {
// Convert the vertex input state create info structure
struct VkPipelineVertexInputStateCreateInfoData {
    core::Slice<VkVertexInputBindingDescription> vkVertexBindingDescriptions;
    core::Slice<VkVertexInputAttributeDescription> vkVertexAttributeDescriptions;
};

bool convertVertexInput(
    const VertexInputCreateInfo& _vertexInputCreateInfo,
    VkPipelineVertexInputStateCreateInfo& _outVkVertexInputCreateInfo,
    VkPipelineVertexInputStateCreateInfoData& _outData,
    core::IAllocator* _allocator)
{
    _outVkVertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    // bindingDescriptions
    if (!_outData.vkVertexBindingDescriptions.allocate(_vertexInputCreateInfo.bindingDescriptions.size(),
            _allocator)) {
        return false;
    }

    for (size_t i = 0u; i < _outData.vkVertexBindingDescriptions.size(); ++i) {
        _outData.vkVertexBindingDescriptions[i].binding = _vertexInputCreateInfo.bindingDescriptions[i].binding;
        _outData.vkVertexBindingDescriptions[i].stride = _vertexInputCreateInfo.bindingDescriptions[i].stride;
        _outData.vkVertexBindingDescriptions[i].inputRate = static_cast<VkVertexInputRate>(_vertexInputCreateInfo.bindingDescriptions[i].inputRate);
    }
    _outVkVertexInputCreateInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(_outData.vkVertexBindingDescriptions.size());
    _outVkVertexInputCreateInfo.pVertexBindingDescriptions = _outData.vkVertexBindingDescriptions.data();

    // attribute descriptions
    if (!_outData.vkVertexAttributeDescriptions.allocate(_vertexInputCreateInfo.attributeDescriptions.size(),
            _allocator)) {
        return false;
    }

    for (size_t i = 0u; i < _outData.vkVertexAttributeDescriptions.size(); ++i) {
        _outData.vkVertexAttributeDescriptions[i].location = _vertexInputCreateInfo.attributeDescriptions[i].location;
        _outData.vkVertexAttributeDescriptions[i].binding = _vertexInputCreateInfo.attributeDescriptions[i].binding;
        _outData.vkVertexAttributeDescriptions[i].format = static_cast<VkFormat>(_vertexInputCreateInfo.attributeDescriptions[i].format);
        _outData.vkVertexAttributeDescriptions[i].offset = _vertexInputCreateInfo.attributeDescriptions[i].offset;
    }
    _outVkVertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(_outData.vkVertexAttributeDescriptions.size());
    _outVkVertexInputCreateInfo.pVertexAttributeDescriptions = _outData.vkVertexAttributeDescriptions.data();

    return true;
}

// convert the input assembly create info
void convertInputAssembly(
    const InputAssemblyStateCreateInfo& _inputAssembly,
    VkPipelineInputAssemblyStateCreateInfo& _outInputAssembly)
{
    _outInputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    _outInputAssembly.topology = static_cast<VkPrimitiveTopology>(_inputAssembly.topology);
    _outInputAssembly.primitiveRestartEnable = _inputAssembly.primitiveRestartEnable;
}

// convert viewport state create info
struct VkPipelineViewportStateCreateInfoData {
    core::Slice<VkViewport> viewports;
    core::Slice<VkRect2D> scissors;
};

bool convertViewport(
    const ViewportStateCreateInfo& _viewportState,
    VkPipelineViewportStateCreateInfo& _outVkViewportState,
    VkPipelineViewportStateCreateInfoData& _outData,
    core::IAllocator* _allocator)
{
    _outVkViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;

    if (!_outData.viewports.allocate(_viewportState.viewports.size(), _allocator)) {
        return false;
    }

    for (size_t i = 0u; i < _outData.viewports.size(); ++i) {
        _outData.viewports[i].x = _viewportState.viewports[i].x;
        _outData.viewports[i].y = _viewportState.viewports[i].y;
        _outData.viewports[i].width = _viewportState.viewports[i].width;
        _outData.viewports[i].height = _viewportState.viewports[i].height;
        _outData.viewports[i].minDepth = _viewportState.viewports[i].minDepth;
        _outData.viewports[i].maxDepth = _viewportState.viewports[i].maxDepth;
    }
    _outVkViewportState.viewportCount = static_cast<uint32_t>(_outData.viewports.size());
    _outVkViewportState.pViewports = _outData.viewports.data();

    if (!_outData.scissors.allocate(_viewportState.scissors.size(), _allocator)) {
        return false;
    }

    for (size_t i = 0u; i < _outData.scissors.size(); ++i) {
        _outData.scissors[i].offset.x = _viewportState.scissors[i].offset.x;
        _outData.scissors[i].offset.y = _viewportState.scissors[i].offset.y;
        _outData.scissors[i].extent.width = _viewportState.scissors[i].extent.width;
        _outData.scissors[i].extent.height = _viewportState.scissors[i].extent.height;
    }
    _outVkViewportState.scissorCount = static_cast<uint32_t>(_outData.scissors.size());
    _outVkViewportState.pScissors = _outData.scissors.data();

    return true;
}

// Convert rasterization state create info
void convert(
    const RasterizationStateCreateInfo& _rasterizationState,
    VkPipelineRasterizationStateCreateInfo& _outVkRasterizationState)
{
    _outVkRasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    _outVkRasterizationState.depthClampEnable = _rasterizationState.depthClampEnable;
    _outVkRasterizationState.rasterizerDiscardEnable = _rasterizationState.rasterizerDiscardEnable;
    _outVkRasterizationState.polygonMode = static_cast<VkPolygonMode>(_rasterizationState.polygonMode);
    _outVkRasterizationState.cullMode = static_cast<VkCullModeFlags>(_rasterizationState.cullMode);
    _outVkRasterizationState.frontFace = static_cast<VkFrontFace>(_rasterizationState.frontFace);
    _outVkRasterizationState.depthBiasEnable = _rasterizationState.depthBiasEnable;
    _outVkRasterizationState.depthBiasConstantFactor = _rasterizationState.depthBiasConstantFactor;
    _outVkRasterizationState.depthBiasClamp = _rasterizationState.depthBiasClamp;
    _outVkRasterizationState.depthBiasSlopeFactor = _rasterizationState.depthBiasSlopeFactor;
    _outVkRasterizationState.lineWidth = _rasterizationState.lineWidth;
}

// Convert multisample state create info
void convertMultisample(
    const MultisampleStateCreateInfo& _MultisampleState,
    VkPipelineMultisampleStateCreateInfo& _OutVkMultisampleState)
{
    _OutVkMultisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    _OutVkMultisampleState.rasterizationSamples = static_cast<VkSampleCountFlagBits>(_MultisampleState.rasterizationSamples);
    _OutVkMultisampleState.sampleShadingEnable = _MultisampleState.sampleShadingEnable;
    _OutVkMultisampleState.minSampleShading = _MultisampleState.minSampleShading;
    _OutVkMultisampleState.pSampleMask = _MultisampleState.pSampleMask;
    _OutVkMultisampleState.alphaToCoverageEnable = _MultisampleState.alphaToCoverageEnable;
    _OutVkMultisampleState.alphaToOneEnable = _MultisampleState.alphaToOneEnable;
}

// Convert color blend state create info
struct VkPipelineColorBlendStateCreateInfoData {
    core::Slice<VkPipelineColorBlendAttachmentState> attachments;
};

bool convertColorBlend(
    const ColorBlendStateCreateInfo& _colorBlendState,
    VkPipelineColorBlendStateCreateInfo& _outVkColorBlendState,
    VkPipelineColorBlendStateCreateInfoData& _outData,
    core::IAllocator* _allocator)
{
    _outVkColorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    _outVkColorBlendState.logicOpEnable = _colorBlendState.logicOpEnable;
    _outVkColorBlendState.logicOp = static_cast<VkLogicOp>(_colorBlendState.logicOp);

    if (!_outData.attachments.allocate(_colorBlendState.attachments.size(), _allocator)) {
        return false;
    }

    for (size_t i = 0u; i < _outData.attachments.size(); ++i) {
        _outData.attachments[i].blendEnable = _colorBlendState.attachments[i].blendEnable;
        _outData.attachments[i].srcColorBlendFactor = static_cast<VkBlendFactor>(_colorBlendState.attachments[i].srcColorBlendFactor);
        _outData.attachments[i].dstColorBlendFactor = static_cast<VkBlendFactor>(_colorBlendState.attachments[i].dstColorBlendFactor);
        _outData.attachments[i].colorBlendOp = static_cast<VkBlendOp>(_colorBlendState.attachments[i].colorBlendOp);
        _outData.attachments[i].srcAlphaBlendFactor = static_cast<VkBlendFactor>(_colorBlendState.attachments[i].srcAlphaBlendFactor);
        _outData.attachments[i].dstAlphaBlendFactor = static_cast<VkBlendFactor>(_colorBlendState.attachments[i].dstAlphaBlendFactor);
        _outData.attachments[i].alphaBlendOp = static_cast<VkBlendOp>(_colorBlendState.attachments[i].alphaBlendOp);
        _outData.attachments[i].colorWriteMask = static_cast<VkColorComponentFlags>(_colorBlendState.attachments[i].colorWriteMask);
    }
    _outVkColorBlendState.attachmentCount = static_cast<uint32_t>(_outData.attachments.size());
    _outVkColorBlendState.pAttachments = _outData.attachments.data();

    for (uint32_t i = 0u; i < 4u; ++i) {
        _outVkColorBlendState.blendConstants[i] = _colorBlendState.blendConstants[i];
    }

    return true;
}

// Convert dynamic state create info
struct VkPipelineDynamicStateCreateInfoData {
    core::Slice<VkDynamicState> dynamicStates;
};

bool convertDynamic(
    const DynamicStateCreateInfo& _dynamicState,
    VkPipelineDynamicStateCreateInfo& _outVkDynamicState,
    VkPipelineDynamicStateCreateInfoData& _outData,
    core::IAllocator* _allocator)
{
    _outVkDynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

    if (!_outData.dynamicStates.allocate(_dynamicState.dynamicStates.size(), _allocator)) {
        return false;
    }

    for (size_t i = 0u; i < _outData.dynamicStates.size(); ++i) {
        _outData.dynamicStates[i] = static_cast<VkDynamicState>(_dynamicState.dynamicStates[i]);
    }
    _outVkDynamicState.dynamicStateCount = static_cast<uint32_t>(_outData.dynamicStates.size());
    _outVkDynamicState.pDynamicStates = _outData.dynamicStates.data();

    return true;
}

// Convert the pipeline create info structure
struct VkPipelineCreateInfoData {
    core::Vector<VkPipelineShaderStageCreateInfo> vkShaderStages;
    VkPipelineVertexInputStateCreateInfo vkVertexInputState {};
    VkPipelineVertexInputStateCreateInfoData vkPipelineVertexInputStateCreateInfoData;
    VkPipelineInputAssemblyStateCreateInfo vkInputAssembly {};
    VkPipelineViewportStateCreateInfo vkViewportState {};
    VkPipelineViewportStateCreateInfoData vkViewportStateData;
    VkPipelineRasterizationStateCreateInfo vkRasterizationState {};
    VkPipelineMultisampleStateCreateInfo vkMultisampleState {};
    VkPipelineColorBlendStateCreateInfo vkColorBlendState {};
    VkPipelineColorBlendStateCreateInfoData vkColorBlendStateData;
    VkPipelineDynamicStateCreateInfo vkDynamicState {};
    VkPipelineDynamicStateCreateInfoData vkDynamicStateData;
};

bool convertGraphicsPipeline(
    const GraphicsPipelineCreateInfo& _pipelineCreateInfo,
    VkGraphicsPipelineCreateInfo& _vkPipelineCreateInfo,
    VkPipelineCreateInfoData& _outData,
    core::IAllocator* _allocator)
{
    _vkPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    _vkPipelineCreateInfo.pNext = nullptr;
    _vkPipelineCreateInfo.flags = {};

    // Convert shader stages
    if (!_outData.vkShaderStages.reserve(_pipelineCreateInfo.shaderStageCreateInfos.size(), _allocator)) {
        return false;
    }

    for (const ShaderStageCreateInfo& shaderStage : _pipelineCreateInfo.shaderStageCreateInfos) {
        VkPipelineShaderStageCreateInfo* vkShaderStage = _outData.vkShaderStages.emplace_back();
        vkShaderStage->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vkShaderStage->flags = {};
        vkShaderStage->pNext = nullptr;
        vkShaderStage->pSpecializationInfo = nullptr;
        vkShaderStage->stage = static_cast<VkShaderStageFlagBits>(shaderStage.stage);
        vkShaderStage->module = reinterpret_cast<VkShaderModule>(shaderStage.module._handle);
        vkShaderStage->pName = shaderStage.entryPointName.c_str();
    }
    _vkPipelineCreateInfo.stageCount = static_cast<uint32_t>(_outData.vkShaderStages.size());
    _vkPipelineCreateInfo.pStages = _outData.vkShaderStages.data();

    // Convert vertex input state
    if (!convertVertexInput(_pipelineCreateInfo.vertexInputState, _outData.vkVertexInputState,
            _outData.vkPipelineVertexInputStateCreateInfoData, _allocator)) {
        return false;
    }
    _vkPipelineCreateInfo.pVertexInputState = &_outData.vkVertexInputState;

    // Convert the input assembly state create info
    convertInputAssembly(_pipelineCreateInfo.inputAssemblyState, _outData.vkInputAssembly);
    _vkPipelineCreateInfo.pInputAssemblyState = &_outData.vkInputAssembly;

    // TODO: Tesselation state
    _vkPipelineCreateInfo.pTessellationState = nullptr;

    // Convert viewport state create info
    if (!convertViewport(_pipelineCreateInfo.viewportState, _outData.vkViewportState, _outData.vkViewportStateData,
            _allocator)) {
        return false;
    }
    _vkPipelineCreateInfo.pViewportState = &_outData.vkViewportState;

    // Convert rasterization state
    convert(_pipelineCreateInfo.rasterizationState, _outData.vkRasterizationState);
    _vkPipelineCreateInfo.pRasterizationState = &_outData.vkRasterizationState;

    // Convert multisample state
    convertMultisample(_pipelineCreateInfo.multisampleState, _outData.vkMultisampleState);
    _vkPipelineCreateInfo.pMultisampleState = &_outData.vkMultisampleState;

    // TODO: Depth stencil state
    _vkPipelineCreateInfo.pDepthStencilState = nullptr;

    // Convert color blend state
    if (!convertColorBlend(_pipelineCreateInfo.colorBlendState, _outData.vkColorBlendState, _outData.vkColorBlendStateData,
            _allocator)) {
        return false;
    }
    _vkPipelineCreateInfo.pColorBlendState = &_outData.vkColorBlendState;

    // Convert dynamic state
    if (!convertDynamic(_pipelineCreateInfo.dynamicState, _outData.vkDynamicState, _outData.vkDynamicStateData,
            _allocator)) {
        return false;
    }
    _vkPipelineCreateInfo.pDynamicState = &_outData.vkDynamicState;

    _vkPipelineCreateInfo.layout = reinterpret_cast<VkPipelineLayout>(_pipelineCreateInfo.layout._handle);
    _vkPipelineCreateInfo.renderPass = reinterpret_cast<VkRenderPass>(_pipelineCreateInfo.renderPass._handle);
    _vkPipelineCreateInfo.subpass = _pipelineCreateInfo.subpass;
    _vkPipelineCreateInfo.basePipelineHandle = reinterpret_cast<VkPipeline>(_pipelineCreateInfo.basePipelineHandle._handle);
    _vkPipelineCreateInfo.basePipelineIndex = _pipelineCreateInfo.basePipelineIndex;

    return true;
}
}

ux3d::gpu::GpuResult ux3d::gpu::createGraphicsPipeline(const LogicalDevice& _logicalDevice,
    const GraphicsPipelineCreateInfo& _pipelineCreateInfo,
    Pipeline& _pipeline,
    core::IAllocator* _allocator)
{
    if (_pipeline._handle != InvalidHandle) {
        return GpuError::InvalidHandle;
    }

    VkPipelineCreateInfoData vkPipelineData;
    VkGraphicsPipelineCreateInfo vkGraphicsPipelineCreateInfo;
    if (!convertGraphicsPipeline(_pipelineCreateInfo, vkGraphicsPipelineCreateInfo, vkPipelineData, _allocator)) {
        return GpuError::OutOfHostMemory;
    }

    VkPipeline vkPipeline;

    const VulkanResult result = vkCreateGraphicsPipelines(reinterpret_cast<VkDevice>(_logicalDevice._handle), nullptr,
        1, &vkGraphicsPipelineCreateInfo, nullptr, &vkPipeline);
    if (result) {
        _pipeline._handle = reinterpret_cast<Handle>(vkPipeline);
        _pipeline._device = _logicalDevice;
    }

    return result;
}

ux3d::gpu::GpuResult
ux3d::gpu::createComputePipeline(
    const LogicalDevice& _device,
    const ComputePipelineCreateInfo& _pipelineCreateInfo,
    Pipeline& _outPipeline)
{
    if (_outPipeline._handle != InvalidHandle) {
        return GpuError::InvalidHandle;
    }

    VkComputePipelineCreateInfo info = {
        VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        nullptr, 0,
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT,
            reinterpret_cast<VkShaderModule>(_pipelineCreateInfo.shaderStageCreateInfo.module._handle),
            _pipelineCreateInfo.shaderStageCreateInfo.entryPointName.c_str(),
            nullptr },
        reinterpret_cast<VkPipelineLayout>(_pipelineCreateInfo.layout._handle),
        VK_NULL_HANDLE, 0
    };

    VkPipeline pipeline;
    const VulkanResult result = vkCreateComputePipelines(
        reinterpret_cast<VkDevice>(_device._handle),
        VK_NULL_HANDLE,
        1, &info,
        nullptr,
        &pipeline);

    if (result) {
        _outPipeline._device = _device;
        _outPipeline._handle = reinterpret_cast<Handle>(pipeline);
    }

    return result;
}

void ux3d::gpu::destroyPipeline(Pipeline& _pipeline)
{
    if (_pipeline._handle != InvalidHandle) {
        vkDestroyPipeline(reinterpret_cast<VkDevice>(_pipeline._device._handle),
            reinterpret_cast<VkPipeline>(_pipeline._handle), nullptr);
        _pipeline._handle = InvalidHandle;
    }
}
