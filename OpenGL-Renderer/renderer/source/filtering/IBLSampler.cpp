// Copyright © 2021 UX3D GmbH. All rights reserved.

#include "renderer/filtering/IBLSampler.h"
#include "core/Logger.h"
#include "filesystem/Dir.h"
#include "filesystem/File.h"
#include "image/Image.h"
#include "imageformats/imageformats.h"
#include "renderer/gl/ShaderCache.h"
#include "renderer/gl/Util.h"
#include "renderer/gltf/GltfResourceManager.h"

#include <cmath>

using namespace ux3d;
using namespace ux3d::renderer;

IBLSampler::IBLSampler(gl::GL* gl, const filesystem::Path& _shaderPath)
    : m_shaderCache(gl, _shaderPath)
{
    m_gl = gl;
}

IBLSampler::~IBLSampler()
{
    destroyRuntimeResources();
    destroyResources();
}

void IBLSampler::setSamplingParamters(const SamplingParameters& _samplingParameters)
{
    if (m_samplingParameters == _samplingParameters) {
        return;
    }
    m_samplingParameters = _samplingParameters;
    m_samplerStatus.isFiltered = false;
}

void IBLSampler::init(const filesystem::Path& _panoramaImage, const InitizalizationParameters& _initizalizationParameters)
{
    if (m_initizalizationParameters == _initizalizationParameters && m_samplerStatus.filteredEnvironment == _panoramaImage.toString() && (m_samplerStatus.isInitialized || m_samplerStatus.isFiltered)) {
        return; // no need to recreate resources
    }

    if (!gl::checkGLError(m_gl, "IBLSampler::init error")) {
        // Clear GL Error log as the sampler is relying on error reports
        // If we get an error here, it comes from a different system
        return;
    }

    if (m_samplerStatus.isInitialized) {
        destroyRuntimeResources();
        destroyResources();
    }

    m_initizalizationParameters = _initizalizationParameters;

    panoramaTextureID = loadTextureHDR(_panoramaImage);
    if (panoramaTextureID == 0) {
        m_samplerStatus.hasFailed = true;
        return;
    }

    cubemapTextureID = createCubemapTexture(true);
    if (cubemapTextureID == 0) {
        m_samplerStatus.hasFailed = true;
        return;
    }

    m_gl->genFramebuffers(1, &framebuffer);

    lambertianTextureID = createCubemapTexture(false);
    ggxTextureID = createCubemapTexture(true);
    sheenTextureID = createCubemapTexture(true);
    ggxLUTTextureID = createLutTexture(m_gl, m_initizalizationParameters.lutResolution, m_initizalizationParameters.lutResolution);
    sheenLUTTextureID = createLutTexture(m_gl, m_initizalizationParameters.lutResolution, m_initizalizationParameters.lutResolution);
    charlieLUTTextureID = createLutTexture(m_gl, m_initizalizationParameters.lutResolution, m_initizalizationParameters.lutResolution);

    if (!gl::checkGLError(m_gl, "IBLSampler::init: Error")) {
        m_samplerStatus.hasFailed = true;
        return;
    }

    m_samplerStatus.isInitialized = true;
    m_samplerStatus.filteredEnvironment = _panoramaImage.toString();
    m_samplerStatus.isFiltered = false;
    m_samplerStatus.hasFailed = false;

    core::logInfo("IBLSampler: initialized with %s", _panoramaImage.toString().c_str());
}

void IBLSampler::destroyRuntimeResources()
{
    if (panoramaTextureID != 0) {
        m_gl->deleteTextures(1, &panoramaTextureID);
        panoramaTextureID = 0;
    }
    if (cubemapTextureID != 0) {
        m_gl->deleteTextures(1, &cubemapTextureID);
        cubemapTextureID = 0;
    }
    if (framebuffer != 0) {
        m_gl->deleteFramebuffers(1, &framebuffer);
        framebuffer = 0;
    }
}

