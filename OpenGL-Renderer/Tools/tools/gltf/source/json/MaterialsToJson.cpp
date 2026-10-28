// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/json/MaterialsToJson.h"
#include "gltf/ExportOptions.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/Material.h"
#include "gltf/TextureInfo.h"
#include "gltf/XMP.h"
#include "gltf/json/GltfObjectToJson.h"
#include "gltf/json/ParsingUtils.h"

using namespace ux3d;
using namespace ux3d::gltf;

MaterialsToJson::MaterialsToJson(const IndexLookup& _indices, const ExportOptions& _exportOptions, const Gltf& _gltf)
    : ChildOfRootToJson(_indices, _exportOptions)
    , m_gltf(_gltf)
{
}

void MaterialsToJson::convert(const Material& _material, nlohmann::json& _jsonMaterial, const JsonPath& _jsonPath) const
{
    convertBase(_material, _jsonMaterial);
    convertPbrMetallicRoughness(_material, _jsonMaterial);
    convertNormalTexture(_material, _jsonMaterial);
    convertOcclusionTexture(_material, _jsonMaterial);
    convertEmissiveTexture(_material, _jsonMaterial);
    convertEmissiveFactor(_material, _jsonMaterial);
    convertAlphaMode(_material, _jsonMaterial);
    convertAlphaCutoff(_material, _jsonMaterial);
    convertDoubleSided(_material, _jsonMaterial);

    convertExtensionSpecularGlossiness(_material, _jsonMaterial);
    convertExtensionUnlit(_material, _jsonMaterial);
    convertExtensionClearcoat(_material, _jsonMaterial);
    convertExtensionSpecular(_material, _jsonMaterial);
    convertExtensionSheen(_material, _jsonMaterial);
    convertExtensionIor(_material, _jsonMaterial);
    convertExtensionEmissiveStrength(_material, _jsonMaterial);
    convertExtensionTransmission(_material, _jsonMaterial);
    convertExtensionXMP(_material, _jsonMaterial);
    convertExtensionVolume(_material, _jsonMaterial);
    convertExtensionIridescence(_material, _jsonMaterial);
    convertExtensionAnisotropy(_material, _jsonMaterial);
}

void MaterialsToJson::convertPbrMetallicRoughness(const Material& _material, nlohmann::json& _jsonMaterial) const
{
    const std::optional<PbrMetallicRoughness>& oPbrMetallicRoughness = _material.getPbrMetallicRoughness();
    if (oPbrMetallicRoughness.has_value() == false) {
        return;
    }

    nlohmann::json jsonPbrMetallicRoughness = nlohmann::json::object();

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(oPbrMetallicRoughness.value(), jsonPbrMetallicRoughness);
    gltfObjectToJson.convertUnknownExtensions(oPbrMetallicRoughness.value(), jsonPbrMetallicRoughness);

    if (oPbrMetallicRoughness->hasBaseColorFactor()) {
        nlohmann::json jsonArray = nlohmann::json::array();
        vectorToDoubles(oPbrMetallicRoughness->getBaseColorFactorOrDefault(), jsonArray);
        jsonPbrMetallicRoughness["baseColorFactor"] = jsonArray;
    }

    convertTextureInfo(oPbrMetallicRoughness->getBaseColorTextureInfo(), "baseColorTexture", jsonPbrMetallicRoughness);

    if (oPbrMetallicRoughness->hasMetallicFactor()) {
        jsonPbrMetallicRoughness["metallicFactor"] = oPbrMetallicRoughness->getMetallicFactorOrDefault();
    }

    if (oPbrMetallicRoughness->hasRoughnessFactor()) {
        jsonPbrMetallicRoughness["roughnessFactor"] = oPbrMetallicRoughness->getRoughnessFactorOrDefault();
    }

    convertTextureInfo(oPbrMetallicRoughness->getMetallicRoughnessTextureInfo(), "metallicRoughnessTexture", jsonPbrMetallicRoughness);

    _jsonMaterial["pbrMetallicRoughness"] = jsonPbrMetallicRoughness;
}

void MaterialsToJson::convertNormalTexture(const Material& _material, nlohmann::json& _jsonMaterial) const
{
    convertNormalTextureInfo(_material.getNormalTextureInfo(), "normalTexture", _jsonMaterial);
}

