// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#include "renderer/passes/TransmissivePass.h"

ux3d::renderer::TransmissivePass::TransmissivePass(gl::GL* gl, uint32_t _id, const core::string& _name)
    : m_gl(gl)
    , RenderPass(_id, _name)
{
}

void ux3d::renderer::TransmissivePass::setTransmissiveTextureFunction(std::function<void()> _transmissiveTexture)
{
    m_preCommands.clear();
    m_preCommands.emplace_back(Command::makeDepthTestCommand(m_gl, true));
    m_preCommands.emplace_back(Command::makeBlendCommand(m_gl, true));
    m_preCommands.emplace_back(Command::makeExternalFunctionCommand(m_gl, _transmissiveTexture));
}
