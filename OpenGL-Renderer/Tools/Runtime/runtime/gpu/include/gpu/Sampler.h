// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "LogicalDevice.h"

namespace ux3d::gpu {
enum class SamplerMipmapMode {
    Nearest = 0,
    Linear = 1,
};

enum class SamplerAddressMode {
    Repeat = 0,
    MirroredRepeat = 1,
    ClampToEdge = 2,
    ClampToBorder = 3,
    MirrorClampToEdge = 4,
};

enum class BorderColor {
    FloatTransparentBlack = 0,
    IntTransparentBlack = 1,
    FloatOpaqueBlack = 2,
    IntOpaqueBlack = 3,
    FloatOpaqueWhite = 4,
    IntOpaqueWhite = 5,
};

struct SamplerCreateInfo {
    enum class CreateFlagBits : uint32_t {
        None = 0,
        Subsampled = 0x1,
        SubsampledCoarseReconstruction_EXT = 0x2
    };

    using CreateFlag = core::Flag<CreateFlagBits>;

    CreateFlag flags {};
    Filter magFilter { Filter::Nearest };
    Filter minFilter { Filter::Nearest };
    SamplerMipmapMode mipmapMode { SamplerMipmapMode::Nearest };
    SamplerAddressMode addressModeU { SamplerAddressMode::ClampToEdge };
    SamplerAddressMode addressModeV { SamplerAddressMode::ClampToEdge };
    SamplerAddressMode addressModeW { SamplerAddressMode::ClampToEdge };
    float mipLodBias { 0.0f };
    bool anisotropyEnable { false };
    float maxAnisotropy { 0.0f };
    bool compareEnable { false };
    CompareOp compareOp { CompareOp::Always };
    float minLod { 0.0f };
    float maxLod { 0.0f };
    BorderColor borderColor {};
    bool unnormalizedCoordinates { false };
};

struct Sampler {
    Handle _handle = InvalidHandle;
    LogicalDevice _device;
};

GpuResult createSampler(
    const LogicalDevice& _device,
    const SamplerCreateInfo& _info,
    Sampler& _outSampler);

void destroySampler(
    Sampler& _sampler);

}
