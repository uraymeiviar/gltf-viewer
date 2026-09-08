// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gpu/Surface.h"
#include "gpu/PhysicalDevice.h"

#include "gpu/impl_vulkan/Result.h"
#include <vulkan/vulkan.h>

#include "core/HeapAllocator.h"
#include "core/LinearAllocator.h"
#include "core/Slice.h"

ux3d::gpu::GpuResult ux3d::gpu::physicalDeviceCanPresentToSurface(
    const PhysicalDevice& _physicalDevice,
    const Surface& _surface,
    bool& _outCanPresent)
{
    const auto vkPhysicalDevice = reinterpret_cast<VkPhysicalDevice>(_physicalDevice._handle);
    const auto vkSurface = reinterpret_cast<VkSurfaceKHR>(_surface._handle);

    uint32_t queueFamilyCount = 0u;
    vkGetPhysicalDeviceQueueFamilyProperties(vkPhysicalDevice, &queueFamilyCount, nullptr);

    VulkanResult result = GpuResult::Success;

    if (queueFamilyCount > 0u) {
        VkBool32 presentSupport = VK_FALSE;
        result = vkGetPhysicalDeviceSurfaceSupportKHR(vkPhysicalDevice, 0u, vkSurface, &presentSupport);
        _outCanPresent = presentSupport == VK_TRUE;
    } else {
        _outCanPresent = false;
    }

    return result;
}

ux3d::gpu::GpuResult ux3d::gpu::getSurfaceCapabilities(
    const PhysicalDevice& _physicalDevice,
    const Surface& _surface,
    SurfaceCapabilities& _outSurfaceCapabilities)
{
    const auto vkPhysicalDevice = reinterpret_cast<VkPhysicalDevice>(_physicalDevice._handle);
    const auto vkSurface = reinterpret_cast<VkSurfaceKHR>(_surface._handle);

    VkSurfaceCapabilitiesKHR vkSurfaceCapabilities {};
    VulkanResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkPhysicalDevice, vkSurface, &vkSurfaceCapabilities);

    _outSurfaceCapabilities.minImageCount = vkSurfaceCapabilities.minImageCount;
    _outSurfaceCapabilities.maxImageCount = vkSurfaceCapabilities.maxImageCount;
    _outSurfaceCapabilities.currentExtent.width = vkSurfaceCapabilities.currentExtent.width;
    _outSurfaceCapabilities.currentExtent.height = vkSurfaceCapabilities.currentExtent.height;
    _outSurfaceCapabilities.currentTransform = static_cast<SurfaceCapabilities::Transform>(vkSurfaceCapabilities.currentTransform);

    return result;
}

ux3d::gpu::GpuResult ux3d::gpu::getSurfaceFormats(
    const PhysicalDevice& _physicalDevice,
    const Surface& _surface,
    core::Vector<SurfaceFormat>& _outSurfaceFormats,
    core::IAllocator* _allocator)
{
    auto vkPhysicalDevice = reinterpret_cast<const VkPhysicalDevice>(_physicalDevice._handle);
    auto vkSurface = reinterpret_cast<const VkSurfaceKHR>(_surface._handle);

    uint32_t formatCount = 0u;
    VulkanResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice, vkSurface, &formatCount, nullptr);

    if (result && formatCount > 0u) {
        core::Slice<VkSurfaceFormatKHR> vkSurfaceFormats;

        if (!vkSurfaceFormats.allocate(formatCount, _allocator)) {
            return GpuError::OutOfHostMemory;
        }

        if ((result = vkGetPhysicalDeviceSurfaceFormatsKHR(vkPhysicalDevice, vkSurface, &formatCount, vkSurfaceFormats.data())) != GpuResult::Success) {
            return result;
        }

        if (!_outSurfaceFormats.reserve(formatCount, _allocator)) {
            return GpuError::OutOfHostMemory;
        }

        for (const VkSurfaceFormatKHR& vkSurfaceFormat : vkSurfaceFormats) {
            SurfaceFormat* surfaceFormat = _outSurfaceFormats.emplace_back();
            surfaceFormat->format = static_cast<Format>(vkSurfaceFormat.format);
            surfaceFormat->colorSpace = static_cast<ColorSpace>(vkSurfaceFormat.colorSpace);
        }
    }

    return result;
}

ux3d::gpu::GpuResult ux3d::gpu::getSurfacePresentMode(
    const PhysicalDevice& _physicalDevice,
    const Surface& _surface,
    core::Vector<PresentMode>& _outPresentModes,
    core::IAllocator* _allocator)
{
    auto vkPhysicalDevice = reinterpret_cast<const VkPhysicalDevice>(_physicalDevice._handle);
    auto vkSurface = reinterpret_cast<const VkSurfaceKHR>(_surface._handle);

    uint32_t presentModeCount = 0u;
    VulkanResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice, vkSurface, &presentModeCount, nullptr);

    if (result && presentModeCount > 0u) {
        core::Slice<VkPresentModeKHR> vkPresentModes;

        if (!vkPresentModes.allocate(presentModeCount, _allocator)) {
            return GpuError::OutOfHostMemory;
        }

        if ((result = vkGetPhysicalDeviceSurfacePresentModesKHR(vkPhysicalDevice, vkSurface, &presentModeCount, vkPresentModes.data())) != GpuResult::Success) {
            return result;
        }

        if (!_outPresentModes.reserve(presentModeCount, _allocator)) {
            return GpuError::OutOfHostMemory;
        }

        for (const VkPresentModeKHR& presentMode : vkPresentModes) {
            (void)_outPresentModes.emplace_back(static_cast<PresentMode>(presentMode));
        }
    }

    return result;
}
