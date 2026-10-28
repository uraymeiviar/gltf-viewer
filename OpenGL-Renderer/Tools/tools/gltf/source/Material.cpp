// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Material.h"
#include "core/Logger.h"
#include "core/String.h"
#include "gltf/Texture.h"
#include "gltf/XMP.h"

#include <algorithm>

using namespace ux3d;
using namespace ux3d::gltf;

Material::Material(uint32_t _id)
    : ChildOfRoot(_id)
{
}

Material::Material(uint32_t _id, const Material& _source)
    : ChildOfRoot(_id, _source)
    , m_alphaMode(_source.m_alphaMode)
    , m_alphaModeDither(_source.m_alphaModeDither)
    , m_alphaCutoff(_source.m_alphaCutoff)
    , m_doubleSided(_source.m_doubleSided)
    , m_emissiveFactor(_source.m_emissiveFactor)
    , m_normalTextureInfo(_source.m_normalTextureInfo)
    , m_occlusionTextureInfo(_source.m_occlusionTextureInfo)
    , m_emissiveTextureInfo(_source.m_emissiveTextureInfo)
    , m_pbrMetallicRoughness(_source.m_pbrMetallicRoughness)
    , m_pbrSpecularGlossiness(_source.m_pbrSpecularGlossiness)
    , m_clearcoat(_source.m_clearcoat)
    , m_sheen(_source.m_sheen)
    , m_specular(_source.m_specular)
    , m_ior(_source.m_ior)
    , m_unlit(_source.m_unlit)
    , m_transmission(_source.m_transmission)
    , m_emissiveStrength(_source.m_emissiveStrength)
    , m_volume(_source.m_volume)
    , m_iridescence(_source.m_iridescence)
    , m_anisotropy(_source.m_anisotropy)
    , m_xmpPacket(_source.m_xmpPacket)
{
}

GltfResult Material::createNormalTextureInfo()
{
    if (!m_normalTextureInfo) {
        m_normalTextureInfo.emplace();
        return GltfResult::Success;
    }
    return GltfError::ObjectAlreadyExists;
}

GltfResult Material::createOcclusionTextureInfo()
{
    if (!m_occlusionTextureInfo) {
        m_occlusionTextureInfo.emplace();
        return GltfResult::Success;
    }
    return GltfError::ObjectAlreadyExists;
}

GltfResult Material::createEmissiveTextureInfo()
{
    if (!m_emissiveTextureInfo) {
        m_emissiveTextureInfo.emplace();
        return GltfResult::Success;
    }
    return GltfError::ObjectAlreadyExists;
}

GltfResult Material::createPbrMetallicRoughness()
{
    if (m_pbrMetallicRoughness.has_value()) {
        return GltfError::ObjectAlreadyExists;
    }
    m_pbrMetallicRoughness.emplace();
    return GltfResult::Success;
}

void Material::destroyPbrMetallicRoughness()
{
    m_pbrMetallicRoughness->destroyBaseColorTextureInfo();
    m_pbrMetallicRoughness->destroyMetallicRoughnessTextureInfo();
    m_pbrMetallicRoughness.reset();
}

GltfResult Material::createPbrSpecularGlossiness()
{
    if (m_pbrSpecularGlossiness.has_value()) {
        return GltfError::ObjectAlreadyExists;
    }
    m_pbrSpecularGlossiness.emplace();
    resetPBRNextExtensions();
    return GltfResult::Success;
}

GltfResult Material::createClearcoat()
{
    if (m_clearcoat.has_value()) {
        return GltfError::ObjectAlreadyExists;
    }
    resetPBRNextExtensionExceptions();
    m_clearcoat.emplace();
    return GltfResult::Success;
}

GltfResult Material::createSpecular()
{
    if (m_specular.has_value()) {
        return GltfError::ObjectAlreadyExists;
    }
    resetPBRNextExtensionExceptions();
    m_specular.emplace();
    return GltfResult::Success;
}

GltfResult Material::createTransmission()
{
    if (m_transmission.has_value()) {
        return GltfError::ObjectAlreadyExists;
    }
    resetPBRNextExtensionExceptions();
    m_transmission.emplace();
    return GltfResult::Success;
}

GltfResult Material::createSheen()
{
    if (m_sheen.has_value()) {
        return GltfError::ObjectAlreadyExists;
    }
    resetPBRNextExtensionExceptions();
    m_sheen.emplace();
    return GltfResult::Success;
}