void MaterialsToJson::convertOcclusionTexture(const Material& _material, nlohmann::json& _jsonMaterial) const
{
    convertOcclusionTextureInfo(_material.getOcclusionTextureInfo(), "occlusionTexture", _jsonMaterial);
}

void MaterialsToJson::convertEmissiveTexture(const Material& _material, nlohmann::json& _jsonMaterial) const
{
    convertTextureInfo(_material.getEmissiveTextureInfo(), "emissiveTexture", _jsonMaterial);
}

void MaterialsToJson::convertEmissiveFactor(const Material& _material, nlohmann::json& _jsonMaterial) const
{
    if (_material.hasEmissiveFactor() == false) {
        return;
    }

    nlohmann::json jsonArray = nlohmann::json::array();
    vectorToDoubles(_material.getEmissiveFactorOrDefault(), jsonArray);
    _jsonMaterial["emissiveFactor"] = jsonArray;
}

void MaterialsToJson::convertAlphaMode(const Material& _material, nlohmann::json& _jsonMaterial) const
{
    if (_material.hasAlphaMode() == false) {
        return;
    }
    _jsonMaterial["alphaMode"] = alphaModeToString(_material.getAlphaModeOrDefault());
}

void MaterialsToJson::convertAlphaCutoff(const Material& _material, nlohmann::json& _jsonMaterial) const
{
    if (_material.hasAlphaCutoff() == false) {
        return;
    }
    _jsonMaterial["alphaCutoff"] = _material.getAlphaCutoffOrDefault();
}

void MaterialsToJson::convertDoubleSided(const Material& _material, nlohmann::json& _jsonMaterial) const
{
    if (_material.hasDoubleSided() == false) {
        return;
    }
    _jsonMaterial["doubleSided"] = _material.isDoubleSidedOrDefault();
}

void MaterialsToJson::convertExtensionSpecularGlossiness(const Material& _material, nlohmann::json& _jsonMaterial) const
{
    const std::optional<gltf::PbrSpecularGlossiness>& oSpecularGlossiness = _material.getPbrSpecularGlossiness();

    if (oSpecularGlossiness.has_value() == false) {
        return;
    }

    nlohmann::json jsonSpecularGlossiness = nlohmann::json::object();

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(oSpecularGlossiness.value(), jsonSpecularGlossiness);
    gltfObjectToJson.convertUnknownExtensions(oSpecularGlossiness.value(), jsonSpecularGlossiness);

    if (oSpecularGlossiness->hasDiffuseFactor()) {
        nlohmann::json jsonArray = nlohmann::json::array();
        vectorToDoubles(oSpecularGlossiness->getDiffuseFactorOrDefault(), jsonArray);
        jsonSpecularGlossiness["diffuseFactor"] = jsonArray;
    }

    convertTextureInfo(oSpecularGlossiness->getDiffuseTextureInfo(), "diffuseTexture", jsonSpecularGlossiness);

    if (oSpecularGlossiness->hasSpecularFactor()) {
        nlohmann::json jsonArray = nlohmann::json::array();
        vectorToDoubles(oSpecularGlossiness->getSpecularFactorOrDefault(), jsonArray);
        jsonSpecularGlossiness["specularFactor"] = jsonArray;
    }

    if (oSpecularGlossiness->hasGlossinessFactor()) {
        jsonSpecularGlossiness["glossinessFactor"] = oSpecularGlossiness->getGlossinessFactorOrDefault();
    }

    convertTextureInfo(oSpecularGlossiness->getSpecularGlossinessTextureInfo(), "specularGlossinessTexture", jsonSpecularGlossiness);

    _jsonMaterial["extensions"]["KHR_materials_pbrSpecularGlossiness"] = jsonSpecularGlossiness;
}

void MaterialsToJson::convertExtensionUnlit(const Material& _material, nlohmann::json& _jsonMaterial) const
{
    const std::optional<gltf::Unlit>& oUnlit = _material.getUnlit();

    if (oUnlit.has_value() == false) {
        return;
    }

    nlohmann::json jsonUnlit = nlohmann::json::object();

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(oUnlit.value(), jsonUnlit);
    gltfObjectToJson.convertUnknownExtensions(oUnlit.value(), jsonUnlit);

    _jsonMaterial["extensions"]["KHR_materials_unlit"] = jsonUnlit;
}

