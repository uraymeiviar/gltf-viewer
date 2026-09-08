// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Moritz Willy Becher 🏝

#pragma once
#include "core/Vector.h"
#include "imageformats/ImageDecodingResult.h"
#include "imageformats/ImageEncodingResult.h"
#

struct ktxTexture2;

namespace ux3d::image {
class Image;
struct KTXMetaData;
}

namespace ux3d::imageformats {

void fillKTXMetaData(const ktxTexture2* texture, image::Image& _image, const core::Vector<uint8_t>& _buffer);

ImageDecodingResult decodeKTX(const core::Vector<uint8_t>& buffer, image::Image& image);

ImageEncodingResult encodeKTX(const image::Image& image, core::Vector<uint8_t>& buffer, const image::KTXMetaData& _basisUCompressionConfig);

}
