// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "AlphaMode.h"
#include "ChildOfRoot.h"
#include "TextureInfo.h"
#include "XMP.h"
#include "material/Anisotropy.h"
#include "material/Clearcoat.h"
#include "material/EmissiveStrength.h"
#include "material/Ior.h"
#include "material/Iridescence.h"
#include "material/MaterialType.h"
#include "material/PbrMetallicRoughness.h"
#include "material/PbrSpecularGlossiness.h"
#include "material/Sheen.h"
#include "material/Specular.h"
#include "material/Transmission.h"
#include "material/Unlit.h"
#include "material/Volume.h"
#include "math/VectorTypes.h"
#include <optional>

// See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#material

namespace ux3d::gltf {

class Material : public ChildOfRoot {
private:
    std::optional<AlphaMode> m_alphaMode;
    // A1.7: the file wrote the MSFS exporter's non-standard core alphaMode "DITHER". It is read
    // as OPAQUE plus this flag, which selects the ASOBO_material_alphamode_dither path.
    bool m_alphaModeDither = false;
    std::optional<double> m_alphaCutoff;
    std::optional<bool> m_doubleSided;
    std::optional<math::dvec3> m_emissiveFactor;

    std::optional<TextureInfo> m_normalTextureInfo;
    std::optional<TextureInfo> m_occlusionTextureInfo;
    std::optional<TextureInfo> m_emissiveTextureInfo;
    std::optional<PbrMetallicRoughness> m_pbrMetallicRoughness;
    std::optional<PbrSpecularGlossiness> m_pbrSpecularGlossiness;
    std::optional<Clearcoat> m_clearcoat;
    std::optional<Sheen> m_sheen;
    std::optional<Specular> m_specular;
    std::optional<Ior> m_ior;
    std::optional<Unlit> m_unlit;
    std::optional<Transmission> m_transmission;
    std::optional<EmissiveStrength> m_emissiveStrength;
    std::optional<Volume> m_volume;
    std::optional<Iridescence> m_iridescence;
    std::optional<Anisotropy> m_anisotropy;

    // extensions
    std::optional<PacketContainer> m_xmpPacket = std::nullopt;

public:
    Material(const Material&) = delete;
    Material(Material&&) = delete;

    Material(uint32_t _id); // regular constructor
    Material(uint32_t _id, const Material& _source); // duplicate in same glTF

    GltfType getGltfType() const override { return GltfType::Material; }

    GltfResult createNormalTextureInfo();
    void destroyNormalTextureInfo() { m_normalTextureInfo.reset(); }
    GltfResult createOcclusionTextureInfo();
    void destroyOcclusionTextureInfo() { m_occlusionTextureInfo.reset(); }
    GltfResult createEmissiveTextureInfo();
    void destroyEmissiveTextureInfo() { m_emissiveTextureInfo.reset(); }

    GltfResult setAlphaMode(AlphaMode _alphaMode);
    GltfResult setAlphaCutoff(double _alphaCutoff);
    GltfResult setDoubleSided(bool _doubleSided);
    GltfResult setEmissiveFactor(const math::dvec3& _emissiveFactor);

    void unsetAlphaMode() { m_alphaMode.reset(); }
    void unsetAlphaCutoff() { m_alphaCutoff.reset(); }
    void unsetDoubleSided() { m_doubleSided.reset(); }
    void unsetEmissiveFactor() { m_emissiveFactor.reset(); }

    bool hasAlphaMode() const { return m_alphaMode.has_value(); }
    bool hasAlphaCutoff() const { return m_alphaCutoff.has_value(); }
    bool hasDoubleSided() const { return m_doubleSided.has_value(); }
    bool hasEmissiveFactor() const { return m_emissiveFactor.has_value(); }

    AlphaMode getAlphaModeOrDefault() const { return m_alphaMode.value_or(AlphaMode::Opaque); }
    // A1.7: true when the core alphaMode string was "DITHER" (stored as OPAQUE). setAlphaMode clears it.
    bool isAlphaModeDither() const { return m_alphaModeDither; }
    void setAlphaModeDither(bool _dither) { m_alphaModeDither = _dither; }
    double getAlphaCutoffOrDefault() const { return m_alphaCutoff.value_or(0.5); }
    bool isDoubleSidedOrDefault() const { return m_doubleSided.value_or(false); }
    math::dvec3 getEmissiveFactorOrDefault() const { return m_emissiveFactor.value_or(math::dvec3(0)); }

    std::optional<TextureInfo>& getNormalTextureInfo() { return m_normalTextureInfo; }
    const std::optional<TextureInfo>& getNormalTextureInfo() const { return m_normalTextureInfo; }

    std::optional<TextureInfo>& getOcclusionTextureInfo() { return m_occlusionTextureInfo; }
    const std::optional<TextureInfo>& getOcclusionTextureInfo() const { return m_occlusionTextureInfo; }

