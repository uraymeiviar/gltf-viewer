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

#include <algorithm>
#include <cmath>
#include <vector>

using namespace ux3d;
using namespace ux3d::renderer;

namespace {

constexpr double kPi = 3.14159265358979323846;

double texelLuminance(const float* texel)
{
    return 0.2126 * texel[0] + 0.7152 * texel[1] + 0.0722 * texel[2];
}

// Cube-map direction of panorama texel (column, row). ibl_panorama_to_cubemap.frag samples the panorama at
// u = 0.5 + atan(z, x) / (2 pi), v = acos(y) / pi for the cube direction (x, y, z) once the render-to-face
// orientation is taken into account (its face table is written with t pointing up, the cube faces store t
// pointing down, so the shader's y is the cube's -y); texture row 0 (v = 0, the first decoded row, the top
// of the file) is straight up.
glm::dvec3 panoramaDirection(double column, double row, uint32_t width, uint32_t height)
{
    const double theta = (row + 0.5) / height * kPi;
    const double phi = ((column + 0.5) / width - 0.5) * 2.0 * kPi;
    return glm::dvec3(std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi));
}

// A3.1: finds the panorama's sun (see EnvironmentSun in IBLSampler.h for the rule) and, when `removed` is
// not null, writes the panorama with the sun region scaled down to the threshold into it.
EnvironmentSun detectEnvironmentSun(const float* pixels, uint32_t width, uint32_t height, uint32_t channels,
                                    std::vector<float>* removed)
{
    EnvironmentSun sun;
    if (pixels == nullptr || width < 8 || height < 4 || channels < 3) {
        return sun;
    }
    const size_t texelCount = static_cast<size_t>(width) * height;
    std::vector<float> luminance(texelCount);
    for (size_t i = 0; i < texelCount; ++i) {
        const double y = texelLuminance(pixels + i * channels);
        luminance[i] = std::isfinite(y) && y > 0.0 ? static_cast<float>(y) : 0.0f;
    }

    // Seed: the brightest block of about one degree (mean luminance), so a single hot texel does not win.
    const uint32_t block = std::max(1u, width / 360u);
    double bestMean = -1.0;
    uint32_t seedColumn = 0, seedRow = 0;
    for (uint32_t by = 0; by < height; by += block) {
        for (uint32_t bx = 0; bx < width; bx += block) {
            double sum = 0.0;
            uint32_t count = 0;
            for (uint32_t y = by; y < std::min(height, by + block); ++y) {
                for (uint32_t x = bx; x < std::min(width, bx + block); ++x) {
                    sum += luminance[static_cast<size_t>(y) * width + x];
                    ++count;
                }
            }
            const double mean = count > 0 ? sum / count : 0.0;
            if (mean > bestMean) {
                bestMean = mean;
                seedColumn = bx + std::min(block, width - bx) / 2;
                seedRow = by + std::min(block, height - by) / 2;
            }
        }
    }
    const glm::dvec3 seed = panoramaDirection(seedColumn, seedRow, width, height);

    // Rows that can lie within 12 degrees of the seed (all columns are tested by angle).
    const double rowsPerDegree = height / 180.0;
    const int rowMin = std::max(0, static_cast<int>(std::floor(seedRow - 13.0 * rowsPerDegree)));
    const int rowMax = std::min(static_cast<int>(height) - 1, static_cast<int>(std::ceil(seedRow + 13.0 * rowsPerDegree)));
    const double cos2 = std::cos(2.0 * kPi / 180.0), cos5 = std::cos(5.0 * kPi / 180.0);
    const double cos10 = std::cos(10.0 * kPi / 180.0), cos12 = std::cos(12.0 * kPi / 180.0);

    std::vector<float> ring;
    for (int y = rowMin; y <= rowMax; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const double c = glm::dot(panoramaDirection(x, y, width, height), seed);
            if (c <= cos5 && c >= cos10) {
                ring.push_back(luminance[static_cast<size_t>(y) * width + x]);
            }
        }
    }
    if (ring.empty()) {
        return sun;
    }
    std::nth_element(ring.begin(), ring.begin() + ring.size() / 2, ring.end());
    const double sky = ring[ring.size() / 2];
    const double threshold = 4.0 * sky;
    sun.skyLevel = static_cast<float>(sky);
    sun.threshold = static_cast<float>(threshold);
    if (!(threshold > 0.0)) {
        return sun;
    }

    // Region: candidates (brighter than T, within 12 degrees) 8-connected to a candidate within 2 degrees.
    // The window spans rows rowMin..rowMax and every column (columns wrap).
    const int windowRows = rowMax - rowMin + 1;
    std::vector<uint8_t> state(static_cast<size_t>(windowRows) * width, 0); // 1 candidate, 2 region
    std::vector<size_t> stack;
    for (int y = rowMin; y <= rowMax; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const size_t w = static_cast<size_t>(y - rowMin) * width + x;
            if (luminance[static_cast<size_t>(y) * width + x] <= threshold) {
                continue;
            }
            const double c = glm::dot(panoramaDirection(x, y, width, height), seed);
            if (c < cos12) {
                continue;
            }
            state[w] = 1;
            if (c >= cos2) {
                state[w] = 2;
                stack.push_back(w);
            }
        }
    }
    while (!stack.empty()) {
        const size_t w = stack.back();
        stack.pop_back();
        const int wy = static_cast<int>(w / width);
        const int wx = static_cast<int>(w % width);
        for (int dy = -1; dy <= 1; ++dy) {
            const int ny = wy + dy;
            if (ny < 0 || ny >= windowRows) {
                continue;
            }
            for (int dx = -1; dx <= 1; ++dx) {
                const int nx = (wx + dx + static_cast<int>(width)) % static_cast<int>(width);
                const size_t n = static_cast<size_t>(ny) * width + nx;
                if (state[n] == 1) {
                    state[n] = 2;
                    stack.push_back(n);
                }
            }
        }
    }

    // Excess over the clamped panorama: the sun's illuminance, colour and direction.
    const double texelArea = (2.0 * kPi / width) * (kPi / height);
    glm::dvec3 energy(0.0), weightedDirection(0.0);
    double solidAngle = 0.0, peak = 0.0;
    uint32_t count = 0;
    if (removed != nullptr) {
        removed->assign(pixels, pixels + texelCount * channels);
    }
    for (int y = rowMin; y <= rowMax; ++y) {
        const double theta = (y + 0.5) / height * kPi;
        const double dOmega = texelArea * std::sin(theta);
        for (uint32_t x = 0; x < width; ++x) {
            if (state[static_cast<size_t>(y - rowMin) * width + x] != 2) {
                continue;
            }
            const size_t i = static_cast<size_t>(y) * width + x;
            const double lum = luminance[i];
            const double scale = threshold / lum;
            const float* texel = pixels + i * channels;
            const glm::dvec3 excess = glm::dvec3(texel[0], texel[1], texel[2]) * (1.0 - scale);
            energy += excess * dOmega;
            weightedDirection += panoramaDirection(x, y, width, height) * ((lum - threshold) * dOmega);
            solidAngle += dOmega;
            peak = std::max(peak, lum);
            ++count;
            if (removed != nullptr) {
                for (uint32_t ch = 0; ch < 3; ++ch) {
                    (*removed)[i * channels + ch] = static_cast<float>(texel[ch] * scale);
                }
            }
        }
    }
    const double illuminance = 0.2126 * energy.r + 0.7152 * energy.g + 0.0722 * energy.b;
    if (count == 0 || !(illuminance > 0.0) || glm::length(weightedDirection) <= 0.0) {
        if (removed != nullptr) {
            removed->clear();
        }
        return sun;
    }
    sun.found = true;
    sun.direction = glm::vec3(glm::normalize(weightedDirection));
    sun.color = glm::vec3(energy / illuminance);
    sun.illuminance = static_cast<float>(illuminance);
    sun.peakLuminance = static_cast<float>(peak);
    sun.solidAngle = static_cast<float>(solidAngle);
    sun.texelCount = count;
    sun.removed = removed != nullptr;
    return sun;
}

} // namespace

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
    // Always rebuilds: the caller has already released the EnvironmentMap that owned the
    // previous textures.
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

    // The source cube keeps full precision; it is released after filtering.
    cubemapTextureID = createCubemapTexture(true, m_initizalizationParameters.textureSize, GL_RGBA32F);
    if (cubemapTextureID == 0) {
        m_samplerStatus.hasFailed = true;
        return;
    }

    m_gl->genFramebuffers(1, &framebuffer);

    const GLint format = outputFormat();
    const uint32_t lutSize = m_initizalizationParameters.lutResolution;
    lambertianTextureID = createCubemapTexture(false, lambertianSize(), format);
    ggxTextureID = createCubemapTexture(true, m_initizalizationParameters.textureSize, format);
    sheenTextureID = createCubemapTexture(true, m_initizalizationParameters.textureSize, format);
    ggxLUTTextureID = createLutTexture(m_gl, lutSize, lutSize, format);
    sheenLUTTextureID = createLutTexture(m_gl, lutSize, lutSize, format);
    charlieLUTTextureID = createLutTexture(m_gl, lutSize, lutSize, format);

    if (!gl::checkGLError(m_gl, "IBLSampler::init: Error")) {
        m_samplerStatus.hasFailed = true;
        return;
    }

    m_samplerStatus.isInitialized = true;
    m_samplerStatus.filteredEnvironment = _panoramaImage.toString();
    m_samplerStatus.isFiltered = false;
    m_samplerStatus.hasFailed = false;

    core::logInfo("IBLSampler: initialized with %s (cube %u, irradiance %u, LUT %u, %s)", _panoramaImage.toString().c_str(),
        m_initizalizationParameters.textureSize, lambertianSize(), m_initizalizationParameters.lutResolution,
        m_initizalizationParameters.halfFloat ? "RGBA16F" : "RGBA32F");
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

    // P7.1: no glFinish. The filter passes are queued in order ahead of any frame that
    // samples their outputs, and GL defers deleting the source textures until the queued
    // passes no longer use them.
    destroyRuntimeResources();

    m_samplerStatus.isFiltered = true;
    core::logInfo("IBLSampler: environment filtered successfully");
    return true;
}