void IBLSampler::destroyResources()
{

    if (lambertianTextureID != 0) {
        m_gl->deleteTextures(1, &lambertianTextureID);
        lambertianTextureID = 0;
    }
    if (ggxTextureID != 0) {
        m_gl->deleteTextures(1, &ggxTextureID);
        ggxTextureID = 0;
    }
    if (sheenTextureID != 0) {
        m_gl->deleteTextures(1, &sheenTextureID);
        sheenTextureID = 0;
    }
    if (ggxLUTTextureID != 0) {
        m_gl->deleteTextures(1, &ggxLUTTextureID);
        ggxLUTTextureID = 0;
    }
    if (charlieLUTTextureID != 0) {
        m_gl->deleteTextures(1, &charlieLUTTextureID);
        charlieLUTTextureID = 0;
    }
    if (sheenLUTTextureID != 0) {
        m_gl->deleteTextures(1, &sheenLUTTextureID);
        sheenLUTTextureID = 0;
    }
}

// in the filter step all the draw calls are executed
bool IBLSampler::filter()
{
    if (m_samplerStatus.isFiltered) {
        return true; // cache is still valid
    }
    if (m_samplerStatus.hasFailed) {
        return false;
    }
    if (!m_samplerStatus.isInitialized) {
        core::logError("IBLSampler: sampler is not initialized");
        return false;
    }
    if (panoramaTextureID == 0) {
        core::logError("IBLSampler: no panorama texture set");
        return false;
    }

    if (!panoramaToCubeMap()) {
        return false;
    }

    sampleLut(1, ggxLUTTextureID, m_initizalizationParameters.lutResolution);
    sampleLut(2, charlieLUTTextureID, m_initizalizationParameters.lutResolution);

    cubeMapToLambertian();
    cubeMapToGGX();
    cubeMapToSheen();

    m_gl->finish();

    destroyRuntimeResources();

    m_samplerStatus.isFiltered = true;
    core::logInfo("IBLSampler: environment filtered successfully");
    return true;
}

uint32_t IBLSampler::getMipmapLevels() const
{
    return std::floor(std::log2(m_initizalizationParameters.textureSize)) + 1 - m_initizalizationParameters.lowestMipLevel;
}

GLuint IBLSampler::createCubemapTexture(bool _withMipmaps)
{
    GLuint targetTexture;
    m_gl->genTextures(1, &targetTexture);
    m_gl->bindTexture(GL_TEXTURE_CUBE_MAP, targetTexture); // GLenum target, GLuint texture

    const GLint maxMipLevels = _withMipmaps ? std::floor(std::log2(m_initizalizationParameters.textureSize)) + 1 : 0;

    // define size and format of level 0
    const GLint level = 0;
    const GLint internalFormat = GL_RGBA32F;
    const GLint border = 0;
    const GLenum format = GL_RGBA;
    const GLenum type = GL_FLOAT;
    const void* data = nullptr;

    for (GLint level = 0; level <= maxMipLevels; ++level) {
        GLint currentTextureSize = m_initizalizationParameters.textureSize >> level;
        for (uint32_t i = 0; i < 6; ++i) {
            m_gl->texImage2D(
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                level,
                internalFormat,
                currentTextureSize,
                currentTextureSize,
                border,
                format,
                type,
                data);
        }
    }

    m_gl->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    m_gl->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, maxMipLevels);

    if (_withMipmaps) {
        m_gl->texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        m_gl->texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }

    m_gl->texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    m_gl->texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    m_gl->texParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (!gl::checkGLError(m_gl, "IBLSampler: Could not generate texture")) {
        return 0;
    }

    return targetTexture;
}

GLuint IBLSampler::createLutTexture(gl::GL* gl, size_t width, size_t height)
{
    GLuint texture;
    gl->genTextures(1, &texture);

    gl->bindTexture(GL_TEXTURE_2D, texture);
    gl->texImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);

    gl->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return texture;
}

