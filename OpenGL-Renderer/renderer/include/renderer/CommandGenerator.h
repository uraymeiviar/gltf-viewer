// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <array>
#include <list>
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
    void setEnvironmentMapBlurNormalized(float _blur);
    void setEnvironmentMapIntensity(float _value);
    void setToneMapping(ToneMappingsEnum _toneMapping);
    void setExposure(float _exposure);
    // A1.2: the asset's ASOBO_normal_map_convention is DirectX and this viewport applies it;
    // the shaders invert the green channel of every normal-map sample.
    void setAsoboNormalMapConvention(bool directX);

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
    bool m_usePunctualLights = true;
    glm::mat3x3 m_environmentMapRotation = glm::mat3x3(1.0f);
    gl::Uniform m_environmentMapRotationUniform;
    gl::Uniform m_environmentMapBlurNormalizedUniform;
    gl::Uniform m_environmentMapIntensityUniform;
    gl::Uniform m_exposureUniform;
    gl::Uniform m_asoboNormalMapDirectXUniform;
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
