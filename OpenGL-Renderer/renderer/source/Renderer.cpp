// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/Renderer.h"
#include "renderer/CommandGenerator.h"
#include "renderer/gl/Util.h"

ux3d::renderer::Renderer::Renderer(gl::GL* gl)
    : m_gl(gl)
    , m_commandExecutor(gl)
    , m_gpuProfiler(gl)
{
}

bool ux3d::renderer::Renderer::init(math::vec4 clearColor)
{
    m_commandExecutor.init();
    preRenderCommands.clear();
    postRenderCommands.clear();
    clearCommands.clear();

    clearCommands.emplace_back(Command::makeClearCommand(m_gl, gl::ClearTarget::All, clearColor.r, clearColor.g, clearColor.b, clearColor.a));
    clearCommands.emplace_back(Command::makeDepthTestCommand(m_gl, true));

    return true;
}

bool ux3d::renderer::Renderer::draw(const core::Vector<CommandGenerator*>& scenes)
{
    m_gpuProfiler.beginFrame(m_gpuFrame, m_gpuViewport, m_gpuLabel);
    m_commandExecutor.resetState();

    if (!checkGLError(m_gl, "Renderer::draw")) {
        m_gpuProfiler.endFrame();
        return false;
    }

    if (!preDrawScenes()) {
        m_gpuProfiler.endFrame();
        return false;
    }

    if (!onDrawScenes(scenes)) {
        m_gpuProfiler.endFrame();
        return false;
    }

    if (!postDrawScenes()) {
        m_gpuProfiler.endFrame();
        return false;
    }

    m_gpuProfiler.endFrame();
    return true;
}

void ux3d::renderer::Renderer::uninit()
{
    preRenderCommands.clear();
    postRenderCommands.clear();
    clearCommands.clear();
}

bool ux3d::renderer::Renderer::preDrawScenes()
{
    for (const Command& command : preRenderCommands) {
        m_commandExecutor.execute(command);
    }
    return true;
}

bool ux3d::renderer::Renderer::setViewportAndClear(int width, int height)
{
    m_commandExecutor.execute(Command::makeViewportCommand(m_gl, width, height));
    m_commandExecutor.execute(Command::makeClearCommand(m_gl, gl::ClearTarget::All, 0, 0, 0, 0));

    for (const Command& command : preRenderCommands) {
        m_commandExecutor.execute(command);
    }

    for (const Command& command : clearCommands) {
        m_commandExecutor.execute(command);
    }

    return true;
}

bool ux3d::renderer::Renderer::onDrawScenes(const core::Vector<CommandGenerator*>& scenes)
{
    for (auto& scene : scenes) {
        uint32_t passIndex = 0;
        for (Pass& passType : scene->getPassCommands()) {
            for (auto& [pass, commands] : passType) {
                const std::string passName = pass->getName().empty()
                    ? ("pass" + std::to_string(passIndex))
                    : pass->getName();
                m_gpuProfiler.beginPass(passName);

                for (const auto& cmd : pass->getPreCommands()) {
                    m_commandExecutor.execute(cmd);
                }

                pass->process(*this, commands);

                for (const auto& cmd : commands) {
                    m_commandExecutor.execute(*cmd.command);
                }

                for (const auto& cmd : pass->getPostCommands()) {
                    m_commandExecutor.execute(cmd);
                }

                pass->deinit(*this); // clean up
                m_gpuProfiler.endPass();
            }
            ++passIndex;
        }
    }

    return true;
}

bool ux3d::renderer::Renderer::postDrawScenes()
{
    for (const Command& command : postRenderCommands) {
        m_commandExecutor.execute(command);
    }
    return true;
}