void IBLSampler::sampleLut(uint32_t _distribution, uint32_t textureID, GLsizei _currentTextureSize)
{
    m_gl->bindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    m_gl->framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureID, 0);

    m_gl->bindTexture(GL_TEXTURE_2D, textureID);

    m_gl->viewport(0, 0, _currentTextureSize, _currentTextureSize);

    GLuint vertexShaderID = m_shaderCache.selectShader("ibl_fullscreen.vert", {});
    GLuint fragmentShaderID = m_shaderCache.selectShader("ibl_filtering.frag", {});

    gl::Program* shaderProgram = m_shaderCache.getProgram(vertexShaderID, fragmentShaderID);

    if (shaderProgram == nullptr) {
        return;
    }
    shaderProgram->use();

    const GLint location = m_gl->getUniformLocation(shaderProgram->getHandle(), "u_cubemapTexture");
    m_gl->uniform1i(location, 0); // texture unit 0 (TEXTURE0)

    GLfloat roughness = 0.0f;
    auto roughnessLocation = m_gl->getUniformLocation(shaderProgram->getHandle(), "u_roughness");
    m_gl->uniform1fv(roughnessLocation, 1, &roughness);

    auto sampleCountLocation = m_gl->getUniformLocation(shaderProgram->getHandle(), "u_sampleCount");
    m_gl->uniform1i(sampleCountLocation, 512);

    auto widthLocation = m_gl->getUniformLocation(shaderProgram->getHandle(), "u_width");
    m_gl->uniform1i(widthLocation, m_initizalizationParameters.textureSize);

    auto lodBiasLocation = m_gl->getUniformLocation(shaderProgram->getHandle(), "u_lodBias");
    m_gl->uniform1fv(lodBiasLocation, 1, &m_samplingParameters.lodBias);

    auto distributionLocation = m_gl->getUniformLocation(shaderProgram->getHandle(), "u_distribution");
    m_gl->uniform1i(distributionLocation, _distribution);

    auto currentFaceLocation = m_gl->getUniformLocation(shaderProgram->getHandle(), "u_currentFace");
    m_gl->uniform1i(currentFaceLocation, 0);

    auto isGeneratingLutLocation = m_gl->getUniformLocation(shaderProgram->getHandle(), "u_isGeneratingLUT");
    m_gl->uniform1i(isGeneratingLutLocation, 1);

    GLuint vao;
    m_gl->genVertexArrays(1, &vao);
    m_gl->bindVertexArray(vao);

    m_gl->drawArrays(GL_TRIANGLES, 0, 3); //fullscreen triangle

    m_gl->deleteVertexArrays(1, &vao);
    gl::checkGLError(m_gl, "IBLSampler: unable to generate LUT");
}

GLuint IBLSampler::loadTextureHDR(const filesystem::Path& _panoramaImagePath)
{
    filesystem::File inputHdrFile = _panoramaImagePath;
    core::Vector<uint8_t> hdrFileData;
    inputHdrFile.load(hdrFileData);

    image::Image panoramaImage;

    if (core::Failed(imageformats::decodeImage(hdrFileData, panoramaImage))) {
        core::logError("Decoding of HDR/EXR environment map failed: %s", _panoramaImagePath.toString().c_str());
        return 0;
    }

    if (!gl::checkGLError(m_gl, "IBLSampler: failed")) {
        return 0;
    }
    GLuint targetTexture;
    m_gl->genTextures(1, &targetTexture);
    if (!gl::checkGLError(m_gl, "IBLSampler: genTextures failed")) {
        return 0;
    }

    m_gl->bindTexture(GL_TEXTURE_2D, targetTexture); // GLenum target, GLuint texture
    if (!gl::checkGLError(m_gl, "IBLSampler: could not generate panorama gl texture")) {
        core::logError("Failed %s", _panoramaImagePath.toString().c_str());
        return 0;
    }
    // fill texture data
    const GLint level = 0;
    const GLint border = 0;
    const GLenum type = GL_FLOAT;
    GLint internalFormat = GL_RGB32F;
    GLenum format = GL_RGB;
    if (panoramaImage.getChannelCount() == 4) {
        internalFormat = GL_RGBA32F;
        format = GL_RGBA;
    }
    const void* data = panoramaImage.getPixels().data();
    GLsizei width = panoramaImage.getWidth();
    GLsizei height = panoramaImage.getHeight();

    m_gl->texImage2D(
        GL_TEXTURE_2D,
        level,
        internalFormat,
        width,
        height,
        border,
        format,
        type,
        data);

    if (!gl::checkGLError(m_gl, "IBLSampler: texImage2D")) {
        return 0;
    }

    m_gl->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
    m_gl->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
    m_gl->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    m_gl->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    const uint32_t maxTextureSize = m_initizalizationParameters.textureSize;
    const uint32_t minTextureSize = std::pow(2, m_initizalizationParameters.lowestMipLevel + 1);
    const uint32_t hdrTextureSize = std::max(panoramaImage.getWidth(), panoramaImage.getHeight()) / 4;

    m_initizalizationParameters.textureSize = std::min(hdrTextureSize, maxTextureSize);
    m_initizalizationParameters.textureSize = std::max(m_initizalizationParameters.textureSize, minTextureSize);

    return targetTexture;
}

