// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/CommandGenerator.h"
#include "filesystem/Dir.h"
#include "math/MatrixFunctions.h"
#include "renderer/RendererScene.h"

#include <algorithm>
#include <cstring>

namespace {
bool isTransparentPass(const ux3d::renderer::Material& material)
{
    if (material.renderPass == nullptr) {
        return false;
    }
    const uint32_t passId = material.renderPass->getId();
    return passId == ux3d::renderer::RenderPassType::Transparent
        || passId == ux3d::renderer::RenderPassType::Transparent2;
}
}

ux3d::renderer::CommandGenerator::CommandGenerator(gl::GL* gl, const filesystem::Path& _shaderPath)
    : m_vertexLayoutCache(gl)
    , m_shaderCache(gl, _shaderPath)
    , m_commandMapping(gl, *this, m_shaderCache)
    , m_environmentMapRotationUniform("u_EnvRotation")
    , m_environmentMapBlurNormalizedUniform("u_EnvBlurNormalized")
    , m_environmentMapIntensityUniform("u_EnvIntensity")
    , m_exposureUniform("u_Exposure")
    , m_asoboNormalMapDirectXUniform("u_AsoboNormalMapDirectX")
    , m_asoboWindshieldVertexColorMaskUniform("u_AsoboWindshieldVertexColorMask")
    , m_asoboDecalReceiverUniform("u_AsoboDecalReceiver")
    , m_asoboGlassCopyValidUniform("u_AsoboGlassCopyValid")
    , m_decalDepthToleranceUniform("u_DecalDepthTolerance")
    , m_lights(gl)
{
    setEnvironmentMapRotation(0.0f);
    setEnvironmentMapBlurNormalized(0.0f);
    setEnvironmentMapIntensity(1.0f);
    setExposure(1.0f);
    setAsoboNormalMapConvention(false);
    setAsoboWindshieldVertexColorMask(false);
    setAsoboDecalReceiver(false);
    setDecalDepthTolerance(0.0012f);

    // A2.2: stable borrowed wrappers for the six accumulation targets. Nearest filtering
    // keeps texelFetch exact; clamp-to-edge is irrelevant for texelFetch but explicit.
    m_decalSamplerState = std::make_unique<gl::SamplerState>();
    m_decalSamplerState->minFilter = gl::TextureMinFilter::Nearest;
    m_decalSamplerState->magFilter = gl::TextureMagFilter::Nearest;
    m_decalSamplerState->wrapS = gl::TextureWrap::ClampToEdge;
    m_decalSamplerState->wrapT = gl::TextureWrap::ClampToEdge;
    m_decalSamplerState->wrapR = gl::TextureWrap::ClampToEdge;
    const char* decalSamplerNames[6] = {
        "u_DecalA0", "u_DecalA1", "u_DecalA2", "u_DecalA3", "u_DecalA4", "u_DecalA5",
    };
    for (size_t i = 0; i < m_decalTextures.size(); ++i) {
        m_decalTextures[i] = std::make_unique<gl::Texture>(gl, 0, gl::TextureTarget::Texture2d, false);
        m_decalSamplers[i] = std::make_unique<gl::Sampler>(decalSamplerNames[i], m_decalTextures[i].get(),
                                                           m_decalSamplerState.get());
    }
    m_ssrDepth = std::make_unique<gl::Texture>(gl, 0, gl::TextureTarget::Texture2d, false);
    m_ssrDepthSampler = std::make_unique<gl::Sampler>("u_SsrDepth", m_ssrDepth.get(), m_decalSamplerState.get());
    setSsrOptions(true, 1, 0.5f);
    m_ssrDepthFloat.write(0);
    m_ssrFrame.write(0.0f);
    m_gl = gl;
}

ux3d::renderer::CommandGenerator::~CommandGenerator()
{
    for (FrameBlock* block : { &m_cameraBlock, &m_environmentBlock }) {
        if (block->buffer != 0) {
            m_gl->deleteBuffers(1, &block->buffer);
        }
    }
}

void ux3d::renderer::CommandGenerator::uploadFrameBlock(FrameBlock& block, GLuint binding, const uint8_t* data, size_t size)
{
    if (block.buffer == 0) {
        m_gl->genBuffers(1, &block.buffer);
    }
    if (block.data.size() != size || std::memcmp(block.data.data(), data, size) != 0) {
        block.data.assign(data, data + size);
        m_gl->bindBuffer(GL_UNIFORM_BUFFER, block.buffer);
        m_gl->bufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(size), data, GL_DYNAMIC_DRAW);
    }
    m_gl->bindBufferBase(GL_UNIFORM_BUFFER, binding, block.buffer);
}

