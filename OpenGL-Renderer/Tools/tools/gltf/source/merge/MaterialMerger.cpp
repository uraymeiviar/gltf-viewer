// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/merge/MaterialMerger.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"

using namespace ux3d;
using namespace ux3d::gltf;

bool MaterialMerger::resolveDependencies(const Material& _SourceMaterial, Material& _Material)
{
    resolveBaseTextures(_SourceMaterial, _Material);
    resolveMetallicRoughnessTextures(_SourceMaterial, _Material);
    resolveSpecularGlossinessTextures(_SourceMaterial, _Material);
    resolveClearcoatTextures(_SourceMaterial, _Material);
    resolveSpecularTextures(_SourceMaterial, _Material);
    resolveSheenTextures(_SourceMaterial, _Material);
    resolveTransmissionTextures(_SourceMaterial, _Material);
    resolveVolumeTextures(_SourceMaterial, _Material);
    resolveIridescenceTextures(_SourceMaterial, _Material);
    resolveAnisotropyTextures(_SourceMaterial, _Material);
    resolveXMP(_SourceMaterial, _Material);

    return true;
}

void MaterialMerger::resolveBaseTextures(const Material& _SourceMaterial, Material& _Material)
{
    resolveTextureInfo(_SourceMaterial.getEmissiveTextureInfo(), _Material.getEmissiveTextureInfo());
    resolveTextureInfo(_SourceMaterial.getOcclusionTextureInfo(), _Material.getOcclusionTextureInfo());
    resolveTextureInfo(_SourceMaterial.getNormalTextureInfo(), _Material.getNormalTextureInfo());
}

void MaterialMerger::resolveMetallicRoughnessTextures(const Material& _SourceMaterial, Material& _Material)
{
    const auto& sourceMetallicRoughness = _SourceMaterial.getPbrMetallicRoughness();
    if (sourceMetallicRoughness.has_value()) {
        auto& metallicRoughness = _Material.getPbrMetallicRoughness();
        resolveTextureInfo(sourceMetallicRoughness->getBaseColorTextureInfo(), metallicRoughness->getBaseColorTextureInfo());
        resolveTextureInfo(sourceMetallicRoughness->getMetallicRoughnessTextureInfo(), metallicRoughness->getMetallicRoughnessTextureInfo());
    }
}

void MaterialMerger::resolveSpecularGlossinessTextures(const Material& _SourceMaterial, Material& _Material)
{
    const auto& sourceSpecularGlossiness = _SourceMaterial.getPbrSpecularGlossiness();
    if (sourceSpecularGlossiness.has_value()) {
        auto& specularGlossiness = _Material.getPbrSpecularGlossiness();
        resolveTextureInfo(sourceSpecularGlossiness->getDiffuseTextureInfo(), specularGlossiness->getDiffuseTextureInfo());
        resolveTextureInfo(sourceSpecularGlossiness->getSpecularGlossinessTextureInfo(), specularGlossiness->getSpecularGlossinessTextureInfo());
    }
}

void MaterialMerger::resolveClearcoatTextures(const Material& _SourceMaterial, Material& _Material)
{
    const auto& sourceClearcoat = _SourceMaterial.getClearcoat();
    if (sourceClearcoat.has_value()) {
        auto& clearcoat = _Material.getClearcoat();
        resolveTextureInfo(sourceClearcoat->getClearcoatNormalTexture(), clearcoat->getClearcoatNormalTexture());
        resolveTextureInfo(sourceClearcoat->getClearcoatRoughnessTexture(), clearcoat->getClearcoatRoughnessTexture());
        resolveTextureInfo(sourceClearcoat->getClearcoatTexture(), clearcoat->getClearcoatTexture());
    }
}

void MaterialMerger::resolveSpecularTextures(const Material& _SourceMaterial, Material& _Material)
{
    const auto& sourceSpecular = _SourceMaterial.getSpecular();
    if (sourceSpecular.has_value()) {
        auto& specular = _Material.getSpecular();
        resolveTextureInfo(sourceSpecular->getSpecularTextureInfo(), specular->getSpecularTextureInfo());
        resolveTextureInfo(sourceSpecular->getSpecularColorTextureInfo(), specular->getSpecularColorTextureInfo());
    }
}

void MaterialMerger::resolveSheenTextures(const Material& _SourceMaterial, Material& _Material)
{
    const auto& sourceSheen = _SourceMaterial.getSheen();
    if (sourceSheen.has_value()) {
        auto& sheen = _Material.getSheen();
        resolveTextureInfo(sourceSheen->getSheenColorTextureInfo(), sheen->getSheenColorTextureInfo());
        resolveTextureInfo(sourceSheen->getSheenRoughnessTextureInfo(), sheen->getSheenRoughnessTextureInfo());
    }
}

void MaterialMerger::resolveTransmissionTextures(const Material& _SourceMaterial, Material& _Material)
{
    const auto& sourceTransmission = _SourceMaterial.getTransmission();
    if (sourceTransmission.has_value()) {
        auto& transmission = _Material.getTransmission();
        resolveTextureInfo(sourceTransmission->getTransmissionTextureInfo(), transmission->getTransmissionTextureInfo());
    }
}

void MaterialMerger::resolveVolumeTextures(const Material& _SourceMaterial, Material& _Material)
{
    const auto& sourceVolume = _SourceMaterial.getVolume();
    if (sourceVolume.has_value()) {
        auto& volume = _Material.getVolume();
        resolveTextureInfo(sourceVolume->getThicknessTextureInfo(), volume->getThicknessTextureInfo());
    }
}

void MaterialMerger::resolveIridescenceTextures(const Material& _SourceMaterial, Material& _Material)
{
    const auto& src = _SourceMaterial.getIridescence();
    if (src.has_value()) {
        auto& iridescence = _Material.getIridescence();
        resolveTextureInfo(src->getIridescenceTextureInfo(), iridescence->getIridescenceTextureInfo());
        resolveTextureInfo(src->getIridescenceThicknessTextureInfo(), iridescence->getIridescenceThicknessTextureInfo());
    }
}

void MaterialMerger::resolveAnisotropyTextures(const Material& _SourceMaterial, Material& _Material)
{
    const auto& src = _SourceMaterial.getAnisotropy();
    if (src.has_value()) {
        resolveTextureInfo(src->getAnisotropyTextureInfo(), _Material.getAnisotropy()->getAnisotropyTextureInfo());
    }
}

void MaterialMerger::resolveXMP(const Material& source, Material& dest)
{
    if (source.hasXMPPacket()) {
        const Packet* pSourcePacket = source.getXMPPacket().value().m_packet;
        const size_t packetIndex = m_Indices[pSourcePacket];

        if (m_Destination.hasXMP() == false) {
            m_Destination.createXMP();
        }

        dest.setXMPPacket(&(m_Destination.getXMP().value().getPackets().at(packetIndex)));
    }
}

void MaterialMerger::resolveTextureInfo(const std::optional<TextureInfo>& _SourceInfo, std::optional<TextureInfo>& _Info)
{
    if (_SourceInfo) {
        const size_t textureIndex = m_Indices[_SourceInfo->getTexture()];
        Texture& texture = m_Destination.getTextures().at(textureIndex);
        _Info->setTexture(texture);
    }
}