bool IBLSampler::panoramaToCubeMap()
{
    for (uint32_t i = 0; i < 6; ++i) {
        m_gl->bindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        if (!gl::checkGLError(m_gl, "IBLSampler::panoramaToCubeMap: bindFramebuffer")) {
            m_gl->bindFramebuffer(GL_FRAMEBUFFER, 0); // reset
            return false;
        }

        uint32_t side = i;
        m_gl->framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + side, cubemapTextureID, 0);
        GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
        m_gl->drawBuffers(1, DrawBuffers); // "1" is the size of DrawBuffers

        if (m_gl->checkFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            renderer::gl::checkGLError(m_gl, "IBLSampler::checkFramebufferStatus: error");
            return false;
        }

        m_gl->bindTexture(GL_TEXTURE_CUBE_MAP, cubemapTextureID);
        m_gl->bindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        if (!gl::checkGLError(m_gl, "IBLSampler::panoramaToCubeMap: bindFramebuffer")) {
            return false;
        }

        m_gl->viewport(0, 0, m_initizalizationParameters.textureSize, m_initizalizationParameters.textureSize);

        if (!gl::checkGLError(m_gl, "IBLSampler::panoramaToCubeMap: viewport")) {
            return false;
        }

        GLuint vertexShaderID = m_shaderCache.selectShader("ibl_fullscreen.vert", {});
        if (vertexShaderID == gl::invalid) {
            core::logError("IBLSampler: invalid vertex shader");
            return false;
        }

        GLuint fragmentShaderID = m_shaderCache.selectShader("ibl_panorama_to_cubemap.frag", {});
        if (fragmentShaderID == gl::invalid) {
            core::logError("IBLSampler: invalid fragment shader");
            return false;
        }

        gl::Program* shaderProgram = m_shaderCache.getProgram(vertexShaderID, fragmentShaderID);

        if (shaderProgram == nullptr) {
            return false;
        }

        m_gl->useProgram(shaderProgram->getHandle());

        //  TEXTURE0 = active.
        m_gl->activeTexture(GL_TEXTURE0 + 0);

        // Bind texture ID to active texture
        m_gl->bindTexture(GL_TEXTURE_2D, panoramaTextureID);

        // map shader uniform to texture unit (TEXTURE0)
        const GLint location = m_gl->getUniformLocation(shaderProgram->getHandle(), "u_panorama");
        m_gl->uniform1i(location, 0); // texture unit 0 (TEXTURE0)

        const GLint currentFaceLocation = m_gl->getUniformLocation(shaderProgram->getHandle(), "u_currentFace");
        m_gl->uniform1i(currentFaceLocation, i);

        GLuint vao;
        m_gl->genVertexArrays(1, &vao);
        m_gl->bindVertexArray(vao);

        m_gl->drawArrays(GL_TRIANGLES, 0, 3); //fullscreen triangle

        m_gl->deleteVertexArrays(1, &vao);
    }

    m_gl->bindTexture(GL_TEXTURE_CUBE_MAP, cubemapTextureID);
    m_gl->generateMipmap(GL_TEXTURE_CUBE_MAP);

    m_gl->bindFramebuffer(GL_FRAMEBUFFER, 0);

    renderer::gl::checkGLError(m_gl, "IBLSampler::panoramaToCubeMap: Error");
    return true;
}

