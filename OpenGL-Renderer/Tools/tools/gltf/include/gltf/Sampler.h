// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRoot.h"
#include "GlTypes.h"
#include <optional>

//See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#sampler

namespace ux3d::gltf {
class Sampler : public ChildOfRoot {
private:
    std::optional<TextureFilterType> m_MagFilter;
    std::optional<TextureFilterType> m_MinFilter;
    std::optional<TextureWrappingMode> m_WrapS;
    std::optional<TextureWrappingMode> m_WrapT;

public:
    Sampler(uint32_t _id); // regular constructor
    Sampler(uint32_t _id, const Sampler& _source); // duplicate in same glTF

    GltfType getGltfType() const override { return GltfType::Sampler; }

    const std::optional<TextureFilterType>& getMagFilter() const { return m_MagFilter; }
    const std::optional<TextureFilterType>& getMinFilter() const { return m_MinFilter; }

    bool hasWrapS() const { return m_WrapS.has_value(); }
    bool hasWrapT() const { return m_WrapT.has_value(); }

    TextureWrappingMode getWrapSOrDefault() const { return m_WrapS.value_or(TextureWrappingMode::Repeat); }
    TextureWrappingMode getWrapTOrDefault() const { return m_WrapT.value_or(TextureWrappingMode::Repeat); }

    GltfResult setMagFilter(TextureFilterType _MagFilter);
    GltfResult setMinFilter(TextureFilterType _MinFilter);
    GltfResult setWrapS(TextureWrappingMode _WrapS);
    GltfResult setWrapT(TextureWrappingMode _WrapT);

    void unsetMagFilter() { m_MagFilter.reset(); }
    void unsetMinFilter() { m_MinFilter.reset(); }
    void unsetWrapS() { m_WrapS.reset(); }
    void unsetWrapT() { m_WrapT.reset(); }

    bool isValid() const override;
    GltfResult getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const override;
};
}