void ux3d::renderer::CommandGenerator::bindFrameBlocks()
{
    // The bytes are the uniforms' own values, placed at their std140 offsets, so shaders
    // see exactly what the individual uniforms carried.
    auto place = [](uint8_t* block, size_t offset, size_t size, const gl::Uniform& uniform) {
        std::memcpy(block + offset, uniform.bytes(), std::min(size, uniform.byteSize()));
    };
    if (m_activeCamera != nullptr) {
        // CameraBlock: three mat4 at 0, 64 and 128, then vec3 u_Camera at 192.
        uint8_t camera[208] = {};
        place(camera, 0, 64, m_activeCamera->getViewUniform());
        place(camera, 64, 64, m_activeCamera->getProjectionUniform());
        place(camera, 128, 64, m_activeCamera->getViewProjectionUniform());
        place(camera, 192, 12, m_activeCamera->getCameraPositionUniform());
        uploadFrameBlock(m_cameraBlock, gl::kCameraBlockBinding, camera, sizeof(camera));
    }
    // EnvironmentBlock: mat3 u_EnvRotation as three 16-byte columns, then u_EnvIntensity
    // at 48, u_EnvBlurNormalized at 52, int u_MipCount at 56, u_Exposure at 60,
    // int u_AsoboNormalMapDirectX at 64, int u_AsoboWindshieldVertexColorMask at 68 and
    // int u_AsoboDecalReceiver at 72 and float u_DecalDepthTolerance at 76 (80 bytes).
    uint8_t environment[80] = {};
    if (m_environmentMapRotationUniform.byteSize() == 36) {
        for (size_t column = 0; column < 3; ++column) {
            std::memcpy(environment + column * 16, m_environmentMapRotationUniform.bytes() + column * 12, 12);
        }
    }
    place(environment, 48, 4, m_environmentMapIntensityUniform);
    place(environment, 52, 4, m_environmentMapBlurNormalizedUniform);
    if (m_activeEnvironmentMap != nullptr) {
        place(environment, 56, 4, m_activeEnvironmentMap->getMipCount());
    }
    place(environment, 60, 4, m_exposureUniform);
    place(environment, 64, 4, m_asoboNormalMapDirectXUniform);
    place(environment, 68, 4, m_asoboWindshieldVertexColorMaskUniform);
    place(environment, 72, 4, m_asoboDecalReceiverUniform);
    place(environment, 76, 4, m_decalDepthToleranceUniform);
    uploadFrameBlock(m_environmentBlock, gl::kEnvironmentBlockBinding, environment, sizeof(environment));
}

void ux3d::renderer::CommandGenerator::update(const RendererScene& scene)
{
    m_asoboGlassCopy = scene.asoboGlassCopy;
    m_windshieldSsr = scene.windshieldSsr;
    m_lights.update(scene.lights);
    m_activeCamera = scene.camera;
    m_activeEnvironmentMap = scene.environmentMap;
    m_commandMapping.update(scene.drawables, m_commands);
    emitCommands();
}

