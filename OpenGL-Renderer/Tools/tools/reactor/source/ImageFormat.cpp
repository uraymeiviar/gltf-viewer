// Copyright © 2022 UX3D GmbH. All rights reserved.
// Previous /tools/runtime/image file

#include "ImageFormat.h"
#include <algorithm>
#include <cmath>

using namespace ux3d;
using namespace ux3d::reactor;

uint32_t image::getTypeSize(gpu::Format _vkFormat)
{
    if ((_vkFormat == gpu::Format::UNDEFINED) || (_vkFormat == gpu::Format::R4G4_UNORM_PACK8) || (_vkFormat >= gpu::Format::R8_UNORM && _vkFormat <= gpu::Format::B8G8R8A8_SRGB)) {
        return 1u;
    } else if ((_vkFormat >= gpu::Format::R16_UNORM && _vkFormat <= gpu::Format::R16G16B16A16_SFLOAT) || (_vkFormat >= gpu::Format::R4G4B4A4_UNORM_PACK16 && _vkFormat <= gpu::Format::A1R5G5B5_UNORM_PACK16)) {
        return 2u;
    } else if ((_vkFormat >= gpu::Format::R32_UINT && _vkFormat <= gpu::Format::R32G32B32A32_SFLOAT) || (_vkFormat >= gpu::Format::A2R10G10B10_UNORM_PACK32 && _vkFormat <= gpu::Format::B10G11R11_UFLOAT_PACK32)) {
        return 4u;
    } else if (_vkFormat >= gpu::Format::R64_UINT && _vkFormat <= gpu::Format::R64G64B64A64_SFLOAT) {
        return 8u;
    } else if ((_vkFormat >= gpu::Format::BC1_RGB_UNORM_BLOCK && _vkFormat <= gpu::Format::ASTC_12x12_SRGB_BLOCK) || (_vkFormat >= gpu::Format::PVRTC1_2BPP_UNORM_BLOCK_IMG && _vkFormat <= gpu::Format::PVRTC2_4BPP_SRGB_BLOCK_IMG)) {
        return 1u; // "For formats whose Vulkan names have the suffix _BLOCK it must equal 1"
    }

    // TODO: Implement depth/stencil formats

    return 0u; // invalid
}