GltfResult Material::createIor()
{
    if (m_ior.has_value()) {
        return GltfError::ObjectAlreadyExists;
    }
    resetPBRNextExtensionExceptions();
    m_ior.emplace();
    return GltfResult::Success;
}

GltfResult Material::createUnlit()
{
    if (m_unlit.has_value()) {
        return GltfError::ObjectAlreadyExists;
    }
    resetPBRNextExtensions();
    m_unlit.emplace();
    return GltfResult::Success;
}

GltfResult Material::createVolume()
{
    if (m_volume.has_value()) {
        return GltfError::ObjectAlreadyExists;
    }
    resetPBRNextExtensionExceptions();
    m_volume.emplace();
    return GltfResult::Success;
}

GltfResult Material::createIridescence()
{
    if (m_iridescence.has_value()) {
        return GltfError::ObjectAlreadyExists;
    }
    resetPBRNextExtensionExceptions();
    m_iridescence.emplace();
    return GltfResult::Success;
}

GltfResult Material::createAnisotropy()
{
    if (m_anisotropy.has_value()) {
        return GltfError::ObjectAlreadyExists;
    }
    resetPBRNextExtensionExceptions();
    m_anisotropy.emplace();
    return GltfResult::Success;
}

GltfResult Material::createEmissiveStrength()
{
    if (m_emissiveStrength.has_value()) {
        return GltfError::ObjectAlreadyExists;
    }
    resetPBRNextExtensionExceptions();
    m_emissiveStrength.emplace();
    return GltfResult::Success;
}

GltfResult Material::setAlphaMode(AlphaMode _alphaMode)
{
    m_alphaMode.emplace(_alphaMode);
    m_alphaModeDither = false;
    return GltfResult::Success;
}

GltfResult Material::setAlphaCutoff(double _alphaCutoff)
{
    if (_alphaCutoff < 0) {
        return GltfError::InvalidInput;
    }
    m_alphaCutoff.emplace(_alphaCutoff);
    return GltfResult::Success;
}

GltfResult Material::setDoubleSided(bool _doubleSided)
{
    m_doubleSided.emplace(_doubleSided);
    return GltfResult::Success;
}

GltfResult Material::setEmissiveFactor(const math::dvec3& _emissiveFactor)
{
    // A2.2: clamp to the glTF core range instead of dropping the value. MSFS exporters write
    // emissiveFactor = colour * emissiveScale (e.g. [3000,3000,3000]); rejecting it lost the
    // emissive entirely. KHR_materials_emissive_strength keeps its own factor. The loader
    // logs the original value once per material (MaterialsFromJson); negatives clamp to 0.
    math::dvec3 unclamped;
    for (int i = 0; i < _emissiveFactor.length(); ++i) {
        unclamped[i] = std::max(_emissiveFactor[i], 0.0);
    }
    m_emissiveFactor.emplace(unclamped);
    return GltfResult::Success;
}

Extensions Material::getUsedExtensions() const
{
    Extensions usedExtensions;

    if (checkForPbrSpecularGlossiness()) {
        usedExtensions |= Extension::PbrSpecularGlossinessMaterial;
    }
    if (checkForUnlit()) {
        usedExtensions |= Extension::UnlitMaterial;
    }
    if (checkForClearcoat()) {
        usedExtensions |= Extension::Clearcoat;
    }
    if (checkForSpecular()) {
        usedExtensions |= Extension::Specular;
    }
    if (checkForTransmission()) {
        usedExtensions |= Extension::Transmission;
    }
    if (checkForTextureTransform()) {
        usedExtensions |= Extension::TextureTransform;
    }
    if (checkForSheen()) {
        usedExtensions |= Extension::Sheen;
    }
    if (checkForIor()) {
        usedExtensions |= Extension::Ior;
    }
    if (checkForVolume()) {
        usedExtensions |= Extension::Volume;
    }
    if (checkForIridescence()) {
        usedExtensions |= Extension::Iridescence;
    }
    if (checkForAnisotropy()) {
        usedExtensions |= Extension::Anisotropy;
    }
    if (m_emissiveStrength.has_value()) {
        usedExtensions |= Extension::EmissiveStrength;
    }

    return usedExtensions;
}

bool Material::isValid() const
{
    return true;
}

GltfResult Material::getInvalidProperties(core::Vector<core::string>&) const
{
    return GltfResult::Success;
}

