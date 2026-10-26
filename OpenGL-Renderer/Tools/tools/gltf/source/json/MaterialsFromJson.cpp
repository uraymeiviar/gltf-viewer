#include "gltf/json/MaterialsFromJson.h"
#include "gltf/Gltf.h"
#include "gltf/json/GltfObjectFromJson.h"
#include "gltf/json/ParsingUtils.h"
#include "gltf/json/PointerLookup.h"

using namespace ux3d;
using namespace ux3d::gltf;

ConversionResult MaterialsFromJson::convert(const nlohmann::json& _jsonMaterial)
{
    Material* pMaterial = m_gltf.create<Material>(getID(_jsonMaterial));
    if (pMaterial == nullptr) {
        return ConversionError::AllocationFailure;
    }

    if (!m_pointerLookup.add(pMaterial)) {
        return ConversionError::AllocationFailure;
    }
    std::string key = "extensions";
    auto extensions = _jsonMaterial.find(key);
    bool containsErrors = false;
    ExtensionCategories exclusiveExtension = ExtensionCategory::None;

    if (extensions != _jsonMaterial.end()) {
        if (extensions->is_object()) {
            std::string extensionName = "KHR_materials_unlit";
            auto extension = extensions->find(extensionName);
            if (extension != extensions->end()) {
                if (extension->is_object()) {
                    pMaterial->createUnlit();
                    exclusiveExtension |= ExtensionCategory::Unlit;
                    if (!GltfObjectFromJson::convertUnknownExtensions(*extension, &pMaterial->getUnlit().value(), "material.extensions.unlit")) {
                        containsErrors = true;
                    }
                    GltfObjectFromJson::convertExtras(*extension, &pMaterial->getUnlit().value());
                } else {
                    core::logWarning("material.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension->dump().c_str());
                    containsErrors = true;
                }
            }

            extensionName = "KHR_materials_clearcoat";
            extension = extensions->find(extensionName);
            if (extension != extensions->end()) {
                if (!exclusiveExtension.isSet(ExtensionCategory::Unlit)) {
                    if (extension->is_object()) {
                        pMaterial->createClearcoat();
                        exclusiveExtension |= ExtensionCategory::PBRNext;
                        if (!convertClearcoat(*extension, pMaterial->getClearcoat().value())) {
                            containsErrors = true;
                        }
                    } else {
                        core::logWarning("material.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension->dump().c_str());
                        containsErrors = true;
                    }
                } else {
                    // The conflicting extensions have to be put in manually
                    core::logWarning("Conflicting extensions in material: %s conflicts with Unlit and is discarded. Discarded Content: %s", extensionName.c_str(), extension->dump().c_str());
                }
            }

            extensionName = "KHR_materials_specular";
            extension = extensions->find(extensionName);
            if (extension != extensions->end()) {
                if (!exclusiveExtension.isSet(ExtensionCategory::Unlit)) {
                    if (extension->is_object()) {
                        pMaterial->createSpecular();
                        exclusiveExtension |= ExtensionCategory::PBRNext;
                        if (!convertSpecular(*extension, pMaterial->getSpecular().value())) {
                            containsErrors = true;
                        }
                    } else {
                        core::logWarning("material.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension->dump().c_str());
                        containsErrors = true;
                    }
                } else {
                    // The conflicting extensions have to be put in manually
                    core::logWarning("Conflicting extensions in material: %s conflicts with Unlit and is discarded. Discarded Content: %s", extensionName.c_str(), extension->dump().c_str());
                }
            }

            extensionName = "KHR_materials_sheen";
            extension = extensions->find(extensionName);
            if (extension != extensions->end()) {
                if (!exclusiveExtension.isSet(ExtensionCategory::Unlit)) {
                    if (extension->is_object()) {
                        pMaterial->createSheen();
                        exclusiveExtension |= ExtensionCategory::PBRNext;
                        if (!convertSheen(*extension, pMaterial->getSheen().value())) {
                            containsErrors = true;
                        }
                    } else {
                        core::logWarning("material.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension->dump().c_str());
                        containsErrors = true;
                    }
                } else {
                    // The conflicting extensions have to be put in manually
                    core::logWarning("Conflicting extensions in material: %s conflicts with Unlit and is discarded. Discarded Content: %s", extensionName.c_str(), extension->dump().c_str());
                }
            }

            extensionName = "KHR_materials_ior";
            extension = extensions->find(extensionName);
            if (extension != extensions->end()) {
                if (!exclusiveExtension.isSet(ExtensionCategory::Unlit)) {
                    if (extension->is_object()) {
                        pMaterial->createIor();
                        exclusiveExtension |= ExtensionCategory::PBRNext;
                        if (!convertIor(*extension, pMaterial->getIor().value())) {
                            containsErrors = true;
                        }
                    } else {
                        core::logWarning("material.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension->dump().c_str());
                        containsErrors = true;
                    }
                } else {
                    // The conflicting extensions have to be put in manually
                    core::logWarning("Conflicting extensions in material: %s conflicts with Unlit and is discarded. Discarded Content: %s", extensionName.c_str(), extension->dump().c_str());
                }
            }

            extensionName = "KHR_materials_emissive_strength";
            extension = extensions->find(extensionName);
            if (extension != extensions->end()) {
                if (!exclusiveExtension.isSet(ExtensionCategory::Unlit)) {
                    if (extension->is_object()) {
                        pMaterial->createEmissiveStrength();
                        exclusiveExtension |= ExtensionCategory::PBRNext;
                        if (!convertEmissiveStrength(*extension, pMaterial->getEmissiveStrength().value())) {
                            containsErrors = true;
                        }
                    } else {
                        core::logWarning("material.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension->dump().c_str());
                        containsErrors = true;
                    }
                } else {
                    // The conflicting extensions have to be put in manually
                    core::logWarning("Conflicting extensions in material: %s conflicts with Unlit and is discarded. Discarded Content: %s", extensionName.c_str(), extension->dump().c_str());
                }
            }

            extensionName = "KHR_materials_transmission";
            extension = extensions->find(extensionName);
            if (extension != extensions->end()) {
                if (!exclusiveExtension.isSet(ExtensionCategory::Unlit)) {
                    if (extension->is_object()) {
                        pMaterial->createTransmission();
                        exclusiveExtension |= ExtensionCategory::PBRNext;
                        if (!convertTransmission(*extension, pMaterial->getTransmission().value())) {
                            containsErrors = true;
                        }
                    } else {
                        core::logWarning("material.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension->dump().c_str());
                        containsErrors = true;
                    }
                } else {
                    // The conflicting extensions have to be put in manually
                    core::logWarning("Conflicting extensions in material: %s conflicts with Unlit and is discarded. Discarded Content: %s", extensionName.c_str(), extension->dump().c_str());
                }
            }

            extensionName = "KHR_materials_volume";
            extension = extensions->find(extensionName);
            if (extension != extensions->end()) {
                if (!exclusiveExtension.isSet(ExtensionCategory::Unlit)) {
                    if (extension->is_object()) {
                        pMaterial->createVolume();
                        exclusiveExtension |= ExtensionCategory::PBRNext;
                        if (!convertVolume(*extension, pMaterial->getVolume().value())) {
                            containsErrors = true;
                        }
                    } else {
                        core::logWarning("material.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension->dump().c_str());
                        containsErrors = true;
                    }
                } else {
                    // The conflicting extensions have to be put in manually
                    core::logWarning("Conflicting extensions in material: %s conflicts with Unlit and is discarded. Discarded Content: %s", extensionName.c_str(), extension->dump().c_str());
                }
            }

            extensionName = "KHR_materials_iridescence";
            extension = extensions->find(extensionName);
            if (extension != extensions->end()) {
                if (!exclusiveExtension.isSet(ExtensionCategory::Unlit) && !exclusiveExtension.isSet(ExtensionCategory::PBRSpecularGlossiness)) {
                    if (extension->is_object()) {
                        pMaterial->createIridescence();
                        exclusiveExtension |= ExtensionCategory::PBRNext;
                        if (!convertIridescence(*extension, pMaterial->getIridescence().value())) {
                            containsErrors = true;
                        }
                    } else {
                        core::logWarning("material.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension->dump().c_str());
                        containsErrors = true;
                    }
                } else {
                    // The conflicting extensions have to be put in manually
                    core::logWarning("Conflicting extensions in material: %s conflicts with Unlit and is discarded. Discarded Content: %s", extensionName.c_str(), extension->dump().c_str());
                }
            }

            extensionName = "KHR_materials_pbrSpecularGlossiness";
            extension = extensions->find(extensionName);
            if (extension != extensions->end()) {
                if (!exclusiveExtension.isSet(ExtensionCategory::PBRNext)) {
                    if (extension->is_object()) {
                        pMaterial->createPbrSpecularGlossiness();
                        exclusiveExtension |= ExtensionCategory::PBRSpecularGlossiness;
                        if (!convertSpecularGlossiness(*extension, pMaterial->getPbrSpecularGlossiness().value())) {
                            containsErrors = true;
                        }
                    } else {
                        core::logWarning("material.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension->dump().c_str());
                        containsErrors = true;
                    }
                } else {
                    // The conflicting extensions have to be put in manually
                    core::logWarning("Conflicting extensions in material: %s conflicts with a PBR Next extension and is discarded. Discarded Content: %s", extensionName.c_str(), extension->dump().c_str());
                }
            }
        }
    }

    for (const auto& [key, value] : _jsonMaterial.items()) {
        if (key == "name") {
            if (value.is_string()) {
                pMaterial->setName(value.get<std::string>());
            } else {
                core::logWarning("material.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "alphaCutoff") {
            if (value.is_number()) {
                pMaterial->setAlphaCutoff(value.get<double>());
            } else {
                core::logWarning("material.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "doubleSided") {
            if (value.is_boolean()) {
                pMaterial->setDoubleSided(value.get<bool>());
            } else {
                core::logWarning("material.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "emissiveFactor") {
            if (core::Succeeded(checkVector(value, 3, "material.emissiveFactor"))) {
                // A2.2: > 1 is written by MSFS exporters (colour * emissiveScale). The
                // material setter clamps to [0, 1]; keep the original value in one warning
                // per material. The validator still reports the out-of-range JSON value.
                const math::dvec3 emissiveFactor = doublesToVector<3>(value.get<std::vector<double>>());
                const bool outOfRange = emissiveFactor.x < 0.0 || emissiveFactor.x > 1.0
                    || emissiveFactor.y < 0.0 || emissiveFactor.y > 1.0
                    || emissiveFactor.z < 0.0 || emissiveFactor.z > 1.0;
                if (outOfRange) {
                    const std::string materialName = _jsonMaterial.contains("name")
                            && _jsonMaterial["name"].is_string()
                        ? _jsonMaterial["name"].get<std::string>()
                        : std::string();
                    core::logWarning("material '%s' emissiveFactor [%g, %g, %g] is outside [0, 1]; "
                                     "keeping authored value",
                                     materialName.c_str(), emissiveFactor.x, emissiveFactor.y,
                                     emissiveFactor.z);
                }
                pMaterial->setEmissiveFactor(emissiveFactor);
            } else {
                containsErrors = true;
            }
        } else if (key == "alphaMode") {
            if (value.is_string()) {
                convertAlphaMode(value, *pMaterial);
            } else {
                core::logWarning("material.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "normalTexture") {
            if (value.is_object()) {
                pMaterial->createNormalTextureInfo();
                if (!convertTextureInfo(value, pMaterial->getNormalTextureInfo().value())) {
                    containsErrors = true;
                }
            } else {
                core::logWarning("material.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "occlusionTexture") {
            if (value.is_object()) {
                pMaterial->createOcclusionTextureInfo();
                if (!convertTextureInfo(value, pMaterial->getOcclusionTextureInfo().value())) {
                    containsErrors = true;
                }
            } else {
                core::logWarning("material.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "emissiveTexture") {
            if (value.is_object()) {
                pMaterial->createEmissiveTextureInfo();
                if (!convertTextureInfo(value, pMaterial->getEmissiveTextureInfo().value())) {
                    containsErrors = true;
                }
            } else {
                core::logWarning("material.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "pbrMetallicRoughness") {
            if (value.is_object()) {
                pMaterial->createPbrMetallicRoughness();
                if (!convertMetallicRoughness(value, pMaterial->getPbrMetallicRoughness().value())) {
                    containsErrors = true;
                }
            } else {
                core::logWarning("material.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }
    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonMaterial, pMaterial, "material")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonMaterial, pMaterial);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult MaterialsFromJson::resolve(const nlohmann::json& _jsonMaterial, Material& _material)
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonMaterial.items()) {
        if (key == "normalTexture") {
            if (value.is_object()) {
                if (!resolveTextureInfo(value, _material.getNormalTextureInfo().value())) {
                    containsErrors = true;
                }
            }
        } else if (key == "occlusionTexture") {
            if (value.is_object()) {
                if (!resolveTextureInfo(value, _material.getOcclusionTextureInfo().value())) {
                    containsErrors = true;
                }
            }
        } else if (key == "emissiveTexture") {
            if (value.is_object()) {
                if (!resolveTextureInfo(value, _material.getEmissiveTextureInfo().value())) {
                    containsErrors = true;
                }
            }
        } else if (key == "pbrMetallicRoughness") {
            if (value.is_object()) {
                if (!resolveMetallicRoughness(value, _material.getPbrMetallicRoughness().value())) {
                    containsErrors = true;
                }
            }
        } else if (key == "extensions") {
            if (value.is_object()) {
                for (const auto& [extensionName, extension] : value.items()) {
                    if (extension.is_object()) {
                        if (extensionName == "KHR_materials_pbrSpecularGlossiness" && _material.getPbrSpecularGlossiness().has_value()) {
                            if (!resolveSpecularGlossiness(extension, _material.getPbrSpecularGlossiness().value())) {
                                containsErrors = true;
                            }
                        }
                        if (extensionName == "KHR_materials_clearcoat" && _material.getClearcoat().has_value()) {
                            if (!resolveClearcoat(extension, _material.getClearcoat().value())) {
                                containsErrors = true;
                            }
                        }
                        if (extensionName == "KHR_materials_specular" && _material.getSpecular().has_value()) {
                            if (!resolveSpecular(extension, _material.getSpecular().value())) {
                                containsErrors = true;
                            }
                        }
                        if (extensionName == "KHR_materials_sheen" && _material.getSheen().has_value()) {
                            if (!resolveSheen(extension, _material.getSheen().value())) {
                                containsErrors = true;
                            }
                        }
                        if (extensionName == "KHR_materials_transmission" && _material.getTransmission().has_value()) {
                            if (!resolveTransmission(extension, _material.getTransmission().value())) {
                                containsErrors = true;
                            }
                        }
                        if (extensionName == "KHR_materials_volume" && _material.getVolume().has_value()) {
                            if (!resolveVolume(extension, _material.getVolume().value())) {
                                containsErrors = true;
                            }
                        }
                        if (extensionName == "KHR_materials_iridescence" && _material.getIridescence().has_value()) {
                            if (!resolveIridescence(extension, _material.getIridescence().value())) {
                                containsErrors = true;
                            }
                        }
                        if (extensionName == "KHR_xmp_json_ld") {
                            if (extension.is_object()) {
                                resolveXMP(extension, _material);
                            } else {
                                core::logWarning("material.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension.dump().c_str());
                                containsErrors = true;
                            }
                        }
                    }
                }
            }
        }
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

void MaterialsFromJson::convertAlphaMode(const core::string& _jsonAlphaMode, Material& _material) const
{
    if (_jsonAlphaMode == "OPAQUE") {
        _material.setAlphaMode(AlphaMode::Opaque);
    } else if (_jsonAlphaMode == "MASK") {
        _material.setAlphaMode(AlphaMode::Mask);
    } else if (_jsonAlphaMode == "BLEND") {
        _material.setAlphaMode(AlphaMode::Blend);
    } else {
        core::logWarning("material.alphaMode has unknown value '%s'", _jsonAlphaMode.c_str());
    }
}

ConversionResult MaterialsFromJson::convertTextureInfo(const nlohmann::json& _jsonTextureInfo, TextureInfo& _textureInfo) const
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonTextureInfo.items()) {
        if (key == "texCoord") {
            if (value.is_number_unsigned()) {
                _textureInfo.setTexCoord(value);
            } else {
                core::logWarning("textureInfo.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "scale" || key == "strength") {
            if (value.is_number()) {
                _textureInfo.setScale(value.get<double>());
            } else {
                core::logWarning("textureInfo.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "extensions") {
            if (value.is_object()) {
                for (const auto& [extensionName, extension] : value.items()) {
                    if (extensionName == "KHR_texture_transform") {
                        if (extension.is_object()) {
                            if (!convertTextureTransform(extension, _textureInfo)) {
                                containsErrors = true;
                            }
                        } else {
                            core::logWarning("textureInfo.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension.dump().c_str());
                            containsErrors = true;
                        }
                    }
                }
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonTextureInfo, &_textureInfo, "textureInfo")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonTextureInfo, &_textureInfo);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult MaterialsFromJson::convertTextureTransform(const nlohmann::json& _jsonTextureTransform, TextureInfo& _textureInfo) const
{
    _textureInfo.createTextureTransform();

    auto& textureTransform = _textureInfo.getTextureTransform();
    if (textureTransform.has_value() == false) {
        return ConversionError::AllocationFailure;
    }

    bool containsErrors = false;
    for (const auto& [key, value] : _jsonTextureTransform.items()) {
        if (key == "offset") {
            if (core::Succeeded(checkVector(value, 2, "KHR_texture_transform.offset"))) {
                textureTransform->setOffset(doublesToVector<2>(value.get<std::vector<double>>()));
            } else {
                containsErrors = true;
            }
        } else if (key == "rotation") {
            if (value.is_number()) {
                textureTransform->setRotation(value.get<double>());
            } else {
                core::logWarning("KHR_texture_transform.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "scale") {
            if (core::Succeeded(checkVector(value, 2, "KHR_texture_transform.scale"))) {
                textureTransform->setScale(doublesToVector<2>(value.get<std::vector<double>>()));
            } else {
                containsErrors = true;
            }
        } else if (key == "texCoord") {
            if (value.is_number_unsigned()) {
                textureTransform->setTexCoord(value.get<uint32_t>());
            } else {
                core::logWarning("KHR_texture_transform.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonTextureTransform, &textureTransform.value(), "KHR_texture_transform")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonTextureTransform, &textureTransform.value());

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult MaterialsFromJson::convertMetallicRoughness(const nlohmann::json& _jsonMetallicRoughness, PbrMetallicRoughness& _metallicRoughness) const
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonMetallicRoughness.items()) {
        if (key == "baseColorFactor") {
            if (core::Succeeded(checkVector(value, 4, "pbrMetallicRoughness.baseColorFactor"))) {
                _metallicRoughness.setBaseColorFactor(doublesToVector<4>(value.get<std::vector<double>>()));
            } else {
                containsErrors = true;
            }
        } else if (key == "metallicFactor") {
            if (value.is_number()) {
                _metallicRoughness.setMetallicFactor(value.get<double>());
            } else {
                core::logWarning("pbrMetallicRoughness.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "roughnessFactor") {
            if (value.is_number()) {
                _metallicRoughness.setRoughnessFactor(value.get<double>());
            } else {
                core::logWarning("pbrMetallicRoughness.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "baseColorTexture") {
            if (value.is_object()) {
                _metallicRoughness.createBaseColorTextureInfo();
                if (!convertTextureInfo(value, _metallicRoughness.getBaseColorTextureInfo().value())) {
                    containsErrors = true;
                }
            } else {
                core::logWarning("pbrMetallicRoughness.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "metallicRoughnessTexture") {
            if (value.is_object()) {
                _metallicRoughness.createMetallicRoughnessTextureInfo();
                if (!convertTextureInfo(value, _metallicRoughness.getMetallicRoughnessTextureInfo().value())) {
                    containsErrors = true;
                }
            } else {
                core::logWarning("pbrMetallicRoughness.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonMetallicRoughness, &_metallicRoughness, "pbrMetallicRoughness")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonMetallicRoughness, &_metallicRoughness);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult MaterialsFromJson::convertSpecularGlossiness(const nlohmann::json& _jsonSpecularGlossiness, PbrSpecularGlossiness& _specularGlossiness) const
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonSpecularGlossiness.items()) {
        if (key == "diffuseFactor") {
            if (core::Succeeded(checkVector(value, 4, "KHR_materials_pbrSpecularGlossiness.diffuseFactor"))) {
                _specularGlossiness.setDiffuseFactor(doublesToVector<4>(value.get<std::vector<double>>()));
            } else {
                containsErrors = true;
            }
        } else if (key == "specularFactor") {
            if (core::Succeeded(checkVector(value, 3, "KHR_materials_pbrSpecularGlossiness.specularFactor"))) {
                _specularGlossiness.setSpecularFactor(doublesToVector<3>(value.get<std::vector<double>>()));
            } else {
                containsErrors = true;
            }
        } else if (key == "glossinessFactor") {
            if (value.is_number()) {
                _specularGlossiness.setGlossinessFactor(value.get<double>());
            } else {
                core::logWarning("KHR_materials_pbrSpecularGlossiness.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "diffuseTexture") {
            if (value.is_object()) {
                _specularGlossiness.createDiffuseTextureInfo();
                if (!convertTextureInfo(value, _specularGlossiness.getDiffuseTextureInfo().value())) {
                    containsErrors = true;
                }
            } else {
                core::logWarning("KHR_materials_pbrSpecularGlossiness.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "specularGlossinessTexture") {
            if (value.is_object()) {
                _specularGlossiness.createSpecularGlossinessTextureInfo();
                if (!convertTextureInfo(value, _specularGlossiness.getSpecularGlossinessTextureInfo().value())) {
                    containsErrors = true;
                }
            } else {
                core::logWarning("KHR_materials_pbrSpecularGlossiness.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonSpecularGlossiness, &_specularGlossiness, "KHR_materials_pbrSpecularGlossiness")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonSpecularGlossiness, &_specularGlossiness);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }
    return ConversionResult::Success;
}

ConversionResult MaterialsFromJson::convertClearcoat(const nlohmann::json& _jsonClearcoat, Clearcoat& _clearcoat) const
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonClearcoat.items()) {
        if (key == "clearcoatFactor") {
            if (value.is_number()) {
                _clearcoat.setClearcoatFactor(value.get<double>());
            } else {
                core::logWarning("KHR_materials_clearcoat.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "clearcoatRoughnessFactor") {
            if (value.is_number()) {
                _clearcoat.setClearcoatRoughnessFactor(value.get<double>());
            } else {
                core::logWarning("KHR_materials_clearcoat.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "clearcoatTexture") {
            if (value.is_object()) {
                _clearcoat.createClearcoatTexture();
                if (!convertTextureInfo(value, _clearcoat.getClearcoatTexture().value())) {
                    containsErrors = true;
                }
            } else {
                core::logWarning("KHR_materials_clearcoat.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "clearcoatRoughnessTexture") {
            if (value.is_object()) {
                _clearcoat.createClearcoatRoughnessTexture();
                if (!convertTextureInfo(value, _clearcoat.getClearcoatRoughnessTexture().value())) {
                    containsErrors = true;
                }
            } else {
                core::logWarning("KHR_materials_clearcoat.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "clearcoatNormalTexture") {
            if (value.is_object()) {
                _clearcoat.createClearcoatNormalTexture();
                if (!convertTextureInfo(value, _clearcoat.getClearcoatNormalTexture().value())) {
                    containsErrors = true;
                }
            } else {
                core::logWarning("KHR_materials_clearcoat.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonClearcoat, &_clearcoat, "KHR_materials_clearcoat")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonClearcoat, &_clearcoat);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult MaterialsFromJson::convertSpecular(const nlohmann::json& _jsonSpecular, Specular& _specular) const
{
    bool containsErrors = false;

    for (const auto& [key, value] : _jsonSpecular.items()) {
        if (key == "specularFactor") {
            if (value.is_number()) {
                _specular.setSpecularFactor(value.get<double>());
            } else {
                containsErrors = true;
                core::logWarning("KHR_materials_specular.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
            }
        }
        if (key == "specularColorFactor") {
            if (core::Succeeded(checkVector(value, 3, "KHR_materials_specular.specularColorFactor"))) {
                _specular.setSpecularColorFactor(doublesToVector<3>(value.get<std::vector<double>>()));
            } else {
                containsErrors = true;
                core::logWarning("KHR_materials_specular.%s with value '%s' has invalid type or length", key.c_str(), value.dump().c_str());
            }
        }
        if (key == "specularTexture") {
            if (value.is_object()) {
                _specular.createSpecularTextureInfo();
                if (!convertTextureInfo(value, _specular.getSpecularTextureInfo().value())) {
                    containsErrors = true;
                }
            } else {
                core::logWarning("KHR_materials_specular.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
        if (key == "specularColorTexture") {
            if (value.is_object()) {
                _specular.createSpecularColorTextureInfo();
                if (!convertTextureInfo(value, _specular.getSpecularColorTextureInfo().value())) {
                    containsErrors = true;
                }
            } else {
                core::logWarning("KHR_materials_specular.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonSpecular, &_specular, "KHR_materials_specular")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonSpecular, &_specular);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult MaterialsFromJson::convertSheen(const nlohmann::json& _jsonSheen, Sheen& _sheen) const
{
    bool containsErrors = false;

    for (const auto& [key, value] : _jsonSheen.items()) {
        if (key == "sheenRoughnessFactor") {
            if (value.is_number()) {
                _sheen.setSheenRoughnessFactor(value.get<double>());
            } else {
                containsErrors = true;
                core::logWarning("KHR_materials_sheen.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
            }
        }
        if (key == "sheenColorFactor") {
            if (core::Succeeded(checkVector(value, 3, "KHR_materials_sheen.specularColorFactor"))) {
                _sheen.setSheenColorFactor(doublesToVector<3>(value.get<std::vector<double>>()));
            } else {
                containsErrors = true;
                core::logWarning("KHR_materials_sheen.%s with value '%s' has invalid type or length", key.c_str(), value.dump().c_str());
            }
        }
        if (key == "sheenColorTexture") {
            if (value.is_object()) {
                _sheen.createSheenColorTextureInfo();
                if (!convertTextureInfo(value, _sheen.getSheenColorTextureInfo().value())) {
                    containsErrors = true;
                }
            } else {
                core::logWarning("KHR_materials_sheen.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
        if (key == "sheenRoughnessTexture") {
            if (value.is_object()) {
                _sheen.createSheenRoughnessTextureInfo();
                if (!convertTextureInfo(value, _sheen.getSheenRoughnessTextureInfo().value())) {
                    containsErrors = true;
                }
            } else {
                core::logWarning("KHR_materials_sheen.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonSheen, &_sheen, "KHR_materials_sheen")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonSheen, &_sheen);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult MaterialsFromJson::convertIor(const nlohmann::json& _jsonIor, Ior& _ior) const
{
    bool containsErrors = false;

    for (const auto& [key, value] : _jsonIor.items()) {
        if (key == "ior") {
            if (value.is_number()) {
                if (_ior.setIor(value.get<double>()) != GltfResult::Success) {
                    core::logWarning("MaterialsFromJson: not able to set ior value");
                    containsErrors = true;
                }
            } else {
                containsErrors = true;
                core::logWarning("KHR_materials_ior.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonIor, &_ior, "KHR_materials_ior")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonIor, &_ior);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult MaterialsFromJson::convertEmissiveStrength(const nlohmann::json& json, EmissiveStrength& emissiveStrength) const
{
    bool containsErrors = false;

    for (const auto& [key, value] : json.items()) {
        if (key == "emissiveStrength") {
            if (value.is_number()) {
                if (!emissiveStrength.setEmissiveStrength(value.get<double>())) {
                    core::logWarning("MaterialsFromJson: not able to set emissive strength value");
                    containsErrors = true;
                }
            } else {
                containsErrors = true;
                core::logWarning("KHR_materials_emissive_strength.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(json, &emissiveStrength, "KHR_materials_emissive_strength")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(json, &emissiveStrength);

    return containsErrors ? ConversionError::SuccessWithErrors : ConversionResult::Success;
}

ConversionResult MaterialsFromJson::convertTransmission(const nlohmann::json& _jsonTransmission, Transmission& _transmission) const
{
    bool containsErrors = false;

    for (const auto& [key, value] : _jsonTransmission.items()) {
        if (key == "transmissionFactor") {
            if (value.is_number()) {
                _transmission.setTransmissionFactor(value.get<double>());
            } else {
                containsErrors = true;
                core::logWarning("KHR_materials_transmission.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
            }
        }
        if (key == "transmissionTexture") {
            if (value.is_object()) {
                _transmission.createTransmissionTextureInfo();
                if (!convertTextureInfo(value, _transmission.getTransmissionTextureInfo().value())) {
                    containsErrors = true;
                }
            } else {
                containsErrors = true;
                core::logWarning("KHR_materials_transmission.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonTransmission, &_transmission, "KHR_materials_transmission")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonTransmission, &_transmission);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult MaterialsFromJson::convertVolume(const nlohmann::json& _jsonvolume, Volume& _volume) const
{
    bool containsErrors = false;

    for (const auto& [key, value] : _jsonvolume.items()) {
        if (key == "thicknessFactor") {
            if (value.is_number()) {
                _volume.setThicknessFactor(value.get<double>());
            } else {
                containsErrors = true;
                core::logWarning("KHR_materials_volume.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
            }
        }
        if (key == "attenuationDistance") {
            if (value.is_number()) {
                _volume.setAttenuationDistance(value.get<double>());
            } else {
                containsErrors = true;
                core::logWarning("KHR_materials_volume.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
            }
        }
        if (key == "attenuationColor") {
            if (core::Succeeded(checkVector(value, 3, "KHR_materials_volume.attenuationColor"))) {
                _volume.setAttenuationColor(doublesToVector<3>(value.get<std::vector<double>>()));
            } else {
                containsErrors = true;
                core::logWarning("KHR_materials_volume.%s with value '%s' has invalid type or length", key.c_str(), value.dump().c_str());
            }
        }
        if (key == "thicknessTexture") {
            if (value.is_object()) {
                _volume.createThicknessTextureInfo();
                if (!convertTextureInfo(value, _volume.getThicknessTextureInfo().value())) {
                    containsErrors = true;
                }
            } else {
                core::logWarning("KHR_materials_volume.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonvolume, &_volume, "KHR_materials_volume")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonvolume, &_volume);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult MaterialsFromJson::convertIridescence(const nlohmann::json& _json, Iridescence& _iridescence) const
{
    bool containsErrors = false;

    for (const auto& [key, value] : _json.items()) {
        if (key == "iridescenceFactor") {
            if (value.is_number()) {
                _iridescence.setIridescenceFactor(value.get<double>());
            } else {
                containsErrors = true;
                core::logWarning("KHR_materials_iridescence.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
            }
        }
        if (key == "iridescenceIor") {
            if (value.is_number()) {
                _iridescence.setIridescenceIor(value.get<double>());
            } else {
                containsErrors = true;
                core::logWarning("KHR_materials_iridescence.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
            }
        }
        if (key == "iridescenceThicknessMinimum") {
            if (value.is_number()) {
                _iridescence.setIridescenceThicknessMinimum(value.get<double>(), true);
            } else {
                containsErrors = true;
                core::logWarning("KHR_materials_iridescence.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
            }
        }
        if (key == "iridescenceThicknessMaximum") {
            if (value.is_number()) {
                _iridescence.setIridescenceThicknessMaximum(value.get<double>(), true);
            } else {
                containsErrors = true;
                core::logWarning("KHR_materials_iridescence.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
            }
        }

        if (key == "iridescenceTexture") {
            if (value.is_object()) {
                _iridescence.createIridescenceTextureInfo();
                if (!convertTextureInfo(value, _iridescence.getIridescenceTextureInfo().value())) {
                    containsErrors = true;
                }
            } else {
                core::logWarning("KHR_materials_iridescence.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
        if (key == "iridescenceThicknessTexture") {
            if (value.is_object()) {
                _iridescence.createIridescenceThicknessTextureInfo();
                if (!convertTextureInfo(value, _iridescence.getIridescenceThicknessTextureInfo().value())) {
                    containsErrors = true;
                }
            } else {
                core::logWarning("KHR_materials_iridescence.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(_json, &_iridescence, "KHR_materials_iridescence")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_json, &_iridescence);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult MaterialsFromJson::resolveMetallicRoughness(const nlohmann::json& _jsonMetallicRoughness, PbrMetallicRoughness& _metallicRoughness)
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonMetallicRoughness.items()) {
        if (key == "baseColorTexture") {
            if (value.is_object()) {
                if (core::Failed(resolveTextureInfo(value, _metallicRoughness.getBaseColorTextureInfo().value()))) {
                    containsErrors = true;
                }
            }
        } else if (key == "metallicRoughnessTexture") {
            if (value.is_object()) {
                if (core::Failed(resolveTextureInfo(value, _metallicRoughness.getMetallicRoughnessTextureInfo().value()))) {
                    containsErrors = true;
                }
            }
        }
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult MaterialsFromJson::resolveSpecularGlossiness(const nlohmann::json& _jsonSpecularGlossiness, PbrSpecularGlossiness& _specularGlossiness)
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonSpecularGlossiness.items()) {
        if (key == "diffuseTexture") {
            if (value.is_object()) {
                if (core::Failed(resolveTextureInfo(value, _specularGlossiness.getDiffuseTextureInfo().value()))) {
                    containsErrors = true;
                }
            }
        } else if (key == "specularGlossinessTexture") {
            if (value.is_object()) {
                if (core::Failed(resolveTextureInfo(value, _specularGlossiness.getSpecularGlossinessTextureInfo().value()))) {
                    containsErrors = true;
                }
            }
        }
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult MaterialsFromJson::resolveClearcoat(const nlohmann::json& _jsonClearcoat, Clearcoat& _clearcoat)
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonClearcoat.items()) {
        if (key == "clearcoatTexture") {
            if (value.is_object()) {
                if (core::Failed(resolveTextureInfo(value, _clearcoat.getClearcoatTexture().value()))) {
                    containsErrors = true;
                }
            }
        } else if (key == "clearcoatRoughnessTexture") {
            if (value.is_object()) {
                if (core::Failed(resolveTextureInfo(value, _clearcoat.getClearcoatRoughnessTexture().value()))) {
                    containsErrors = true;
                }
            }
        } else if (key == "clearcoatNormalTexture") {
            if (value.is_object()) {
                if (core::Failed(resolveTextureInfo(value, _clearcoat.getClearcoatNormalTexture().value()))) {
                    containsErrors = true;
                }
            }
        }
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult MaterialsFromJson::resolveTextureInfo(const nlohmann::json& _jsonTextureInfo, TextureInfo& _textureInfo)
{
    if (_jsonTextureInfo.contains("index") == false) {
        return ConversionError::MissingProperty;
    }

    if (ConversionResult result = checkIndex(_jsonTextureInfo["index"], m_pointerLookup.Textures.size(), "textureInfo.index");
        core::Failed(result)) {
        return result;
    }

    Texture* pTexture = m_pointerLookup.Textures[_jsonTextureInfo["index"].get<uint32_t>()];
    _textureInfo.setTexture(*pTexture);
    return ConversionResult::Success;
}

ConversionResult MaterialsFromJson::resolveSpecular(const nlohmann::json& _jsonSpecular, Specular& _specular)
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonSpecular.items()) {
        if (key == "specularTexture") {
            if (value.is_object()) {
                if (core::Failed(resolveTextureInfo(value, _specular.getSpecularTextureInfo().value()))) {
                    containsErrors = true;
                }
            }
        } else if (key == "specularColorTexture") {
            if (value.is_object()) {
                if (core::Failed(resolveTextureInfo(value, _specular.getSpecularColorTextureInfo().value()))) {
                    containsErrors = true;
                }
            }
        }
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult MaterialsFromJson::resolveSheen(const nlohmann::json& _jsonSheen, Sheen& _sheen)
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonSheen.items()) {
        if (key == "sheenColorTexture") {
            if (value.is_object()) {
                if (core::Failed(resolveTextureInfo(value, _sheen.getSheenColorTextureInfo().value()))) {
                    containsErrors = true;
                }
            }
        }
        if (key == "sheenRoughnessTexture") {
            if (value.is_object()) {
                if (core::Failed(resolveTextureInfo(value, _sheen.getSheenRoughnessTextureInfo().value()))) {
                    containsErrors = true;
                }
            }
        }
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult MaterialsFromJson::resolveTransmission(const nlohmann::json& _jsonTransmission, Transmission& _transmission)
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonTransmission.items()) {
        if (key == "transmissionTexture") {
            if (value.is_object()) {
                if (core::Failed(resolveTextureInfo(value, _transmission.getTransmissionTextureInfo().value()))) {
                    containsErrors = true;
                }
            }
        }
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult MaterialsFromJson::resolveXMP(const nlohmann::json& _jsonXMP, Material& _material)
{
    bool containsErrors = false;

    for (const auto& [xmpKey, xmpValue] : _jsonXMP.items()) {
        if (xmpKey == "packet") {
            if (m_gltf.getXMP().has_value() == false) {
                core::logWarning("MaterialsFromJson: Material.XMP is defined, but the root extension KHR_xmp_json_ld is missing");
                containsErrors = true;
                continue;
            }

            XMP& xmp = m_gltf.getXMP().value();
            core::LinkedList<Packet>& packets = xmp.getPackets();
            const size_t index = xmpValue.get<size_t>();

            Packet* pPacket = &packets.at(index);
            _material.setXMPPacket(pPacket);
        }
    }
    if (_material.hasXMPPacket()) {
        if (!GltfObjectFromJson::convertUnknownExtensions(_jsonXMP, &_material.getXMPPacket().value(), "material.extensions.KHR_xmp_json_ld")) {
            containsErrors = true;
        }
        GltfObjectFromJson::convertExtras(_jsonXMP, &_material.getXMPPacket().value());
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult MaterialsFromJson::resolveVolume(const nlohmann::json& _jsonVolume, Volume& _volume)
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonVolume.items()) {
        if (key == "thicknessTexture") {
            if (value.is_object()) {
                if (core::Failed(resolveTextureInfo(value, _volume.getThicknessTextureInfo().value()))) {
                    containsErrors = true;
                }
            }
        }
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult MaterialsFromJson::resolveIridescence(const nlohmann::json& _json, Iridescence& _iridescence)
{
    bool containsErrors = false;
    for (const auto& [key, value] : _json.items()) {
        if (key == "iridescenceTexture") {
            if (value.is_object()) {
                if (core::Failed(resolveTextureInfo(value, _iridescence.getIridescenceTextureInfo().value()))) {
                    containsErrors = true;
                }
            }
        }
        if (key == "iridescenceThicknessTexture") {
            if (value.is_object()) {
                if (core::Failed(resolveTextureInfo(value, _iridescence.getIridescenceThicknessTextureInfo().value()))) {
                    containsErrors = true;
                }
            }
        }
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}
