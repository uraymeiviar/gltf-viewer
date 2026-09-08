// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Hash.h"
#include "renderer/gl/Enums.h"

#include <string>

namespace ux3d::renderer::gl {
class Texture;
class GL;
struct UniformBinding;

struct SamplerState {
    TextureMinFilter minFilter = TextureMinFilter::LinearMipmapLinear;
    TextureMagFilter magFilter = TextureMagFilter::Linear;
    TextureWrap wrapS = TextureWrap::Repeat;
    TextureWrap wrapT = TextureWrap::Repeat;
    TextureWrap wrapR = TextureWrap::Repeat;
};

struct Sampler {
    Sampler(const std::string& name, const Texture* texture = nullptr, const SamplerState* samplerState = nullptr);

    void commit(GL* gl, const UniformBinding& binding, GLint textureUnit) const;

    std::string name;
    const Texture* texture = nullptr;
    const SamplerState* samplerState = nullptr;
};
}

namespace ux3d {
template <>
struct core::Hash<renderer::gl::SamplerState> {
    uint64_t operator()(const renderer::gl::SamplerState& state, uint64_t _seed = detail::g_seed) const noexcept
    {
        Hasher h(_seed);
        h << state.minFilter << state.magFilter << state.wrapS << state.wrapT << state.wrapR;
        return h.getHash();
    }
};
}
