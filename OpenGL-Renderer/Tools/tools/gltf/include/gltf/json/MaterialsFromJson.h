#pragma once

#include "ChildOfRootFromJson.h"
#include "core/Flag.h"
#include "core/String.h"
#include "gltf/Extensions.h"

namespace ux3d::gltf {
class Material;
class TextureInfo;
class PbrMetallicRoughness;
class PbrSpecularGlossiness;
class Clearcoat;
class Specular;
class Sheen;
class Ior;
class Transmission;
class Volume;
class Iridescence;
class EmissiveStrength;

class MaterialsFromJson : public ChildOfRootFromJson {
private:
    enum class ExtensionCategory : uint32_t {
        None = 0x0u,
        PBRSpecularGlossiness = 0x1u,
        Unlit = 0x2u,
        PBRNext = 0x4u
    };
    using ExtensionCategories = core::Flag<ExtensionCategory>;

    void convertAlphaMode(const core::string& _jsonAlphaMode, Material& _material) const;
    ConversionResult convertTextureInfo(const nlohmann::json& _jsonTextureInfo, TextureInfo& _textureInfo) const;
    ConversionResult convertTextureTransform(const nlohmann::json& _jsonTextureTransform, TextureInfo& _textureInfo) const;
    ConversionResult convertMetallicRoughness(const nlohmann::json& _jsonMetallicRoughness, PbrMetallicRoughness& _metallicRoughness) const;
    ConversionResult convertSpecularGlossiness(const nlohmann::json& _jsonSpecularGlossiness, PbrSpecularGlossiness& _specularGlossiness) const;
    ConversionResult convertClearcoat(const nlohmann::json& _jsonClearcoat, Clearcoat& _clearcoat) const;
    ConversionResult convertSpecular(const nlohmann::json& _jsonSpecular, Specular& _specular) const;
    ConversionResult convertSheen(const nlohmann::json& _jsonSheen, Sheen& _sheen) const;
    ConversionResult convertIor(const nlohmann::json& _jsonIor, Ior& ior) const;
    ConversionResult convertTransmission(const nlohmann::json& _jsonTransmission, Transmission& _transmission) const;
    ConversionResult convertVolume(const nlohmann::json& _jsonvolume, Volume& _volume) const;
    ConversionResult convertIridescence(const nlohmann::json& _json, Iridescence& _iridescence) const;
    ConversionResult convertEmissiveStrength(const nlohmann::json&, EmissiveStrength&) const;

    ConversionResult resolveMetallicRoughness(const nlohmann::json& _jsonMetallicRoughness, PbrMetallicRoughness& _metallicRoughness);
    ConversionResult resolveSpecularGlossiness(const nlohmann::json& _jsonSpecularGlossiness, PbrSpecularGlossiness& _specularGlossiness);
    ConversionResult resolveClearcoat(const nlohmann::json& _jsonClearcoat, Clearcoat& _clearcoat);
    ConversionResult resolveTextureInfo(const nlohmann::json& _jsonTextureInfo, TextureInfo& _textureInfo);
    ConversionResult resolveSpecular(const nlohmann::json& _jsonSpecular, Specular& _specular);
    ConversionResult resolveSheen(const nlohmann::json& _jsonSheen, Sheen& _sheen);
    ConversionResult resolveTransmission(const nlohmann::json& _jsonTransmission, Transmission& _transmission);
    ConversionResult resolveVolume(const nlohmann::json& _jsonVolume, Volume& _volume);
    ConversionResult resolveIridescence(const nlohmann::json& _json, Iridescence& _iridescence);
    ConversionResult resolveXMP(const nlohmann::json& _jsonXMP, Material& _material);

public:
    using ChildOfRootFromJson::ChildOfRootFromJson;
    ConversionResult convert(const nlohmann::json& _jsonMaterial) override;
    ConversionResult resolve(const nlohmann::json& _jsonMaterial, Material& _material);
};
}
