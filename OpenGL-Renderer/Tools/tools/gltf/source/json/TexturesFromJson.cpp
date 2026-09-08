#include "gltf/json/TexturesFromJson.h"
#include "gltf/Gltf.h"
#include "gltf/json/GltfObjectFromJson.h"
#include "gltf/json/PointerLookup.h"

ux3d::gltf::ConversionResult ux3d::gltf::TexturesFromJson::convert(const nlohmann::json& _jsonTexture)
{
    Texture* pTexture = m_gltf.create<Texture>(getID(_jsonTexture));
    if (pTexture == nullptr) {
        return ConversionError::AllocationFailure;
    }

    if (!m_pointerLookup.add(pTexture)) {
        return ConversionError::AllocationFailure;
    }

    bool containsErrors = false;
    for (const auto& [key, value] : _jsonTexture.items()) {
        if (key == "name") {
            if (value.is_string()) {
                pTexture->setName(value.get<std::string>());
            } else {
                core::logWarning("texture.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "extensions") {
            if (value.is_object()) {
                for (const auto& [extensionName, extension] : value.items()) {
                    if (extensionName == "EXT_texture_astc_hdr") {
                        // handled in resolve
                    } else if (extensionName == "EXT_texture_bc6h") {
                        // handled in resolve
                    } else if (extensionName == "KHR_texture_basisu") {
                        // handled in resolve
                    } else if (extensionName == "EXT_texture_webp") {
                        // handled in resolve
                    }
                }
            }
        }
    }
    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonTexture, pTexture, "texture")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonTexture, pTexture);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ux3d::gltf::ConversionResult ux3d::gltf::TexturesFromJson::resolve(const nlohmann::json& _jsonTexture, Texture& _texture)
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonTexture.items()) {
        if (key == "sampler") {
            if (checkIndex(value, m_pointerLookup.Samplers.size(), "texture.sampler")) {
                Sampler* pSampler = m_pointerLookup.Samplers[value.get<uint32_t>()];
                _texture.setSampler(*pSampler);
            } else {
                containsErrors = true;
            }
        } else if (key == "source") {
            if (checkIndex(value, m_pointerLookup.Images.size(), "texture.source")) {
                Image* pSource = m_pointerLookup.Images[value.get<uint32_t>()];
                _texture.setSource(*pSource);
            } else {
                containsErrors = true;
            }
        } else if (key == "extensions") {
            if (value.is_object()) {
                for (const auto& [extensionName, extension] : value.items()) {
                    if (extension.is_object()) {
                        if (extensionName == "KHR_texture_basisu") {
                            _texture.createBasisU();
                            if (!GltfObjectFromJson::convertUnknownExtensions(extension, &_texture.getBasisU().value(), "texture.extensions.KHR_texture_basisu")) {
                                containsErrors = true;
                            }
                            GltfObjectFromJson::convertExtras(extension, &_texture.getBasisU().value());

                            for (const auto& [subKey, subValue] : extension.items()) {
                                if (subKey == "source") {
                                    if (checkIndex(subValue, m_pointerLookup.Images.size(), "texture.extensions.KHR_texture_basisu.source")) {
                                        Image* pBasisU = m_pointerLookup.Images[subValue];
                                        _texture.getBasisU()->setSource(pBasisU);
                                    } else {
                                        containsErrors = true;
                                    }
                                }
                            }
                        }
                        if (extensionName == "EXT_texture_webp") {
                            _texture.createWebP();
                            if (!GltfObjectFromJson::convertUnknownExtensions(extension, &_texture.getWebP().value(), "texture.extensions.EXT_texture_webp")) {
                                containsErrors = true;
                            }
                            GltfObjectFromJson::convertExtras(extension, &_texture.getWebP().value());

                            for (const auto& [subKey, subValue] : extension.items()) {
                                if (subKey == "source") {
                                    if (checkIndex(subValue, m_pointerLookup.Images.size(), "texture.extensions.EXT_texture_webp.source")) {
                                        Image* pWebP = m_pointerLookup.Images[subValue];
                                        _texture.getWebP()->setSource(pWebP);
                                    } else {
                                        containsErrors = true;
                                    }
                                }
                            }
                        }
                    } else {
                        core::logWarning("texture.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension.dump().c_str());
                        containsErrors = true;
                    }
                }
            } else {
                core::logWarning("texture.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}
