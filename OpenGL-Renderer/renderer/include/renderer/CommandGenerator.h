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

    void update(const RendererScene& scene);

    // TODO: this should be handled per renderpass / per render layer
    void setIBLEnabled(bool _enabled);
    void setPunctualLightsEnabled(bool _enabled);
    void setEnvironmentMapRotation(float _rotation);
    const glm::mat3x3& getEnvironmentMapRotation() const { return m_environmentMapRotation; }
    void setEnvironmentMapBlurNormalized(float _blur);
    void setEnvironmentMapIntensity(float _value);
    void setToneMapping(ToneMappingsEnum _toneMapping);
    void setExposure(float _exposure);

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
    EnvironmentMap* m_activeEnvironmentMap = nullptr;
    const renderer::Camera* m_activeCamera = nullptr;
    LightData m_lights;
};
}
