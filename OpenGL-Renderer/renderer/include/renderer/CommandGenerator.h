// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <algorithm>
#include <array>
#include <list>
#include <memory>
#include <unordered_map>

#include "animation/Animation.h"
#include "core/Vector.h"
#include "renderer/Camera.h"
#include "renderer/Command.h"
#include "renderer/CommandMapping.h"
#include "renderer/EnvironmentMap.h"
#include "renderer/Lights.h"
#include "renderer/Material.h"
#include "renderer/Mesh.h"
#include "renderer/RenderPass.h"
#include "renderer/RendererScene.h"
#include "renderer/enums/ToneMappingsEnum.h"
#include "renderer/gl/Buffer.h"
#include "renderer/gl/ShaderCache.h"
#include "renderer/gl/Texture.h"
#include "renderer/gl/VertexLayoutCache.h"

#include "filesystem/Path.h"

namespace ux3d::renderer {
struct RendererScene;

struct CmdDesc {
    CmdDesc(gl::GL* gl, renderer::CommandTag tag, RenderPass* pass)
        : command(gl, tag)
        , pass(pass)
    {
    }

    Command command;
    RenderPass* pass;
    float distanceSortKey = 0;
    // The drawable this command was built for and a number that changes whenever the
    // command is rebuilt (P8.3 groups and refreshes instanced draws by them).
    Mesh* mesh = nullptr;
    Material* material = nullptr;
    uint64_t revision = 0;
    // P8.3: set when the command is built. instanceKey.camera is filled per update.
    bool instanceable = false;
    CommandMapping::InstanceKey instanceKey;
    const gl::Uniform* modelUniform = nullptr;
    const gl::Uniform* normalUniform = nullptr;
};

using Pass = std::unordered_map<RenderPass*, SortedCommands>;

// CommandGenerator is responsible for translating between the renderer interface (Mesh, Material, Camera, etc.)
// to Commands that can be executed by the renderer. Threre can be multiple CommandGenerators, for example when
// different scenes with distinct sets of lights, cameras, meshes and/or materials are used.
// The CommandGenerator only manages commands, all renderer objects need to be managed externally and passed as input.
class CommandGenerator {
    friend class CommandMapping; // TODO: it should be possible to remove this at some point
public:
    CommandGenerator(gl::GL*, const filesystem::Path& _shaderPath);
    CommandGenerator(CommandGenerator&& _other) = delete;
    ~CommandGenerator();

    void update(const RendererScene& scene);

    // P8.1b/c: uploads the active camera's values for the shaders' CameraBlock and the
    // environment and exposure values for EnvironmentBlock (each when it changed) and
    // binds them at gl::kCameraBlockBinding and gl::kEnvironmentBlockBinding. The renderer
    // calls it right before executing this generator's commands, so draws see the values
    // as they are then, as they did when these were individual uniforms.
    void bindFrameBlocks();

