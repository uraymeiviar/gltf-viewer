// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  Pipeline.h
//  middleware
//
//  Created by Moritz Becher on 24.05.19.
//

#pragma once

#include "core/EnumOps.h"
#include "core/Vector.h"

#include "LogicalDevice.h"
#include "PipelineLayout.h"
#include "RenderPass.h"
#include "ShaderModule.h"
#include "Types.h"

namespace ux3d::gpu {
// Vertex Input

enum class VertexInputRate : uint32_t {
    Vertex = 0u,
    Instance = 1u,
};

struct VertexInputBindingDescription {
    uint32_t binding {};
    uint32_t stride {};
    VertexInputRate inputRate {};
};

struct VertexInputAttributeDescription {
    uint32_t location;
    uint32_t binding;
    Format format;
    uint32_t offset;
};

struct VertexInputCreateInfo {
    core::Vector<VertexInputBindingDescription> bindingDescriptions;
    core::Vector<VertexInputAttributeDescription> attributeDescriptions;
};

// Input Assembly

enum class PrimitiveTopology : uint32_t {
    PointList = 0,
    LineList = 1,
    LineStrip = 2,
    TriangleList = 3,
    TriangleStrip = 4,
    TriangleFan = 5,
    LineListWithAdjacency = 6,
    LineStripWithAdjacency = 7,
    TriangleListWithAdjacency = 8,
    TriangleStripWithAdjacency = 9,
    PatchList = 10,
};

struct InputAssemblyStateCreateInfo {
    PrimitiveTopology topology { PrimitiveTopology::TriangleList };
    bool primitiveRestartEnable { false };
};

// Viewport

struct Viewport {
    float x;
    float y;
    float width;
    float height;
    float minDepth;
    float maxDepth;
};

struct ViewportStateCreateInfo {
    core::Vector<Viewport> viewports;
    core::Vector<Rect> scissors;
};

//  Rasterization

enum class PolygonMode : uint32_t {
    Fill = 0u,
    Line = 1u,
    Point = 2u,
    FillRectangleNV = 1000153000u,
};

enum class CullModeFlagBits : uint32_t {
    None = 0u,
    Front = 1u,
    Back = 2u,
    FrontAndBack = Front | Back,
};

using CullModeFlag = core::Flag<CullModeFlagBits>;

enum class FrontFace : uint32_t {
    CounterClockwise = 0u,
    Clockwise = 1u,
};

struct RasterizationStateCreateInfo {
    bool depthClampEnable = false;
    bool rasterizerDiscardEnable = false;
    PolygonMode polygonMode = PolygonMode::Fill;
    CullModeFlag cullMode = CullModeFlagBits::None;
    FrontFace frontFace = FrontFace::Clockwise; // TODO: counter clockwise?
    bool depthBiasEnable = false;
    float depthBiasConstantFactor;
    float depthBiasClamp;
    float depthBiasSlopeFactor;
    float lineWidth = 1.0;
};

struct MultisampleStateCreateInfo {
    SampleCount rasterizationSamples = SampleCount::One;
    bool sampleShadingEnable = false;
    float minSampleShading = 1.0;
    uint32_t* pSampleMask = nullptr;
    bool alphaToCoverageEnable = false;
    bool alphaToOneEnable = false;
};

// Color Blending

enum class ColorComponentFlagBits : uint32_t {
    None = 0,
    R = 0x00000001u,
    G = 0x00000002u,
    B = 0x00000004u,
    A = 0x00000008u,
    RG = R | G,
    RGB = R | G | B,
    RGBA = R | G | B | A
};

using ColorComponentFlag = core::Flag<ColorComponentFlagBits>;

enum class BlendFactor : uint32_t {
    Zero = 0u,
    One = 1u,
    SrcColor = 2u,
    OneMinusSrcColor = 3u,
    DstColor = 4u,
    OneMinusDstColor = 5u,
    SrcAlpha = 6u,
    OneMinusSrcAlpha = 7u,
    DstAlpha = 8u,
    OneMinusDstAlpha = 9u,
    ConstantColor = 10u,
    OneMinusConstantColor = 11u,
    ConstantAlpha = 12u,
    OneMinusConstantAlpha = 13u,
    SrcAlphaSaturate = 14u,
    Src1Color = 15u,
    OneMinusSrc1Color = 16u,
    Src1Alpha = 17u,
    OneMinusSrc1Alpha = 18u,
};

enum class BlendOp : uint32_t {
    Add = 0u,
    Subtract = 1u,
    ReverseSubtract = 2u,
    Min = 3u,
    Max = 4u,
    // TODO: add extensions
};

struct ColorBlendAttachmentState {
    bool blendEnable = false;
    BlendFactor srcColorBlendFactor;
    BlendFactor dstColorBlendFactor;
    BlendOp colorBlendOp;
    BlendFactor srcAlphaBlendFactor;
    BlendFactor dstAlphaBlendFactor;
    BlendOp alphaBlendOp;
    ColorComponentFlag colorWriteMask { gpu::ColorComponentFlagBits::RGBA };
};

enum class LogicOp : uint32_t {
    Clear = 0u,
    And = 1u,
    AndReverse = 2u,
    Copy = 4u,
    AndInverted = 5u,
    NoOp = 5u,
    XOR = 6u,
    Or = 7u,
    NOR = 8u,
    Equivalent = 9u,
    Invert = 10u,
    OrReverse = 11u,
    CopyInverted = 12u,
    OrInverted = 13u,
    NAND = 14u,
    Set = 15u,
};

struct ColorBlendStateCreateInfo {
    bool logicOpEnable { false };
    LogicOp logicOp {};
    core::Vector<ColorBlendAttachmentState> attachments;
    float blendConstants[4] = {};
};

// Dynamic States that can be modified after pipeline creation

enum class DynamicState : uint32_t {
    Viewport = 0u,
    Scissor = 1u,
    LineWidth = 2u,
    DepthBias = 3u,
    BlendConstants = 4u,
    DepthBounds = 5u,
    StencilCompareMask = 6u,
    StencilWriteMask = 7u,
    StencilReference = 8u,
    // TODO: add extensions
};

struct DynamicStateCreateInfo {
    core::Vector<DynamicState> dynamicStates;
};

// Pipeline

struct ShaderStageCreateInfo {
    ShaderStageFlagBits stage;
    ShaderModule module;
    core::string entryPointName { "main" };
    // TODO: specializationinfo
};

struct Pipeline {
    Handle _handle = InvalidHandle;
    LogicalDevice _device;
};

struct GraphicsPipelineCreateInfo {
    core::Vector<ShaderStageCreateInfo> shaderStageCreateInfos;
    VertexInputCreateInfo vertexInputState;
    InputAssemblyStateCreateInfo inputAssemblyState;
    // TODO: TesselationState
    ViewportStateCreateInfo viewportState;
    RasterizationStateCreateInfo rasterizationState;
    MultisampleStateCreateInfo multisampleState;
    // TODO: DepthStencilState
    ColorBlendStateCreateInfo colorBlendState;
    DynamicStateCreateInfo dynamicState;
    PipelineLayout layout;
    RenderPass renderPass;
    uint32_t subpass {};
    Pipeline basePipelineHandle {};
    int32_t basePipelineIndex {};
};

GpuResult createGraphicsPipeline(
    const LogicalDevice& logicalDevice,
    const GraphicsPipelineCreateInfo& pipelineCreateInfo,
    Pipeline& pipeline,
    core::IAllocator* allocator);

struct ComputePipelineCreateInfo {
    ShaderStageCreateInfo shaderStageCreateInfo;
    PipelineLayout layout;
    Pipeline basePipelineHandle {};
    int32_t basePipelineIndex {};
};

GpuResult createComputePipeline(
    const LogicalDevice& _device,
    const ComputePipelineCreateInfo& _pipelineCreateInfo,
    Pipeline& _outPipeline);

void destroyPipeline(Pipeline& pipeline);
}
