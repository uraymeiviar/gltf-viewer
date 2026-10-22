// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Hash.h"
#include "renderer/gl/Enums.h"

#include <string>
#include <unordered_map>

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

    bool operator==(const SamplerState& other) const noexcept
    {
        return minFilter == other.minFilter &&
               magFilter == other.magFilter &&
               wrapS == other.wrapS &&
               wrapT == other.wrapT &&
               wrapR == other.wrapR;
    }
    bool operator!=(const SamplerState& other) const noexcept
    {
        return !(*this == other);
    }
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

namespace std {
template <>
struct hash<ux3d::renderer::gl::SamplerState> {
    size_t operator()(const ux3d::renderer::gl::SamplerState& state) const noexcept
    {
        return static_cast<size_t>(ux3d::core::Hash<ux3d::renderer::gl::SamplerState>{}(state));
    }
};
}

namespace ux3d::renderer::gl {
class SamplerCache {
public:
    SamplerCache() = default;
    explicit SamplerCache(GL* gl) : m_gl(gl) {}
    ~SamplerCache();

    SamplerCache(const SamplerCache&) = delete;
    SamplerCache& operator=(const SamplerCache&) = delete;
    SamplerCache(SamplerCache&& other) noexcept;
    SamplerCache& operator=(SamplerCache&& other) noexcept;

    void init(GL* gl);
    void clear();

    GLuint getOrCreateSampler(const SamplerState& state);
    size_t size() const { return m_samplers.size(); }
    const std::unordered_map<SamplerState, GLuint, core::Hash<SamplerState>>& getSamplers() const { return m_samplers; }

private:
    GL* m_gl = nullptr;
    std::unordered_map<SamplerState, GLuint, core::Hash<SamplerState>> m_samplers;
};

struct Sampler {
    Sampler(const std::string& name, const Texture* texture = nullptr, const SamplerState* samplerState = nullptr);

    void commit(GL* gl, const UniformBinding& binding, GLint textureUnit, GLuint samplerHandle = 0) const;

    std::string name;
    const Texture* texture = nullptr;
    const SamplerState* samplerState = nullptr;
};
}
