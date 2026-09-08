// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#include "renderer/RenderPass.h"

#include <algorithm>

ux3d::renderer::RenderPass::RenderPass(uint32_t _id, const core::string& _name)
    : m_id(_id)
    , m_name(_name)
{
}

void ux3d::renderer::RenderPass::process(Renderer& renderer, SortedCommands& commands)
{
    std::sort(commands.begin(), commands.end(), [](const SortedCommand& l, const SortedCommand& r) { return l.sortKey < r.sortKey; });
}
