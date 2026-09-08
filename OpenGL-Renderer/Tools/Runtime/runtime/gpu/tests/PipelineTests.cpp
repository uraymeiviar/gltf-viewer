// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  PipelineTests.cpp
//  middleware
//
//  Created by Moritz Becher on 07.06.19.
//

#include <catch2/catch.hpp>

#include "TestInitializer.h"

#include "core/HeapAllocator.h"
#include "core/Logger.h"
#include "gpu/Pipeline.h"

TEST_CASE("Create Pipeline", "[runtime.gpu.Pipeline]")
{
    using namespace ux3d;

    //    gpu::LogicalDevice logicalDevice;
    //    core::Slice<gpu::Queue, core::HeapAllocator> queues;
    //    gpu::LogicalDeviceCreateInfo logicalDeviceCreateInfo;
    //    logicalDeviceCreateInfo.usedFeatures.graphicsSupport = true;
    //    core::Slice<gpu::QueueCreateInfo, core::HeapAllocator> queueInfos(allocator, 1, gpu::QueueCreateInfo());
    //    gpu::createLogicalDevice(allocator, physicalDevices[0], logicalDeviceCreateInfo, queueInfos, logicalDevice, queues);
    //
    //    gpu::PipelineLayout pipelineLayout;
    //    gpu::PipelineLayoutCreateInfo<core::HeapAllocator> pipelineLayoutCreateInfo;
    //    gpu::createPipelineLayout(logicalDevice, pipelineLayoutCreateInfo, pipelineLayout, allocator);
    //
    //
    //    gpu::Pipeline pipeline;
    //    gpu::GraphicsPipelineCreateInfo<core::HeapAllocator> pipelineCreateInfo;
    //    gpu::Viewport viewport;
    //    pipelineCreateInfo.viewportState.viewports.allocate(allocator, 1);
    //    pipelineCreateInfo.viewportState.viewports[0] = viewport;
    //    pipelineCreateInfo.viewportState.scissors.allocate(allocator, 1);
    //    pipelineCreateInfo.viewportState.scissors[0].extent = {};
    //    pipelineCreateInfo.viewportState.scissors[0].offset = {};
    //    pipelineCreateInfo.layout = pipelineLayout;
    //    gpu::createGraphicsPipeline(logicalDevice, pipelineCreateInfo, pipeline, allocator);
}
