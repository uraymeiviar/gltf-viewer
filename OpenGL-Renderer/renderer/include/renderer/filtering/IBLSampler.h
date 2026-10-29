// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "filesystem/Path.h"
#include "renderer/gl/GL.h"
#include "renderer/gl/ShaderCache.h"

#include "math/MatrixFunctions.h"

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

// A3.1: the sun found in the HDR panorama. Detection runs on every panorama load and only reads; the sun
// region is removed from the panorama (before the cube map, the IBL filters and the background) only when
// sun removal is requested. Rule (docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.1):
//  * seed: the brightest 1-degree block of the panorama (block-averaged luminance, so one hot texel does not
//    win); sky level B: the median luminance of the ring 5..10 degrees around the seed;
//  * region: the texels brighter than T = 4 B within 12 degrees of the seed that are 8-connected to a texel
//    brighter than T within 2 degrees of the seed;
//  * removal: every region texel is scaled to luminance T, keeping its hue (continuous at the region edge);
//  * the sun is what removal takes away: illuminance E = sum((L - L') dw) (luminance of the RGB sum; dw the
//    texel solid angle), colour = that RGB sum / E (luminance 1), direction = the E-weighted mean direction.
// Directions are in cube-map space, the space the shaders sample with u_EnvRotation * worldDirection, so a
// world direction is transpose(u_EnvRotation) * direction. Luminance is Rec. 709 (0.2126, 0.7152, 0.0722).
struct EnvironmentSun {
    bool found = false;
    glm::vec3 direction = glm::vec3(0.0f, 1.0f, 0.0f); // unit, cube-map space, towards the sun
    glm::vec3 color = glm::vec3(1.0f);                 // linear RGB, luminance 1
    float illuminance = 0.0f;                          // HDR radiance units x sr
    float skyLevel = 0.0f;                             // B, luminance
    float threshold = 0.0f;                            // T, luminance
    float peakLuminance = 0.0f;                        // brightest region texel
    float solidAngle = 0.0f;                           // sr covered by the region
    uint32_t texelCount = 0;
    bool removed = false;                              // the region was removed from this load's panorama
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
    // A6.2: frees every texture (the filtered cubes, LUTs and the panorama) while another environment is in use; the
    // next init loads again.
    void release();

    // IBL textures for use in rendering (only available after init and filter)
    GLuint lambertianTextureID = 0;
    GLuint ggxTextureID = 0;
    GLuint sheenTextureID = 0;
    GLuint ggxLUTTextureID = 0;
    GLuint sheenLUTTextureID = 0;
    GLuint charlieLUTTextureID = 0;

    uint32_t getMipmapLevels() const;

    // A3.1: remove the panorama's sun on the next init (it changes the filtered environment and the
    // background, so the caller re-inits and re-filters after changing it).
    void setSunRemoval(bool remove) { m_sunRemoval = remove; }
    bool getSunRemoval() const { return m_sunRemoval; }
    // The sun found in the panorama of the last init (EnvironmentSun.removed says whether it was removed).
    const EnvironmentSun& getEnvironmentSun() const { return m_environmentSun; }
    // A3.10: cached horizontal irradiance of the filtered HDR (Lambertian cube at +Y, times pi).
    const glm::vec3& horizontalIlluminance() const { return m_horizontalIlluminance; }

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
    bool m_sunRemoval = false;
    EnvironmentSun m_environmentSun;
    glm::vec3 m_horizontalIlluminance = glm::vec3(0.0f);
};
}
