// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gpu/Instance.h"

#include <vulkan/vulkan.h>

#include <algorithm>

#include "gpu/impl_vulkan/Result.h"

#include "core/StaticVector.h"
#include "core/Vector.h"

#include "core/Allocator.h"
#include "core/HeapAllocator.h"
#include "core/LinearAllocator.h"

namespace ux3d::gpu {
bool convertInstance(const VkInstance& _vkInstance, Instance& _instance)
{
    _instance.backendType = BackendType::Vulkan;
    _instance._handle = reinterpret_cast<Handle>(_vkInstance);
    return true;
}

bool convertApplicationInfo(const ApplicationInfo& _info, VkApplicationInfo& _vkInfo)
{
    _vkInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    _vkInfo.pNext = nullptr;
    _vkInfo.pApplicationName = _info.appName.c_str();
    _vkInfo.applicationVersion = _info.appVersion;
    _vkInfo.pEngineName = _info.engineName.c_str();
    _vkInfo.engineVersion = _info.engineVersion;
    _vkInfo.apiVersion = VK_API_VERSION_1_1;

    return true;
}

VulkanResult checkLayerSupport(
    const core::Vector<core::string>& _requestedLayers,
    core::Vector<const char*>& _foundLayers,
    core::IAllocator* _allocator)
{
    uint32_t layerCount = 0u;
    core::Vector<VkLayerProperties> availableLayers;
    if (!_foundLayers.reserve(_requestedLayers.size(), _allocator)) {
        core::logError("Found layers resize: Out of host memory");
        return GpuError::OutOfHostMemory;
    }

    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    if (!availableLayers.resize(layerCount, _allocator)) {
        core::logError("Available layers resize: out of host memory");
        return GpuError::OutOfHostMemory;
    }

    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const core::string& requestedLayer : _requestedLayers) {
        auto layerNamePredicate = [&requestedLayer](const VkLayerProperties& _Properties) {
            core::string propName(_Properties.layerName);
            return propName == requestedLayer;
        };

        // layer support
        if (std::find_if(availableLayers.begin(), availableLayers.end(), layerNamePredicate) != availableLayers.end()) {
            (void)(_foundLayers.emplace_back(requestedLayer.c_str()));
        }
    }

    return VulkanResult::Success;
}
}

ux3d::gpu::GpuResult ux3d::gpu::createInstance(
    const ux3d::gpu::InstanceCreateInfo& _info,
    ux3d::gpu::Instance& _outInstance,
    core::IAllocator* _allocator)
{
    if (_outInstance._handle != InvalidHandle) {
        core::logError("GPU: tried to create instance twice");
        return GpuError::InvalidHandle;
    }

    VkApplicationInfo applicationInfo = {};
    convertApplicationInfo(_info.applicationInfo, applicationInfo);

    core::Vector<core::string> requiredLayers;
    if (_info.enableValidation && requiredLayers.reserve(1u, _allocator)) {
        (void)(requiredLayers.emplace_back("VK_LAYER_LUNARG_standard_validation"));
    }

    core::Vector<const char*> availableLayers;
    VulkanResult result = checkLayerSupport(requiredLayers, availableLayers, _allocator);
    if (!result) {
        return std::move(result);
    }
    if (availableLayers.size() != requiredLayers.size()) {
        core::logError("Required vulkan layer not available");
        return GpuError::LayerNotPresent;
    }

    core::Vector<const char*> enabledExtensions;
    if (false == enabledExtensions.reserve(5, core::HeapAllocator::defaultInstance())) {
        return GpuError::OutOfHostMemory;
    }

    if (_info.surfaceSupport) {
        (void)enabledExtensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME);
#ifdef UX3D_IS_APPLE
        (void)enabledExtensions.emplace_back("VK_MVK_macos_surface");
#endif
    }

    for (const auto& ext : _info.requiredExtensions) {
        if (nullptr == enabledExtensions.push_back(ext.c_str())) {
            return GpuError::OutOfHostMemory;
        }
    }

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &applicationInfo;
    // TODO: handle layers and extensions
    instanceCreateInfo.enabledLayerCount = availableLayers.size();
    instanceCreateInfo.ppEnabledLayerNames = availableLayers.data();
    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
    instanceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();

    VkInstance instance {};

    result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);

    if (result) {
        convertInstance(instance, _outInstance);
    }

    return std::move(result);
}

void ux3d::gpu::destroyInstance(Instance& _instance)
{
    if (_instance._handle != InvalidHandle) {
        vkDestroyInstance(reinterpret_cast<VkInstance>(_instance._handle), nullptr);
        _instance._handle = InvalidHandle;
    }
}
