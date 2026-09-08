// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/json/BuffersToJson.h"
#include "gltf/Buffer.h"
#include "gltf/ExportOptions.h"

using namespace ux3d;
using namespace ux3d::gltf;

void BuffersToJson::convert(const Buffer& _buffer, nlohmann::json& _jsonBuffer, const JsonPath& _jsonPath) const
{
    convertBase(_buffer, _jsonBuffer);
    convertUri(_buffer, _jsonBuffer);
    convertByteLength(_buffer, _jsonBuffer);

    if (m_exportOptions.conversionUsage == ConversionUsage::StateHistory || m_exportOptions.conversionUsage == ConversionUsage::ProjectFile) {
        convertStorageFormat(_buffer, _jsonBuffer);
    }
}

void BuffersToJson::convertUri(const Buffer& _buffer, nlohmann::json& _jsonBuffer) const
{
    if (_buffer.hasUri()) {
        _jsonBuffer["uri"] = _buffer.getUri();
    }
}

void BuffersToJson::convertByteLength(const Buffer& _buffer, nlohmann::json& _jsonBuffer) const
{
    core::MemoryRange<const uint8_t> bufferData;
    _buffer.getData(bufferData);

    size_t byteLength = bufferData.size();
    if (byteLength == 0u) {
        core::logWarning("BuffersToJson: ByteLength is 0");
        return;
    }

    _jsonBuffer["byteLength"] = byteLength;
}

void BuffersToJson::convertStorageFormat(const Buffer& _buffer, nlohmann::json& _jsonBuffer) const
{
    if (_buffer.getStorageFormat() != StorageFormat::Undefined) {
        _jsonBuffer["extensions"]["ux3d_gestaltor"]["storageFormat"] = storageFormatToString(_buffer.getStorageFormat());
    }
}