    // TODO: this should be handled per renderpass / per render layer
    void setIBLEnabled(bool _enabled);
    void setPunctualLightsEnabled(bool _enabled);
    void setEnvironmentMapRotation(float _rotation);
    const glm::mat3x3& getEnvironmentMapRotation() const { return m_environmentMapRotation; }
    // The u_EnvRotation matrix setEnvironmentMapRotation writes for `degrees` (shaders sample the cube map at
    // u_EnvRotation * worldDirection).
    static glm::mat3x3 environmentRotationMatrix(float degrees);
    void setEnvironmentMapBlurNormalized(float _blur);
    void setEnvironmentMapIntensity(float _value);
    void setToneMapping(ToneMappingsEnum _toneMapping);
    void setExposure(float _exposure);
    // A1.2: the asset's ASOBO_normal_map_convention is DirectX and this viewport applies it;
    // the shaders invert the green channel of every normal-map sample.
    void setAsoboNormalMapConvention(bool directX);
    // A5.1: this viewport applies ASOBO_material_detail_map. The material define is
    // asset-level (a material that declares the extension compiles its variant), so the
    // viewport switch is a frame-block uniform (like A1.2's normal-map convention).
    void setAsoboDetailMap(bool enabled);
    void setAsoboUVOptions(bool enabled);
    void setAsoboFresnelFade(bool enabled);
    void setAsoboGhostEffect(bool enabled);
    // A1.7: this viewport applies ASOBO_material_alphamode_dither; frame advances the dither
    // pattern per TAA accumulation frame (0 without TAA keeps it fixed). Both are frame-block values.
    void setAsoboAlphaDither(bool enabled);
    void setAsoboDitherFrame(float frame);
    void setWindshieldIcing(float icing);
    // A4 (partial): this viewport applies ASOBO_material_windshield_v3. Windshield v3
    // materials then keep COLOR_0 out of the base colour; it holds masks instead.
    void setAsoboWindshieldVertexColorMask(bool enabled);
    // A2.2: this viewport reads the shared decal accumulation targets. The material define
    // is asset-level, so the viewport switch is a frame-block uniform (like A1.2's
    // normal-map convention) and the command cache is not split per viewport.
    void setAsoboDecalReceiver(bool enabled);
    void setAsoboGlassCopyValid(bool valid) { m_asoboGlassCopyValidUniform.write(valid ? 1 : 0); }
    void setAsoboEmissiveActive(bool active);
    void setMsfsEmissiveScale(float scale);
    // 0 day, 1 night, 2 blend (A6.5: the night weight is setEmissiveNightBlend's).
    void setTimeOfDay(int timeOfDay);
    void setEmissiveNightBlend(float blend) { m_emissiveNightBlend = blend; }
    // A3.8: the viewer ambient light (EnvironmentBlock u_AmbientRadiance / u_AmbientEnabled): a uniform radiance in
    // environment units added to the IBL terms; off uploads zeros (every render unchanged).
    void setAmbient(bool enabled, const glm::vec3& radiance, bool automatic = false)
    {
        m_ambientEnabled = enabled ? (automatic ? 2 : 1) : 0;
        m_ambientRadiance = enabled ? radiance : glm::vec3(0.0f);
    }
    // A6.4: multiplier of the scene's linear outputs (EnvironmentBlock u_PreExposure); 1 = none.
    void setPreExposure(float preExposure) { m_preExposure = preExposure; }
    float getPreExposure() const { return m_preExposure; }
    void setAsoboMaterialEmissiveApplied(bool applied);
    void setAsoboOcclusionStrengthApplied(bool applied);
    // A3.1: the sun, a directional light evaluated by pbr.frag through the punctual-light BRDF path.
    // directionToSun: world space, unit, from the surface towards the sun; illuminance: linear RGB
    // illuminance (colour x intensity, environment radiance units x sr); angularRadius in radians (the
    // specular lobe is widened by it). Frame-block values (EnvironmentBlock), not per-draw uniforms.
    void setSun(bool enabled, const glm::vec3& directionToSun, const glm::vec3& illuminance, float angularRadius)
    {
        m_sunEnabled = enabled ? 1 : 0;
        m_sunDirection = directionToSun;
        m_sunIlluminance = illuminance;
        m_sunAngularRadius = angularRadius;
    }
    // A3.2: the active viewport's cascaded sun shadow map (EnvironmentBlock u_Shadow*, sampler u_SunShadowMap).
    // texture: a GL_TEXTURE_2D_ARRAY of depth layers, one per cascade (0 when shadows are off; the shaders then
    // never sample it). Values are per viewport; the frame block is uploaded at every draw.
    struct SunShadowParameters {
        bool enabled = false;
        GLuint texture = 0;
        int cascadeCount = 0;
        int filter = 1; // 0 hard, 1 PCF, 2 soft
        float mapSize = 1.0f;
        float normalBias = 1.0f; // texels
        std::array<glm::mat4, 4> matrices { glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f) };
        glm::vec4 splits = glm::vec4(0.0f);
        glm::vec4 texelSizes = glm::vec4(0.0f);
        float nearDepth = 0.0f;
        float fadeStart = 3.0e38f;
        float fadeEnd = 3.0e38f;
    };
    void setSunShadow(const SunShadowParameters& shadow)
    {
        m_sunShadow = shadow;
        m_sunShadowTexture->handle = shadow.enabled ? shadow.texture : 0;
    }
    const SunShadowParameters& getSunShadow() const { return m_sunShadow; }
    // A3.5: the active viewport's SSAO result (EnvironmentBlock u_SsaoEnabled, sampler u_SsaoTexture). The texture
    // is filled during the draw (after the prepass); its handle must be the one bound here.
    void setSsao(bool enabled, GLuint texture)
    {
        m_ssaoEnabled = enabled && texture != 0 ? 1 : 0;
        m_ssaoTexture->handle = m_ssaoEnabled != 0 ? texture : 0;
    }
    bool isSsaoEnabled() const { return m_ssaoEnabled != 0; }
    // A3.9: the sky-visibility volume (EnvironmentBlock u_SkyOcclusion*, sampler u_SkyOcclusionVolume, a borrowed
    // GL_TEXTURE_3D handle). Only materials compiled with SKY_OCCLUSION read them; the application defines it on the
    // drawables of a viewport that applies the volume.
    struct SkyOcclusionParameters {
        bool enabled = false;
        GLuint texture = 0;
        glm::vec3 origin = glm::vec3(0.0f);
        float cell = 1.0f;
        glm::vec3 dims = glm::vec3(0.0f);
        float normalOffsetCells = 1.0f;
        float openFraction = 1.0f;
    };
    void setSkyOcclusion(const SkyOcclusionParameters& parameters)
    {
        m_skyOcclusion = parameters;
        m_skyOcclusionTexture->handle = parameters.enabled ? parameters.texture : 0;
    }
    const SkyOcclusionParameters& getSkyOcclusion() const { return m_skyOcclusion; }
    // A3.7: the active viewport's sky portals (EnvironmentBlock u_SkyPortal*; at most kMaxSkyPortals are uploaded)
    // and its strength. Only materials compiled with ASOBO_SKY_PORTAL read them.
    void setSkyPortals(const std::vector<SkyPortalLight>& portals, float strength)
    {
        m_skyPortalCount = static_cast<int32_t>(std::min(portals.size(), kMaxSkyPortals));
        std::copy_n(portals.begin(), m_skyPortalCount, m_skyPortals.begin());
        m_skyPortalStrength = strength;
    }
    int getSkyPortalCount() const { return m_skyPortalCount; }
    void setSsrOptions(bool enabled, int quality, float sky) {
        m_ssrOptions.write(math::vec3(enabled ? 1.0f : 0.0f, quality == 0 ? 256.0f : quality == 2 ? 1024.0f : 512.0f, sky));
    }
    void setSsrDepth(GLuint texture, bool floatingDepth) {
        m_ssrDepth->handle = texture;
        m_ssrDepthFloat.write(floatingDepth ? 1 : 0);
    }
    GLuint getSsrDepth() const { return m_ssrDepth->handle; }
    // Ray start-phase noise frame: advances while TAA accumulates, 0 otherwise.
    void setSsrFrame(float frame) { m_ssrFrame.write(frame); }
    void setDecalDepthTolerance(float metres);
    // A2.2: the shared buffer's A0-A5 texture handles, owned by the application's DecalBuffer.
    // They are wrapped here so every receiver command can keep stable sampler pointers while
    // the buffer is created, resized and released. Zero handles are valid (no buffer).
    void setDecalTargets(GLuint a0, GLuint a1, GLuint a2, GLuint a3, GLuint a4, GLuint a5);