void IBLSampler::applyFilter(uint32_t _distribution, float _roughness, uint32_t _mipLevel, GLuint _targetTexture, uint32_t _sampleCount, float _lodBias)
{
    uint32_t currentTextreSize = m_initizalizationParameters.textureSize >> _mipLevel;
    for (uint32_t sideIdx = 0u; sideIdx < 6; ++sideIdx) {
        m_gl->bindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        m_gl->framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + sideIdx, _targetTexture, _mipLevel);
        m_gl->bindTexture(GL_TEXTURE_CUBE_MAP, _targetTexture);
        m_gl->viewport(0, 0, currentTextreSize, currentTextreSize);

        auto clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
        m_gl->clearBufferfv(GL_COLOR, 0, clearColor.begin());
        const GLfloat depth = 1.0f;
        m_gl->clearBufferfv(GL_DEPTH, 0, &depth);

        GLuint vertexShaderID = m_shaderCache.selectShader("ibl_fullscreen.vert", {});
        if (vertexShaderID == gl::invalid) {
            core::logError("IBLSampler: invalid vertex shader");
            return;
        }
        GLuint fragmentShaderID = m_shaderCache.selectShader("ibl_filtering.frag", {});
        if (fragmentShaderID == gl::invalid) {
            core::logError("IBLSampler: invalid fragment shader");
            return;
        }
        gl::Program* shaderProgram = m_shaderCache.getProgram(vertexShaderID, fragmentShaderID);
        if (shaderProgram == nullptr) {
            return;
        }
        m_gl->validateProgram(shaderProgram->getHandle());
        m_gl->useProgram(shaderProgram->getHandle());

        m_gl->activeTexture(GL_TEXTURE0 + 0);
        m_gl->bindTexture(GL_TEXTURE_CUBE_MAP, cubemapTextureID);

        const GLint location = m_gl->getUniformLocation(shaderProgram->getHandle(), "u_cubemapTexture");
        m_gl->uniform1i(location, 0); // texture unit 0 (TEXTURE0)

        auto roughnessLocation = m_gl->getUniformLocation(shaderProgram->getHandle(), "u_roughness");
        m_gl->uniform1fv(roughnessLocation, 1, &_roughness);

        auto sampleCountLocation = m_gl->getUniformLocation(shaderProgram->getHandle(), "u_sampleCount");
        m_gl->uniform1i(sampleCountLocation, _sampleCount);

        auto widthLocation = m_gl->getUniformLocation(shaderProgram->getHandle(), "u_width");
        m_gl->uniform1i(widthLocation, m_initizalizationParameters.textureSize);

        auto lodBiasLocation = m_gl->getUniformLocation(shaderProgram->getHandle(), "u_lodBias");
        m_gl->uniform1fv(lodBiasLocation, 1, &m_samplingParameters.lodBias);

        auto distributionLocation = m_gl->getUniformLocation(shaderProgram->getHandle(), "u_distribution");
        m_gl->uniform1i(distributionLocation, _distribution);

        auto currentFaceLocation = m_gl->getUniformLocation(shaderProgram->getHandle(), "u_currentFace");
        m_gl->uniform1i(currentFaceLocation, sideIdx);

        auto isGeneratingLutLocation = m_gl->getUniformLocation(shaderProgram->getHandle(), "u_isGeneratingLUT");
        m_gl->uniform1i(isGeneratingLutLocation, 0);

        GLuint vao;
        m_gl->genVertexArrays(1, &vao);
        m_gl->bindVertexArray(vao);

        m_gl->drawArrays(GL_TRIANGLES, 0, 3); //fullscreen triangle

        m_gl->deleteVertexArrays(1, &vao);
        renderer::gl::checkGLError(m_gl, "IBLSampler::applyFilter: draw");
    }
}

void IBLSampler::cubeMapToLambertian()
{
    applyFilter(0, 0.0f, 0, lambertianTextureID, m_samplingParameters.lambertianSampleCount);
}

void IBLSampler::cubeMapToGGX()
{
    for (uint32_t currentMipLevel = 0u; currentMipLevel <= getMipmapLevels(); ++currentMipLevel) {
        const float roughness = static_cast<float>(currentMipLevel) / (getMipmapLevels() - 1u);
        applyFilter(1, roughness, currentMipLevel, ggxTextureID, m_samplingParameters.ggxSampleCount);
    }
}

void IBLSampler::cubeMapToSheen()
{
    for (uint32_t currentMipLevel = 0u; currentMipLevel <= getMipmapLevels(); ++currentMipLevel) {
        const float roughness = static_cast<float>(currentMipLevel) / (getMipmapLevels() - 1u);
        applyFilter(2, roughness, currentMipLevel, sheenTextureID, m_samplingParameters.sheenSampleCount);
    }
}