uint32_t IBLSampler::getMipmapLevels() const
{
    return std::floor(std::log2(m_initizalizationParameters.textureSize)) + 1 - m_initizalizationParameters.lowestMipLevel;
}

uint32_t IBLSampler::lambertianSize() const
{
    const uint32_t size = m_initizalizationParameters.lambertianSize;
    return size == 0u ? m_initizalizationParameters.textureSize : std::min(size, m_initizalizationParameters.textureSize);
}

GLint IBLSampler::outputFormat() const
{
    return m_initizalizationParameters.halfFloat ? GL_RGBA16F : GL_RGBA32F;
}

void IBLSampler::setOutputClamp(gl::Program* shaderProgram)
{
    // Half floats overflow to infinity above 65504; clamp bright HDR highlights instead.
    const GLfloat maxValue = m_initizalizationParameters.halfFloat ? 65504.0f : 3.0e38f;
    const GLint location = m_gl->getUniformLocation(shaderProgram->getHandle(), "u_maxValue");
    m_gl->uniform1fv(location, 1, &maxValue);
}

GLuint IBLSampler::createCubemapTexture(bool _withMipmaps, uint32_t _size, GLint _internalFormat)
{
    GLuint targetTexture;
    m_gl->genTextures(1, &targetTexture);
    m_gl->bindTexture(GL_TEXTURE_CUBE_MAP, targetTexture); // GLenum target, GLuint texture

    const GLint maxMipLevels = _withMipmaps ? std::floor(std::log2(_size)) + 1 : 0;

    // define size and format of level 0
    const GLint internalFormat = _internalFormat;
    const GLint border = 0;
    const GLenum format = GL_RGBA;
    const GLenum type = GL_FLOAT;
    const void* data = nullptr;

    for (GLint level = 0; level <= maxMipLevels; ++level) {
        GLint currentTextureSize = std::max<GLint>(1, _size >> level);
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

GLuint IBLSampler::createLutTexture(gl::GL* gl, size_t width, size_t height, GLint internalFormat)
{
    GLuint texture;
    gl->genTextures(1, &texture);

    gl->bindTexture(GL_TEXTURE_2D, texture);
    gl->texImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);

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

    setOutputClamp(shaderProgram);

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

    // A3.1: find the panorama's sun (always; it only reads) and remove it when requested.
    m_environmentSun = EnvironmentSun();
    std::vector<float> sunRemovedPixels;
    const uint32_t panoramaChannels = panoramaImage.getChannelCount();
    const bool floatPixels = panoramaImage.getFormat() == image::Format::FLOAT &&
                             (panoramaChannels == 3 || panoramaChannels == 4) &&
                             panoramaImage.getPixelSize() == panoramaChannels * sizeof(float) &&
                             panoramaImage.getPixels().size() >= static_cast<size_t>(panoramaImage.getWidth()) *
                                 panoramaImage.getHeight() * panoramaImage.getPixelSize();
    if (floatPixels) {
        m_environmentSun = detectEnvironmentSun(reinterpret_cast<const float*>(panoramaImage.getPixels().data()),
                                                panoramaImage.getWidth(), panoramaImage.getHeight(), panoramaChannels,
                                                m_sunRemoval ? &sunRemovedPixels : nullptr);
        // Logged only while the sun is in use, so runs without a sun keep their previous log.
        if (m_environmentSun.found && m_sunRemoval) {
            const glm::vec3& d = m_environmentSun.direction;
            core::logInfo("IBLSampler: sun found (cube direction %.4f %.4f %.4f, illuminance %.5g, colour %.3f %.3f %.3f, "
                          "%u texels, sky %.4g, threshold %.4g, peak %.4g)%s",
                d.x, d.y, d.z, m_environmentSun.illuminance, m_environmentSun.color.r, m_environmentSun.color.g,
                m_environmentSun.color.b, m_environmentSun.texelCount, m_environmentSun.skyLevel,
                m_environmentSun.threshold, m_environmentSun.peakLuminance, m_environmentSun.removed ? ", removed" : "");
        } else if (m_sunRemoval) {
            core::logInfo("IBLSampler: no sun found in the panorama");
        }
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
    const void* data = sunRemovedPixels.empty() ? static_cast<const void*>(panoramaImage.getPixels().data())
                                                : static_cast<const void*>(sunRemovedPixels.data());
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
    const uint32_t targetSize = _targetTexture == lambertianTextureID ? lambertianSize() : m_initizalizationParameters.textureSize;
    uint32_t currentTextreSize = targetSize >> _mipLevel;
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

        setOutputClamp(shaderProgram);

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