uint32_t image::getFormatSize(gpu::Format _vkFormat)
{
    switch (_vkFormat) {
        // 1 BYTE
    case gpu::Format::R4G4_UNORM_PACK8:

    case gpu::Format::R8_UNORM:
    case gpu::Format::R8_SNORM:
    case gpu::Format::R8_UINT:
    case gpu::Format::R8_SINT:
    case gpu::Format::R8_SRGB:
        return 1u;

        // 2 BYTE
    case gpu::Format::R4G4B4A4_UNORM_PACK16:
    case gpu::Format::B4G4R4A4_UNORM_PACK16:
    case gpu::Format::R5G6B5_UNORM_PACK16:
    case gpu::Format::B5G6R5_UNORM_PACK16:
    case gpu::Format::R5G5B5A1_UNORM_PACK16:
    case gpu::Format::B5G5R5A1_UNORM_PACK16:
    case gpu::Format::A1R5G5B5_UNORM_PACK16:

    case gpu::Format::R8G8_UNORM:
    case gpu::Format::R8G8_SNORM:
    case gpu::Format::R8G8_UINT:
    case gpu::Format::R8G8_SINT:
    case gpu::Format::R8G8_SRGB:

    case gpu::Format::R16_UNORM:
    case gpu::Format::R16_SNORM:
    case gpu::Format::R16_UINT:
    case gpu::Format::R16_SINT:
    case gpu::Format::R16_SFLOAT:
        return 2u;

        // 3 BYTE
    case gpu::Format::R8G8B8_UNORM:
    case gpu::Format::R8G8B8_SNORM:
    case gpu::Format::R8G8B8_UINT:
    case gpu::Format::R8G8B8_SINT:
    case gpu::Format::R8G8B8_SRGB:

    case gpu::Format::B8G8R8_UNORM:
    case gpu::Format::B8G8R8_SNORM:
    case gpu::Format::B8G8R8_UINT:
    case gpu::Format::B8G8R8_SINT:
    case gpu::Format::B8G8R8_SRGB:
        return 3u;

        // 4 BYTE
    case gpu::Format::A2R10G10B10_UNORM_PACK32:
    case gpu::Format::A2R10G10B10_SNORM_PACK32:

    case gpu::Format::A2R10G10B10_UINT_PACK32:
    case gpu::Format::A2R10G10B10_SINT_PACK32:
    case gpu::Format::A2B10G10R10_UNORM_PACK32:
    case gpu::Format::A2B10G10R10_SNORM_PACK32:

    case gpu::Format::A2B10G10R10_UINT_PACK32:
    case gpu::Format::A2B10G10R10_SINT_PACK32:

    case gpu::Format::B10G11R11_UFLOAT_PACK32:

    case gpu::Format::R8G8B8A8_UNORM:
    case gpu::Format::R8G8B8A8_SNORM:
    case gpu::Format::R8G8B8A8_UINT:
    case gpu::Format::R8G8B8A8_SINT:
    case gpu::Format::R8G8B8A8_SRGB:

    case gpu::Format::B8G8R8A8_UNORM:
    case gpu::Format::B8G8R8A8_SNORM:
    case gpu::Format::B8G8R8A8_UINT:
    case gpu::Format::B8G8R8A8_SINT:
    case gpu::Format::B8G8R8A8_SRGB:

    case gpu::Format::R16G16_UNORM:
    case gpu::Format::R16G16_SNORM:
    case gpu::Format::R16G16_UINT:
    case gpu::Format::R16G16_SINT:
    case gpu::Format::R16G16_SFLOAT:

    case gpu::Format::R32_UINT:
    case gpu::Format::R32_SINT:
    case gpu::Format::R32_SFLOAT:
        return 4u;

        // 6 BYTE
    case gpu::Format::R16G16B16_UNORM:
    case gpu::Format::R16G16B16_SNORM:
    case gpu::Format::R16G16B16_UINT:
    case gpu::Format::R16G16B16_SINT:
    case gpu::Format::R16G16B16_SFLOAT:
        return 6u;

        // 8 BYTE
    case gpu::Format::R16G16B16A16_UNORM:
    case gpu::Format::R16G16B16A16_SNORM:
    case gpu::Format::R16G16B16A16_UINT:
    case gpu::Format::R16G16B16A16_SINT:
    case gpu::Format::R16G16B16A16_SFLOAT:

    case gpu::Format::R32G32_UINT:
    case gpu::Format::R32G32_SINT:
    case gpu::Format::R32G32_SFLOAT:

    case gpu::Format::R64_UINT:
    case gpu::Format::R64_SINT:
    case gpu::Format::R64_SFLOAT:
        return 8u;

        // 12 BYTE
    case gpu::Format::R32G32B32_UINT:
    case gpu::Format::R32G32B32_SINT:
    case gpu::Format::R32G32B32_SFLOAT:
        return 12u;

        // 16 BYTE
    case gpu::Format::R32G32B32A32_UINT:
    case gpu::Format::R32G32B32A32_SINT:
    case gpu::Format::R32G32B32A32_SFLOAT:

    case gpu::Format::R64G64_UINT:
    case gpu::Format::R64G64_SINT:
    case gpu::Format::R64G64_SFLOAT:
        return 16u;

        // 24 BYTE
    case gpu::Format::R64G64B64_UINT:
    case gpu::Format::R64G64B64_SINT:
    case gpu::Format::R64G64B64_SFLOAT:
        return 24u;

        // 32 BYTE
    case gpu::Format::R64G64B64A64_UINT:
    case gpu::Format::R64G64B64A64_SINT:
    case gpu::Format::R64G64B64A64_SFLOAT:
        return 32u;

        // 64-bit

    case gpu::Format::BC1_RGB_UNORM_BLOCK:
    case gpu::Format::BC1_RGB_SRGB_BLOCK:
    case gpu::Format::BC1_RGBA_UNORM_BLOCK:
    case gpu::Format::BC1_RGBA_SRGB_BLOCK:
    case gpu::Format::BC4_UNORM_BLOCK:
    case gpu::Format::BC4_SNORM_BLOCK:

    case gpu::Format::ETC2_R8G8B8_UNORM_BLOCK:
    case gpu::Format::ETC2_R8G8B8_SRGB_BLOCK:
    case gpu::Format::ETC2_R8G8B8A1_UNORM_BLOCK:
    case gpu::Format::ETC2_R8G8B8A1_SRGB_BLOCK:
    case gpu::Format::EAC_R11_UNORM_BLOCK:
    case gpu::Format::EAC_R11_SNORM_BLOCK:
    case gpu::Format::PVRTC1_2BPP_UNORM_BLOCK_IMG:
    case gpu::Format::PVRTC1_4BPP_UNORM_BLOCK_IMG:
    case gpu::Format::PVRTC2_2BPP_UNORM_BLOCK_IMG:
    case gpu::Format::PVRTC2_4BPP_UNORM_BLOCK_IMG:
    case gpu::Format::PVRTC1_2BPP_SRGB_BLOCK_IMG:
    case gpu::Format::PVRTC1_4BPP_SRGB_BLOCK_IMG:
    case gpu::Format::PVRTC2_2BPP_SRGB_BLOCK_IMG:
    case gpu::Format::PVRTC2_4BPP_SRGB_BLOCK_IMG:
        return 8u;

        // 128-bit
    case gpu::Format::BC2_UNORM_BLOCK:
    case gpu::Format::BC2_SRGB_BLOCK:
    case gpu::Format::BC3_UNORM_BLOCK:
    case gpu::Format::BC3_SRGB_BLOCK:
    case gpu::Format::BC5_UNORM_BLOCK:
    case gpu::Format::BC5_SNORM_BLOCK:
    case gpu::Format::BC6H_UFLOAT_BLOCK:
    case gpu::Format::BC6H_SFLOAT_BLOCK:
    case gpu::Format::BC7_UNORM_BLOCK:
    case gpu::Format::BC7_SRGB_BLOCK:

    case gpu::Format::ETC2_R8G8B8A8_UNORM_BLOCK:
    case gpu::Format::ETC2_R8G8B8A8_SRGB_BLOCK:
    case gpu::Format::EAC_R11G11_UNORM_BLOCK:
    case gpu::Format::EAC_R11G11_SNORM_BLOCK:
    case gpu::Format::ASTC_4x4_UNORM_BLOCK:
    case gpu::Format::ASTC_4x4_SRGB_BLOCK:
        return 16u;

    default:
        return 0u; // invalid
    }
}

