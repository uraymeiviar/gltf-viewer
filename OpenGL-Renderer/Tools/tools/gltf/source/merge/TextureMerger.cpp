// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/merge/TextureMerger.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"

bool ux3d::gltf::TextureMerger::resolveDependencies(const Texture& _SourceTexture, Texture& _Texture)
{
    resolveSampler(_SourceTexture, _Texture);
    resolveImage(_SourceTexture, _Texture);
    resolveBasisU(_SourceTexture, _Texture);
    resolveWebP(_SourceTexture, _Texture);
    resolveTextureDds(_SourceTexture, _Texture);
    return true;
}

void ux3d::gltf::TextureMerger::resolveSampler(const Texture& _SourceTexture, Texture& _Texture)
{
    const Sampler* pSourceSampler = _SourceTexture.getSampler();
    if (pSourceSampler != nullptr) {
        const size_t samplerIndex = m_Indices[pSourceSampler];
        Sampler& sampler = m_Destination.getSamplers().at(samplerIndex);
        _Texture.setSampler(sampler);
    }
}

void ux3d::gltf::TextureMerger::resolveImage(const Texture& sourceTexture, Texture& texture)
{
    const Image* pSourceImage = sourceTexture.getSource();
    if (pSourceImage != nullptr) {
        const size_t imageIndex = m_Indices[pSourceImage];
        Image& image = m_Destination.getImages().at(imageIndex);
        texture.setSource(image);
    }
}

void ux3d::gltf::TextureMerger::resolveBasisU(const Texture& sourceTexture, Texture& texture)
{
    if (!sourceTexture.getBasisU().has_value()) {
        return;
    }

    const Image* sourceImage = sourceTexture.getBasisU()->getSource();
    if (sourceImage != nullptr) {
        const size_t imageIndex = m_Indices[sourceImage];
        Image& image = m_Destination.getImages().at(imageIndex);
        texture.createBasisU();
        texture.getBasisU()->setSource(&image);
    }
}

void ux3d::gltf::TextureMerger::resolveWebP(const Texture& sourceTexture, Texture& texture)
{
    if (!sourceTexture.getWebP().has_value()) {
        return;
    }

    const Image* sourceImage = sourceTexture.getWebP()->getSource();
    if (sourceImage != nullptr) {
        const size_t imageIndex = m_Indices[sourceImage];
        Image& image = m_Destination.getImages().at(imageIndex);
        texture.createWebP();
        texture.getWebP()->setSource(&image);
    }
}

void ux3d::gltf::TextureMerger::resolveTextureDds(const Texture& sourceTexture, Texture& texture)
{
    if (!sourceTexture.getTextureDds().has_value()) {
        return;
    }

    const Image* sourceImage = sourceTexture.getTextureDds()->getSource();
    if (sourceImage != nullptr) {
        const size_t imageIndex = m_Indices[sourceImage];
        Image& image = m_Destination.getImages().at(imageIndex);
        texture.createTextureDds();
        texture.getTextureDds()->setSource(&image);
    }
}