void MaterialsToJson::convertExtensionClearcoat(const Material& _material, nlohmann::json& _jsonMaterial) const
{
    const std::optional<gltf::Clearcoat>& oClearcoat = _material.getClearcoat();
    if (oClearcoat.has_value() == false) {
        return;
    }

    nlohmann::json jsonClearcoat = nlohmann::json::object();

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(oClearcoat.value(), jsonClearcoat);
    gltfObjectToJson.convertUnknownExtensions(oClearcoat.value(), jsonClearcoat);

    if (oClearcoat->hasClearcoatFactor()) {
        jsonClearcoat["clearcoatFactor"] = oClearcoat->getClearcoatFactorOrDefault(); //	number
    }
    convertTextureInfo(oClearcoat->getClearcoatTexture(), "clearcoatTexture", jsonClearcoat); //	textureInfo
    if (oClearcoat->hasClearcoatRoughnessFactor()) {
        jsonClearcoat["clearcoatRoughnessFactor"] = oClearcoat->getClearcoatRoughnessFactorOrDefault(); //	number
    }
    convertTextureInfo(oClearcoat->getClearcoatRoughnessTexture(), "clearcoatRoughnessTexture", jsonClearcoat); //	textureInfo
    convertNormalTextureInfo(oClearcoat->getClearcoatNormalTexture(), "clearcoatNormalTexture", jsonClearcoat); // clearcoatNormalTexture

    _jsonMaterial["extensions"]["KHR_materials_clearcoat"] = jsonClearcoat;
}

void MaterialsToJson::convertExtensionEmissiveStrength(const Material& material, nlohmann::json& jsonMaterial) const
{
    const auto& emissiveStrength = material.getEmissiveStrength();
    if (!emissiveStrength.has_value()) {
        return;
    }

    nlohmann::json jsonEmissiveStrength = nlohmann::json::object();

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(emissiveStrength.value(), jsonEmissiveStrength);
    gltfObjectToJson.convertUnknownExtensions(emissiveStrength.value(), jsonEmissiveStrength);

    if (emissiveStrength->hasEmissiveStrength()) {
        jsonEmissiveStrength["emissiveStrength"] = emissiveStrength->getEmissiveStrengthOrDefault();
    }

    jsonMaterial["extensions"]["KHR_materials_emissive_strength"] = jsonEmissiveStrength;
}

void MaterialsToJson::convertExtensionSpecular(const Material& _material, nlohmann::json& _jsonMaterial) const
{
    const std::optional<gltf::Specular>& oSpecular = _material.getSpecular();
    if (oSpecular.has_value() == false) {
        return;
    }

    nlohmann::json jsonSpecular = nlohmann::json::object();

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(oSpecular.value(), jsonSpecular);
    gltfObjectToJson.convertUnknownExtensions(oSpecular.value(), jsonSpecular);

    if (oSpecular->hasSpecularFactor()) {
        jsonSpecular["specularFactor"] = oSpecular->getSpecularFactorOrDefault();
    }
    if (oSpecular->hasSpecularColorFactor()) {
        nlohmann::json jsonArray = nlohmann::json::array();
        vectorToDoubles(oSpecular->getSpecularColorFactorOrDefault(), jsonArray);
        jsonSpecular["specularColorFactor"] = jsonArray;
    }
    if (oSpecular->hasSpecularTextureInfo()) {
        convertTextureInfo(oSpecular->getSpecularTextureInfo(), "specularTexture", jsonSpecular);
    }
    if (oSpecular->hasSpecularColorTextureInfo()) {
        convertTextureInfo(oSpecular->getSpecularColorTextureInfo(), "specularColorTexture", jsonSpecular);
    }

    _jsonMaterial["extensions"]["KHR_materials_specular"] = jsonSpecular;
}

