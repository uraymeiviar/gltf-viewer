// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/gl/Texture.h"
#include "renderer/gl/GL.h"
#include "renderer/gl/Util.h"

#include <array>
#include <stdexcept>

ux3d::renderer::gl::Texture::Texture(GL* gl, TextureTarget target)
    : target(target)
    , m_gl(gl)
{
    gl->genTextures(1, &handle);
}

ux3d::renderer::gl::Texture::Texture(GL* gl, GLuint handle, TextureTarget target)
    : target(target)
    , handle(handle)
    , m_gl(gl)
{
}

ux3d::renderer::gl::Texture::Texture(GL* gl, const image::Image& image)
    : target(extractTarget(image))
    , m_gl(gl)
{
    gl->genTextures(1, &handle);
    write(image, true);
}

ux3d::renderer::gl::Texture::Texture(Texture&& other)
    : m_gl(other.m_gl)
    , handle(other.handle)
    , target(other.target)
{
    other.handle = invalid;
}

ux3d::renderer::gl::Texture::~Texture()
{
    if (handle != invalid) {
        m_gl->deleteTextures(1, &handle);
    }
}

void ux3d::renderer::gl::Texture::bind() const
{
    if (handle == invalid || target == TextureTarget::Invalid) {
        throw std::runtime_error("Cannot bind invalid texture");
    }
    m_gl->bindTexture(static_cast<GLenum>(target), handle);
    checkGLError(m_gl, "Bind Texture");
}

void ux3d::renderer::gl::Texture::write(size_t width, size_t height, Format format, ComponentType pixelFormat, const void* pixels, InternalFormat internalFormat)
{
    if (handle == invalid || target == TextureTarget::Invalid) {
        throw std::runtime_error("Cannot write to invalid texture");
    }

    bind();

    m_gl->texImage2D(
        static_cast<GLenum>(target),
        0,
        internalFormat == InternalFormat::Identity ? static_cast<GLint>(format) : static_cast<GLint>(internalFormat),
        width,
        height,
        0,
        static_cast<GLint>(format),
        static_cast<GLenum>(pixelFormat),
        pixels);
    m_gl->generateMipmap(static_cast<GLenum>(target));
    checkGLError(m_gl, "Cannot write texture");
}

void ux3d::renderer::gl::Texture::write(const image::Image& image, bool isLinear)
{
    if (image.getWidth() == 0u || image.getHeight() == 0u || image.getDepth() != 1u) {
        core::debugLog(core::LogLevel::Error, "Unsupported image format");
        return;
    }
    if (extractTarget(image) != target) {
        throw std::runtime_error("Cannot write image to texture with inconsistent targets");
    }

    ComponentType pixelFormat = extractComponentType(image);
    if (pixelFormat == ComponentType::Unknown) {
        core::debugLog(core::LogLevel::Error, "Unsupported image format");
        return;
    }

    auto format = extractFormat(image);
    auto internalFormat = InternalFormat::Identity;

    if (!isLinear) {
        if (pixelFormat != ComponentType::UByte) {
            core::debugLog(core::LogLevel::Error, "Unsupported image format");
            return;
        }

        if (format == Format::RGB) {
            internalFormat = InternalFormat::SRGB8;
        } else if (format == Format::RGBA) {
            internalFormat = InternalFormat::SRGB8_A8;
        } else if (format == Format::R) {
            internalFormat = InternalFormat::SR8;
        } else if (format == Format::RG) {
            internalFormat = InternalFormat::SRG8;
        } else {
            auto string = image.getMemoryLayoutString();
            core::debugLog(core::LogLevel::Error,
                "Cannot convert the image into a linear format, thus the non-linear image will be shown as is (%s)",
                string.c_str());
        }
    }

    bind();
    m_gl->pixelStorei(GL_UNPACK_ALIGNMENT, 1);
    m_gl->pixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    m_gl->pixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    m_gl->pixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    m_gl->texParameteri(static_cast<GLenum>(target), GL_TEXTURE_BASE_LEVEL, 0);
    m_gl->texParameteri(static_cast<GLenum>(target), GL_TEXTURE_MAX_LEVEL, 64);

    std::array<GLint, 4> swizzleMask;
    if (format == Format::R) {
        swizzleMask = { GL_RED, GL_RED, GL_RED, GL_ONE };
    } else if (format == Format::RG) {
        swizzleMask = { GL_RED, GL_RED, GL_RED, GL_GREEN };
    } else {
        swizzleMask = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
    }
    m_gl->texParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask.data());

    write(image.getWidth(), image.getHeight(), format, pixelFormat, image.getPixels().data(), internalFormat);
}

auto ux3d::renderer::gl::Texture::extractFormat(const image::Image& image) -> Format
{
    switch (image.getChannelCount()) {
    case 1:
        return Format::R;
    case 2:
        return Format::RG;
    case 3:
        return Format::RGB;
    case 4:
        return Format::RGBA;
    default:
        throw std::runtime_error("Cannot extract format of image with channel count outside of [0, 4]");
    }
}

auto ux3d::renderer::gl::Texture::extractComponentType(const image::Image& image) -> ComponentType
{
    switch (image.getFormat()) {
    case image::Format::FLOAT:
        switch (image.getChannelSize(image::Channel::RED)) {
        case 16:
            return ComponentType::HalfFloat;
        case 32:
            return ComponentType::Float;
        default:
            return ComponentType::Unknown;
        }
    case image::Format::INT:
    case image::Format::NORM:
        if (image.isSigned() && image.getFormat() == image::Format::NORM) {
            // signed normalized format currently not supported
            return ComponentType::Unknown;
        }
        switch (image.getChannelSize(image::Channel::RED)) {
        case 8:
            return ComponentType::UByte;
        case 16:
            return ComponentType::UShort;
        case 32:
            return ComponentType::UInt;
        default:
            return ComponentType::Unknown;
        }
    default:
        return ComponentType::Unknown;
    }
}

auto ux3d::renderer::gl::Texture::extractTarget(const image::Image& image) -> TextureTarget
{
    if (image.getFaceCount() == 1 && image.getArrayCount() == 1) {
        return TextureTarget::Texture2d;
    }
    if (image.getFaceCount() == 1 && image.getArrayCount() > 1) {
        return TextureTarget::Texture2dArray;
    }
    if (image.getFaceCount() == 6 && image.getArrayCount() == 1) {
        return TextureTarget::TextureCubeMap;
    }
    throw std::runtime_error("Cannot extract texture target from image");
}
