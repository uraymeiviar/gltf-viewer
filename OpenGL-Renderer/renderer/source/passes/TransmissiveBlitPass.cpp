// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#include "renderer/passes/TransmissiveBlitPass.h"

ux3d::renderer::TransmissiveBlitPass::TransmissiveBlitPass(gl::GL* gl, uint32_t _id, const core::string& _name)
    : RenderPass(_id, _name)
    , m_gl(gl)
{
}

void ux3d::renderer::TransmissiveBlitPass::setBlitFramebufferFunction(std::function<void()> _blitFramebuffer, math::vec4 _clearColor)
{
    (void)_clearColor;
    m_preCommands.clear();
    m_postCommands.clear();
    m_preCommands.emplace_back(Command::makeExternalFunctionCommand(m_gl, _blitFramebuffer));
    m_postCommands.emplace_back(Command::makeDepthTestCommand(m_gl, true));
    m_postCommands.emplace_back(Command::makeBlendCommand(m_gl, false));
}