bool image::getBlockSize(gpu::Format _vkFormat, uint32_t& _outWdith, uint32_t& _outHeight)
{
    _outWdith = _outHeight = 1u;
    switch (_vkFormat) {
    case gpu::Format::BC1_RGB_SRGB_BLOCK:
    case gpu::Format::BC1_RGB_UNORM_BLOCK:
    case gpu::Format::BC1_RGBA_SRGB_BLOCK:
    case gpu::Format::BC1_RGBA_UNORM_BLOCK:
    case gpu::Format::BC2_UNORM_BLOCK:
    case gpu::Format::BC2_SRGB_BLOCK:
    case gpu::Format::BC3_SRGB_BLOCK:
    case gpu::Format::BC3_UNORM_BLOCK:
    case gpu::Format::BC4_UNORM_BLOCK:
    case gpu::Format::BC4_SNORM_BLOCK:
    case gpu::Format::BC5_UNORM_BLOCK:
    case gpu::Format::BC5_SNORM_BLOCK:
    case gpu::Format::BC6H_UFLOAT_BLOCK:
    case gpu::Format::BC6H_SFLOAT_BLOCK:
    case gpu::Format::BC7_SRGB_BLOCK:
    case gpu::Format::BC7_UNORM_BLOCK:

    case gpu::Format::ETC2_R8G8B8_SRGB_BLOCK:
    case gpu::Format::ETC2_R8G8B8_UNORM_BLOCK:
    case gpu::Format::ETC2_R8G8B8A8_SRGB_BLOCK:
    case gpu::Format::ETC2_R8G8B8A8_UNORM_BLOCK:
    case gpu::Format::ETC2_R8G8B8A1_SRGB_BLOCK:
    case gpu::Format::ETC2_R8G8B8A1_UNORM_BLOCK:

    case gpu::Format::EAC_R11_UNORM_BLOCK:
    case gpu::Format::EAC_R11_SNORM_BLOCK:
    case gpu::Format::EAC_R11G11_UNORM_BLOCK:
    case gpu::Format::EAC_R11G11_SNORM_BLOCK:

    case gpu::Format::ASTC_4x4_SRGB_BLOCK:
    case gpu::Format::ASTC_4x4_UNORM_BLOCK:

    case gpu::Format::PVRTC1_4BPP_SRGB_BLOCK_IMG:
    case gpu::Format::PVRTC1_4BPP_UNORM_BLOCK_IMG:
    case gpu::Format::PVRTC2_4BPP_SRGB_BLOCK_IMG:
    case gpu::Format::PVRTC2_4BPP_UNORM_BLOCK_IMG:

        _outWdith = 4u;
        _outHeight = 4u;
        break;

    case gpu::Format::PVRTC1_2BPP_SRGB_BLOCK_IMG:
    case gpu::Format::PVRTC1_2BPP_UNORM_BLOCK_IMG:
    case gpu::Format::PVRTC2_2BPP_SRGB_BLOCK_IMG:
    case gpu::Format::PVRTC2_2BPP_UNORM_BLOCK_IMG:

        _outWdith = 8u;
        _outHeight = 4u;
        break;

    default:
        return false; // not block compressed
    }

    return true;
}

