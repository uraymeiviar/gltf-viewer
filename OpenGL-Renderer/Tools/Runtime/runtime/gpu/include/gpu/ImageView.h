// Copyright (c) 2019 UX3D GmbH. All rights reserved.

//
//  ImageView.h
//  middleware
//
//  Created by Jim Eckerlein on 7.06.2019.
//

#pragma once

#include "Image.h"

namespace ux3d::gpu {
enum class ImageViewType : uint32_t {
    OneDimensional = 0,
    TwoDimensional = 1,
    ThreeDimensional = 2,
    Cube = 3,
    Array1D = 4,
    Array2D = 5,
    CubeArray = 6,
};

enum class ComponentSwizzle : uint32_t {
    Identity = 0,
    Zero = 1,
    One = 2,
    R = 3,
    G = 4,
    B = 5,
    A = 6,
};

struct ComponentMapping {
    ComponentSwizzle r {};
    ComponentSwizzle g {};
    ComponentSwizzle b {};
    ComponentSwizzle a {};
};

enum class AspectFlagBits : uint32_t {
    None = 0,
    Color = 0x00000001,
    Depth = 0x00000002,
    Stencil = 0x00000004,
    MetaData = 0x00000008,
    Plane0 = 0x00000010,
    Plane1 = 0x00000020,
    Plane2 = 0x00000040,
    MemoryPlane0_EXT = 0x00000080,
    MemoryPlane1_EXT = 0x00000100,
    MemoryPlane2_EXT = 0x00000200,
    MemoryPlane3_EXT = 0x00000400,
    Plane0_KHR = Plane0,
    Plane1_KHR = Plane1,
    Plane2_KHR = Plane2,
};

using AspectFlag = core::Flag<AspectFlagBits>;

struct ImageSubresource {
    AspectFlag aspectMask { AspectFlagBits::Color };
    uint32_t mip { 0 };
    uint32_t layer { 0 };
};

struct ImageSubresourceRange {
    AspectFlag aspectMask { AspectFlagBits::Color };
    uint32_t baseMip { 0 };
    uint32_t mipCount { 1 };
    uint32_t baseLayer { 0 };
    uint32_t layerCount { 1 };
};

struct ImageSubresourceLayers {
    AspectFlag aspectMask { AspectFlagBits::Color };
    uint32_t mip { 0 };
    uint32_t baseLayer { 0 };
    uint32_t layerCount { 1 };
};

struct ImageViewCreateInfo {
    enum class CreateFlagBits : uint32_t {
        None = 0,
        FragmentDensityMapDynamic_EXT = 0x00000001,
    };
    using CreateFlag = core::Flag<CreateFlagBits>;

    CreateFlag flags;
    Image image;
    ImageViewType viewType;
    Format format;
    ComponentMapping componentMapping;
    ImageSubresourceRange subresourceRange;
};

struct ImageView {
    Handle _handle = InvalidHandle;
    LogicalDevice _device;
};

GpuResult createImageView(
    const LogicalDevice& device,
    const ImageViewCreateInfo& createInfo,
    ImageView& outImageView);

void destroyImageView(
    ImageView& imageView);

}
