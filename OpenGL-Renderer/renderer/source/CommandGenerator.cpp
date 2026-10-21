// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/CommandGenerator.h"
#include "filesystem/Dir.h"
#include "math/MatrixFunctions.h"
#include "renderer/RendererScene.h"

#include <algorithm>

ux3d::renderer::CommandGenerator::CommandGenerator(gl::GL* gl, const filesystem::Path& _shaderPath)
    : m_shaderCache(gl, _shaderPath)
    , m_commandMapping(gl, *this, m_shaderCache)
    , m_environmentMapRotationUniform("u_EnvRotation")
    , m_environmentMapIntensityUniform("u_EnvIntensity")
    , m_exposureUniform("u_Exposure")
    , m_environmentMapBlurNormalizedUniform("u_EnvBlurNormalized")
    , m_lights(gl)
{
    setEnvironmentMapRotation(0.0f);
    setEnvironmentMapBlurNormalized(0.0f);
    setEnvironmentMapIntensity(1.0f);
    setExposure(1.0f);
}

void ux3d::renderer::CommandGenerator::update(const RendererScene& scene)
{
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

    for (CmdDesc& desc : m_commands) {

        // sort into passes
        auto& pass = m_passCommands[desc.pass->getId()];
        pass[desc.pass].emplace_back(&desc.command, desc.distanceSortKey);
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
