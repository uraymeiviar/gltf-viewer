#pragma once

#include "core/String.h"
#include "core/Vector.h"
#include "json/ConversionResult.h"

namespace ux3d::gltf::embedded_data {

bool isDataUri(const core::string& _inputUrl);

ConversionResult extractBase64Data(const core::string& _inputUri, core::string& _outMimeType, core::Vector<uint8_t>& _outBuffer);

ConversionResult createBase64Data(core::MemoryRange<const uint8_t>& _inBuffer, const core::string& _inMimeType, core::string& _outUri);

}
