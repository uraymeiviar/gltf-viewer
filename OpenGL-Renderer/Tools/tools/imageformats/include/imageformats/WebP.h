// Copyright © 2024 UX3D GmbH. All rights reserved.
// Authors: Phillip Hohenester

#pragma once
#include "core/Vector.h"
#include "imageformats/ImageDecodingResult.h"
#include "imageformats/ImageEncodingResult.h"

struct ktxTexture2;

namespace ux3d::image {
class Image;
}

namespace ux3d::imageformats {

ImageDecodingResult decodeWebP(const core::Vector<uint8_t>& buffer, image::Image& image);

}
