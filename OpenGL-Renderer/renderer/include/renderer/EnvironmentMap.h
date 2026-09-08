// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/String.h"
#include "core/Vector.h"
#include "renderer/gl/Sampler.h"
#include "renderer/gl/Uniform.h"

namespace ux3d::renderer::gl {
struct SamplerState;
class Texture;
}

namespace ux3d::renderer {
class EnvironmentMap {
public:
    EnvironmentMap(gl::GL*);

    void setDiffuse(const gl::Texture* texture, const gl::SamplerState* samplerState);
    const gl::Sampler& getDiffuseSampler() const { return m_diffuseSampler; }

    void setSpecular(const gl::Texture* texture, const gl::SamplerState* samplerState, uint32_t mipCount);
    const gl::Sampler& getSpecularSampler() const { return m_specularSampler; }

    void setSheen(const gl::Texture* texture, const gl::SamplerState* samplerState, uint32_t mipCount);
    const gl::Sampler& getSheenSampler() const { return m_sheenSampler; }

    void setGGXLUT(const gl::Texture* texture, const gl::SamplerState* samplerState);
    const gl::Sampler& getGGXLUTSampler() const { return m_ggxLUTSampler; }

    void setCharieLUT(const gl::Texture* texture, const gl::SamplerState* samplerState);
    const gl::Sampler& getCharieLUTSampler() const { return m_charlieLUTSampler; }

    void setSheenLUT(const gl::Texture* texture, const gl::SamplerState* samplerState);
    const gl::Sampler& getSheenLUTSampler() const { return m_sheenLUTSampler; }

    const gl::Uniform& getMipCount() const { return m_mipCount; }
    const gl::Uniform& getSheenMipCount() const { return m_sheenMipCount; }

private:
    gl::Sampler m_diffuseSampler;
    gl::Sampler m_specularSampler;
    gl::Sampler m_sheenSampler;
    gl::Sampler m_ggxLUTSampler;
    gl::Sampler m_charlieLUTSampler;
    gl::Sampler m_sheenLUTSampler;
    gl::Uniform m_sheenMipCount;
    gl::Uniform m_mipCount;
};
}
