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
    // A1.4: without a covered ASOBO_material_draw_order draw, keep the original order exactly.
    const bool hasDrawOrder = std::any_of(commands.begin(), commands.end(), [](const SortedCommand& command) {
        return command.drawOrderClass != DrawOrderClass::None;
    });
    if (!hasDrawOrder) {
        // sort far to near to blend
        std::sort(commands.begin(), commands.end(), [](const SortedCommand& l, const SortedCommand& r) { return l.sortKey > r.sortKey; });
        return;
    }

    std::sort(commands.begin(), commands.end(), [](const SortedCommand& l, const SortedCommand& r) {
        const bool lDecal = l.drawOrderClass == DrawOrderClass::Decal;
        const bool rDecal = r.drawOrderClass == DrawOrderClass::Decal;
        // Decals land on the surface before glass is drawn (A2 will make them deferred).
        if (lDecal != rDecal) {
            return lDecal;
        }
        if (lDecal) {
            // Decals: the offset strictly specifies the order, higher drawn later (above);
            // equal offsets keep the normal back-to-front distance order.
            if (l.drawOrderOffset != r.drawOrderOffset) {
                return l.drawOrderOffset < r.drawOrderOffset;
            }
        }
        // Glass/windshield keys already contain their offset; all remaining draws keep the
        // previous distance key. Larger keys are farther and are drawn first.
        return l.sortKey > r.sortKey;
    });
}
