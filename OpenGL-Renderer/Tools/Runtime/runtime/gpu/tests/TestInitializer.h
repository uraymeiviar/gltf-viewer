// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  Instance.h
//  middleware
//
//  Created by Moritz Becher on 13.06.19.
//

#pragma once

#include "core/Logger.h"
#include "core/MemoryUtilities.h"
#include "gpu/Instance.h"
#include "gpu/LogicalDevice.h"

namespace ux3d {
namespace gpu_tests {
    class Initializer {

    public:
        Initializer(core::IAllocator* allocator);
        ~Initializer();

        const gpu::Instance& createInstance();
        const gpu::LogicalDevice& selectDevice();

    private:
        void reset();

        core::IAllocator* m_allocator;
        gpu::Instance m_instance = {};
        gpu::LogicalDevice m_device = {};
    };

    inline Initializer::Initializer(core::IAllocator* allocator)
        : m_allocator(allocator)
    {
    }

    inline Initializer::~Initializer()
    {
        reset();
    }

    inline const gpu::Instance& Initializer::createInstance()
    {
        gpu::InstanceCreateInfo instanceCreateInfo;
        instanceCreateInfo.applicationInfo.appName = "Unit Testing";
        gpu::createInstance(instanceCreateInfo, m_instance, m_allocator);
        return m_instance;
    }

    inline const gpu::LogicalDevice& Initializer::selectDevice()
    {
        createInstance();

        core::Vector<gpu::PhysicalDevice> physicalDevices;
        gpu::enumeratePhysicalDevices(m_instance, physicalDevices, m_allocator);

        gpu::PhysicalDevice* pSelectedPhysicalDevice = nullptr;
        for (auto physicalDevice : physicalDevices) {
            gpu::PhysicalDeviceProperties properties;
            gpu::getPhysicalDeviceProperties(physicalDevice, properties, m_allocator);
            if (false == properties.hasSwapchainSupport) {
                continue;
            }

            pSelectedPhysicalDevice = &physicalDevice;
        }

        if (pSelectedPhysicalDevice == nullptr) {
            core::log(core::LogLevel::Fatal, "Could not find a GPU with swapchain support");
        }

        gpu::LogicalDeviceCreateInfo logicalDeviceCreateInfo;
        core::Vector<gpu::Queue> queues;
        logicalDeviceCreateInfo.usedFeatures.graphicsSupport = true;
        core::Vector<gpu::QueueCreateInfo> queueInfos(m_allocator);
        queueInfos.push_back(gpu::QueueCreateInfo());
        gpu::createLogicalDevice(physicalDevices[0], logicalDeviceCreateInfo, queueInfos, m_device, queues, m_allocator);

        return m_device;
    }

    inline void Initializer::reset()
    {
        //  add additional steps ahead

        if (m_device._handle != gpu::InvalidHandle) {
            gpu::destroyLogicalDevice(m_device);
        }

        // destroy the instance if it exists
        if (m_instance._handle != gpu::InvalidHandle) {
            gpu::destroyInstance(m_instance);
        }
    }
}
}
