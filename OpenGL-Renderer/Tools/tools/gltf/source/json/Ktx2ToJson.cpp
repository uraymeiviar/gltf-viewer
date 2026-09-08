// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/json/Ktx2ToJson.h"
#include "gltf/Ktx2.h"

void ux3d::gltf::Ktx2ToJson::convert(const Ktx2& _ktx2, nlohmann::json& _jsonKtx2)
{
    convertBasicProperties(_ktx2, _jsonKtx2);
    convertLevels(_ktx2, _jsonKtx2);
    convertMetadata(_ktx2, _jsonKtx2);
}

void ux3d::gltf::Ktx2ToJson::convertBasicProperties(const Ktx2& _ktx2, nlohmann::json& _jsonKtx2) const
{
    _jsonKtx2["vkFormat"] = _ktx2.getVkFormat();
    _jsonKtx2["pixelWidth"] = _ktx2.getPixelWidth();
    _jsonKtx2["pixelHeight"] = _ktx2.getPixelHeight();
    _jsonKtx2["pixelDepth"] = _ktx2.getPixelDepth();
    _jsonKtx2["layerCount"] = _ktx2.getLayerCount();
    _jsonKtx2["faceCount"] = _ktx2.getFaceCount();
    _jsonKtx2["supercompressionScheme"] = _ktx2.getSupercompressionScheme();
    _jsonKtx2["dfdByteOffset"] = _ktx2.getDfdByteOffset();
    _jsonKtx2["dfdByteLength"] = _ktx2.getDfdByteLength();
    _jsonKtx2["sgdByteOffset"] = _ktx2.getSgdByteOffset();
    _jsonKtx2["sgdByteLength"] = _ktx2.getSgdByteLength();
    _jsonKtx2["generateMipmaps"] = _ktx2.getGenerateMipMaps();
}

void ux3d::gltf::Ktx2ToJson::convertLevels(const Ktx2& _ktx2, nlohmann::json& _jsonKtx2) const
{
    //"levels": type = array
    if (_ktx2.getLevels().empty()) {
        return;
    }

    nlohmann::json jsonLevels = nlohmann::json::array();

    for (const Ktx2Level& level : _ktx2.getLevels()) {
        nlohmann::json jsonLevel;
        jsonLevel["byteOffset"] = level.getByteOffset();
        jsonLevel["byteLength"] = level.getByteLength();
        jsonLevel["uncompressedByteLength"] = level.getUncompressedByteLength();

        jsonLevels.emplace_back(jsonLevel);
    }

    _jsonKtx2["levels"] = jsonLevels;
}

void ux3d::gltf::Ktx2ToJson::convertMetadata(const Ktx2& _ktx2, nlohmann::json& _jsonKtx2) const
{
    //"metadata": type = object
    nlohmann::json jsonMetadata;
    const Ktx2::KTXMetaData& metadata = _ktx2.getMetaData();

    jsonMetadata["KTXcubemapIncomplete"] = metadata.KTXcubemapIncomplete;
    jsonMetadata["KTXorientation"] = metadata.KTXorientation;
    nlohmann::json jsonGlFormat = nlohmann::json::array();
    jsonGlFormat.emplace_back(metadata.KTXglFormat[0]);
    jsonGlFormat.emplace_back(metadata.KTXglFormat[1]);
    jsonGlFormat.emplace_back(metadata.KTXglFormat[2]);
    jsonMetadata["KTXglFormat"] = jsonGlFormat;
    jsonMetadata["KTXdxgiFormat__"] = metadata.KTXdxgiFormat;
    jsonMetadata["KTXmetalPixelFormat"] = metadata.KTXmetalPixelFormat;
    jsonMetadata["KTXswizzle"] = metadata.KTXswizzle;
    jsonMetadata["KTXwriter"] = metadata.KTXwriter;
    jsonMetadata["KTXastcDecodeRGB9E5"] = metadata.KTXastcDecodeRGB9E5;

    _jsonKtx2["metadata"] = jsonMetadata;
}
