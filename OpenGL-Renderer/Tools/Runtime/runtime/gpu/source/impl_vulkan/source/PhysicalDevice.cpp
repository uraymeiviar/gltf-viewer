// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gpu/PhysicalDevice.h"
#include "gpu/Instance.h"

#include "core/Allocator.h"
#include "core/HeapAllocator.h"
#include "core/LinearAllocator.h"
#include "core/Slice.h"

#include <algorithm>
#include <vulkan/vulkan.h>

#include "gpu/impl_vulkan/Result.h"

ux3d::gpu::GpuResult ux3d::gpu::enumeratePhysicalDevices(
    const Instance& _instance,
    core::Vector<PhysicalDevice>& _outPhysicalDevices,
    core::IAllocator* _allocator)
{
    uint32_t physicalDeviceCount;
    VulkanResult result = vkEnumeratePhysicalDevices(reinterpret_cast<VkInstance>(_instance._handle),
        &physicalDeviceCount, nullptr);
    if (!result) {
        return std::move(result);
    }

    core::Slice<VkPhysicalDevice> vkPhysicalDevices;

    if (!vkPhysicalDevices.allocate(physicalDeviceCount, _allocator)) {
        return GpuError::OutOfHostMemory;
    }

    result = vkEnumeratePhysicalDevices(reinterpret_cast<VkInstance>(_instance._handle), &physicalDeviceCount,
        vkPhysicalDevices.data());
    if (!result) {
        return std::move(result);
    }

    if (!_outPhysicalDevices.reserve(physicalDeviceCount, _allocator)) {
        return GpuError::OutOfHostMemory;
    }

    if (false == _outPhysicalDevices.reserve(vkPhysicalDevices.size(), _allocator)) {
        return GpuError::OutOfHostMemory;
    }
    for (const VkPhysicalDevice& vkPhysicalDevice : vkPhysicalDevices) {
        _outPhysicalDevices.emplace_back()->_handle = reinterpret_cast<Handle>(vkPhysicalDevice);
    }

    return std::move(result);
}

namespace ux3d {
namespace gpu {
    PhysicalDeviceType convert(const VkPhysicalDeviceType _type)
    {
        return static_cast<PhysicalDeviceType>(_type);
    }

    PhysicalDeviceLimits convert(const VkPhysicalDeviceLimits& _limits)
    {
        (void)(_limits);
        // TODO: add some fields
        return PhysicalDeviceLimits {};
    }

    PhysicalDeviceFeatures convert(const VkPhysicalDeviceFeatures& _features)
    {
        (void)(_features);
        // TODO: add some fields
        return PhysicalDeviceFeatures {};
    }

    GpuResult getPhysicalDeviceQueueFamilyProperties(
        const VkPhysicalDevice& _vkPhysicalDevice,
        core::Slice<VkQueueFamilyProperties>& _properties,
        core::IAllocator* _allocator)
    {
        uint32_t queueFamilyCount = 0u;
        vkGetPhysicalDeviceQueueFamilyProperties(_vkPhysicalDevice, &queueFamilyCount, nullptr);

        if (_properties.allocate(queueFamilyCount, _allocator)) {
            vkGetPhysicalDeviceQueueFamilyProperties(_vkPhysicalDevice, &queueFamilyCount, _properties.data());
            return GpuResult::Success;
        }

        return GpuError::OutOfHostMemory;
    }

    GpuResult enumerateExtensions(
        const VkPhysicalDevice& _vkPhysicalDevice,
        core::Slice<VkExtensionProperties>& _extensions,
        core::IAllocator* _allocator)
    {
        uint32_t extensionCount = 0u;
        vkEnumerateDeviceExtensionProperties(_vkPhysicalDevice, nullptr, &extensionCount, nullptr);

        if (_extensions.allocate(extensionCount, _allocator)) {
            vkEnumerateDeviceExtensionProperties(_vkPhysicalDevice, nullptr, &extensionCount, _extensions.data());
            return GpuResult::Success;
        }

        return GpuError::OutOfHostMemory;
    }
}
}

ux3d::gpu::GpuResult ux3d::gpu::getPhysicalDeviceProperties(
    const PhysicalDevice& _physicalDevice,
    PhysicalDeviceProperties& _outPhysicalDeviceProperties,
    core::IAllocator* _allocator)
{
    _outPhysicalDeviceProperties = {};

    VkPhysicalDevice vkDevice = reinterpret_cast<VkPhysicalDevice>(_physicalDevice._handle);
    VkPhysicalDeviceProperties vkProperties = {};
    VkPhysicalDeviceFeatures vkFeatures = {};

    vkGetPhysicalDeviceProperties(vkDevice, &vkProperties);
    vkGetPhysicalDeviceFeatures(vkDevice, &vkFeatures);

    _outPhysicalDeviceProperties.driverVersion = vkProperties.driverVersion;
    _outPhysicalDeviceProperties.vendorID = vkProperties.vendorID;
    _outPhysicalDeviceProperties.deviceID = vkProperties.deviceID;
    _outPhysicalDeviceProperties.deviceType = convert(vkProperties.deviceType);
    _outPhysicalDeviceProperties.limits = convert(vkProperties.limits);
    _outPhysicalDeviceProperties.features = convert(vkFeatures);

    // check supported queue families
    ux3d::core::Slice<VkQueueFamilyProperties> queueFamilyProperties(_allocator);
    GpuResult result = getPhysicalDeviceQueueFamilyProperties(vkDevice, queueFamilyProperties, _allocator);

    if (!result) {
        return result;
    }

    for (const VkQueueFamilyProperties& prop : queueFamilyProperties) {
        if (prop.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            _outPhysicalDeviceProperties.features.graphicsSupport = true;
        }
        if (prop.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            _outPhysicalDeviceProperties.features.computeSupport = true;
        }
    }

    // check extensions
    core::Slice<VkExtensionProperties> vkExtensions;
    result = enumerateExtensions(vkDevice, vkExtensions, _allocator);

    if (!result) {
        return result;
    }

    auto swapchainNamePredicate = [](const VkExtensionProperties& _Properties) {
        core::string propName(_Properties.extensionName);
        return propName == VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    };

    // swapchain support
    if (std::find_if(vkExtensions.begin(), vkExtensions.end(), swapchainNamePredicate) != vkExtensions.end()) {
        _outPhysicalDeviceProperties.hasSwapchainSupport = true;
    }

    return result;
}
