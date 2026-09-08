// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gpu/LogicalDevice.h"
#include "gpu/Allocator.h"

#include "core/Allocator.h"
#include "core/HeapAllocator.h"
#include "core/LinearAllocator.h"
#include "core/Logger.h"
#include "core/Slice.h"
#include "core/StaticVector.h"

#include <vulkan/vulkan.h>

#include "gpu/impl_vulkan/Result.h"

namespace ux3d::gpu {
struct VkDeviceQueueCreateInfoData {
    static constexpr size_t maxQueueCount = 64u;
    core::StaticVector<float, maxQueueCount> priorities;
};

VulkanResult selectQueueCreateInfos(
    const PhysicalDevice& _physicalDevice,
    const core::Vector<QueueCreateInfo>& _queueCreateInfos,
    core::Slice<VkDeviceQueueCreateInfo>& _vkQueueCreateInfos,
    core::Vector<VkDeviceQueueCreateInfoData>& _data,
    core::IAllocator* _allocator)
{
    core::Slice<VkQueueFamilyProperties> vkQueueFamilyProperties;

    // List available queue families
    auto device = reinterpret_cast<VkPhysicalDevice>(_physicalDevice._handle);
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    if (!vkQueueFamilyProperties.allocate(queueFamilyCount, _allocator)) {
        return GpuError::OutOfHostMemory;
    }

    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, vkQueueFamilyProperties.data());

    // allocate the output
    if (!_vkQueueCreateInfos.resizeWithValue(_queueCreateInfos.size(), VkDeviceQueueCreateInfo {}, _allocator)) {
        return GpuError::OutOfHostMemory;
    }

    // reserve space for the data needed in the create info
    if (!_data.resize(queueFamilyCount, _allocator)) {
        return GpuError::OutOfHostMemory;
    }

    // Distribute requested queues over available queue families
    for (const auto& requestedQueue : _queueCreateInfos) {
        VkQueueFlags vkQueueFlag = {};
        if (requestedQueue.computeSupport) {
            vkQueueFlag |= VK_QUEUE_COMPUTE_BIT;
        }
        if (requestedQueue.graphicsSupport) {
            vkQueueFlag |= VK_QUEUE_GRAPHICS_BIT;
        }

        bool queueFamilyFound = false;
        for (uint32_t i = 0u; i < queueFamilyCount; ++i) {
            VkQueueFamilyProperties& vkQueueFamily = vkQueueFamilyProperties[i];

            if (vkQueueFamily.queueCount == 0u) {
                continue;
            }

            if ((vkQueueFamily.queueFlags & vkQueueFlag) != vkQueueFlag) {
                continue;
            }

            if (_data[i].priorities.size() >= _data[i].priorities.capacity()) {
                core::logError(
                    "Skipping queue family as the number of queues created on it would exceed the maximum of %d",
                    _data[i].priorities.capacity());
                continue;
            }

            // you can add additional requirements to the queue here

            // the requested queue can be created from this queue family --> accept
            vkQueueFamily.queueCount--;
            queueFamilyFound = true;
            VkDeviceQueueCreateInfo& vkQueueCreateInfo = _vkQueueCreateInfos[i];
            vkQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            vkQueueCreateInfo.queueFamilyIndex = i;
            vkQueueCreateInfo.queueCount++;
            _data[i].priorities.push_back(requestedQueue.priority);

            vkQueueCreateInfo.pQueuePriorities = _data[i].priorities.data();
            break;
        }

        if (!queueFamilyFound) {
            ux3d::core::log(ux3d::core::LogLevel::Error, "Could not find a queue family for requested queue");
            return GpuError::FeatureNotPresent;
        }
    }

    return VulkanResult::Success;
}

VkPhysicalDeviceFeatures convert(const PhysicalDeviceFeatures& _features)
{
    // TODO: implement
    (void)(_features);
    return VkPhysicalDeviceFeatures {};
}
}

ux3d::gpu::GpuResult ux3d::gpu::createLogicalDevice(
    const PhysicalDevice& _physicalDevice,
    const LogicalDeviceCreateInfo& _logicalDeviceCreateInfo,
    const core::Vector<QueueCreateInfo>& _queueInfos,
    LogicalDevice& _outLogicalDevice,
    core::Vector<Queue>& _outQueues,
    core::IAllocator* _allocator)
{
    core::Slice<VkDeviceQueueCreateInfo> vkQueueCreateInfos;
    //core::Vector<core::Vector<float, Allocator>, Allocator> queuePriorities(_allocator);
    core::Vector<VkDeviceQueueCreateInfoData> vkQueueCreateInfosData;

    VulkanResult result = selectQueueCreateInfos(_physicalDevice, _queueInfos, vkQueueCreateInfos,
        vkQueueCreateInfosData, _allocator);
    if (!result) {
        return std::move(result);
    }

    const VkPhysicalDeviceFeatures features = convert(_logicalDeviceCreateInfo.usedFeatures);

    core::StaticVector<const char*, 1u> enabledExtensions;
    if (_logicalDeviceCreateInfo.needsSwapchainSupport) {
        enabledExtensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    VkDeviceCreateInfo vkDeviceCreateInfo {};
    vkDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    vkDeviceCreateInfo.pQueueCreateInfos = vkQueueCreateInfos.data();
    vkDeviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(vkQueueCreateInfos.size());
    vkDeviceCreateInfo.pEnabledFeatures = &features;
    vkDeviceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();
    vkDeviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());

    VkDevice vkDevice = {};
    auto vkPhysicalDevice = reinterpret_cast<VkPhysicalDevice>(_physicalDevice._handle);
    result = vkCreateDevice(vkPhysicalDevice, &vkDeviceCreateInfo, nullptr, &vkDevice);
    if (!result) {
        ux3d::core::log(ux3d::core::LogLevel::Error, "Failed to create logical device");
        return std::move(result);
    }

    _outLogicalDevice._handle = reinterpret_cast<Handle>(vkDevice);

    // provide access to all queues created on the device
    if (!_outQueues.resize(_queueInfos.size(), _allocator)) {
        return GpuError::OutOfHostMemory;
    }

    auto itQueue = _outQueues.begin();
    for (const auto& vkQueueCreateInfo : vkQueueCreateInfos) {
        for (uint32_t queueIndex = 0u; queueIndex < vkQueueCreateInfo.queueCount; ++queueIndex) {
            VkQueue vkQueue;
            vkGetDeviceQueue(vkDevice, vkQueueCreateInfo.queueFamilyIndex, queueIndex, &vkQueue);
            itQueue->_handle = reinterpret_cast<Handle>(vkQueue);
            itQueue->_queueFamilyIndex = vkQueueCreateInfo.queueFamilyIndex;
            itQueue->_queueIndex = queueIndex;
            ++itQueue;
        }
    }

    return std::move(result);
}

void ux3d::gpu::destroyLogicalDevice(LogicalDevice& _logicalDevice)
{
    if (_logicalDevice._handle != InvalidHandle) {
        vkDestroyDevice(reinterpret_cast<VkDevice>(_logicalDevice._handle), nullptr);
        _logicalDevice._handle = InvalidHandle;
    }
}
