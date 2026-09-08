// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "Types.h"
#include "core/Vector.h"

namespace ux3d::gpu {
// forward decls:
struct PhysicalDevice;

struct Surface {
    Handle _handle { InvalidHandle };
};

struct SurfaceCapabilities {
    enum class Transform {
        Identity = 0x00000001,
        Rotate_90 = 0x00000002,
        Rotate_180 = 0x00000004,
        Rotate_270 = 0x00000008,
        Mirror = 0x00000010,
        MirrorRotate_90 = 0x00000020,
        MirrorRotate_180 = 0x00000040,
        MirrorRotate_270 = 0x00000080,
    };

    uint32_t minImageCount;
    uint32_t maxImageCount;
    Extent2d currentExtent;
    //			Extent2d minImageExtent;
    //			Extent2d maxImageExtent;
    Transform currentTransform;
};

// TODO: we might need to query this on queue family level if there are devices with differing surface support between queue families
GpuResult physicalDeviceCanPresentToSurface(
    const PhysicalDevice& physicalDevice,
    const Surface& surface,
    bool& outCanPresent);

GpuResult getSurfaceCapabilities(
    const PhysicalDevice& physicalDevice,
    const Surface& surface,
    SurfaceCapabilities& outSurfaceCapabilities);

enum class ColorSpace : uint32_t {
    sRGBNonLinear = 0,
};

struct SurfaceFormat {
    Format format;
    ColorSpace colorSpace;
};

GpuResult getSurfaceFormats(
    const PhysicalDevice& physicalDevice,
    const Surface& surface, core::Vector<SurfaceFormat>& outSurfaceFormats, core::IAllocator* allocator);

enum class PresentMode : uint32_t {
    Immediate = 0u,
    Mailbox = 1u,
    FIFO = 2u,
    FIFO_Relaxed = 3u
};

GpuResult getSurfacePresentMode(
    const PhysicalDevice& physicalDevice,
    const Surface& surface,
    core::Vector<PresentMode>& outPresentModes,
    core::IAllocator* allocator);
}
