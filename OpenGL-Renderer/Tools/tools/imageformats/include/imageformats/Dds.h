// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "imageformats/ImageDecodingResult.h"
#include "core/Vector.h"
#include "image/Image.h"

namespace ux3d::imageformats {

ImageDecodingResult decodeDDS(const core::Vector<uint8_t>& _buffer, image::Image& _image);

} // namespace ux3d::imageformats
