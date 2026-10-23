// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "filesystem/Path.h"
#include "renderer/gl/GL.h"
#include "renderer/gl/ShaderCache.h"

namespace ux3d::renderer {
struct InitizalizationParameters {
    // Upper bound for the specular, sheen and source cube faces; the panorama may lower it.
    uint32_t textureSize = 1024u;
    // Lambertian irradiance cube face size; 0 uses textureSize.
    uint32_t lambertianSize = 0u;
    uint32_t lutResolution = 1024u;
    uint32_t lowestMipLevel = 4u;
    // Store the filtered cubes and LUTs as RGBA16F instead of RGBA32F.
    bool halfFloat = false;

    bool operator==(const InitizalizationParameters& rhs) const = default;
};

struct SamplingParameters {
    uint32_t lambertianSampleCount = 2048u;
    uint32_t ggxSampleCount = 1024u;
    uint32_t sheenSampleCount = 64u;
    float lodBias = 0.0f;

    bool operator==(const SamplingParameters& rhs) const = default;
};

struct SamplerStatus {
    bool isInitialized = false;
    bool isFiltered = false;
    bool hasFailed = false;
    std::string filteredEnvironment = "";
};

class IBLSampler {
public:
    IBLSampler(gl::GL* gl, const filesystem::Path& _shaderPath);
    ~IBLSampler();
    void setSamplingParamters(const SamplingParameters& _samplingParameters);

    void init(const filesystem::Path& _panoramaImage, const InitizalizationParameters& initizalizationParameters = InitizalizationParameters());
    bool filter();

    // IBL textures for use in rendering (only available after init and filter)
    GLuint lambertianTextureID = 0;
    GLuint ggxTextureID = 0;
    GLuint sheenTextureID = 0;
    GLuint ggxLUTTextureID = 0;
    GLuint sheenLUTTextureID = 0;
    GLuint charlieLUTTextureID = 0;

    uint32_t getMipmapLevels() const;

private:
    GLuint createCubemapTexture(bool _withMipmaps, uint32_t _size, GLint _internalFormat);
    static GLuint createLutTexture(gl::GL* gl, size_t width, size_t height, GLint internalFormat);
    uint32_t lambertianSize() const;
    GLint outputFormat() const;
    void setOutputClamp(gl::Program* shaderProgram);
    void sampleLut(uint32_t _distribution, uint32_t _textureLayer, GLsizei _currentTextureSize);
    bool panoramaToCubeMap();
    GLuint loadTextureHDR(const filesystem::Path& _panoramaImagePath);
    void applyFilter(uint32_t _distribution, float _roughness, uint32_t _mipLevel, GLuint _targetTexture, uint32_t _sampleCount, float _lodBias = 0.0f);

    void cubeMapToLambertian();
    void cubeMapToGGX();
    void cubeMapToSheen();

    void destroyResources();
    void destroyRuntimeResources();

    gl::GL* m_gl = nullptr;
    gl::ShaderCache m_shaderCache;

    GLuint panoramaTextureID = 0;
    GLuint cubemapTextureID = 0;
    GLuint framebuffer = 0;

    SamplerStatus m_samplerStatus;
    InitizalizationParameters m_initizalizationParameters;
    SamplingParameters m_samplingParameters;
};
}