uint32_t image::getChannelCount(gpu::Format _vkFormat)
{
    switch (_vkFormat) {
    case gpu::Format::R8_UNORM:
    case gpu::Format::R8_SNORM:
    case gpu::Format::R8_UINT:
    case gpu::Format::R8_SINT:
    case gpu::Format::R8_SRGB:

    case gpu::Format::R16_UNORM:
    case gpu::Format::R16_SNORM:
    case gpu::Format::R16_UINT:
    case gpu::Format::R16_SINT:
    case gpu::Format::R16_SFLOAT:

    case gpu::Format::R32_UINT:
    case gpu::Format::R32_SINT:
    case gpu::Format::R32_SFLOAT:

    case gpu::Format::R64_UINT:
    case gpu::Format::R64_SINT:
    case gpu::Format::R64_SFLOAT:

    case gpu::Format::BC4_UNORM_BLOCK:
    case gpu::Format::BC4_SNORM_BLOCK:

    case gpu::Format::EAC_R11_UNORM_BLOCK:
    case gpu::Format::EAC_R11_SNORM_BLOCK:
        return 1u;

    case gpu::Format::R4G4_UNORM_PACK8:

    case gpu::Format::R8G8_UNORM:
    case gpu::Format::R8G8_SNORM:
    case gpu::Format::R8G8_UINT:
    case gpu::Format::R8G8_SINT:
    case gpu::Format::R8G8_SRGB:

    case gpu::Format::R16G16_UNORM:
    case gpu::Format::R16G16_SNORM:
    case gpu::Format::R16G16_UINT:
    case gpu::Format::R16G16_SINT:
    case gpu::Format::R16G16_SFLOAT:

    case gpu::Format::R32G32_UINT:
    case gpu::Format::R32G32_SINT:
    case gpu::Format::R32G32_SFLOAT:

    case gpu::Format::R64G64_UINT:
    case gpu::Format::R64G64_SINT:
    case gpu::Format::R64G64_SFLOAT:

    case gpu::Format::BC5_UNORM_BLOCK:
    case gpu::Format::BC5_SNORM_BLOCK:

    case gpu::Format::EAC_R11G11_UNORM_BLOCK:
    case gpu::Format::EAC_R11G11_SNORM_BLOCK:
        return 2u;

    case gpu::Format::R5G6B5_UNORM_PACK16:
    case gpu::Format::B5G6R5_UNORM_PACK16:
    case gpu::Format::B10G11R11_UFLOAT_PACK32:

    case gpu::Format::R8G8B8_UNORM:
    case gpu::Format::R8G8B8_SNORM:
    case gpu::Format::R8G8B8_UINT:
    case gpu::Format::R8G8B8_SINT:
    case gpu::Format::R8G8B8_SRGB:
    case gpu::Format::B8G8R8_UNORM:
    case gpu::Format::B8G8R8_SNORM:
    case gpu::Format::B8G8R8_UINT:
    case gpu::Format::B8G8R8_SINT:
    case gpu::Format::B8G8R8_SRGB:

    case gpu::Format::R16G16B16_UNORM:
    case gpu::Format::R16G16B16_SNORM:
    case gpu::Format::R16G16B16_UINT:
    case gpu::Format::R16G16B16_SINT:
    case gpu::Format::R16G16B16_SFLOAT:

    case gpu::Format::R32G32B32_UINT:
    case gpu::Format::R32G32B32_SINT:
    case gpu::Format::R32G32B32_SFLOAT:

    case gpu::Format::R64G64B64_UINT:
    case gpu::Format::R64G64B64_SINT:
    case gpu::Format::R64G64B64_SFLOAT:

    case gpu::Format::BC1_RGB_UNORM_BLOCK:
    case gpu::Format::BC1_RGB_SRGB_BLOCK:

    case gpu::Format::BC6H_UFLOAT_BLOCK:
    case gpu::Format::BC6H_SFLOAT_BLOCK:

    case gpu::Format::ETC2_R8G8B8_UNORM_BLOCK:
    case gpu::Format::ETC2_R8G8B8_SRGB_BLOCK:
        return 3u;

    case gpu::Format::R4G4B4A4_UNORM_PACK16:
    case gpu::Format::B4G4R4A4_UNORM_PACK16:
    case gpu::Format::R5G5B5A1_UNORM_PACK16:
    case gpu::Format::B5G5R5A1_UNORM_PACK16:
    case gpu::Format::A1R5G5B5_UNORM_PACK16:

    case gpu::Format::A2R10G10B10_UNORM_PACK32:
    case gpu::Format::A2R10G10B10_SNORM_PACK32:

    case gpu::Format::A2R10G10B10_UINT_PACK32:
    case gpu::Format::A2R10G10B10_SINT_PACK32:
    case gpu::Format::A2B10G10R10_UNORM_PACK32:
    case gpu::Format::A2B10G10R10_SNORM_PACK32:

    case gpu::Format::A2B10G10R10_UINT_PACK32:
    case gpu::Format::A2B10G10R10_SINT_PACK32:

    case gpu::Format::R8G8B8A8_UNORM:
    case gpu::Format::R8G8B8A8_SNORM:
    case gpu::Format::R8G8B8A8_UINT:
    case gpu::Format::R8G8B8A8_SINT:
    case gpu::Format::R8G8B8A8_SRGB:
    case gpu::Format::B8G8R8A8_UNORM:
    case gpu::Format::B8G8R8A8_SNORM:
    case gpu::Format::B8G8R8A8_UINT:
    case gpu::Format::B8G8R8A8_SINT:
    case gpu::Format::B8G8R8A8_SRGB:

    case gpu::Format::R16G16B16A16_UNORM:
    case gpu::Format::R16G16B16A16_SNORM:
    case gpu::Format::R16G16B16A16_UINT:
    case gpu::Format::R16G16B16A16_SINT:
    case gpu::Format::R16G16B16A16_SFLOAT:

    case gpu::Format::R32G32B32A32_UINT:
    case gpu::Format::R32G32B32A32_SINT:
    case gpu::Format::R32G32B32A32_SFLOAT:

    case gpu::Format::R64G64B64A64_UINT:
    case gpu::Format::R64G64B64A64_SINT:
    case gpu::Format::R64G64B64A64_SFLOAT:

    case gpu::Format::BC1_RGBA_UNORM_BLOCK:
    case gpu::Format::BC1_RGBA_SRGB_BLOCK:
    case gpu::Format::BC2_UNORM_BLOCK:
    case gpu::Format::BC2_SRGB_BLOCK:
    case gpu::Format::BC3_UNORM_BLOCK:
    case gpu::Format::BC3_SRGB_BLOCK:

    case gpu::Format::BC7_UNORM_BLOCK:
    case gpu::Format::BC7_SRGB_BLOCK:

    case gpu::Format::ETC2_R8G8B8A1_UNORM_BLOCK:
    case gpu::Format::ETC2_R8G8B8A1_SRGB_BLOCK:
    case gpu::Format::ETC2_R8G8B8A8_UNORM_BLOCK:
    case gpu::Format::ETC2_R8G8B8A8_SRGB_BLOCK:

    case gpu::Format::ASTC_4x4_UNORM_BLOCK:
    case gpu::Format::ASTC_4x4_SRGB_BLOCK:

    case gpu::Format::PVRTC1_2BPP_UNORM_BLOCK_IMG:
    case gpu::Format::PVRTC1_4BPP_UNORM_BLOCK_IMG:
    case gpu::Format::PVRTC2_2BPP_UNORM_BLOCK_IMG:
    case gpu::Format::PVRTC2_4BPP_UNORM_BLOCK_IMG:
    case gpu::Format::PVRTC1_2BPP_SRGB_BLOCK_IMG:
    case gpu::Format::PVRTC1_4BPP_SRGB_BLOCK_IMG:
    case gpu::Format::PVRTC2_2BPP_SRGB_BLOCK_IMG:
    case gpu::Format::PVRTC2_4BPP_SRGB_BLOCK_IMG:
        return 4u;

    default:
        return 0u; // invalid
    }
}

