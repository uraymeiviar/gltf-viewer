// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRoot.h"
#include "gltf/extensions/BasisU.h"
#include "gltf/extensions/WebP.h"
#include "gltf/extensions/TextureDds.h"
#include <optional>

// See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#texture

namespace ux3d::gltf {
class Image;
class Sampler;

class Texture : public ChildOfRoot {
private:
    Image* m_pSource = nullptr;
    Sampler* m_pSampler = nullptr;
    // Image* m_pBasisU = nullptr;
    std::optional<BasisU> m_basisU;
    std::optional<WebP> m_webP;
    std::optional<TextureDds> m_textureDds;

public:
    Texture(uint32_t _id); // regular constructor
    Texture(uint32_t _id, const Texture& _source); // duplicate in same glTF

    GltfType getGltfType() const override { return GltfType::Texture; }

    Image* getSource() const;
    Sampler* getSampler() const;

    std::optional<BasisU>& getBasisU();
    const std::optional<BasisU>& getBasisU() const;
    GltfResult createBasisU();
    void destroyBasisU();

    std::optional<WebP>& getWebP();
    const std::optional<WebP>& getWebP() const;
    GltfResult createWebP();
    void destroyWebP();

    std::optional<TextureDds>& getTextureDds();
    const std::optional<TextureDds>& getTextureDds() const;
    GltfResult createTextureDds();
    void destroyTextureDds();

    GltfResult setSource(Image& _Source);
    GltfResult setSampler(Sampler& _Sampler);

    void unsetSource() { m_pSource = nullptr; }
    void unsetSample() { m_pSampler = nullptr; }

    Extensions getUsedExtensions() const override;

    bool isValid() const override;
    GltfResult getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const override;
};
}