void MaterialsToJson::convertExtensionSheen(const Material& _material, nlohmann::json& _jsonMaterial) const
{
    const std::optional<gltf::Sheen>& oSheen = _material.getSheen();
    if (oSheen.has_value() == false) {
        return;
    }

    nlohmann::json jsonSheen = nlohmann::json::object();

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(oSheen.value(), jsonSheen);
    gltfObjectToJson.convertUnknownExtensions(oSheen.value(), jsonSheen);

    if (oSheen->hasSheenRoughnessFactor()) {
        jsonSheen["sheenRoughnessFactor"] = oSheen->getSheenRoughnessFactorOrDefault();
    }
    if (oSheen->hasSheenColorFactor()) {
        nlohmann::json jsonArray = nlohmann::json::array();
        vectorToDoubles(oSheen->getSheenColorFactorOrDefault(), jsonArray);
        jsonSheen["sheenColorFactor"] = jsonArray;
    }
    if (oSheen->hasSheenColorTextureInfo()) {
        convertTextureInfo(oSheen->getSheenColorTextureInfo(), "sheenColorTexture", jsonSheen);
    }
    if (oSheen->hasSheenRoughnessTextureInfo()) {
        convertTextureInfo(oSheen->getSheenRoughnessTextureInfo(), "sheenRoughnessTexture", jsonSheen);
    }

    _jsonMaterial["extensions"]["KHR_materials_sheen"] = jsonSheen;
}

void MaterialsToJson::convertExtensionTransmission(const Material& _material, nlohmann::json& _jsonMaterial) const
{
    const std::optional<gltf::Transmission>& oTransmission = _material.getTransmission();
    if (oTransmission.has_value() == false) {
        return;
    }

    nlohmann::json jsonTransmission = nlohmann::json::object();

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(oTransmission.value(), jsonTransmission);
    gltfObjectToJson.convertUnknownExtensions(oTransmission.value(), jsonTransmission);

    if (oTransmission->hasTransmissionFactor()) {
        jsonTransmission["transmissionFactor"] = oTransmission->getTransmissionFactorOrDefault();
    }
    if (oTransmission->hasTransmissionTextureInfo()) {
        convertTextureInfo(oTransmission->getTransmissionTextureInfo(), "transmissionTexture", jsonTransmission);
    }

    _jsonMaterial["extensions"]["KHR_materials_transmission"] = jsonTransmission;
}

void MaterialsToJson::convertExtensionIor(const Material& _material, nlohmann::json& _jsonMaterial) const
{
    const std::optional<gltf::Ior>& oIor = _material.getIor();
    if (oIor.has_value() == false) {
        return;
    }

    nlohmann::json jsonIor = nlohmann::json::object();

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(oIor.value(), jsonIor);
    gltfObjectToJson.convertUnknownExtensions(oIor.value(), jsonIor);

    if (oIor->hasIor()) {
        jsonIor["ior"] = oIor->getIorOrDefault();
    }
    _jsonMaterial["extensions"]["KHR_materials_ior"] = jsonIor;
}

void MaterialsToJson::convertExtensionTextureTransform(const std::optional<TextureInfo>& _oTextureInfo, nlohmann::json& _jsonRoot) const
{
    if (_oTextureInfo.has_value() == false) {
        return;
    }

    const std::optional<gltf::TextureTransformExtension>& textureTransform = _oTextureInfo->getTextureTransform();
    if (textureTransform.has_value() == false) {
        return;
    }

    nlohmann::json jsonTextureTransform = nlohmann::json::object();

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(textureTransform.value(), jsonTextureTransform);
    gltfObjectToJson.convertUnknownExtensions(textureTransform.value(), jsonTextureTransform);

    if (textureTransform->getOffset().has_value()) {
        nlohmann::json jsonOffset = nlohmann::json::array();
        jsonOffset[0] = textureTransform->getOffset().value()[0];
        jsonOffset[1] = textureTransform->getOffset().value()[1];
        jsonTextureTransform["offset"] = jsonOffset;
    }

    if (textureTransform->getRotation().has_value()) {
        jsonTextureTransform["rotation"] = textureTransform->getRotation().value();
    }

    if (textureTransform->getScale().has_value()) {
        nlohmann::json jsonScale = nlohmann::json::array();
        jsonScale[0] = textureTransform->getScale().value()[0];
        jsonScale[1] = textureTransform->getScale().value()[1];
        jsonTextureTransform["scale"] = jsonScale;
    }

    if (textureTransform->getTexCoord().has_value()) {
        jsonTextureTransform["texCoord"] = textureTransform->getTexCoord().value();
    }

    _jsonRoot["extensions"]["KHR_texture_transform"] = jsonTextureTransform;
}

