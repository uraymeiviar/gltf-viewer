// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "Image.h"
#include "LogicalDevice.h"
#include "core/Vector.h"

namespace ux3d::gpu {
// forward decls
struct PhysicalDevice;
struct Surface;
struct Semaphore;
struct Queue;

struct Swapchain {
    LogicalDevice _device;
    Handle _handle { InvalidHandle };
    core::Vector<Image> _images;

    size_t length() const
    {
        return _images.size();
    }
};

GpuResult createSwapchain(
    const PhysicalDevice& _physicalDevice,
    const LogicalDevice& _device,
    const Surface& _surface,
    uint32_t _preferredLength,
    Swapchain& _outSwapchain);

void destroySwapchain(Swapchain& _swapchain);

GpuResult acquireNextImage(
    const Swapchain& _swapchain,
    const Semaphore& _imageAvailableSemaphore,
    uint32_t& _outIndex);

GpuResult present(
    const Swapchain& _swapchain,
    uint32_t _idx,
    const Semaphore& _waitSemaphores,
    const Queue& _queue);
}
