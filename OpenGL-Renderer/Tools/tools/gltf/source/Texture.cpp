// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Texture.h"
#include "gltf/Image.h"
#include "gltf/Sampler.h"

using namespace ux3d;
using namespace ux3d::gltf;

Texture::Texture(uint32_t _id)
    : ChildOfRoot(_id)
{
}

Texture::Texture(uint32_t _id, const Texture& _source)
    : ChildOfRoot(_id, _source)
    , m_pSource(_source.m_pSource)
    , m_pSampler(_source.m_pSampler)
    , m_basisU(_source.m_basisU)
{
}

Image* Texture::getSource() const
{
    return m_pSource;
}

Sampler* Texture::getSampler() const
{
    return m_pSampler;
}

std::optional<BasisU>& Texture::getBasisU()
{
    return m_basisU;
}

const std::optional<BasisU>& Texture::getBasisU() const
{
    return m_basisU;
}

GltfResult Texture::createBasisU()
{
    if (m_basisU.has_value()) {
        return GltfError::ObjectAlreadyExists;
    }
    m_basisU.emplace();
    return GltfResult::Success;
}

void Texture::destroyBasisU()
{
    m_basisU.reset();
}

std::optional<WebP>& Texture::getWebP()
{
    return m_webP;
}

const std::optional<WebP>& Texture::getWebP() const
{
    return m_webP;
}

GltfResult Texture::createWebP()
{
    if (m_webP.has_value()) {
        return GltfError::ObjectAlreadyExists;
    }
    m_webP.emplace();
    return GltfResult::Success;
}

void Texture::destroyWebP()
{
    m_webP.reset();
}

GltfResult Texture::setSource(Image& _Source)
{
    m_pSource = &_Source;
    return GltfResult::Success;
}

GltfResult Texture::setSampler(Sampler& _Sampler)
{
    m_pSampler = &_Sampler;
    return GltfResult::Success;
}

Extensions Texture::getUsedExtensions() const
{
    Extensions usedExtensions;

    if (m_basisU.has_value()) {
        usedExtensions |= Extension::TextureBasisU;
    }

    return usedExtensions;
}

bool Texture::isValid() const
{
    return true;
}

GltfResult Texture::getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const
{
    return GltfResult::Success;
}
