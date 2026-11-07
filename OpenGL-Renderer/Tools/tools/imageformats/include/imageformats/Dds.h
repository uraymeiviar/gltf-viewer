// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "imageformats/ImageDecodingResult.h"
#include "core/Vector.h"
#include "image/Image.h"
#include "imageformats/ktx2.h"

namespace ux3d::imageformats {

ImageDecodingResult decodeDDS(const core::Vector<uint8_t>& _buffer, image::Image& _image);

// The top level with the channels the file stores (ktx2.h StoredPixels): BC4 = R, BC5 = RG (no
// rebuilt Z), RGB8 = 3 channels. decodeDDS expands everything to RGBA8. False for BC6H.
bool decodeDDSStoredPixels(const core::Vector<uint8_t>& _buffer, StoredPixels& _out);

} // namespace ux3d::imageformats