bool Material::checkForTextureTransform() const
{
    if ((m_emissiveTextureInfo.has_value() && m_emissiveTextureInfo->getTextureTransform().has_value()) || (m_occlusionTextureInfo.has_value() && m_occlusionTextureInfo->getTextureTransform().has_value()) || (m_normalTextureInfo.has_value() && m_normalTextureInfo->getTextureTransform().has_value())) {
        return true;
    }

    if (m_pbrMetallicRoughness.has_value()) {
        if ((m_pbrMetallicRoughness->getMetallicRoughnessTextureInfo().has_value() && m_pbrMetallicRoughness->getMetallicRoughnessTextureInfo()->getTextureTransform().has_value()) || (m_pbrMetallicRoughness->getBaseColorTextureInfo().has_value() && m_pbrMetallicRoughness->getBaseColorTextureInfo()->getTextureTransform().has_value())) {
            return true;
        }
    }

    if (m_pbrSpecularGlossiness.has_value()) {
        if ((m_pbrSpecularGlossiness->getDiffuseTextureInfo().has_value() && m_pbrSpecularGlossiness->getDiffuseTextureInfo()->getTextureTransform().has_value()) || (m_pbrSpecularGlossiness->getSpecularGlossinessTextureInfo().has_value() && m_pbrSpecularGlossiness->getSpecularGlossinessTextureInfo()->getTextureTransform().has_value())) {
            return true;
        }
    }

    if (m_clearcoat.has_value()) {
        if ((m_clearcoat->getClearcoatTexture().has_value() && m_clearcoat->getClearcoatTexture()->getTextureTransform().has_value()) || (m_clearcoat->getClearcoatNormalTexture().has_value() && m_clearcoat->getClearcoatNormalTexture()->getTextureTransform().has_value()) || (m_clearcoat->getClearcoatRoughnessTexture().has_value() && m_clearcoat->getClearcoatRoughnessTexture()->getTextureTransform().has_value())) {
            return true;
        }
    }

    if (m_specular.has_value()) {
        if (m_specular->getSpecularTextureInfo().has_value() && m_specular->getSpecularTextureInfo()->getTextureTransform().has_value()) {
            return true;
        }
        if (m_specular->getSpecularColorTextureInfo().has_value() && m_specular->getSpecularColorTextureInfo()->getTextureTransform().has_value()) {
            return true;
        }
    }

    if (m_sheen.has_value()) {
        if ((m_sheen->getSheenColorTextureInfo().has_value() && m_sheen->getSheenColorTextureInfo()->getTextureTransform().has_value()) || (m_sheen->getSheenRoughnessTextureInfo().has_value() && m_sheen->getSheenRoughnessTextureInfo()->getTextureTransform().has_value())) {
            return true;
        }
    }

    if (m_transmission.has_value()) {
        if (m_transmission->hasTransmissionTextureInfo()) {
            if (m_transmission->getTransmissionTextureInfo()->getTextureTransform().has_value()) {
                return true;
            }
        }
    }

    if (m_volume.has_value()) {
        if (m_volume->getThicknessTextureInfo()) {
            if (m_volume->getThicknessTextureInfo()->getTextureTransform().has_value()) {
                return true;
            }
        }
    }
    if (m_iridescence.has_value()) {
        if (m_iridescence->getIridescenceTextureInfo()) {
            if (m_iridescence->getIridescenceTextureInfo()->getTextureTransform().has_value()) {
                return true;
            }
        }
        if (m_iridescence->getIridescenceThicknessTextureInfo()) {
            if (m_iridescence->getIridescenceThicknessTextureInfo()->getTextureTransform().has_value()) {
                return true;
            }
        }
    }
    if (m_anisotropy.has_value() && m_anisotropy->getAnisotropyTextureInfo()
        && m_anisotropy->getAnisotropyTextureInfo()->getTextureTransform().has_value()) {
        return true;
    }

    return false;
}

void Material::resetPBRNextExtensions()
{
    m_clearcoat.reset();
    m_specular.reset();
    m_sheen.reset();
    m_ior.reset();
    m_transmission.reset();
    m_volume.reset();
    m_emissiveStrength.reset();
    m_anisotropy.reset();
}

void Material::resetPBRNextExtensionExceptions()
{
    m_pbrSpecularGlossiness.reset();
    m_unlit.reset();
}

GltfResult Material::setXMPPacket(Packet* _xmpPacket)
{
    if (m_xmpPacket.has_value() == false) {
        m_xmpPacket.emplace(PacketContainer());
    }
    m_xmpPacket.value().m_packet = _xmpPacket;
    return GltfResult::Success;
}

GltfResult Material::unsetXMPPacket()
{
    m_xmpPacket.reset();
    return GltfResult::Success;
}
