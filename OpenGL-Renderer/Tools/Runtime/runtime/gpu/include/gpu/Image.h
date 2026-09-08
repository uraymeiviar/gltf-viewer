// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  Image.h
//  middleware
//
//  Created by Jim Eckerlein on 7.06.2019.
//

#pragma once

#include "core/Flag.h"
#include "core/Vector.h"

#include "Allocator.h"
#include "LogicalDevice.h"

namespace ux3d::gpu {
// forward decls
struct AllocationCreateInfo;

enum class ImageType : uint32_t {
    OneDimensional = 0,
    TwoDimensional = 1,
    ThreeDimensional = 2,
};

enum class ImageTiling : uint32_t {
    Optimal = 0,
    Linear = 1,
    DrmFormatModifier_EXT = 1000158000
};

struct ImageCreateInfo {
    enum class CreateFlagBits : uint32_t {
        None = 0,
        SparseBinding = 0x00000001,
        SparceResidency = 0x00000002,
        SparceAliased = 0x00000004,
        MutableFormat = 0x00000008,
        CubeCompatible = 0x00000010,
        Alias = 0x00000400,
        SplitInstanceBindRegions = 0x00000040,
        Array2DCompatible = 0x00000020,
        BlockTexelViewCompatible = 0x00000080,
        ExtendedUsage = 0x00000100,
        Protected = 0x00000800,
        Disjoint = 0x00000200,
        CornerSampled_NV = 0x00002000,
        SampleLocationsCompatibleDepth_EXT = 0x00001000,
        SubsampledExt = 0x00004000,
        SplitInstanceBindRegions_KHR = SplitInstanceBindRegions,
        Array2DCompatible_KHR = Array2DCompatible,
        BlockTexelViewCompatible_KHR = BlockTexelViewCompatible,
        ExtendedUsage_KHR = ExtendedUsage,
        Disjoint_KHR = Disjoint,
        Alias_KHR = Alias,
    };
    using CreateFlag = core::Flag<CreateFlagBits>;

    enum class UsageFlagBits : uint32_t {
        None = 0,
        TransferSrc = 0x00000001,
        TransferDst = 0x00000002,
        Sampled = 0x00000004,
        Storage = 0x00000008,
        ColorAttachment = 0x00000010,
        DepthStencilAttachment = 0x00000020,
        TransientAttachment = 0x00000040,
        InputAttachment = 0x00000080,
        ShadingRateImage_NV = 0x00000100,
        FragmentDensityMap_EXT = 0x00000200
    };
    using UsageFlag = core::Flag<UsageFlagBits>;

    CreateFlag flags;
    ImageType type;
    Format format;
    Extent3d extent;
    uint32_t mipLevels { 1 };
    uint32_t arrayLayers { 1 };
    SampleCount sampleCount { SampleCount::One };
    ImageTiling tiling { ImageTiling::Optimal };
    UsageFlag usage;
    SharingMode sharingMode;
    core::Vector<uint32_t> queueFamilyIndices;
    ImageLayout initialLayout;
};

struct Image {
    Handle _handle = InvalidHandle;
    GPUAllocator _gpuAllocator;
    GPUAllocation _gpuAllocation;
};

GpuResult createImage(
    GPUAllocator& gpuAllocator,
    const ImageCreateInfo& createInfo,
    Image& outImage,
    const AllocationCreateInfo& allocationCreateInfo,
    core::IAllocator* allocator);

void destroyImage(Image& image);
} // !ux3d
