// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <optional>

namespace ux3d::gltf {
class Gltf;
class Material;
class TextureInfo;
class IndexLookup;

class MaterialMerger {
private:
    Gltf& m_Destination;
    const IndexLookup& m_Indices;

    void resolveBaseTextures(const Material& _SourceMaterial, Material& _Material);
    void resolveMetallicRoughnessTextures(const Material& _SourceMaterial, Material& _Material);
    void resolveSpecularGlossinessTextures(const Material& _SourceMaterial, Material& _Material);
    void resolveClearcoatTextures(const Material& _SourceMaterial, Material& _Material);
    void resolveSpecularTextures(const Material& _SourceMaterial, Material& _Material);
    void resolveSheenTextures(const Material& _SourceMaterial, Material& _Material);
    void resolveTransmissionTextures(const Material& _SourceMaterial, Material& _Material);
    void resolveVolumeTextures(const Material& _SourceMaterial, Material& _Material);
    void resolveIridescenceTextures(const Material& _SourceMaterial, Material& _Material);
    void resolveXMP(const Material& _SourceMaterial, Material& _Material);
    void resolveTextureInfo(const std::optional<ux3d::gltf::TextureInfo>& _SourceInfo, std::optional<ux3d::gltf::TextureInfo>& _Info);

public:
    MaterialMerger(Gltf& _Destination, const IndexLookup& _IndexLookup)
        : m_Destination(_Destination)
        , m_Indices(_IndexLookup)
    {
    }

    bool resolveDependencies(const Material& _SourceMaterial, Material& _Material);
};
}
