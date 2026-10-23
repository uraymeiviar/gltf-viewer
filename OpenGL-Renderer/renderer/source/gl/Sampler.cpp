// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/gl/Sampler.h"
#include "renderer/gl/GL.h"
#include "renderer/gl/Texture.h"
#include "renderer/gl/UniformBinding.h"
#include "renderer/gl/Util.h"

ux3d::renderer::gl::SamplerCache::~SamplerCache()
{
    clear();
}

ux3d::renderer::gl::SamplerCache::SamplerCache(SamplerCache&& other) noexcept
    : m_gl(other.m_gl)
    , m_samplers(std::move(other.m_samplers))
{
    other.m_gl = nullptr;
}

auto ux3d::renderer::gl::SamplerCache::operator=(SamplerCache&& other) noexcept -> SamplerCache&
{
    if (this != &other) {
        clear();
        m_gl = other.m_gl;
        m_samplers = std::move(other.m_samplers);
        other.m_gl = nullptr;
    }
    return *this;
}

void ux3d::renderer::gl::SamplerCache::init(GL* gl)
{
    if (m_gl != gl) {
        clear();
        m_gl = gl;
    }
}

void ux3d::renderer::gl::SamplerCache::clear()
{
    if (m_gl != nullptr && !m_samplers.empty()) {
        for (const auto& [state, handle] : m_samplers) {
            if (handle != 0 && handle != gl::invalid) {
                m_gl->deleteSamplers(1, &handle);
            }
        }
        m_samplers.clear();
    }
}

GLuint ux3d::renderer::gl::SamplerCache::getOrCreateSampler(const SamplerState& state)
{
    auto it = m_samplers.find(state);
    if (it != m_samplers.end()) {
        return it->second;
    }

    if (m_gl == nullptr) {
        return 0;
    }

    GLuint sampler = 0;
    m_gl->genSamplers(1, &sampler);
    if (sampler == 0) {
        return 0;
    }

    m_gl->samplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(state.minFilter));
    m_gl->samplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(state.magFilter));
    m_gl->samplerParameteri(sampler, GL_TEXTURE_WRAP_S, static_cast<GLint>(state.wrapS));
    m_gl->samplerParameteri(sampler, GL_TEXTURE_WRAP_T, static_cast<GLint>(state.wrapT));
    m_gl->samplerParameteri(sampler, GL_TEXTURE_WRAP_R, static_cast<GLint>(state.wrapR));

    if (m_gl->supportsAnisotropicFiltering()) {
        m_gl->samplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, m_gl->maxTextureMaxAnisotropy());
    }

    m_samplers.emplace(state, sampler);
    return sampler;
}

ux3d::renderer::gl::Sampler::Sampler(const std::string& name, const Texture* texture, const SamplerState* samplerState)
    : name(name)
    , texture(texture)
    , samplerState(samplerState)
{
}

void ux3d::renderer::gl::Sampler::commitUnit(GL* gl, const UniformBinding& binding, GLint textureUnit) const
{
    if (binding.committedUnit != textureUnit) {
        gl->uniform1i(binding.location, textureUnit);
        binding.committedUnit = textureUnit;
    }
}

void ux3d::renderer::gl::Sampler::commit(GL* gl, const UniformBinding& binding, GLint textureUnit, GLuint samplerHandle) const
{
    if (texture == nullptr) {
        return;
    }

    gl->activeTexture(GL_TEXTURE0 + textureUnit);
    texture->bind();

    if (samplerHandle != 0) {
        gl->bindSampler(static_cast<GLuint>(textureUnit), samplerHandle);
    } else {
        gl->bindSampler(static_cast<GLuint>(textureUnit), 0);
        auto target = static_cast<GLenum>(texture->target);

        if (samplerState != nullptr) {
            gl->texParameteri(target, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(samplerState->minFilter));
            gl->texParameteri(target, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(samplerState->magFilter));
            gl->texParameteri(target, GL_TEXTURE_WRAP_S, static_cast<GLint>(samplerState->wrapS));
            gl->texParameteri(target, GL_TEXTURE_WRAP_T, static_cast<GLint>(samplerState->wrapT));
            gl->texParameteri(target, GL_TEXTURE_WRAP_R, static_cast<GLint>(samplerState->wrapR));
        } else {
            SamplerState defaultSamplerState;
            gl->texParameteri(target, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(defaultSamplerState.minFilter));
            gl->texParameteri(target, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(defaultSamplerState.magFilter));
            gl->texParameteri(target, GL_TEXTURE_WRAP_S, static_cast<GLint>(defaultSamplerState.wrapS));
            gl->texParameteri(target, GL_TEXTURE_WRAP_T, static_cast<GLint>(defaultSamplerState.wrapT));
            gl->texParameteri(target, GL_TEXTURE_WRAP_R, static_cast<GLint>(defaultSamplerState.wrapR));
        }

        if (gl->supportsAnisotropicFiltering()) {
            gl->texParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, gl->maxTextureMaxAnisotropy());
        }
    }

    commitUnit(gl, binding, textureUnit);
    GLIDER_HOT_GL_CHECK(gl, "Failed to commit sampler");
}
