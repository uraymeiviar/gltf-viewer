// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
// Created by jim on 9/11/19.
//

#include "gpu/Swapchain.h"
#include "gpu/PhysicalDevice.h"
#include "gpu/Semaphore.h"
#include "gpu/Surface.h"
//#include "Queue.h"

#include <algorithm>
#include <limits>

#include "core/HeapAllocator.h"
#include "core/Slice.h"

#include <vulkan/vulkan.h>

namespace ux3d::gpu {
static ux3d::gpu::SurfaceFormat
pickSurfaceFormat(
    const ux3d::gpu::PhysicalDevice& physicalDevice,
    const ux3d::gpu::Surface& surface)
{
    core::Vector<SurfaceFormat> formats;
    getSurfaceFormats(physicalDevice, surface, formats, core::HeapAllocator::defaultInstance());

    // Try to find the preferred format.
    for (const auto format : formats) {
        if (format.format == Format::B8G8R8A8_UNORM && format.colorSpace == ColorSpace::sRGBNonLinear) {
            return format;
        }
    }

    return formats[0];
}

static ux3d::gpu::PresentMode
pickPresentMode(
    const ux3d::gpu::PhysicalDevice& physicalDevice,
    const ux3d::gpu::Surface& surface)
{
    core::Vector<PresentMode> modes;
    getSurfacePresentMode(physicalDevice, surface, modes, core::HeapAllocator::defaultInstance());

    // Try to find the preferred mode.
    for (const auto& mode : modes) {
        if (mode == PresentMode::Mailbox) {
            return mode;
        }
    }

    return PresentMode::FIFO;
}

static uint32_t
pickImageCount(const SurfaceCapabilities& capabilities, const uint32_t preferred)
{
    const uint32_t maximum = capabilities.maxImageCount > 0 ? capabilities.maxImageCount : UINT32_MAX;
    return std::clamp(preferred, capabilities.minImageCount, maximum);
}
}

ux3d::gpu::GpuResult
ux3d::gpu::createSwapchain(
    const PhysicalDevice& _physicalDevice,
    const LogicalDevice& _device,
    const Surface& _surface,
    const uint32_t _preferredLength,
    Swapchain& _outSwapchain)
{
    SurfaceCapabilities capabilities;
    getSurfaceCapabilities(_physicalDevice, _surface, capabilities);

    const auto format = pickSurfaceFormat(_physicalDevice, _surface);
    const auto mode = pickPresentMode(_physicalDevice, _surface);
    const auto extent = capabilities.currentExtent; // TODO: Edge case when `currentExtent` is -1 or 0.
    const auto length = pickImageCount(capabilities, _preferredLength);

    VkSwapchainCreateInfoKHR info {
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        nullptr, {},
        reinterpret_cast<VkSurfaceKHR>(_surface._handle),
        length,
        static_cast<VkFormat>(format.format),
        static_cast<VkColorSpaceKHR>(format.colorSpace),
        { extent.width, extent.height },
        1,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0, nullptr,
        static_cast<VkSurfaceTransformFlagBitsKHR>(capabilities.currentTransform),
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        static_cast<VkPresentModeKHR>(mode),
        VK_TRUE, // TODO: review
        VK_NULL_HANDLE
    };

    // TODO: Error handling
    VkSwapchainKHR swapchain;
    vkCreateSwapchainKHR(reinterpret_cast<VkDevice>(_device._handle), &info, nullptr, &swapchain);

    // Obtain images from swapchain.
    core::Slice<VkImage> images;
    core::Slice<VkImageView> imageViews;
    uint32_t count;
    vkGetSwapchainImagesKHR(
        reinterpret_cast<VkDevice>(_device._handle),
        swapchain, &count, nullptr);
    if (!images.allocate(count, core::HeapAllocator::defaultInstance())) {
        return GpuError::OutOfHostMemory;
    }
    vkGetSwapchainImagesKHR(
        reinterpret_cast<VkDevice>(_device._handle),
        swapchain, &count, images.data());

    if (!_outSwapchain._images.resize(count, core::HeapAllocator::defaultInstance())) {
        return GpuError::OutOfHostMemory;
    }

    for (size_t i = 0; i < count; ++i) {
        _outSwapchain._images[i]._handle = reinterpret_cast<Handle>(images[i]);
    }

    _outSwapchain._device = _device;
    _outSwapchain._handle = reinterpret_cast<Handle>(swapchain);

    return GpuResult::Success;
}

void ux3d::gpu::destroySwapchain(ux3d::gpu::Swapchain& _swapchain)
{
    if (_swapchain._handle != InvalidHandle) {
        vkDestroySwapchainKHR(
            reinterpret_cast<VkDevice>(_swapchain._device._handle),
            reinterpret_cast<VkSwapchainKHR>(_swapchain._handle),
            nullptr);

        _swapchain._handle = InvalidHandle;
    }
}

ux3d::gpu::GpuResult
ux3d::gpu::acquireNextImage(
    const Swapchain& _swapchain,
    const Semaphore& _imageAvailableSemaphore,
    uint32_t& _outIndex)
{
    const auto result = vkAcquireNextImageKHR(
        reinterpret_cast<VkDevice>(_swapchain._device._handle),
        reinterpret_cast<VkSwapchainKHR>(_swapchain._handle),
        std::numeric_limits<uint64_t>::max(),
        reinterpret_cast<VkSemaphore>(_imageAvailableSemaphore._handle),
        VK_NULL_HANDLE,
        &_outIndex);

    if (result == VK_SUCCESS) {
        return GpuResult::Success;
    } else {
        return GpuError(result);
    }
}

ux3d::gpu::GpuResult
ux3d::gpu::present(
    const Swapchain& _swapchain,
    const uint32_t _idx,
    const Semaphore& _waitSemaphores,
    const Queue& _queue)
{
    VkPresentInfoKHR info {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.swapchainCount = 1;
    info.pSwapchains = reinterpret_cast<const VkSwapchainKHR*>(&_swapchain._handle);
    info.pImageIndices = &_idx;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = reinterpret_cast<const VkSemaphore*>(&_waitSemaphores._handle);

    const auto result = vkQueuePresentKHR(
        reinterpret_cast<VkQueue>(_queue._handle),
        &info);

    if (result == VK_SUCCESS) {
        return GpuResult::Success;
    } else {
        return GpuError(result);
    }
}