uint32_t image::getChannelSize(gpu::Format _vkFormat, uint32_t _channelIndex)
{
    if (_channelIndex >= 4u) {
        return 0u;
    }

    switch (_vkFormat) {
        // 8 bits; does not depend on channel.
    case gpu::Format::R8_UNORM:
    case gpu::Format::R8_SNORM:
    case gpu::Format::R8_UINT:
    case gpu::Format::R8_SINT:
    case gpu::Format::R8_SRGB:

    case gpu::Format::R8G8_UNORM:
    case gpu::Format::R8G8_SNORM:
    case gpu::Format::R8G8_UINT:
    case gpu::Format::R8G8_SINT:
    case gpu::Format::R8G8_SRGB:

    case gpu::Format::R8G8B8_UNORM:
    case gpu::Format::R8G8B8_SNORM:
    case gpu::Format::R8G8B8_UINT:
    case gpu::Format::R8G8B8_SINT:
    case gpu::Format::R8G8B8_SRGB:

    case gpu::Format::B8G8R8_UNORM:
    case gpu::Format::B8G8R8_SNORM:
    case gpu::Format::B8G8R8_UINT:
    case gpu::Format::B8G8R8_SINT:
    case gpu::Format::B8G8R8_SRGB:

    case gpu::Format::R8G8B8A8_UNORM:
    case gpu::Format::R8G8B8A8_SNORM:
    case gpu::Format::R8G8B8A8_UINT:
    case gpu::Format::R8G8B8A8_SINT:
    case gpu::Format::R8G8B8A8_SRGB:

    case gpu::Format::B8G8R8A8_UNORM:
    case gpu::Format::B8G8R8A8_SNORM:
    case gpu::Format::B8G8R8A8_UINT:
    case gpu::Format::B8G8R8A8_SINT:
    case gpu::Format::B8G8R8A8_SRGB:
        return 8u;

        // 16 bits; does not depend on channel.
    case gpu::Format::R16_UNORM:
    case gpu::Format::R16_SNORM:
    case gpu::Format::R16_UINT:
    case gpu::Format::R16_SINT:
    case gpu::Format::R16_SFLOAT:

    case gpu::Format::R16G16_UNORM:
    case gpu::Format::R16G16_SNORM:
    case gpu::Format::R16G16_UINT:
    case gpu::Format::R16G16_SINT:
    case gpu::Format::R16G16_SFLOAT:

    case gpu::Format::R16G16B16_UNORM:
    case gpu::Format::R16G16B16_SNORM:
    case gpu::Format::R16G16B16_UINT:
    case gpu::Format::R16G16B16_SINT:
    case gpu::Format::R16G16B16_SFLOAT:

    case gpu::Format::R16G16B16A16_UNORM:
    case gpu::Format::R16G16B16A16_SNORM:
    case gpu::Format::R16G16B16A16_UINT:
    case gpu::Format::R16G16B16A16_SINT:
    case gpu::Format::R16G16B16A16_SFLOAT:
        return 16u;

        // 32 bits; does not depend on channel.
    case gpu::Format::R32_UINT:
    case gpu::Format::R32_SINT:
    case gpu::Format::R32_SFLOAT:

    case gpu::Format::R32G32_UINT:
    case gpu::Format::R32G32_SINT:
    case gpu::Format::R32G32_SFLOAT:

    case gpu::Format::R32G32B32_UINT:
    case gpu::Format::R32G32B32_SINT:
    case gpu::Format::R32G32B32_SFLOAT:

    case gpu::Format::R32G32B32A32_UINT:
    case gpu::Format::R32G32B32A32_SINT:
    case gpu::Format::R32G32B32A32_SFLOAT:
        return 32u;

        // 64 bits; does not depend on channel.
    case gpu::Format::R64_UINT:
    case gpu::Format::R64_SINT:
    case gpu::Format::R64_SFLOAT:

    case gpu::Format::R64G64_UINT:
    case gpu::Format::R64G64_SINT:
    case gpu::Format::R64G64_SFLOAT:

    case gpu::Format::R64G64B64_UINT:
    case gpu::Format::R64G64B64_SINT:
    case gpu::Format::R64G64B64_SFLOAT:

    case gpu::Format::R64G64B64A64_UINT:
    case gpu::Format::R64G64B64A64_SINT:
    case gpu::Format::R64G64B64A64_SFLOAT:
        return 64u;

    case gpu::Format::R4G4_UNORM_PACK8:
        if (_channelIndex < 2) {
            return 4u;
        } else {
            return 0u;
        }

    case gpu::Format::R5G6B5_UNORM_PACK16:
    case gpu::Format::B5G6R5_UNORM_PACK16:
        if (_channelIndex < 3) {
            if (_channelIndex == 1) {
                return 6u;
            } else {
                return 5u;
            }
        } else {
            return 0u;
        }

    case gpu::Format::B10G11R11_UFLOAT_PACK32:
        if (_channelIndex < 3) {
            if (_channelIndex == 0) {
                return 10u;
            } else {
                return 11u;
            }
        } else {
            return 0u;
        }

    case gpu::Format::R4G4B4A4_UNORM_PACK16:
    case gpu::Format::B4G4R4A4_UNORM_PACK16:
        return 4u;

    case gpu::Format::R5G5B5A1_UNORM_PACK16:
    case gpu::Format::B5G5R5A1_UNORM_PACK16:
        if (_channelIndex < 3) {
            return 5u;
        } else {
            return 1u;
        }

    case gpu::Format::A1R5G5B5_UNORM_PACK16:
        if (_channelIndex > 0) {
            return 5u;
        } else {
            return 1u;
        }

    case gpu::Format::A2R10G10B10_UNORM_PACK32:
    case gpu::Format::A2R10G10B10_SNORM_PACK32:

    case gpu::Format::A2R10G10B10_UINT_PACK32:
    case gpu::Format::A2R10G10B10_SINT_PACK32:
    case gpu::Format::A2B10G10R10_UNORM_PACK32:
    case gpu::Format::A2B10G10R10_SNORM_PACK32:

    case gpu::Format::A2B10G10R10_UINT_PACK32:
    case gpu::Format::A2B10G10R10_SINT_PACK32:
        if (_channelIndex > 0) {
            return 10u;
        } else {
            return 2u;
        }
    default:
        return 0u;
    }
}

