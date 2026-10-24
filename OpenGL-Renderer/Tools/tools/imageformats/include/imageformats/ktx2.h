// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Moritz Willy Becher 🏝

#pragma once
#include "core/Vector.h"
#include "gpu/Types.h"
#include "imageformats/ImageDecodingResult.h"
#include "imageformats/ImageEncodingResult.h"
#

struct ktxTexture2;

namespace ux3d::image {
class Image;
struct KTXMetaData;
}

namespace ux3d::imageformats {

// Header-only description of a KTX2 file, used to keep block-compressed BCn data compressed
// from decode to the GL upload instead of expanding it to RGBA8.
struct KTX2ImageInfo {
    gpu::Format format = gpu::Format::UNDEFINED;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 1;
    uint32_t layerCount = 1;
    uint32_t faceCount = 1;
    uint32_t levelCount = 1;
    uint32_t supercompressionScheme = 0;
    bool blockCompressed = false;
};

// Parses the KTX2 header. Returns false when the buffer is not a KTX2 file.
bool getKTX2ImageInfo(const core::Vector<uint8_t>& _buffer, KTX2ImageInfo& _info);

// S3TC/RGTC/BPTC formats Glider uploads without a CPU decode.
bool isSupportedCompressedFormat(gpu::Format _format);
// A supported BCn file with no supercompression and a single 2D image.
bool isSupportedCompressedKTX2(const KTX2ImageInfo& _info);
// Compressed byte size of one mip level (one face/layer).
uint64_t getKTX2CompressedLevelSize(const KTX2ImageInfo& _info, uint32_t _level);
// Byte size of all mip levels (faces and layers included), matching decodeKTX's output order.
uint64_t getKTX2CompressedDataSize(const KTX2ImageInfo& _info);

// Human-readable Vulkan format name for the KTX2 header facts in the Inspector.
const char* getKTX2FormatName(gpu::Format _format);

// Decodes the mip level whose largest dimension is at most _maxDimension to RGBA8 for
// the Inspector preview. BC5 SNORM is treated as a normal map: signed X/Y are rebuilt into
// Z and remapped with x * 0.5 + 0.5. With _signedSquareNormal the packaged A1.12 encoding
// x = sign(p) * p * p is applied first. Returns false for non-KTX2 or unsupported data.
bool decodeKTX2Preview(const core::Vector<uint8_t>& _buffer, uint32_t _maxDimension,
                       core::Vector<uint8_t>& _rgba, uint32_t& _width, uint32_t& _height,
                       bool _signedSquareNormal);

void fillKTXMetaData(const ktxTexture2* texture, image::Image& _image, const core::Vector<uint8_t>& _buffer);

ImageDecodingResult decodeKTX(const core::Vector<uint8_t>& buffer, image::Image& image);

ImageEncodingResult encodeKTX(const image::Image& image, core::Vector<uint8_t>& buffer, const image::KTXMetaData& _basisUCompressionConfig);

}
