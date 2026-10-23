// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/CommandExecutor.h"
#include "renderer/Command.h"
#include "renderer/gl/FrameBuffer.h"
#include "renderer/gl/Program.h"
#include "renderer/gl/Util.h"

ux3d::renderer::CommandExecutor::CommandExecutor(gl::GL* gl)
    : m_gl(gl)
    , m_samplerCache(gl)
{
}

void ux3d::renderer::CommandExecutor::init()
{
    m_samplerCache.init(m_gl);
    m_gl->getIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &m_maxTextureUnits);
    m_gl->enable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    m_activePolygonMode.reset();
    m_cullingEnabled.reset();
    m_cullFace.reset();
    m_activeWinding.reset();
    m_depthTestEnabled.reset();
    m_depthFunc.reset();
    m_blendEnabled.reset();
    m_blendSrcFactor.reset();
    m_blendDstFactor.reset();
    m_blendEquation.reset();
}

void ux3d::renderer::CommandExecutor::resetState()
{
    unbindSamplers();
    m_activeVertexArray = gl::invalid;
    m_activePolygonMode.reset();
    m_cullingEnabled.reset();
    m_cullFace.reset();
    m_activeWinding.reset();
    m_depthTestEnabled.reset();
    m_depthFunc.reset();
    m_blendEnabled.reset();
    m_blendSrcFactor.reset();
    m_blendDstFactor.reset();
    m_blendEquation.reset();
}

void ux3d::renderer::CommandExecutor::unbindSamplers()
{
    if (m_maxBoundTextureUnit >= 0) {
        for (GLint unit = 0; unit <= m_maxBoundTextureUnit; ++unit) {
            m_gl->bindSampler(static_cast<GLuint>(unit), 0);
        }
        m_maxBoundTextureUnit = -1;
    }
}

void ux3d::renderer::CommandExecutor::execute(const Command& command)
{
    switch (command.tag) {
    case CommandTag::Clear:
        clear(command);
        break;
    case CommandTag::Draw:
        draw(command);
        break;
    case CommandTag::Viewport:
        m_gl->viewport(0, 0, command.viewportWidth, command.viewportHeight);
        break;
    case CommandTag::Framebuffer:
        bindFrameBuffer(command);
        break;
    case CommandTag::Blend:
        setupBlendFunc(command);
        break;
    case CommandTag::DepthTest:
        setupDepthTest(command);
        break;
    case CommandTag::ExternalFunction:
        unbindSamplers();
        m_activeVertexArray = gl::invalid;
        m_activePolygonMode.reset();
        m_cullingEnabled.reset();
        m_cullFace.reset();
        m_activeWinding.reset();
        m_depthTestEnabled.reset();
        m_depthFunc.reset();
        m_blendEnabled.reset();
        m_blendSrcFactor.reset();
        m_blendDstFactor.reset();
        m_blendEquation.reset();
        command.externalFunction();
        break;
    default:
        break;
    }
}

void ux3d::renderer::CommandExecutor::draw(const Command& command)
{
    // bind shader program
    if (activateProgram(command) == false) // TODO: remove (done explicitly by pass command)
    {
        return;
    }

    const bool uniformsResolved = command.uniformBindings.size() == command.uniforms.size();
    for (size_t i = 0; i < command.uniforms.size(); ++i) {
        const gl::Uniform* uniform = command.uniforms[i];
        if (uniform == nullptr) {
            continue;
        }
        if (uniformsResolved) {
            if (command.uniformBindings[i] != nullptr) {
                uniform->commit(m_gl, *command.uniformBindings[i]);
            }
        } else {
            command.program->commitUniform(*uniform);
        }
    }

    for (const auto& block : command.uniformBlocks) {
        if (block != nullptr) {
            command.program->commitUniformBlock(*block);
        }
    }

    for (GLint i = 0; i < command.samplers.size(); ++i) {
        const auto& sampler = command.samplers[i];
        if (sampler == nullptr) {
            continue;
        }
        if (i >= m_maxTextureUnits) {
            core::logError("Using too many textures, ignoring %s", sampler->name.data());
            break;
        }

        static const gl::SamplerState s_defaultSamplerState;
        const gl::SamplerState& state = (sampler->samplerState != nullptr)
            ? *sampler->samplerState
            : s_defaultSamplerState;
        GLuint samplerHandle = m_samplerCache.getOrCreateSampler(state);

        if (command.samplerBindings.size() == command.samplers.size()) {
            if (command.samplerBindings[i] != nullptr) {
                sampler->commit(m_gl, *command.samplerBindings[i], i, samplerHandle);
            }
        } else {
            command.program->commitSampler(*sampler, i, samplerHandle);
        }
        if (i > m_maxBoundTextureUnit) {
            m_maxBoundTextureUnit = i;
        }
    }

    setupVertexArray(command);
    setupCulling(command);
    setupPolygonMode(command);
    setupFrontFace(command);

    GLIDER_HOT_GL_CHECK(m_gl, "before draw failed");

    if (command.indexBuffer != nullptr) {
        m_gl->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, command.indexBuffer->handle);
        const auto* offset = reinterpret_cast<const void*>(static_cast<size_t>(command.indexByteOffset));
        if (command.instanceCount > 0) {
            m_gl->drawElementsInstanced(static_cast<GLenum>(command.primitiveTopology), command.primitiveCount,
                                        static_cast<GLenum>(command.indexFormat), offset, command.instanceCount);
        } else {
            m_gl->drawElements(static_cast<GLenum>(command.primitiveTopology), command.primitiveCount,
                               static_cast<GLenum>(command.indexFormat), offset);
        }
    } else if (command.instanceCount > 0) {
        m_gl->drawArraysInstanced(static_cast<GLenum>(command.primitiveTopology), command.primitiveOffset,
                                  command.primitiveCount, command.instanceCount);
    } else {
        m_gl->drawArrays(
            static_cast<GLenum>(command.primitiveTopology),
            command.primitiveOffset,
            command.primitiveCount);
    }

    GLIDER_HOT_GL_CHECK(m_gl, "draw failed");
}

