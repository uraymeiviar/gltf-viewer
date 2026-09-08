// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/EmbeddedData.h"
#include "core/Slice.h"
#include "core/String.h"
#include "gltf/Base64.h"
#include "imageformats/imageformats.h"

bool ux3d::gltf::embedded_data::isDataUri(const core::string& _inputUrl)
{
    if (core::startsWith(core::toLower(_inputUrl), "data:")) {
        return true;
    }

    return false;
}

ux3d::gltf::ConversionResult ux3d::gltf::embedded_data::extractBase64Data(const core::string& _inputUri, core::string& _outMimeType, core::Vector<uint8_t>& _outBuffer)
{
    if (!isDataUri(_inputUri)) {
        return ConversionError::UnsupportedFileFormat;
    }

    if (!core::contains(core::toLower(_inputUri), ";base64,")) {
        return ConversionError::UnsupportedFileFormat;
    }

    _outMimeType = "";
    if (core::contains(core::toLower(_inputUri), imageformats::imageEncodingToString(imageformats::ImageEncoding::PNG).c_str())) {
        _outMimeType = imageformats::imageEncodingToString(imageformats::ImageEncoding::PNG);
    }
    if (core::contains(core::toLower(_inputUri), imageformats::imageEncodingToString(imageformats::ImageEncoding::JPEG).c_str())) {
        _outMimeType = imageformats::imageEncodingToString(imageformats::ImageEncoding::JPEG);
    }
    if (core::contains(core::toLower(_inputUri), imageformats::imageEncodingToString(imageformats::ImageEncoding::KTX2).c_str())) {
        _outMimeType = imageformats::imageEncodingToString(imageformats::ImageEncoding::KTX2);
    }
    if (core::contains(core::toLower(_inputUri), "application/octet-stream")) {
        _outMimeType = "application/octet-stream";
    }

    core::string dataString;
    size_t pos = _inputUri.find(";base64,");
    pos += strlen(";base64,");
    dataString = _inputUri.substr(pos);
    Base64::decode(dataString, _outBuffer);

    return ConversionResult::Success;
}

ux3d::gltf::ConversionResult ux3d::gltf::embedded_data::createBase64Data(core::MemoryRange<const uint8_t>& _inBuffer, const core::string& _inMimeType, core::string& _outUri)
{
    _outUri = "data:" + _inMimeType + ";base64,";

    if (_inBuffer.empty()) {
        return ConversionResult::Success;
    }

    _outUri += Base64::encode(_inBuffer);

    return ConversionResult::Success;
}
