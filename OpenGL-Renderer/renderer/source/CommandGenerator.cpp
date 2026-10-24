// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/CommandGenerator.h"
#include "filesystem/Dir.h"
#include "math/MatrixFunctions.h"
#include "renderer/RendererScene.h"

#include <algorithm>
#include <cstring>

ux3d::renderer::CommandGenerator::CommandGenerator(gl::GL* gl, const filesystem::Path& _shaderPath)
    : m_vertexLayoutCache(gl)
    , m_shaderCache(gl, _shaderPath)
    , m_commandMapping(gl, *this, m_shaderCache)
    , m_environmentMapRotationUniform("u_EnvRotation")
    , m_environmentMapBlurNormalizedUniform("u_EnvBlurNormalized")
    , m_environmentMapIntensityUniform("u_EnvIntensity")
    , m_exposureUniform("u_Exposure")
    , m_asoboNormalMapDirectXUniform("u_AsoboNormalMapDirectX")
    , m_lights(gl)
{
    setEnvironmentMapRotation(0.0f);
    setEnvironmentMapBlurNormalized(0.0f);
    setEnvironmentMapIntensity(1.0f);
    setExposure(1.0f);
    setAsoboNormalMapConvention(false);
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
    // at 48, u_EnvBlurNormalized at 52, int u_MipCount at 56, u_Exposure at 60 and
    // int u_AsoboNormalMapDirectX at 64 (the block is padded to 80 for std140).
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
    uploadFrameBlock(m_environmentBlock, gl::kEnvironmentBlockBinding, environment, sizeof(environment));
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

    for (CmdDesc* desc : m_commands) {

        // sort into passes
        auto& pass = m_passCommands[desc->pass->getId()];
        pass[desc->pass].emplace_back(&desc->command, desc->distanceSortKey);
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
