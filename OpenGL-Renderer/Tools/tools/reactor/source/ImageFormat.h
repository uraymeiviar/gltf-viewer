// Copyright © 2022 UX3D GmbH. All rights reserved.
// Adapted version of previous /tools/runtime/image file

#pragma once

#include "reactor/GpuTypes.h"
#include <cstdint>
#include <nlohmann/json.hpp>

namespace ux3d::reactor::image {
enum class Channel : uint32_t {
    RED = 0u,
    GREEN = 1u,
    BLUE = 2u,
    ALPHA = 3u,
};

// as defined by KTX (channel size)
uint32_t getTypeSize(gpu::Format _vkFormat);

// as defined by vulkan (element size, block or texel)
uint32_t getFormatSize(gpu::Format _vkFormat);

// get number of pixels (width and hight) per block, returns if format is not BLOCK
bool getBlockSize(gpu::Format _vkFormat, uint32_t& _outWidth, uint32_t& _outHeight);

uint32_t getChannelCount(gpu::Format _vkFormat);

uint32_t getChannelSize(gpu::Format _vkFormat, uint32_t _channelIndex);

int32_t getChannelIndex(gpu::Format _vkFormat, Channel _channel);

bool isFloat(gpu::Format _vkFormat);

bool isSigned(gpu::Format _vkFormat);

bool isNormalized(gpu::Format _vkFormat);

bool isSrgb(gpu::Format _vkFormat);

bool isPacked(gpu::Format _vkFormat);

bool isCompressed(gpu::Format _vkFormat);

uint64_t getFaceSize(gpu::Format _vkFormat, uint32_t _level, uint32_t _width, uint32_t _height, uint32_t _depth = 0u);

// computes the pixel count (resolution) of an image of the given level
uint32_t getPixelCount(uint32_t _level, uint32_t _width, uint32_t _height, uint32_t _depth);

uint32_t getPadding(uint64_t _value, uint32_t _alginment);

// greedy least common multiple
uint32_t getLCM(uint32_t _x, uint32_t _y);

// _value / offset to apply lcm4 padding to (if applicable)
uint32_t getMipPadding(uint64_t _value, gpu::Format _vkFormat, bool _superCompression);

size_t getOffset(
    const nlohmann::json& ImageDataInfo,
    const uint32_t _InX, const uint32_t _InY, const uint32_t _InZ,
    const uint32_t _InFace, const uint32_t _InArray, const uint32_t _InLevel);

}
