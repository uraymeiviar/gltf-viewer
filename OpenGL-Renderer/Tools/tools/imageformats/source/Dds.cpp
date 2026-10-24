// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imageformats/Dds.h"
#include "core/Logger.h"

#define BCDEC_IMPLEMENTATION
#include "bcdec.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace ux3d::imageformats {

namespace {

enum class DdsFormat {
    Unknown,
    BC1,
    BC2,
    BC3,
    BC4,
    BC5,
    BC6H_UF16,
    BC6H_SF16,
    BC7,
    RGBA8,
    BGRA8,
    RGB8,
    BGR8
};

inline uint32_t readU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
          (static_cast<uint32_t>(p[1]) << 8) |
          (static_cast<uint32_t>(p[2]) << 16) |
          (static_cast<uint32_t>(p[3]) << 24);
}

} // anonymous namespace

ImageDecodingResult decodeDDS(const core::Vector<uint8_t>& _buffer, image::Image& _image)
{
    // Minimal valid DDS file has 4 bytes magic + 124 bytes header = 128 bytes
    if (_buffer.size() < 128) {
        return ImageDecodingError::InvalidInput;
    }

    const uint8_t* data = _buffer.data();
    size_t size = _buffer.size();

    // Check "DDS " magic (0x20534444)
    if (data[0] != 'D' || data[1] != 'D' || data[2] != 'S' || data[3] != ' ') {
        return ImageDecodingError::InvalidInput;
    }

    // Parse DDS_HEADER
    uint32_t headerSize = readU32LE(data + 4);
    if (headerSize != 124) {
        return ImageDecodingError::InvalidInput;
    }

    uint32_t height = readU32LE(data + 12);
    uint32_t width = readU32LE(data + 16);

    if (width == 0 || height == 0) {
        return ImageDecodingError::InvalidInput;
    }

    // DDS_PIXELFORMAT starts at offset 4 (magic) + 72 = 76
    uint32_t pfFlags = readU32LE(data + 80);
    uint32_t pfFourCC = readU32LE(data + 84);
    uint32_t pfRGBBitCount = readU32LE(data + 88);
    uint32_t pfRBitMask = readU32LE(data + 92);
    uint32_t pfGBitMask = readU32LE(data + 96);
    uint32_t pfBBitMask = readU32LE(data + 100);
    uint32_t pfABitMask = readU32LE(data + 104);

    size_t dataOffset = 128;
    DdsFormat format = DdsFormat::Unknown;

    // Check for DX10 extended header ("DX10" = 0x30315844)
    if (pfFourCC == 0x30315844) {
        if (size < 148) {
            return ImageDecodingError::InvalidInput;
        }
        uint32_t dxgiFormat = readU32LE(data + 128);
        dataOffset = 148;

        switch (dxgiFormat) {
        case 70: // DXGI_FORMAT_BC1_TYPELESS
        case 71: // DXGI_FORMAT_BC1_UNORM
        case 72: // DXGI_FORMAT_BC1_UNORM_SRGB
            format = DdsFormat::BC1;
            break;
        case 73: // DXGI_FORMAT_BC2_TYPELESS
        case 74: // DXGI_FORMAT_BC2_UNORM
        case 75: // DXGI_FORMAT_BC2_UNORM_SRGB
            format = DdsFormat::BC2;
            break;
        case 76: // DXGI_FORMAT_BC3_TYPELESS
        case 77: // DXGI_FORMAT_BC3_UNORM
        case 78: // DXGI_FORMAT_BC3_UNORM_SRGB
            format = DdsFormat::BC3;
            break;
        case 79: // DXGI_FORMAT_BC4_TYPELESS
        case 80: // DXGI_FORMAT_BC4_UNORM
        case 81: // DXGI_FORMAT_BC4_SNORM
            format = DdsFormat::BC4;
            break;
        case 82: // DXGI_FORMAT_BC5_TYPELESS
        case 83: // DXGI_FORMAT_BC5_UNORM
        case 84: // DXGI_FORMAT_BC5_SNORM
            format = DdsFormat::BC5;
            break;
        case 94: // DXGI_FORMAT_BC6H_TYPELESS
        case 95: // DXGI_FORMAT_BC6H_UF16
            format = DdsFormat::BC6H_UF16;
            break;
        case 96: // DXGI_FORMAT_BC6H_SF16
            format = DdsFormat::BC6H_SF16;
            break;
        case 97: // DXGI_FORMAT_BC7_TYPELESS
        case 98: // DXGI_FORMAT_BC7_UNORM
        case 99: // DXGI_FORMAT_BC7_UNORM_SRGB
            format = DdsFormat::BC7;
            break;
        case 27: // DXGI_FORMAT_R8G8B8A8_TYPELESS
        case 28: // DXGI_FORMAT_R8G8B8A8_UNORM
        case 29: // DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            format = DdsFormat::RGBA8;
            break;
        case 87: // DXGI_FORMAT_B8G8R8A8_UNORM
        case 91: // DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
        case 88: // DXGI_FORMAT_B8G8R8X8_UNORM
        case 93: // DXGI_FORMAT_B8G8R8X8_UNORM_SRGB
            format = DdsFormat::BGRA8;
            break;
        default:
            core::logError("[DDS] Unsupported DXGI format: %u", dxgiFormat);
            return ImageDecodingError::InvalidInput;
        }
    } else {
        // Standard FourCC / DirectDraw pixel formats
        if (pfFourCC == 0x31545844) { // "DXT1"
            format = DdsFormat::BC1;
        } else if (pfFourCC == 0x32545844 || pfFourCC == 0x33545844) { // "DXT2" or "DXT3"
            format = DdsFormat::BC2;
        } else if (pfFourCC == 0x34545844 || pfFourCC == 0x35545844) { // "DXT4" or "DXT5"
            format = DdsFormat::BC3;
        } else if (pfFourCC == 0x31495441 || pfFourCC == 0x55344342 || pfFourCC == 0x53344342) { // "ATI1" / "BC4U" / "BC4S"
            format = DdsFormat::BC4;
        } else if (pfFourCC == 0x32495441 || pfFourCC == 0x55354342 || pfFourCC == 0x53354342) { // "ATI2" / "BC5U" / "BC5S"
            format = DdsFormat::BC5;
        } else if (pfFourCC == 0x55374342 || pfFourCC == 0x20374342 || pfFourCC == 0x00374342) { // "BC7U" / "BC7 "
            format = DdsFormat::BC7;
        } else if (pfFlags & 0x00000040) { // DDPF_RGB
            if (pfRGBBitCount == 32) {
                if (pfRBitMask == 0x00FF0000 && pfBBitMask == 0x000000FF) {
                    format = DdsFormat::BGRA8;
                } else if (pfRBitMask == 0x000000FF && pfBBitMask == 0x00FF0000) {
                    format = DdsFormat::RGBA8;
                } else {
                    format = DdsFormat::RGBA8;
                }
            } else if (pfRGBBitCount == 24) {
                if (pfRBitMask == 0x00FF0000 && pfBBitMask == 0x000000FF) {
                    format = DdsFormat::BGR8;
                } else {
                    format = DdsFormat::RGB8;
                }
            }
        }
    }

    if (format == DdsFormat::Unknown) {
        core::logError("[DDS] Unknown or unsupported DDS format (FourCC: 0x%08X, pfFlags: 0x%08X)", pfFourCC, pfFlags);
        return ImageDecodingError::InvalidInput;
    }

    const uint8_t* pSrc = data + dataOffset;
    size_t availableBytes = (size > dataOffset) ? (size - dataOffset) : 0;

    std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4);

    if (format == DdsFormat::RGBA8) {
        size_t needed = static_cast<size_t>(width) * height * 4;
        if (availableBytes < needed) return ImageDecodingError::InvalidInput;
        std::memcpy(rgba.data(), pSrc, needed);
    } else if (format == DdsFormat::BGRA8) {
        size_t needed = static_cast<size_t>(width) * height * 4;
        if (availableBytes < needed) return ImageDecodingError::InvalidInput;
        for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i) {
            rgba[i * 4 + 0] = pSrc[i * 4 + 2]; // R
            rgba[i * 4 + 1] = pSrc[i * 4 + 1]; // G
            rgba[i * 4 + 2] = pSrc[i * 4 + 0]; // B
            rgba[i * 4 + 3] = pSrc[i * 4 + 3]; // A
        }
    } else if (format == DdsFormat::RGB8) {
        size_t needed = static_cast<size_t>(width) * height * 3;
        if (availableBytes < needed) return ImageDecodingError::InvalidInput;
        for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i) {
            rgba[i * 4 + 0] = pSrc[i * 3 + 0];
            rgba[i * 4 + 1] = pSrc[i * 3 + 1];
            rgba[i * 4 + 2] = pSrc[i * 3 + 2];
            rgba[i * 4 + 3] = 255;
        }
    } else if (format == DdsFormat::BGR8) {
        size_t needed = static_cast<size_t>(width) * height * 3;
        if (availableBytes < needed) return ImageDecodingError::InvalidInput;
        for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i) {
            rgba[i * 4 + 0] = pSrc[i * 3 + 2];
            rgba[i * 4 + 1] = pSrc[i * 3 + 1];
            rgba[i * 4 + 2] = pSrc[i * 3 + 0];
            rgba[i * 4 + 3] = 255;
        }
    } else {
        // Block compressed formats
        size_t blockSize = (format == DdsFormat::BC1 || format == DdsFormat::BC4) ? 8 : 16;
        size_t numBlocksX = (width + 3) / 4;
        size_t numBlocksY = (height + 3) / 4;

        for (size_t by = 0; by < numBlocksY; ++by) {
            for (size_t bx = 0; bx < numBlocksX; ++bx) {
                size_t blockOffset = (by * numBlocksX + bx) * blockSize;
                if (blockOffset + blockSize > availableBytes) {
                    break;
                }

                const uint8_t* pBlock = pSrc + blockOffset;
                bool isFullBlock = (bx * 4 + 4 <= width) && (by * 4 + 4 <= height);

                uint8_t tempBlock[4 * 4 * 4];
                uint8_t* pDst = isFullBlock
                    ? (rgba.data() + (by * 4 * width + bx * 4) * 4)
                    : tempBlock;
                int pitch = isFullBlock ? static_cast<int>(width * 4) : 16;

                switch (format) {
                case DdsFormat::BC1:
                    bcdec_bc1(pBlock, pDst, pitch);
                    break;
                case DdsFormat::BC2:
                    bcdec_bc2(pBlock, pDst, pitch);
                    break;
                case DdsFormat::BC3:
                    bcdec_bc3(pBlock, pDst, pitch);
                    break;
                case DdsFormat::BC4: {
                    uint8_t rVals[16];
                    bcdec_bc4(pBlock, rVals, 4);
                    for (int py = 0; py < 4; ++py) {
                        for (int px = 0; px < 4; ++px) {
                            uint8_t r = rVals[py * 4 + px];
                            uint8_t* pix = pDst + py * pitch + px * 4;
                            pix[0] = r;
                            pix[1] = r;
                            pix[2] = r;
                            pix[3] = 255;
                        }
                    }
                    break;
                }
                case DdsFormat::BC5: {
                    // Two-channel ATI2 format (typically MSFS normal map: Red=X, Green=Y)
                    uint8_t rgVals[16 * 2];
                    bcdec_bc5(pBlock, rgVals, 4 * 2);
                    for (int py = 0; py < 4; ++py) {
                        for (int px = 0; px < 4; ++px) {
                            uint8_t r = rgVals[(py * 4 + px) * 2 + 0];
                            uint8_t g = rgVals[(py * 4 + px) * 2 + 1];
                            float nx = (r / 255.0f) * 2.0f - 1.0f;
                            float ny = (g / 255.0f) * 2.0f - 1.0f;
                            float nz2 = 1.0f - nx * nx - ny * ny;
                            float nz = (nz2 > 0.0f) ? std::sqrt(nz2) : 0.0f;
                            uint8_t b = static_cast<uint8_t>(std::clamp((nz * 0.5f + 0.5f) * 255.0f, 0.0f, 255.0f));

                            uint8_t* pix = pDst + py * pitch + px * 4;
                            pix[0] = r;
                            pix[1] = g;
                            pix[2] = b;
                            pix[3] = 255;
                        }
                    }
                    break;
                }
                case DdsFormat::BC6H_UF16:
                case DdsFormat::BC6H_SF16: {
                    float rgbF[16 * 3];
                    bcdec_bc6h_float(pBlock, rgbF, 4 * 3 * sizeof(float), format == DdsFormat::BC6H_SF16 ? 1 : 0);
                    for (int py = 0; py < 4; ++py) {
                        for (int px = 0; px < 4; ++px) {
                            float rf = rgbF[(py * 4 + px) * 3 + 0];
                            float gf = rgbF[(py * 4 + px) * 3 + 1];
                            float bf = rgbF[(py * 4 + px) * 3 + 2];
                            uint8_t* pix = pDst + py * pitch + px * 4;
                            pix[0] = static_cast<uint8_t>(std::clamp(rf * 255.0f, 0.0f, 255.0f));
                            pix[1] = static_cast<uint8_t>(std::clamp(gf * 255.0f, 0.0f, 255.0f));
                            pix[2] = static_cast<uint8_t>(std::clamp(bf * 255.0f, 0.0f, 255.0f));
                            pix[3] = 255;
                        }
                    }
                    break;
                }
                case DdsFormat::BC7:
                    bcdec_bc7(pBlock, pDst, pitch);
                    break;
                default:
                    break;
                }

                // If edge block, copy only in-bounds pixels
                if (!isFullBlock) {
                    size_t copyW = std::min<size_t>(4, width - bx * 4);
                    size_t copyH = std::min<size_t>(4, height - by * 4);
                    for (size_t py = 0; py < copyH; ++py) {
                        uint8_t* dstRow = rgba.data() + ((by * 4 + py) * width + bx * 4) * 4;
                        const uint8_t* srcRow = tempBlock + py * 16;
                        std::memcpy(dstRow, srcRow, copyW * 4);
                    }
                }
            }
        }
    }

    // Initialize output image with 8-bit RGBA uncompressed pixels
    bool success = _image.initializeLayout(
        8, 8, 8, 8,
        image::Format::NORM, false, false, false, false,
        0, 1, 2, 3);
    if (!success) {
        return ImageDecodingError::InvalidInput;
    }

    if (!_image.initialize(rgba.data(), rgba.size(), width, height, 1, 1, 1, 1)) {
        return ImageDecodingError::InvalidInput;
    }

    return ImageDecodingResult::Success;
}

} // namespace ux3d::imageformats