int32_t image::getChannelIndex(gpu::Format _vkFormat, Channel _channel)
{
    switch (_vkFormat) {
    case gpu::Format::R8_UNORM:
    case gpu::Format::R8_SNORM:
    case gpu::Format::R8_UINT:
    case gpu::Format::R8_SINT:
    case gpu::Format::R8_SRGB:

    case gpu::Format::R16_UNORM:
    case gpu::Format::R16_SNORM:
    case gpu::Format::R16_UINT:
    case gpu::Format::R16_SINT:
    case gpu::Format::R16_SFLOAT:

    case gpu::Format::R32_UINT:
    case gpu::Format::R32_SINT:
    case gpu::Format::R32_SFLOAT:

    case gpu::Format::R64_UINT:
    case gpu::Format::R64_SINT:
    case gpu::Format::R64_SFLOAT:
        if (_channel <= Channel::RED) {
            return static_cast<int32_t>(_channel);
        } else {
            return -1;
        }

    case gpu::Format::R4G4_UNORM_PACK8:

    case gpu::Format::R8G8_UNORM:
    case gpu::Format::R8G8_SNORM:
    case gpu::Format::R8G8_UINT:
    case gpu::Format::R8G8_SINT:
    case gpu::Format::R8G8_SRGB:

    case gpu::Format::R16G16_UNORM:
    case gpu::Format::R16G16_SNORM:
    case gpu::Format::R16G16_UINT:
    case gpu::Format::R16G16_SINT:
    case gpu::Format::R16G16_SFLOAT:

    case gpu::Format::R32G32_UINT:
    case gpu::Format::R32G32_SINT:
    case gpu::Format::R32G32_SFLOAT:

    case gpu::Format::R64G64_UINT:
    case gpu::Format::R64G64_SINT:
    case gpu::Format::R64G64_SFLOAT:
        if (_channel <= Channel::GREEN) {
            return static_cast<int32_t>(_channel);
        } else {
            return -1;
        }

    case gpu::Format::R5G6B5_UNORM_PACK16:

    case gpu::Format::R8G8B8_UNORM:
    case gpu::Format::R8G8B8_SNORM:
    case gpu::Format::R8G8B8_UINT:
    case gpu::Format::R8G8B8_SINT:
    case gpu::Format::R8G8B8_SRGB:

    case gpu::Format::R16G16B16_UNORM:
    case gpu::Format::R16G16B16_SNORM:
    case gpu::Format::R16G16B16_UINT:
    case gpu::Format::R16G16B16_SINT:
    case gpu::Format::R16G16B16_SFLOAT:

    case gpu::Format::R32G32B32_UINT:
    case gpu::Format::R32G32B32_SINT:
    case gpu::Format::R32G32B32_SFLOAT:

    case gpu::Format::R64G64B64_UINT:
    case gpu::Format::R64G64B64_SINT:
    case gpu::Format::R64G64B64_SFLOAT:
        if (_channel <= Channel::BLUE) {
            return static_cast<int32_t>(_channel);
        } else {
            return -1;
        }

    case gpu::Format::R4G4B4A4_UNORM_PACK16:
    case gpu::Format::R5G5B5A1_UNORM_PACK16:

    case gpu::Format::R8G8B8A8_UNORM:
    case gpu::Format::R8G8B8A8_SNORM:
    case gpu::Format::R8G8B8A8_UINT:
    case gpu::Format::R8G8B8A8_SINT:
    case gpu::Format::R8G8B8A8_SRGB:

    case gpu::Format::R16G16B16A16_UNORM:
    case gpu::Format::R16G16B16A16_SNORM:
    case gpu::Format::R16G16B16A16_UINT:
    case gpu::Format::R16G16B16A16_SINT:
    case gpu::Format::R16G16B16A16_SFLOAT:

    case gpu::Format::R32G32B32A32_UINT:
    case gpu::Format::R32G32B32A32_SINT:
    case gpu::Format::R32G32B32A32_SFLOAT:

    case gpu::Format::R64G64B64A64_UINT:
    case gpu::Format::R64G64B64A64_SINT:
    case gpu::Format::R64G64B64A64_SFLOAT:
        return static_cast<int32_t>(_channel);

    case gpu::Format::B5G6R5_UNORM_PACK16:
    case gpu::Format::B10G11R11_UFLOAT_PACK32:

    case gpu::Format::B8G8R8_UNORM:
    case gpu::Format::B8G8R8_SNORM:
    case gpu::Format::B8G8R8_UINT:
    case gpu::Format::B8G8R8_SINT:
    case gpu::Format::B8G8R8_SRGB:
        if (_channel <= Channel::BLUE) {
            if (_channel == Channel::RED) {
                return 2;
            }
            if (_channel == Channel::GREEN) {
                return 1;
            } else {
                return 0;
            }
        } else {
            return -1;
        }

    case gpu::Format::B4G4R4A4_UNORM_PACK16:
    case gpu::Format::B5G5R5A1_UNORM_PACK16:

    case gpu::Format::B8G8R8A8_UNORM:
    case gpu::Format::B8G8R8A8_SNORM:
    case gpu::Format::B8G8R8A8_UINT:
    case gpu::Format::B8G8R8A8_SINT:
    case gpu::Format::B8G8R8A8_SRGB:
        if (_channel == Channel::RED) {
            return 2;
        }
        if (_channel == Channel::GREEN) {
            return 1;
        }
        if (_channel == Channel::BLUE) {
            return 0;
        } else {
            return 3;
        }

    case gpu::Format::A1R5G5B5_UNORM_PACK16:

    case gpu::Format::A2R10G10B10_UNORM_PACK32:
    case gpu::Format::A2R10G10B10_SNORM_PACK32:

    case gpu::Format::A2R10G10B10_UINT_PACK32:
    case gpu::Format::A2R10G10B10_SINT_PACK32:
        if (_channel == Channel::RED) {
            return 1;
        }
        if (_channel == Channel::GREEN) {
            return 2;
        }
        if (_channel == Channel::BLUE) {
            return 3;
        } else {
            return 0;
        }

    case gpu::Format::A2B10G10R10_UNORM_PACK32:
    case gpu::Format::A2B10G10R10_SNORM_PACK32:

    case gpu::Format::A2B10G10R10_UINT_PACK32:
    case gpu::Format::A2B10G10R10_SINT_PACK32:
        if (_channel == Channel::RED) {
            return 3;
        }
        if (_channel == Channel::GREEN) {
            return 2;
        }
        if (_channel == Channel::BLUE) {
            return 1;
        } else {
            return 0;
        }
    default:
        return -1;
    }
}