    // Generate the commands and sort them to renderpasses
    void emitCommands();

    // Interface to the renderer
    const std::array<Pass, RenderPassType::MaxRenderPasses>& getPassCommands() const { return m_passCommands; }
    std::array<Pass, RenderPassType::MaxRenderPasses>& getPassCommands() { return m_passCommands; }

    CommandMapping& getCommandMapping() { return m_commandMapping; }
    const CommandMapping& getCommandMapping() const { return m_commandMapping; }

    gl::VertexLayoutCache& getVertexLayoutCache() { return m_vertexLayoutCache; }
    const gl::VertexLayoutCache& getVertexLayoutCache() const { return m_vertexLayoutCache; }
    void clearVertexLayoutCache() {
        m_commands.clear();
        for (auto& pass : m_passCommands) {
            pass.clear();
        }
        m_commandMapping.clearCommandCache(); // cached commands hold vertex layouts
        m_vertexLayoutCache.clear();
    }

protected:
    std::vector<CmdDesc*> m_commands; // owned by m_commandMapping's command cache
    std::array<Pass, RenderPassType::MaxRenderPasses> m_passCommands;
    gl::VertexLayoutCache m_vertexLayoutCache;
    CommandMapping m_commandMapping;
    gl::ShaderCache m_shaderCache;

