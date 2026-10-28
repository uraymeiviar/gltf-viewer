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
    , m_asoboEmissiveActiveUniform("u_AsoboEmissiveActive")
    , m_msfsEmissiveScaleUniform("u_MsfsEmissiveScale")
    , m_timeOfDayUniform("u_TimeOfDay")
    , m_asoboMaterialEmissiveAppliedUniform("u_AsoboMaterialEmissiveApplied")
    , m_asoboDetailMapUniform("u_AsoboDetailMapEnabled")
    , m_asoboUVOptionsUniform("u_AsoboUVOptionsEnabled")
    , m_asoboFresnelFadeUniform("u_AsoboFresnelFadeApplied")
    , m_asoboGhostEffectUniform("u_AsoboGhostEffectApplied")
    , m_asoboAlphaDitherUniform("u_AsoboAlphaDitherApplied")
    , m_asoboDitherFrameUniform("u_AsoboDitherFrame")
    , m_windshieldIcingUniform("u_WindshieldIcing")
    , m_asoboOcclusionStrengthUniform("u_AsoboOcclusionStrengthApplied")
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
    setWindshieldIcing(0.0f);
    setAsoboEmissiveActive(false);
    setMsfsEmissiveScale(0.001f);
    setTimeOfDay(0);
    setAsoboMaterialEmissiveApplied(false);
    setAsoboDetailMap(false);
    setAsoboUVOptions(false);
    setAsoboFresnelFade(false);
    setAsoboGhostEffect(false);
    setAsoboAlphaDither(false);
    setAsoboDitherFrame(0.0f);
    setAsoboOcclusionStrengthApplied(false);

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
    // A3.2: hardware depth comparison with bilinear filtering (2x2 PCF per lookup), clamped at the edges.
    m_sunShadowTexture = std::make_unique<gl::Texture>(gl, 0, gl::TextureTarget::Texture2dArray, false);
    m_sunShadowSamplerState = std::make_unique<gl::SamplerState>();
    m_sunShadowSamplerState->minFilter = gl::TextureMinFilter::Linear;
    m_sunShadowSamplerState->magFilter = gl::TextureMagFilter::Linear;
    m_sunShadowSamplerState->wrapS = gl::TextureWrap::ClampToEdge;
    m_sunShadowSamplerState->wrapT = gl::TextureWrap::ClampToEdge;
    m_sunShadowSamplerState->wrapR = gl::TextureWrap::ClampToEdge;
    m_sunShadowSamplerState->compareRefToTexture = true;
    m_sunShadowSampler = std::make_unique<gl::Sampler>("u_SunShadowMap", m_sunShadowTexture.get(),
                                                       m_sunShadowSamplerState.get());
    // A3.5: texelFetch of the SSAO result at the fragment's pixel (nearest, clamped).
    m_ssaoTexture = std::make_unique<gl::Texture>(gl, 0, gl::TextureTarget::Texture2d, false);
    m_ssaoSampler = std::make_unique<gl::Sampler>("u_SsaoTexture", m_ssaoTexture.get(), m_decalSamplerState.get());
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
    // int u_AsoboNormalMapDirectX at 64, int u_AsoboWindshieldVertexColorMask at 68,
    // int u_AsoboDecalReceiver at 72, float u_DecalDepthTolerance at 76,
    // int u_AsoboEmissiveActive at 80, float u_MsfsEmissiveScale at 84,
    // int u_TimeOfDay at 88, int u_AsoboMaterialEmissiveApplied at 92,
    // int u_AsoboDetailMapEnabled at 96, float u_WindshieldIcing at 100,
    // int u_AsoboOcclusionStrengthApplied at 104, int u_AsoboUVOptionsEnabled at 108,
    // int u_AsoboFresnelFadeApplied at 112, int u_AsoboGhostEffectApplied at 116,
    // int u_AsoboAlphaDitherApplied at 120, float u_AsoboDitherFrame at 124,
    // A3.1: vec3 u_SunDirection at 128, int u_SunEnabled at 140, vec3 u_SunIlluminance at 144,
    // float u_SunAngularRadius at 156,
    // A3.2: mat4 u_ShadowMatrices[4] at 160, vec4 u_ShadowSplits at 416, vec4 u_ShadowTexelSizes at 432,
    // vec4 u_ShadowParams at 448, int u_ShadowEnabled at 464, int u_ShadowCascadeCount at 468,
    // int u_ShadowFilter at 472, float u_ShadowMapSize at 476,
    // A3.5: int u_SsaoEnabled at 480,
    // A3.7: int u_SkyPortalCount at 484, float u_SkyPortalStrength at 488, vec4 u_SkyPortalPositionRadius[8] at 496,
    // vec4 u_SkyPortalNormalCosInner[8] at 624, vec4 u_SkyPortalCosOuter[8] at 752 (880 bytes, a whole number of vec4).
    uint8_t environment[880] = {};
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
    place(environment, 80, 4, m_asoboEmissiveActiveUniform);
    place(environment, 84, 4, m_msfsEmissiveScaleUniform);
    place(environment, 88, 4, m_timeOfDayUniform);
    place(environment, 92, 4, m_asoboMaterialEmissiveAppliedUniform);
    place(environment, 96, 4, m_asoboDetailMapUniform);
    place(environment, 100, 4, m_windshieldIcingUniform);
    place(environment, 104, 4, m_asoboOcclusionStrengthUniform);
    place(environment, 108, 4, m_asoboUVOptionsUniform);
    place(environment, 112, 4, m_asoboFresnelFadeUniform);
    place(environment, 116, 4, m_asoboGhostEffectUniform);
    place(environment, 120, 4, m_asoboAlphaDitherUniform);
    place(environment, 124, 4, m_asoboDitherFrameUniform);
    std::memcpy(environment + 128, &m_sunDirection[0], 12);
    std::memcpy(environment + 140, &m_sunEnabled, 4);
    std::memcpy(environment + 144, &m_sunIlluminance[0], 12);
    std::memcpy(environment + 156, &m_sunAngularRadius, 4);
    if (m_sunShadow.enabled) {
        for (size_t i = 0; i < 4; ++i) {
            std::memcpy(environment + 160 + i * 64, &m_sunShadow.matrices[i][0][0], 64);
        }
        std::memcpy(environment + 416, &m_sunShadow.splits[0], 16);
        std::memcpy(environment + 432, &m_sunShadow.texelSizes[0], 16);
        const glm::vec4 params(m_sunShadow.nearDepth, m_sunShadow.fadeStart, m_sunShadow.fadeEnd, m_sunShadow.normalBias);
        std::memcpy(environment + 448, &params[0], 16);
        const int32_t shadowInts[3] = { 1, m_sunShadow.cascadeCount, m_sunShadow.filter };
        std::memcpy(environment + 464, shadowInts, 12);
        std::memcpy(environment + 476, &m_sunShadow.mapSize, 4);
    }
    std::memcpy(environment + 480, &m_ssaoEnabled, 4);
    std::memcpy(environment + 484, &m_skyPortalCount, 4);
    std::memcpy(environment + 488, &m_skyPortalStrength, 4);
    for (int32_t i = 0; i < m_skyPortalCount; ++i) {
        const SkyPortalLight& portal = m_skyPortals[static_cast<size_t>(i)];
        const glm::vec4 positionRadius(portal.position, portal.radius);
        const glm::vec4 normalCosInner(portal.normal, portal.cosInner);
        const glm::vec4 cosOuter(portal.cosOuter, 0.0f, 0.0f, 0.0f);
        std::memcpy(environment + 496 + i * 16, &positionRadius[0], 16);
        std::memcpy(environment + 624 + i * 16, &normalCosInner[0], 16);
        std::memcpy(environment + 752 + i * 16, &cosOuter[0], 16);
    }
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

        // Debug windshield pixels replace colour and carry alpha 2 as a post-pass tag.
        if (material != nullptr && material->defines.isDefined("RAW_SSR_OVERLAY") &&
            material->defines.isDefined("ASOBO_WINDSHIELD_SSR")) {
            desc->command.overrideBlend = true;
            desc->command.blendingEnabled = false;
        }

        if (material != nullptr && (material->overrideBlend || material->customBlend)) {
            desc->command.overrideBlend = true;
            desc->command.customBlend = material->customBlend;
            desc->command.blendingEnabled = material->blendingEnabled;
            desc->command.blendSrcFactor = material->blendSrcFactor;
            desc->command.blendDstFactor = material->blendDstFactor;
            desc->command.blendEquation = material->blendEquation;
            desc->command.logicOpEnabled = material->logicOpEnabled;
            desc->command.logicOp = material->logicOp;
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

glm::mat3x3 ux3d::renderer::CommandGenerator::environmentRotationMatrix(float degrees)
{
    return glm::mat3x3(math::rotate(math::radians(degrees), math::vec3(0, 1, 0)));
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

void ux3d::renderer::CommandGenerator::setAsoboDetailMap(bool enabled)
{
    m_asoboDetailMapUniform.write(static_cast<int>(enabled ? 1 : 0));
}

void ux3d::renderer::CommandGenerator::setAsoboUVOptions(bool enabled)
{
    m_asoboUVOptionsUniform.write(static_cast<int>(enabled ? 1 : 0));
}

void ux3d::renderer::CommandGenerator::setAsoboFresnelFade(bool enabled)
{
    m_asoboFresnelFadeUniform.write(static_cast<int>(enabled ? 1 : 0));
}

void ux3d::renderer::CommandGenerator::setAsoboGhostEffect(bool enabled)
{
    m_asoboGhostEffectUniform.write(static_cast<int>(enabled ? 1 : 0));
}

void ux3d::renderer::CommandGenerator::setAsoboAlphaDither(bool enabled)
{
    m_asoboAlphaDitherUniform.write(static_cast<int>(enabled ? 1 : 0));
}

void ux3d::renderer::CommandGenerator::setAsoboDitherFrame(float frame)
{
    m_asoboDitherFrameUniform.write(frame);
}

void ux3d::renderer::CommandGenerator::setWindshieldIcing(float icing)
{
    m_windshieldIcingUniform.write(std::clamp(icing, 0.0f, 1.0f));
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

void ux3d::renderer::CommandGenerator::setAsoboEmissiveActive(bool active)
{
    m_asoboEmissiveActiveUniform.write(static_cast<int>(active ? 1 : 0));
}

void ux3d::renderer::CommandGenerator::setMsfsEmissiveScale(float scale)
{
    m_msfsEmissiveScaleUniform.write(scale);
}

void ux3d::renderer::CommandGenerator::setTimeOfDay(int timeOfDay)
{
    m_timeOfDayUniform.write(timeOfDay);
}

void ux3d::renderer::CommandGenerator::setAsoboMaterialEmissiveApplied(bool applied)
{
    m_asoboMaterialEmissiveAppliedUniform.write(static_cast<int>(applied ? 1 : 0));
}

void ux3d::renderer::CommandGenerator::setAsoboOcclusionStrengthApplied(bool applied)
{
    m_asoboOcclusionStrengthUniform.write(static_cast<int>(applied ? 1 : 0));
}

void ux3d::renderer::CommandGenerator::setDecalTargets(GLuint a0, GLuint a1, GLuint a2, GLuint a3, GLuint a4,
                                                       GLuint a5)
{
    const GLuint targets[6] = { a0, a1, a2, a3, a4, a5 };
    for (size_t i = 0; i < m_decalTextures.size(); ++i) {
        m_decalTextures[i]->handle = targets[i];
    }
}