bool image::isFloat(gpu::Format _vkFormat)
{
    switch (_vkFormat) {
    case gpu::Format::B10G11R11_UFLOAT_PACK32:
    case gpu::Format::R16_SFLOAT:
    case gpu::Format::R16G16_SFLOAT:
    case gpu::Format::R16G16B16_SFLOAT:
    case gpu::Format::R16G16B16A16_SFLOAT:
    case gpu::Format::R32_SFLOAT:
    case gpu::Format::R32G32_SFLOAT:
    case gpu::Format::R32G32B32_SFLOAT:
    case gpu::Format::R32G32B32A32_SFLOAT:
    case gpu::Format::R64_SFLOAT:
    case gpu::Format::R64G64_SFLOAT:
    case gpu::Format::R64G64B64_SFLOAT:
    case gpu::Format::R64G64B64A64_SFLOAT:
        return true;
    default:
        return false;
    }
}

bool image::isSigned(gpu::Format _vkFormat)
{
    switch (_vkFormat) {
    case gpu::Format::A2R10G10B10_SNORM_PACK32:

    case gpu::Format::A2R10G10B10_SINT_PACK32:
    case gpu::Format::A2B10G10R10_SNORM_PACK32:

    case gpu::Format::A2B10G10R10_UINT_PACK32:
    case gpu::Format::A2B10G10R10_SINT_PACK32:

    case gpu::Format::R8_SNORM:
    case gpu::Format::R8_SINT:
    case gpu::Format::R8_SRGB:

    case gpu::Format::R8G8_SNORM:
    case gpu::Format::R8G8_SINT:
    case gpu::Format::R8G8_SRGB:

    case gpu::Format::R8G8B8_SNORM:
    case gpu::Format::R8G8B8_SINT:
    case gpu::Format::R8G8B8_SRGB:

    case gpu::Format::B8G8R8_SNORM:
    case gpu::Format::B8G8R8_SINT:
    case gpu::Format::B8G8R8_SRGB:

    case gpu::Format::R8G8B8A8_SNORM:
    case gpu::Format::R8G8B8A8_SINT:
    case gpu::Format::R8G8B8A8_SRGB:

    case gpu::Format::B8G8R8A8_SNORM:
    case gpu::Format::B8G8R8A8_SINT:
    case gpu::Format::B8G8R8A8_SRGB:

    case gpu::Format::R16_SNORM:
    case gpu::Format::R16_SINT:
    case gpu::Format::R16_SFLOAT:

    case gpu::Format::R16G16_SNORM:
    case gpu::Format::R16G16_SINT:
    case gpu::Format::R16G16_SFLOAT:

    case gpu::Format::R16G16B16_SNORM:
    case gpu::Format::R16G16B16_SINT:
    case gpu::Format::R16G16B16_SFLOAT:

    case gpu::Format::R16G16B16A16_SNORM:
    case gpu::Format::R16G16B16A16_SINT:
    case gpu::Format::R16G16B16A16_SFLOAT:

    case gpu::Format::R32_SINT:
    case gpu::Format::R32_SFLOAT:

    case gpu::Format::R32G32_SINT:
    case gpu::Format::R32G32_SFLOAT:

    case gpu::Format::R32G32B32_SINT:
    case gpu::Format::R32G32B32_SFLOAT:

    case gpu::Format::R32G32B32A32_SINT:
    case gpu::Format::R32G32B32A32_SFLOAT:

    case gpu::Format::R64_SINT:
    case gpu::Format::R64_SFLOAT:

    case gpu::Format::R64G64_SINT:
    case gpu::Format::R64G64_SFLOAT:

    case gpu::Format::R64G64B64_SINT:
    case gpu::Format::R64G64B64_SFLOAT:

    case gpu::Format::R64G64B64A64_SINT:
    case gpu::Format::R64G64B64A64_SFLOAT:
        return true;
    default:
        return false;
    }
}

bool image::isNormalized(gpu::Format _vkFormat)
{
    switch (_vkFormat) {
    case gpu::Format::R4G4_UNORM_PACK8:
    case gpu::Format::R4G4B4A4_UNORM_PACK16:
    case gpu::Format::B4G4R4A4_UNORM_PACK16:
    case gpu::Format::R5G6B5_UNORM_PACK16:
    case gpu::Format::B5G6R5_UNORM_PACK16:
    case gpu::Format::R5G5B5A1_UNORM_PACK16:
    case gpu::Format::B5G5R5A1_UNORM_PACK16:
    case gpu::Format::A1R5G5B5_UNORM_PACK16:

    case gpu::Format::A2R10G10B10_UNORM_PACK32:
    case gpu::Format::A2R10G10B10_SNORM_PACK32:

    case gpu::Format::A2B10G10R10_UNORM_PACK32:
    case gpu::Format::A2B10G10R10_SNORM_PACK32:

    case gpu::Format::R8_UNORM:
    case gpu::Format::R8_SNORM:

    case gpu::Format::R8G8_UNORM:
    case gpu::Format::R8G8_SNORM:

    case gpu::Format::R8G8B8_UNORM:
    case gpu::Format::R8G8B8_SNORM:

    case gpu::Format::B8G8R8_UNORM:
    case gpu::Format::B8G8R8_SNORM:

    case gpu::Format::R8G8B8A8_UNORM:
    case gpu::Format::R8G8B8A8_SNORM:

    case gpu::Format::B8G8R8A8_UNORM:
    case gpu::Format::B8G8R8A8_SNORM:

    case gpu::Format::R16_UNORM:
    case gpu::Format::R16_SNORM:

    case gpu::Format::R16G16_UNORM:
    case gpu::Format::R16G16_SNORM:

    case gpu::Format::R16G16B16_UNORM:
    case gpu::Format::R16G16B16_SNORM:

    case gpu::Format::R16G16B16A16_UNORM:
    case gpu::Format::R16G16B16A16_SNORM:
        return true;
    default:
        return false;
    }
}

