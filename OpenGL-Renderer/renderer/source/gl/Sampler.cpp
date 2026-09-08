// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/gl/Sampler.h"
#include "renderer/gl/Texture.h"
#include "renderer/gl/UniformBinding.h"
#include "renderer/gl/Util.h"

ux3d::renderer::gl::Sampler::Sampler(const std::string& name, const Texture* texture, const SamplerState* samplerState)
    : name(name)
    , texture(texture)
    , samplerState(samplerState)
{
}

void ux3d::renderer::gl::Sampler::commit(GL* gl, const UniformBinding& binding, GLint textureUnit) const
{
    gl->activeTexture(GL_TEXTURE0 + textureUnit);

    texture->bind();

    auto target = static_cast<GLenum>(texture->target);

    if (samplerState != nullptr) {
        gl->texParameteri(target, GL_TEXTURE_MIN_FILTER, GLenum(samplerState->minFilter));
        gl->texParameteri(target, GL_TEXTURE_MAG_FILTER, GLenum(samplerState->magFilter));
        gl->texParameteri(target, GL_TEXTURE_WRAP_S, GLenum(samplerState->wrapS));
        gl->texParameteri(target, GL_TEXTURE_WRAP_T, GLenum(samplerState->wrapT));
        gl->texParameteri(target, GL_TEXTURE_WRAP_R, GLenum(samplerState->wrapR));
    } else {
        SamplerState defaultSamplerState;
        gl->texParameteri(target, GL_TEXTURE_MIN_FILTER, GLenum(defaultSamplerState.minFilter));
        gl->texParameteri(target, GL_TEXTURE_MAG_FILTER, GLenum(defaultSamplerState.magFilter));
        gl->texParameteri(target, GL_TEXTURE_WRAP_S, GLenum(defaultSamplerState.wrapS));
        gl->texParameteri(target, GL_TEXTURE_WRAP_T, GLenum(defaultSamplerState.wrapT));
        gl->texParameteri(target, GL_TEXTURE_WRAP_R, GLenum(defaultSamplerState.wrapR));
    }

    // Also enable anisotropic filtering if available.
    if (gl->hasExtension("GL_EXT_texture_filter_anisotropic")) {
        GLfloat maxAnisotropy;
        gl->getFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);
        gl->texParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisotropy);
    }

    gl->uniform1i(binding.location, textureUnit);
    checkGLError(gl, "Failed to commit sampler");
}
