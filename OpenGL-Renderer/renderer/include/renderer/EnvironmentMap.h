// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/String.h"
#include "core/Vector.h"
#include "renderer/gl/Sampler.h"
#include "renderer/gl/Uniform.h"
#include <atomic>

namespace ux3d::renderer::gl {
struct SamplerState;
class Texture;
}

namespace ux3d::renderer {
class EnvironmentMap {
public:
    EnvironmentMap(gl::GL*);
    uint64_t generation() const { return m_generation; }

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

    // A6.3: points the cube samplers at other texture names (a dynamic environment swapping in newly filtered cubes)
    // and updates the mip counts. The samplers, uniforms and this map stay the same objects, so the commands that
    // reference them stay valid. Only for maps whose cube textures do not own their handles.
    void retargetCubes(GLuint diffuse, GLuint specular, GLuint sheen, uint32_t mipCount);
    const gl::Uniform& getSheenMipCount() const { return m_sheenMipCount; }

private:
    inline static std::atomic<uint64_t> s_nextGeneration{0};
    const uint64_t m_generation = ++s_nextGeneration;
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