bool image::isSrgb(gpu::Format _vkFormat)
{
    switch (_vkFormat) {
    case gpu::Format::R8_SRGB:
    case gpu::Format::R8G8_SRGB:
    case gpu::Format::R8G8B8_SRGB:
    case gpu::Format::B8G8R8_SRGB:
    case gpu::Format::R8G8B8A8_SRGB:
    case gpu::Format::B8G8R8A8_SRGB:
        return true;
    default:
        return false;
    }
}

bool image::isPacked(gpu::Format _vkFormat)
{
    switch (_vkFormat) {
    case gpu::Format::R4G4_UNORM_PACK8:
    case gpu::Format::R4G4B4A4_UNORM_PACK16:
    case gpu::Format::B4G4R4A4_UNORM_PACK16:
    case gpu::Format::R5G6B5_UNORM_PACK16:
    case gpu::Format::B5G6R5_UNORM_PACK16:
    case gpu::Format::R5G5B5A1_UNORM_PACK16:
    case gpu::Format::B5G5R5A1_UNORM_PACK16:
    case gpu::Format::A1R5G5B5_UNORM_PACK16:

    case gpu::Format::A2R10G10B10_UNORM_PACK32:
    case gpu::Format::A2R10G10B10_SNORM_PACK32:

    case gpu::Format::A2R10G10B10_UINT_PACK32:
    case gpu::Format::A2R10G10B10_SINT_PACK32:
    case gpu::Format::A2B10G10R10_UNORM_PACK32:
    case gpu::Format::A2B10G10R10_SNORM_PACK32:

    case gpu::Format::A2B10G10R10_UINT_PACK32:
    case gpu::Format::A2B10G10R10_SINT_PACK32:

    case gpu::Format::B10G11R11_UFLOAT_PACK32:
        return true;
    default:
        return false;
    }
}

bool image::isCompressed(gpu::Format _vkFormat)
{
    return _vkFormat >= gpu::Format::BC1_RGB_UNORM_BLOCK && _vkFormat <= gpu::Format::PVRTC2_4BPP_SRGB_BLOCK_IMG;
}

uint64_t image::getFaceSize(gpu::Format _vkFormat, uint32_t _level, uint32_t _width, uint32_t _height, uint32_t _depth)
{
    uint32_t blockWidth = 1u;
    uint32_t blockHeight = 1u;

    const uint32_t bytesPerBlock = getFormatSize(_vkFormat);

    if (getBlockSize(_vkFormat, blockWidth, blockHeight)) {
        const uint32_t width = _width >> _level;
        const uint32_t height = _height >> _level;

        const uint32_t blockCountX = static_cast<uint32_t>(ceilf(width / static_cast<float>(blockWidth)));
        const uint32_t blockCountY = static_cast<uint32_t>(ceilf(height / static_cast<float>(blockHeight)));

        return bytesPerBlock * blockCountX * blockCountY;
    } else {
        const uint64_t resolution = getPixelCount(_level, _width, _height, _depth);

        return resolution * bytesPerBlock;
    }
}

uint32_t image::getPixelCount(uint32_t _level, uint32_t _width, uint32_t _height, uint32_t _depth)
{
    uint32_t result = std::max(_width >> _level, 1u);

    if (_height != 0u) {
        result *= std::max(_height >> _level, 1u);
    }
    if (_depth != 0u) {
        result *= std::max(_depth >> _level, 1u);
    }

    return result;
}

uint32_t image::getPadding(uint64_t _value, uint32_t _alginment)
{
    return (_alginment - (_value % _alginment)) % _alginment;
}

uint32_t image::getLCM(uint32_t _x, uint32_t _y)
{
    uint32_t max = _x > _y ? _x : _y;

    while (!((max % _x == 0u) && (max % _y == 0u))) {
        ++max;
    }

    return max;
}

uint32_t image::getMipPadding(uint64_t _value, gpu::Format _vkFormat, bool _superCompression)
{
    if (_superCompression) {
        return 0; // 1u byte alignment
    } else if (_vkFormat == gpu::Format::UNDEFINED) {
        return getPadding(_value, 16u); // 16 byte alignment
    }

    const uint32_t texelBlockSize = getFormatSize(_vkFormat);
    const uint32_t lcm = getLCM(texelBlockSize, 4u);
    const uint32_t padding = getPadding(_value, lcm);
    return padding;
}

size_t image::getOffset(
    const nlohmann::json& ImageDataInfo,
    const uint32_t _InX, const uint32_t _InY, const uint32_t _InZ,
    const uint32_t _InFace, const uint32_t _InArray, const uint32_t _InLevel)
{
    const uint32_t m_Width = ImageDataInfo["width"].get<uint32_t>();
    const uint32_t m_Height = ImageDataInfo["height"].get<uint32_t>();
    const uint32_t pixelSize = ImageDataInfo["pixelByteSize"].get<uint32_t>();

    uint32_t m_Depth = ImageDataInfo.value("depth", 1);
    uint32_t m_FaceCount = ImageDataInfo.value("numFaces", 1);
    uint32_t m_ArrayCount = ImageDataInfo.value("numLayers", 1);

    const size_t width = std::max(m_Width >> _InLevel, 1u);
    const size_t height = std::max(m_Height >> _InLevel, 1u);
    const size_t depth = std::max(m_Depth >> _InLevel, 1u);

    size_t totalOffset = 0;
    for (uint32_t level = 0; level < _InLevel; level++) {
        totalOffset += static_cast<size_t>(std::max(m_Width >> level, 1u)) * static_cast<size_t>(std::max(m_Height >> level, 1u)) * static_cast<size_t>(std::max(m_Depth >> level, 1u)) * static_cast<size_t>(pixelSize) * static_cast<size_t>(m_FaceCount) * static_cast<size_t>(m_ArrayCount);
    }

    // face within the current mip level array
    totalOffset += _InX * pixelSize + _InY * width * pixelSize + _InZ * width * height * pixelSize + _InFace * width * height * depth * pixelSize + _InArray * width * height * depth * m_FaceCount * pixelSize;
    return totalOffset;
}