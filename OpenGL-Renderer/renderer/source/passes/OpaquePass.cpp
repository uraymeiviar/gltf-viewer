// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#include "renderer/passes/OpaquePass.h"

#include <algorithm>

ux3d::renderer::OpaquePass::OpaquePass(gl::GL* gl, uint32_t _id, const core::string& _name)
    : RenderPass(_id, _name)
{
    m_preCommands.emplace_back(Command::makeDepthTestCommand(gl, true));
    m_preCommands.emplace_back(Command::makeBlendCommand(gl, false));
}

void ux3d::renderer::OpaquePass::process(Renderer& renderer, SortedCommands& commands)
{
    // sort near (close to camera) to far to reduce overdraw
    std::sort(commands.begin(), commands.end(), [](const SortedCommand& l, const SortedCommand& r) { return l.sortKey < r.sortKey; });
    // Draw-first commands (the background while a ground surface is drawn) go to the front. Stable, and applied after
    // the unchanged sort, so every other draw keeps exactly its previous order (equal sort keys of coplanar model
    // parts decide which of them wins under GL_LEQUAL; a different sort key would reshuffle them).
    std::stable_partition(commands.begin(), commands.end(), [](const SortedCommand& c) { return c.drawFirst; });
}
