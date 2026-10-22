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
}

void ux3d::renderer::CommandExecutor::resetState()
{
    unbindSamplers();
    m_activeVertexArray = gl::invalid;
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

    for (const auto& uniform : command.uniforms) {
        if (uniform != nullptr) {
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

        command.program->commitSampler(*sampler, i, samplerHandle);
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
        m_gl->drawElements(
            static_cast<GLenum>(command.primitiveTopology),
            command.primitiveCount,
            static_cast<GLenum>(command.indexFormat),
            reinterpret_cast<const void*>(static_cast<size_t>(command.indexByteOffset)));
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

void ux3d::renderer::CommandExecutor::setupBlendFunc(const Command& command) const
{
    if (command.blendingEnabled) {
        m_gl->enable(GL_BLEND);
        m_gl->blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        m_gl->blendEquation(GL_FUNC_ADD);
    } else {
        m_gl->disable(GL_BLEND);
    }

    GLIDER_HOT_GL_CHECK(m_gl, "Failed to set blend mode");
}

void ux3d::renderer::CommandExecutor::setupCulling(const Command& command) const
{
    if (command.culling != gl::Culling::None) {
        m_gl->enable(GL_CULL_FACE);
        m_gl->cullFace(GLenum(command.culling));
    } else {
        m_gl->disable(GL_CULL_FACE);
    }

    GLIDER_HOT_GL_CHECK(m_gl, "Failed to set culling mode");
}

void ux3d::renderer::CommandExecutor::setupPolygonMode(const Command& command) const
{
    m_gl->polygonMode(GL_FRONT_AND_BACK, GLenum(command.polygonMode));

    GLIDER_HOT_GL_CHECK(m_gl, "Failed to set polygon mode");
}

void ux3d::renderer::CommandExecutor::setupFrontFace(const Command& command) const
{
    m_gl->frontFace(GLenum(command.winding));
}

void ux3d::renderer::CommandExecutor::setupDepthTest(const Command& command) const
{
    if (command.depthTestEnabled) {
        m_gl->enable(GL_DEPTH_TEST);
        m_gl->depthFunc(GL_LEQUAL);
    } else {
        m_gl->disable(GL_DEPTH_TEST);
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
