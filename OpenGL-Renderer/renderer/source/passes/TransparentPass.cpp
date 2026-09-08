// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#include "renderer/passes/TransparentPass.h"

#include <algorithm>

ux3d::renderer::TransparentPass::TransparentPass(gl::GL* gl, uint32_t _id, const core::string& _name)
    : RenderPass(_id, _name)
{
    m_preCommands.emplace_back(Command::makeDepthTestCommand(gl, true));
    m_preCommands.emplace_back(Command::makeBlendCommand(gl, true));
}

void ux3d::renderer::TransparentPass::process(Renderer& renderer, SortedCommands& commands)
{
    // sort far to near to blend
    std::sort(commands.begin(), commands.end(), [](const SortedCommand& l, const SortedCommand& r) { return l.sortKey > r.sortKey; });
}
