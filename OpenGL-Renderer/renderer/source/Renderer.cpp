// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/Renderer.h"
#include "renderer/CommandGenerator.h"
#include "renderer/gl/Util.h"
#include "renderer/DrawTrace.h"

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

    // C15: glGetError makes a threaded driver wait for its queue, so it runs at the first draw of a frame; an error from
    // this frame's draws is logged by the next frame's check. A draw outside the frame loop (frame 0) keeps its own check.
    if (m_gpuFrame == 0 || m_gpuFrame != m_glCheckedFrame) {
        m_glCheckedFrame = m_gpuFrame;
        if (!checkGLError(m_gl, "Renderer::draw")) {
            m_commandExecutor.resetState();
            m_gpuProfiler.endFrame();
            return false;
        }
    }

    if (!preDrawScenes()) {
        m_commandExecutor.resetState();
        m_gpuProfiler.endFrame();
        return false;
    }

    if (!onDrawScenes(scenes)) {
        m_commandExecutor.resetState();
        m_gpuProfiler.endFrame();
        return false;
    }

    if (!postDrawScenes()) {
        m_commandExecutor.resetState();
        m_gpuProfiler.endFrame();
        return false;
    }

    m_commandExecutor.resetState();
    m_gpuProfiler.endFrame();
    return true;
}

void ux3d::renderer::Renderer::uninit()
{
    m_commandExecutor.resetState();
    m_commandExecutor.getSamplerCache().clear();
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
    // C15: the list keeps its strings between draws, so the pass names of a repeated frame allocate nothing.
    size_t executedPasses = 0;
    std::string unnamedPass;
    for (auto& scene : scenes) {
        scene->bindFrameBlocks();
        uint32_t passIndex = 0;
        for (Pass& passType : scene->getPassCommands()) {
            for (auto& [pass, commands] : passType) {
                if (!pass) continue;
                const bool named = !pass->getName().empty();
                if (!named) {
                    unnamedPass = "pass" + std::to_string(passIndex);
                }
                const std::string& passName = named ? pass->getName() : unnamedPass;
                m_gpuProfiler.beginPass(passName);
                if (drawtrace::active) {
                    drawtrace::stage = m_gpuLabel;
                    drawtrace::viewport = m_gpuViewport;
                    drawtrace::pass = passName;
                }
                if (!commands.empty()) {
                    if (executedPasses < m_lastExecutedPasses.size()) {
                        m_lastExecutedPasses[executedPasses] = passName;
                    } else {
                        m_lastExecutedPasses.push_back(passName);
                    }
                    ++executedPasses;
                }

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
                drawtrace::pass.clear();
                m_gpuProfiler.endPass();
            }
            ++passIndex;
        }
    }
    m_lastExecutedPasses.resize(executedPasses);

    return true;
}

bool ux3d::renderer::Renderer::postDrawScenes()
{
    for (const Command& command : postRenderCommands) {
        m_commandExecutor.execute(command);
    }
    return true;
}