    std::optional<TextureInfo>& getEmissiveTextureInfo() { return m_emissiveTextureInfo; }
    const std::optional<TextureInfo>& getEmissiveTextureInfo() const { return m_emissiveTextureInfo; }

    GltfResult createPbrMetallicRoughness();
    void destroyPbrMetallicRoughness();
    std::optional<PbrMetallicRoughness>& getPbrMetallicRoughness() { return m_pbrMetallicRoughness; }
    const std::optional<PbrMetallicRoughness>& getPbrMetallicRoughness() const { return m_pbrMetallicRoughness; }

    GltfResult createPbrSpecularGlossiness();
    void destroyPbrSpecularGlossiness() { m_pbrSpecularGlossiness.reset(); }
    std::optional<PbrSpecularGlossiness>& getPbrSpecularGlossiness() { return m_pbrSpecularGlossiness; }
    const std::optional<PbrSpecularGlossiness>& getPbrSpecularGlossiness() const { return m_pbrSpecularGlossiness; }

    GltfResult createClearcoat();
    void destroyClearcoat() { m_clearcoat.reset(); }
    std::optional<Clearcoat>& getClearcoat() { return m_clearcoat; }
    const std::optional<Clearcoat>& getClearcoat() const { return m_clearcoat; }

    GltfResult createSpecular();
    void destroySpecular() { m_specular.reset(); }
    std::optional<Specular>& getSpecular() { return m_specular; }
    const std::optional<Specular>& getSpecular() const { return m_specular; }

    GltfResult createIor();
    void destroyIor() { m_ior.reset(); }
    std::optional<Ior>& getIor() { return m_ior; }
    const std::optional<Ior>& getIor() const { return m_ior; }

    GltfResult createTransmission();
    void destroyTransmission() { m_transmission.reset(); }
    std::optional<Transmission>& getTransmission() { return m_transmission; }
    const std::optional<Transmission>& getTransmission() const { return m_transmission; }

    GltfResult createEmissiveStrength();
    void destroyEmissiveStrength() { m_emissiveStrength.reset(); }
    std::optional<EmissiveStrength>& getEmissiveStrength() { return m_emissiveStrength; }
    const std::optional<EmissiveStrength>& getEmissiveStrength() const { return m_emissiveStrength; }

    GltfResult createSheen();
    void destroySheen() { m_sheen.reset(); }
    std::optional<Sheen>& getSheen() { return m_sheen; }
    const std::optional<Sheen>& getSheen() const { return m_sheen; }

    GltfResult createVolume();
    void destroyVolume() { m_volume.reset(); }
    std::optional<Volume>& getVolume() { return m_volume; }
    const std::optional<Volume>& getVolume() const { return m_volume; }

    GltfResult createIridescence();
    void destroyIridescence() { m_iridescence.reset(); }
    std::optional<Iridescence>& getIridescence() { return m_iridescence; }
    const std::optional<Iridescence>& getIridescence() const { return m_iridescence; }

    GltfResult createAnisotropy();
    void destroyAnisotropy() { m_anisotropy.reset(); }
    std::optional<Anisotropy>& getAnisotropy() { return m_anisotropy; }
    const std::optional<Anisotropy>& getAnisotropy() const { return m_anisotropy; }

    GltfResult createUnlit();
    void destroyUnlit() { m_unlit.reset(); }
    std::optional<Unlit>& getUnlit() { return m_unlit; }
    const std::optional<Unlit>& getUnlit() const { return m_unlit; }
    bool isUnlit() const { return m_unlit.has_value(); }

    bool hasXMPPacket() const { return m_xmpPacket.has_value(); }
    std::optional<PacketContainer>& getXMPPacket() { return m_xmpPacket; }
    const std::optional<PacketContainer>& getXMPPacket() const { return m_xmpPacket; }
    GltfResult setXMPPacket(Packet* _xmpPacket);
    GltfResult unsetXMPPacket();

    Extensions getUsedExtensions() const override;

    bool isValid() const override;
    GltfResult getInvalidProperties(core::Vector<core::string>& _invalidProperties) const override;

private:
    bool checkForPbrSpecularGlossiness() const { return m_pbrSpecularGlossiness.has_value(); }
    bool checkForUnlit() const { return m_unlit.has_value(); }
    bool checkForClearcoat() const { return m_clearcoat.has_value(); }
    bool checkForSpecular() const { return m_specular.has_value(); }
    bool checkForSheen() const { return m_sheen.has_value(); }
    bool checkForIor() const { return m_ior.has_value(); }
    bool checkForTransmission() const { return m_transmission.has_value(); }
    bool checkForVolume() const { return m_volume.has_value(); }
    bool checkForIridescence() const { return m_iridescence.has_value(); }
    bool checkForAnisotropy() const { return m_anisotropy.has_value(); }
    bool checkForTextureTransform() const;
    void resetPBRNextExtensions();
    void resetPBRNextExtensionExceptions();
};
}