void MaterialsToJson::convertExtensionXMP(const Material& _material, nlohmann::json& _jsonMaterial) const
{
    if (_material.hasXMPPacket()) {
        const PacketContainer& container = _material.getXMPPacket().value();
        Packet* pPacket = container.m_packet;
        if (m_gltf.getXMP().has_value()) {
            const XMP& xmp = m_gltf.getXMP().value();
            const core::LinkedList<Packet>& packets = xmp.getPackets();

            for (size_t i = 0; i < packets.size(); ++i) {
                if (&packets.at(i) == pPacket) {
                    _jsonMaterial["extensions"]["KHR_xmp_json_ld"]["packet"] = i;
                    break;
                }
            }
        }

        // convert extras and extensions
        GltfObjectToJson gltfObjectToJson(m_exportOptions);
        gltfObjectToJson.convertExtras(container, _jsonMaterial["extensions"]["KHR_xmp_json_ld"]);
        gltfObjectToJson.convertUnknownExtensions(container, _jsonMaterial["extensions"]["KHR_xmp_json_ld"]);
    }
}

bool MaterialsToJson::convertTextureInfo(const std::optional<TextureInfo>& _oTextureInfo, const std::string& _entry, nlohmann::json& _jsonRoot) const
{
    if (_oTextureInfo.has_value() == false) {
        return false;
    }

    if (false == _oTextureInfo->isValid() && m_exportOptions.conversionUsage == ConversionUsage::ValidGltf) {
        return false;
    }

    nlohmann::json jsonTextureInfo = nlohmann::json::object();

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(_oTextureInfo.value(), jsonTextureInfo);
    gltfObjectToJson.convertUnknownExtensions(_oTextureInfo.value(), jsonTextureInfo);

    if (_oTextureInfo->isValid()) {
        size_t textureIndex = m_indices[_oTextureInfo->getTexture()];
        jsonTextureInfo["index"] = textureIndex;
    }

    if (_oTextureInfo->getTexCoord().has_value()) {
        jsonTextureInfo["texCoord"] = _oTextureInfo->getTexCoord().value();
    }

    convertExtensionTextureTransform(_oTextureInfo, jsonTextureInfo);

    _jsonRoot[_entry] = jsonTextureInfo;
    return true;
}

void MaterialsToJson::convertNormalTextureInfo(const std::optional<TextureInfo>& _oTextureInfo, const std::string& _entry, nlohmann::json& _jsonRoot) const
{
    if (_oTextureInfo.has_value() == false) {
        return;
    }
    // Fill properties: index, texCoord, extensions
    bool textureInfoConverted = convertTextureInfo(_oTextureInfo, _entry, _jsonRoot);

    if (!textureInfoConverted && m_exportOptions.conversionUsage == ConversionUsage::ValidGltf) {
        return;
    }

    if (_oTextureInfo->hasScaleOrStrength()) {
        _jsonRoot[_entry]["scale"] = _oTextureInfo->getScale();
    }
}

void MaterialsToJson::convertOcclusionTextureInfo(const std::optional<TextureInfo>& _oTextureInfo, const std::string& _entry, nlohmann::json& _jsonRoot) const
{
    if (_oTextureInfo.has_value() == false) {
        return;
    }
    // Fill properties: index, texCoord, extensions
    bool textureInfoConverted = convertTextureInfo(_oTextureInfo, _entry, _jsonRoot);

    if (!textureInfoConverted && m_exportOptions.conversionUsage == ConversionUsage::ValidGltf) {
        return;
    }

    if (_oTextureInfo->hasScaleOrStrength()) {
        _jsonRoot[_entry]["strength"] = _oTextureInfo->getStrength();
    }
}