void ux3d::renderer::CommandGenerator::emitCommands()
{
    for (auto& pass : m_passCommands) {
        pass.clear();
    }

    for (CmdDesc* desc : m_commands) {

        // sort into passes
        uint32_t bucket = desc->pass->getId();
        if (m_asoboGlassCopy && !m_windshieldSsr && bucket == RenderPassType::Transparent) {
            // Non-glass transparents draw before the behind copy is captured so ASOBO
            // glass v2 can refract them without redrawing the opaque scene.
            const bool reflection = desc->material != nullptr && desc->material->defines.isDefined("ASOBO_GLASS_REFLECTION");
            if (!reflection) {
                bucket = RenderPassType::Transparent2;
            }
        }
        auto& pass = m_passCommands[bucket];
        const Material* material = desc->material;
        // A2.2: the decal accumulation pass owns the decal draws; those never write depth.
        const bool decalPass = material != nullptr && desc->pass->getId() == RenderPassType::DecalAccumulation;
        const bool covered = material != nullptr
            && ((isTransparentPass(*material) && material->drawOrderActive)
                || decalPass)
            && material->drawOrderClass != DrawOrderClass::None;
        // A1.4: covered transparent draws keep depth testing against opaque geometry but stop
        // writing depth, so their explicit order decides what is on top.
        desc->command.depthWriteEnabled = !covered;
        // A4: glass v2 replaces the behind colour from the linear scene copy; windshield
        // keeps the destination and adds reflection independently of base coverage.
        const bool reflection = material != nullptr && material->defines.isDefined("ASOBO_GLASS_REFLECTION");
        desc->command.overrideBlend = material != nullptr && isTransparentPass(*material);
        desc->command.blendingEnabled = desc->command.overrideBlend;
        desc->command.premultipliedAlpha = reflection;
        if (reflection && isTransparentPass(*material)) {
            // A complete copy emits alpha 1 (replacement); a failed copy emits
            // coverage alpha and safely falls back to premultiplied transparency.
            desc->command.blendingEnabled = true;
            desc->command.depthWriteEnabled = false;
        }

        float sortKey = desc->distanceSortKey;
        if (covered && material->drawOrderClass == DrawOrderClass::Glass) {
            // MSFS SDK: for glass and windshield the value is added to the distance used for
            // sorting. The SDK does not state the sign more precisely; this is the literal
            // key = distance + offset, and TransparentPass draws larger keys first.
            sortKey += static_cast<float>(material->drawOrderOffset);
        }
        // A2.2: the accumulation pass sorts by the A1.4 decal key. The offset is an exact
        // integer draw control; it only applies while ASOBO_material_draw_order is on.
        const int drawOrderOffset = covered && material->drawOrderActive ? material->drawOrderOffset : 0;
        pass[desc->pass].emplace_back(&desc->command, sortKey,
                                      covered ? material->drawOrderClass : DrawOrderClass::None,
                                      drawOrderOffset);
    }
}

void ux3d::renderer::CommandGenerator::setIBLEnabled(bool _enabled)
{
    m_useIBL = _enabled;
}

void ux3d::renderer::CommandGenerator::setPunctualLightsEnabled(bool _enabled)
{
    m_usePunctualLights = _enabled;
}

void ux3d::renderer::CommandGenerator::setEnvironmentMapRotation(float _rotation)
{
    m_environmentMapRotation = math::rotate(math::radians(_rotation), math::vec3(0, 1, 0));
    m_environmentMapRotationUniform.write(m_environmentMapRotation);
}

void ux3d::renderer::CommandGenerator::setEnvironmentMapBlurNormalized(float _blur)
{
    m_environmentMapBlurNormalizedUniform.write(_blur);
}

void ux3d::renderer::CommandGenerator::setEnvironmentMapIntensity(float _value)
{
    m_environmentMapIntensityUniform.write(_value);
}

void ux3d::renderer::CommandGenerator::setToneMapping(ToneMappingsEnum _toneMapping)
{
    m_toneMapping = _toneMapping;
}

void ux3d::renderer::CommandGenerator::setExposure(float _exposure)
{
    m_exposureUniform.write(_exposure);
}

void ux3d::renderer::CommandGenerator::setAsoboNormalMapConvention(bool directX)
{
    m_asoboNormalMapDirectXUniform.write(static_cast<int>(directX ? 1 : 0));
}

void ux3d::renderer::CommandGenerator::setAsoboWindshieldVertexColorMask(bool enabled)
{
    m_asoboWindshieldVertexColorMaskUniform.write(static_cast<int>(enabled ? 1 : 0));
}

void ux3d::renderer::CommandGenerator::setAsoboDecalReceiver(bool enabled)
{
    m_asoboDecalReceiverUniform.write(static_cast<int>(enabled ? 1 : 0));
}

void ux3d::renderer::CommandGenerator::setDecalDepthTolerance(float metres)
{
    m_decalDepthToleranceUniform.write(std::clamp(metres, 0.0f, 0.01f));
}

void ux3d::renderer::CommandGenerator::setDecalTargets(GLuint a0, GLuint a1, GLuint a2, GLuint a3, GLuint a4,
                                                       GLuint a5)
{
    const GLuint targets[6] = { a0, a1, a2, a3, a4, a5 };
    for (size_t i = 0; i < m_decalTextures.size(); ++i) {
        m_decalTextures[i]->handle = targets[i];
    }
}
