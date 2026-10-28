// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/EnvironmentMap.h"
#include "renderer/gl/Texture.h"

#include <algorithm>

ux3d::renderer::EnvironmentMap::EnvironmentMap(gl::GL* gl)
    : m_diffuseSampler("u_LambertianEnvSampler")
    , m_specularSampler("u_GGXEnvSampler")
    , m_sheenSampler("u_CharlieEnvSampler")
    , m_ggxLUTSampler("u_GGXLUT")
    , m_charlieLUTSampler("u_CharlieLUT")
    , m_sheenLUTSampler("u_SheenELUT")
    , m_sheenMipCount("u_sheenMipCount")
    , m_mipCount("u_MipCount")
{
}

void ux3d::renderer::EnvironmentMap::setDiffuse(const gl::Texture* _texture, const gl::SamplerState* _samplerState)
{
    m_diffuseSampler.texture = _texture;
    m_diffuseSampler.samplerState = _samplerState;
}

void ux3d::renderer::EnvironmentMap::setSpecular(const gl::Texture* _texture, const gl::SamplerState* _samplerState, uint32_t _mipCount)
{
    m_specularSampler.texture = _texture;
    m_specularSampler.samplerState = _samplerState;
    m_mipCount.write(static_cast<int32_t>(_mipCount));
}

void ux3d::renderer::EnvironmentMap::setSheen(const gl::Texture* _texture, const gl::SamplerState* _samplerState, uint32_t _mipCount)
{
    m_sheenSampler.texture = _texture;
    m_sheenSampler.samplerState = _samplerState;
    m_sheenMipCount.write(static_cast<int32_t>(_mipCount));
}

void ux3d::renderer::EnvironmentMap::setGGXLUT(const gl::Texture* texture, const gl::SamplerState* samplerState)
{
    m_ggxLUTSampler.texture = texture;
    m_ggxLUTSampler.samplerState = samplerState;
}

void ux3d::renderer::EnvironmentMap::setCharieLUT(const gl::Texture* texture, const gl::SamplerState* samplerState)
{
    m_charlieLUTSampler.texture = texture;
    m_charlieLUTSampler.samplerState = samplerState;
}

void ux3d::renderer::EnvironmentMap::setSheenLUT(const gl::Texture* texture, const gl::SamplerState* samplerState)
{
    m_sheenLUTSampler.texture = texture;
    m_sheenLUTSampler.samplerState = samplerState;
}

void ux3d::renderer::EnvironmentMap::retargetCubes(GLuint diffuse, GLuint specular, GLuint sheen, uint32_t mipCount)
{
    // The textures were created non-const by the resource manager; the samplers only expose them as const.
    if (m_diffuseSampler.texture != nullptr) const_cast<gl::Texture*>(m_diffuseSampler.texture)->handle = diffuse;
    if (m_specularSampler.texture != nullptr) const_cast<gl::Texture*>(m_specularSampler.texture)->handle = specular;
    if (m_sheenSampler.texture != nullptr) const_cast<gl::Texture*>(m_sheenSampler.texture)->handle = sheen;
    m_mipCount.write(static_cast<int32_t>(mipCount));
    m_sheenMipCount.write(static_cast<int32_t>(mipCount));
}