void ux3d::renderer::CommandExecutor::clear(const Command& command) const
{
    auto flag = static_cast<GLbitfield>(command.clearTarget);

    if (flag & GL_COLOR_BUFFER_BIT) {
        m_gl->clearBufferfv(GL_COLOR, 0, command.clearColor.data());
    }
    if (flag & GL_DEPTH_BUFFER_BIT) {
        GLfloat depth = 1;
        m_gl->clearBufferfv(GL_DEPTH, 0, &depth);
    }

    checkGLError(m_gl, "Failed to clear");
}

bool ux3d::renderer::CommandExecutor::bindFrameBuffer(const Command& command) const
{
    if (command.framebuffer == nullptr) {
        return false;
    }
    return command.framebuffer->bind();
}

void ux3d::renderer::CommandExecutor::setupVertexArray(const Command& command)
{
    if (command.vertexLayout != nullptr && command.vertexLayout->handle != m_activeVertexArray) {
        m_activeVertexArray = command.vertexLayout->handle;
        m_gl->bindVertexArray(m_activeVertexArray);
    }
    GLIDER_HOT_GL_CHECK(m_gl, "Failed to bind vertex array");
}

void ux3d::renderer::CommandExecutor::setupBlendFunc(const Command& command)
{
    if (command.blendingEnabled) {
        if (!m_blendEnabled.has_value() || !m_blendEnabled.value()) {
            m_blendEnabled = true;
            m_gl->enable(GL_BLEND);
        }
        if (!m_blendSrcFactor.has_value() || !m_blendDstFactor.has_value() ||
            m_blendSrcFactor.value() != GL_SRC_ALPHA || m_blendDstFactor.value() != GL_ONE_MINUS_SRC_ALPHA) {
            m_blendSrcFactor = GL_SRC_ALPHA;
            m_blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
            m_gl->blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        if (!m_blendEquation.has_value() || m_blendEquation.value() != GL_FUNC_ADD) {
            m_blendEquation = GL_FUNC_ADD;
            m_gl->blendEquation(GL_FUNC_ADD);
        }
    } else {
        if (!m_blendEnabled.has_value() || m_blendEnabled.value()) {
            m_blendEnabled = false;
            m_gl->disable(GL_BLEND);
        }
    }

    GLIDER_HOT_GL_CHECK(m_gl, "Failed to set blend mode");
}

void ux3d::renderer::CommandExecutor::setupCulling(const Command& command)
{
    if (command.culling != gl::Culling::None) {
        if (!m_cullingEnabled.has_value() || !m_cullingEnabled.value()) {
            m_cullingEnabled = true;
            m_gl->enable(GL_CULL_FACE);
        }
        if (!m_cullFace.has_value() || m_cullFace.value() != command.culling) {
            m_cullFace = command.culling;
            m_gl->cullFace(GLenum(command.culling));
        }
    } else {
        if (!m_cullingEnabled.has_value() || m_cullingEnabled.value()) {
            m_cullingEnabled = false;
            m_gl->disable(GL_CULL_FACE);
        }
    }

    GLIDER_HOT_GL_CHECK(m_gl, "Failed to set culling mode");
}

void ux3d::renderer::CommandExecutor::setupPolygonMode(const Command& command)
{
    if (!m_activePolygonMode.has_value() || m_activePolygonMode.value() != command.polygonMode) {
        m_activePolygonMode = command.polygonMode;
        m_gl->polygonMode(GL_FRONT_AND_BACK, GLenum(command.polygonMode));
        GLIDER_HOT_GL_CHECK(m_gl, "Failed to set polygon mode");
    }
}

void ux3d::renderer::CommandExecutor::setupFrontFace(const Command& command)
{
    if (!m_activeWinding.has_value() || m_activeWinding.value() != command.winding) {
        m_activeWinding = command.winding;
        m_gl->frontFace(GLenum(command.winding));
        GLIDER_HOT_GL_CHECK(m_gl, "Failed to set front face winding");
    }
}

void ux3d::renderer::CommandExecutor::setupDepthTest(const Command& command)
{
    if (command.depthTestEnabled) {
        if (!m_depthTestEnabled.has_value() || !m_depthTestEnabled.value()) {
            m_depthTestEnabled = true;
            m_gl->enable(GL_DEPTH_TEST);
        }
        if (!m_depthFunc.has_value() || m_depthFunc.value() != GL_LEQUAL) {
            m_depthFunc = GL_LEQUAL;
            m_gl->depthFunc(GL_LEQUAL);
        }
    } else {
        if (!m_depthTestEnabled.has_value() || m_depthTestEnabled.value()) {
            m_depthTestEnabled = false;
            m_gl->disable(GL_DEPTH_TEST);
        }
    }

    checkGLError(m_gl, "Failed to set depth test");
}

bool ux3d::renderer::CommandExecutor::activateProgram(const Command& command)
{
    if (command.program == nullptr) {
        return false;
    }

    command.program->use();

    return true;
}
