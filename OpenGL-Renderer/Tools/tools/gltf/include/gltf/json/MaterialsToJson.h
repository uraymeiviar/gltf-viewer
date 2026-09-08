// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRootToJson.h"
#include "gltf/json/JsonPath.h"

#include <optional>

namespace ux3d::gltf {
class Material;
class TextureInfo;
class Gltf;

class MaterialsToJson : ChildOfRootToJson {
public:
    MaterialsToJson(const IndexLookup& _indices, const ExportOptions& _exportOptions, const Gltf& _gltf);
    void convert(const Material& _material, nlohmann::json& _jsonMaterial, const JsonPath& _jsonPath) const;

private:
    void convertPbrMetallicRoughness(const Material& _material, nlohmann::json& _jsonMaterial) const;
    void convertNormalTexture(const Material& _material, nlohmann::json& _jsonMaterial) const;
    void convertOcclusionTexture(const Material& _material, nlohmann::json& _jsonMaterial) const;
    void convertEmissiveTexture(const Material& _material, nlohmann::json& _jsonMaterial) const;
    void convertEmissiveFactor(const Material& _material, nlohmann::json& _jsonMaterial) const;
    void convertAlphaMode(const Material& _material, nlohmann::json& _jsonMaterial) const;
    void convertAlphaCutoff(const Material& _material, nlohmann::json& _jsonMaterial) const;
    void convertDoubleSided(const Material& _material, nlohmann::json& _jsonMaterial) const;
    void convertExtensionSpecularGlossiness(const Material& _material, nlohmann::json& _jsonMaterial) const;
    void convertExtensionUnlit(const Material& _material, nlohmann::json& _jsonMaterial) const;
    void convertExtensionClearcoat(const Material& _material, nlohmann::json& _jsonMaterial) const;
    void convertExtensionSpecular(const Material& _material, nlohmann::json& _jsonMaterial) const;
    void convertExtensionEmissiveStrength(const Material&, nlohmann::json&) const;
    void convertExtensionSheen(const Material& _material, nlohmann::json& _jsonMaterial) const;
    void convertExtensionIor(const Material& _material, nlohmann::json& _jsonMaterial) const;
    void convertExtensionTransmission(const Material& _material, nlohmann::json& _jsonMaterial) const;
    void convertExtensionTextureTransform(const std::optional<TextureInfo>& _oTextureInfo, nlohmann::json& _jsonMaterial) const;
    void convertExtensionXMP(const Material& _material, nlohmann::json& _jsonMaterial) const;
    void convertExtensionVolume(const Material& _material, nlohmann::json& _jsonMaterial) const;
    void convertExtensionIridescence(const Material& _material, nlohmann::json& _jsonMaterial) const;

    //helper functions
    bool convertTextureInfo(const std::optional<TextureInfo>& _oTextureInfo, const std::string& _entry, nlohmann::json& _jsonRoot) const;
    void convertNormalTextureInfo(const std::optional<TextureInfo>& _oTextureInfo, const std::string& _entry, nlohmann::json& _jsonRoot) const;
    void convertOcclusionTextureInfo(const std::optional<TextureInfo>& _oTextureInfo, const std::string& _entry, nlohmann::json& _jsonRoot) const;

    const Gltf& m_gltf;
};
}