void MaterialsToJson::convertExtensionVolume(const Material& _material, nlohmann::json& _jsonMaterial) const
{
    const std::optional<gltf::Volume>& oVolume = _material.getVolume();
    if (oVolume.has_value() == false) {
        return;
    }

    nlohmann::json jsonVolume = nlohmann::json::object();

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(oVolume.value(), jsonVolume);
    gltfObjectToJson.convertUnknownExtensions(oVolume.value(), jsonVolume);

    if (oVolume->hasThicknessFactor()) {
        jsonVolume["thicknessFactor"] = oVolume->getThicknessFactorOrDefault();
    }
    if (oVolume->hasAttenuationDistance()) {
        jsonVolume["attenuationDistance"] = oVolume->getAttenuationDistance().value();
    }

    if (oVolume->hasAttenuationColor()) {
        nlohmann::json jsonArray = nlohmann::json::array();
        vectorToDoubles(oVolume->getAttenuationColorOrDefault(), jsonArray);
        jsonVolume["attenuationColor"] = jsonArray;
    }
    if (oVolume->hasThicknessTextureInfo()) {
        convertTextureInfo(oVolume->getThicknessTextureInfo(), "thicknessTexture", jsonVolume);
    }

    _jsonMaterial["extensions"]["KHR_materials_volume"] = jsonVolume;
}

void MaterialsToJson::convertExtensionIridescence(const Material& _material, nlohmann::json& _jsonMaterial) const
{
    const std::optional<gltf::Iridescence>& oIridescence = _material.getIridescence();
    if (oIridescence.has_value() == false) {
        return;
    }

    nlohmann::json jsonIridescence = nlohmann::json::object();

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(oIridescence.value(), jsonIridescence);
    gltfObjectToJson.convertUnknownExtensions(oIridescence.value(), jsonIridescence);

    if (oIridescence->hasIridescenceFactor()) {
        jsonIridescence["iridescenceFactor"] = oIridescence->getIridescenceFactorOrDefault();
    }

    if (oIridescence->hasIridescenceIor()) {
        jsonIridescence["iridescenceIor"] = oIridescence->getIridescenceIorOrDefault();
    }

    if (oIridescence->hasIridescenceThicknessMinimum()) {
        jsonIridescence["iridescenceThicknessMinimum"] = oIridescence->getIridescenceThicknessMinimumOrDefault();
    }

    if (oIridescence->hasIridescenceThicknessMaximum()) {
        jsonIridescence["iridescenceThicknessMaximum"] = oIridescence->getIridescenceThicknessMaximumOrDefault();
    }

    if (oIridescence->hasIridescenceTextureInfo()) {
        convertTextureInfo(oIridescence->getIridescenceTextureInfo(), "iridescenceTexture", jsonIridescence);
    }

    if (oIridescence->hasIridescenceThicknessTextureInfo()) {
        convertTextureInfo(oIridescence->getIridescenceThicknessTextureInfo(), "iridescenceThicknessTexture", jsonIridescence);
    }

    _jsonMaterial["extensions"]["KHR_materials_iridescence"] = jsonIridescence;
}

void MaterialsToJson::convertExtensionAnisotropy(const Material& _material, nlohmann::json& _jsonMaterial) const
{
    const std::optional<gltf::Anisotropy>& oAnisotropy = _material.getAnisotropy();
    if (oAnisotropy.has_value() == false) {
        return;
    }

    nlohmann::json jsonAnisotropy = nlohmann::json::object();

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(oAnisotropy.value(), jsonAnisotropy);
    gltfObjectToJson.convertUnknownExtensions(oAnisotropy.value(), jsonAnisotropy);

    if (oAnisotropy->hasAnisotropyStrength()) {
        jsonAnisotropy["anisotropyStrength"] = oAnisotropy->getAnisotropyStrengthOrDefault();
    }
    if (oAnisotropy->hasAnisotropyRotation()) {
        jsonAnisotropy["anisotropyRotation"] = oAnisotropy->getAnisotropyRotationOrDefault();
    }
    if (oAnisotropy->hasAnisotropyTextureInfo()) {
        convertTextureInfo(oAnisotropy->getAnisotropyTextureInfo(), "anisotropyTexture", jsonAnisotropy);
    }

    _jsonMaterial["extensions"]["KHR_materials_anisotropy"] = jsonAnisotropy;
}