    // TODO: remove these, they should be configured by the renderpass
    ToneMappingsEnum m_toneMapping = ToneMappingsEnum::None;
    bool m_useIBL = true;
    bool m_asoboGlassCopy = false;
    bool m_windshieldSsr = false;
    bool m_usePunctualLights = true;
    glm::mat3x3 m_environmentMapRotation = glm::mat3x3(1.0f);
    gl::Uniform m_environmentMapRotationUniform;
    gl::Uniform m_environmentMapBlurNormalizedUniform;
    gl::Uniform m_environmentMapIntensityUniform;
    gl::Uniform m_exposureUniform;
    gl::Uniform m_asoboNormalMapDirectXUniform;
    gl::Uniform m_asoboWindshieldVertexColorMaskUniform;
    gl::Uniform m_asoboDecalReceiverUniform;
    gl::Uniform m_asoboGlassCopyValidUniform;
    gl::Uniform m_decalDepthToleranceUniform;
    gl::Uniform m_asoboEmissiveActiveUniform;
    gl::Uniform m_msfsEmissiveScaleUniform;
    gl::Uniform m_timeOfDayUniform;
    gl::Uniform m_asoboMaterialEmissiveAppliedUniform;
    gl::Uniform m_asoboDetailMapUniform;
    gl::Uniform m_asoboUVOptionsUniform;
    gl::Uniform m_asoboFresnelFadeUniform;
    gl::Uniform m_asoboGhostEffectUniform;
    gl::Uniform m_asoboAlphaDitherUniform;
    gl::Uniform m_asoboDitherFrameUniform;
    gl::Uniform m_windshieldIcingUniform;
    gl::Uniform m_asoboOcclusionStrengthUniform;
    // A3.1 sun (EnvironmentBlock u_SunDirection, u_SunEnabled, u_SunIlluminance, u_SunAngularRadius).
    int32_t m_sunEnabled = 0;
    glm::vec3 m_sunDirection = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 m_sunIlluminance = glm::vec3(0.0f);
    float m_sunAngularRadius = 0.0f;
    float m_preExposure = 1.0f;
    float m_emissiveNightBlend = 0.0f;
    int32_t m_ambientEnabled = 0;
    glm::vec3 m_ambientRadiance = glm::vec3(0.0f);
    // A3.2 sun shadow map (borrowed texture handle; stable wrapper and comparison sampler).
    SunShadowParameters m_sunShadow;
    std::unique_ptr<gl::Texture> m_sunShadowTexture;
    std::unique_ptr<gl::SamplerState> m_sunShadowSamplerState;
    std::unique_ptr<gl::Sampler> m_sunShadowSampler;
    // A3.7 sky portals of the active viewport.
    int32_t m_skyPortalCount = 0;
    float m_skyPortalStrength = 0.35f;
    std::array<SkyPortalLight, kMaxSkyPortals> m_skyPortals {};
    // A3.5 SSAO result (borrowed texture handle; stable wrapper, nearest sampler).
    int32_t m_ssaoEnabled = 0;
    std::unique_ptr<gl::Texture> m_ssaoTexture;
    std::unique_ptr<gl::Sampler> m_ssaoSampler;
    // A3.9 sky-visibility volume (borrowed texture handle; stable wrapper, nearest sampler).
    SkyOcclusionParameters m_skyOcclusion;
    std::unique_ptr<gl::Texture> m_skyOcclusionTexture;
    std::unique_ptr<gl::Sampler> m_skyOcclusionSampler;
    // A2.2: six borrowed views of the shared accumulation targets and their samplers. The
    // wrappers live as long as the generator; setDecalTargets refreshes only the handles.
    std::array<std::unique_ptr<gl::Texture>, 6> m_decalTextures;
    std::array<std::unique_ptr<gl::Sampler>, 6> m_decalSamplers;
    std::unique_ptr<gl::SamplerState> m_decalSamplerState;
    gl::Uniform m_ssrOptions{"u_SsrOptions"};
    gl::Uniform m_ssrDepthFloat{"u_SsrDepthFloat"};
    gl::Uniform m_ssrFrame{"u_SsrFrame"};
    std::unique_ptr<gl::Texture> m_ssrDepth;
    std::unique_ptr<gl::Sampler> m_ssrDepthSampler;
    EnvironmentMap* m_activeEnvironmentMap = nullptr;
    const renderer::Camera* m_activeCamera = nullptr;
    LightData m_lights;

    gl::GL* m_gl = nullptr;
    struct FrameBlock {
        GLuint buffer = 0;
        std::vector<uint8_t> data;
    };
    void uploadFrameBlock(FrameBlock& block, GLuint binding, const uint8_t* data, size_t size);
    FrameBlock m_cameraBlock;
    FrameBlock m_environmentBlock;
};
}
