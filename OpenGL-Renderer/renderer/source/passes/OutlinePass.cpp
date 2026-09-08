// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#include "renderer/passes/OutlinePass.h"

using namespace ux3d;
using namespace ux3d::renderer;

OutlinePass::OutlinePass(gl::GL* gl, uint32_t _id, const core::string& _name)
    : RenderPass(_id, _name)
    , m_gl(gl)
{
    // set clear color that can be identified (alpha==0) when finally drawing the per-pixel outline
    m_preCommands.emplace_back(Command::makeClearCommand(m_gl, gl::ClearTarget::All, 0, 0, 0, 0));

    // disable depth testing
    m_preCommands.emplace_back(Command::makeDepthTestCommand(gl, false));
    m_preCommands.emplace_back(Command::makeExternalFunctionCommand(gl, [gl = m_gl]() {
        gl->colorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    }));
}

void ux3d::renderer::OutlinePass::setBlitFramebufferFunction(std::function<void()> _blitFramebuffer, math::vec4 _clearColor)
{
    m_postCommands.clear();

    m_postCommands.emplace_back(Command::makeExternalFunctionCommand(m_gl, [gl = m_gl]() {
        gl->colorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }));
    m_postCommands.emplace_back(Command::makeExternalFunctionCommand(m_gl, _blitFramebuffer));
    m_postCommands.emplace_back(Command::makeClearCommand(m_gl, gl::ClearTarget::All, _clearColor.r, _clearColor.g, _clearColor.b, _clearColor.a));
}
