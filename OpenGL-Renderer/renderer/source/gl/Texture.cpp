// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/gl/Texture.h"
#include "core/Logger.h"
#include "imageformats/ktx2.h"
#include "renderer/gl/GL.h"
#include "renderer/gl/Util.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace {

bool usesBPTC(ux3d::gpu::Format _format)
{
    return _format == ux3d::gpu::Format::BC7_UNORM_BLOCK || _format == ux3d::gpu::Format::BC7_SRGB_BLOCK;
}

bool usesS3TC(ux3d::gpu::Format _format)
{
    switch (_format) {
    case ux3d::gpu::Format::BC1_RGB_UNORM_BLOCK:
    case ux3d::gpu::Format::BC1_RGB_SRGB_BLOCK:
    case ux3d::gpu::Format::BC1_RGBA_UNORM_BLOCK:
    case ux3d::gpu::Format::BC1_RGBA_SRGB_BLOCK:
    case ux3d::gpu::Format::BC3_UNORM_BLOCK:
    case ux3d::gpu::Format::BC3_SRGB_BLOCK:
        return true;
    default:
        return false;
    }
}

GLenum compressedInternalFormat(ux3d::gpu::Format _format, bool _isLinear)
{
    switch (_format) {
    case ux3d::gpu::Format::BC1_RGB_UNORM_BLOCK:
    case ux3d::gpu::Format::BC1_RGB_SRGB_BLOCK:
    case ux3d::gpu::Format::BC1_RGBA_UNORM_BLOCK:
    case ux3d::gpu::Format::BC1_RGBA_SRGB_BLOCK:
        return _isLinear ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT : GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT;
    case ux3d::gpu::Format::BC3_UNORM_BLOCK:
    case ux3d::gpu::Format::BC3_SRGB_BLOCK:
        return _isLinear ? GL_COMPRESSED_RGBA_S3TC_DXT5_EXT : GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT;
    case ux3d::gpu::Format::BC5_SNORM_BLOCK:
        return GL_COMPRESSED_SIGNED_RG_RGTC2;
    case ux3d::gpu::Format::BC7_UNORM_BLOCK:
    case ux3d::gpu::Format::BC7_SRGB_BLOCK:
        return _isLinear ? GL_COMPRESSED_RGBA_BPTC_UNORM : GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;
    default:
        return 0;
    }
}

} // namespace

ux3d::renderer::gl::Texture::Texture(GL* gl, TextureTarget target)
    : target(target)
    , m_gl(gl)
{
    gl->genTextures(1, &handle);
}

ux3d::renderer::gl::Texture::Texture(GL* gl, GLuint handle, TextureTarget target, bool ownsHandle)
    : target(target)
    , handle(handle)
    , m_gl(gl)
    , m_ownsHandle(ownsHandle)
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
    , m_ownsHandle(other.m_ownsHandle)
{
    other.handle = invalid;
}

ux3d::renderer::gl::Texture::~Texture()
{
    if (m_ownsHandle && handle != invalid) {
        m_gl->deleteTextures(1, &handle);
    }
}

void ux3d::renderer::gl::Texture::bind() const
{
    if (handle == invalid || target == TextureTarget::Invalid) {
        throw std::runtime_error("Cannot bind invalid texture");
    }
    m_gl->bindTexture(static_cast<GLenum>(target), handle);
    GLIDER_HOT_GL_CHECK(m_gl, "Bind Texture");
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

bool ux3d::renderer::gl::Texture::writeCompressed(const image::Image& image, const imageformats::KTX2ImageInfo& info, bool isLinear)
{
    if (target != TextureTarget::Texture2d || info.faceCount != 1u || info.layerCount != 1u || info.depth != 1u) {
        core::logWarning("KTX2 vkFormat %u: only single 2D images are supported for compressed upload", static_cast<uint32_t>(info.format));
        return false;
    }
    if (!imageformats::isSupportedCompressedKTX2(info)) {
        core::logWarning("KTX2 vkFormat %u: compressed upload is not supported", static_cast<uint32_t>(info.format));
        return false;
    }

    const uint64_t expectedSize = imageformats::getKTX2CompressedDataSize(info);
    if (expectedSize == 0u || expectedSize != image.getPixels().size()) {
        core::logWarning("KTX2 vkFormat %u: compressed image data size does not match its header", static_cast<uint32_t>(info.format));
        return false;
    }

    if (usesBPTC(info.format)
        && !m_gl->hasExtension("GL_ARB_texture_compression_bptc")
        && !m_gl->hasExtension("GL_EXT_texture_compression_bptc")) {
        core::logWarning("KTX2 vkFormat %u: GL_ARB_texture_compression_bptc is unavailable", static_cast<uint32_t>(info.format));
        return false;
    }
    if (usesS3TC(info.format) && !m_gl->hasExtension("GL_EXT_texture_compression_s3tc")) {
        core::logWarning("KTX2 vkFormat %u: GL_EXT_texture_compression_s3tc is unavailable", static_cast<uint32_t>(info.format));
        return false;
    }

    const GLenum internalFormat = compressedInternalFormat(info.format, isLinear);
    if (internalFormat == 0 || handle == invalid) {
        return false;
    }

    bind();
    m_gl->pixelStorei(GL_UNPACK_ALIGNMENT, 1);
    m_gl->texParameteri(static_cast<GLenum>(target), GL_TEXTURE_BASE_LEVEL, 0);
    m_gl->texParameteri(static_cast<GLenum>(target), GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(info.levelCount - 1u));
    const GLint swizzle[4] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
    m_gl->texParameteriv(static_cast<GLenum>(target), GL_TEXTURE_SWIZZLE_RGBA, swizzle);

    size_t offset = 0u;
    for (uint32_t level = 0u; level < info.levelCount; ++level) {
        const uint32_t width = std::max(info.width >> level, 1u);
        const uint32_t height = std::max(info.height >> level, 1u);
        const uint64_t levelSize = imageformats::getKTX2CompressedLevelSize(info, level);
        if (levelSize == 0u
            || levelSize > static_cast<uint64_t>(std::numeric_limits<GLsizei>::max())
            || offset + levelSize > image.getPixels().size()) {
            core::logWarning("KTX2 vkFormat %u: mip level %u exceeds the compressed image data", static_cast<uint32_t>(info.format), level);
            return false;
        }

        m_gl->compressedTexImage2D(
            static_cast<GLenum>(target),
            static_cast<GLint>(level),
            internalFormat,
            static_cast<GLsizei>(width),
            static_cast<GLsizei>(height),
            0,
            static_cast<GLsizei>(levelSize),
            image.getPixels().data() + offset);
        offset += static_cast<size_t>(levelSize);
    }

    return checkGLError(m_gl, "Cannot write compressed texture");
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
        if (pixelFormat != ComponentType::UByte && pixelFormat != ComponentType::UShort) {
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
