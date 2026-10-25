// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "renderer/RenderPass.h"
#include "renderer/gl/GL.h"

#include <functional>

namespace ux3d::renderer {
// A2.2: draws the decals into the shared accumulation targets after the prepass and
// before the forward opaque pass. Every target has its own blend rule (A0-A4 over, A5
// multiply), which OpenGL 4.1 expresses with per-draw-buffer blending. The pass also
// needs the A1.4 decal order, so it replaces the generic distance sort.
class DecalAccumulationPass : public RenderPass {
public:
    DecalAccumulationPass(gl::GL* gl, uint32_t _id = RenderPassType::DecalAccumulation,
                          const core::string& _name = "DecalAccumulation");
    virtual ~DecalAccumulationPass() = default;

    // Bind the accumulation target before the draws and restore the viewport target
    // afterwards. Set by the application, like DecalPrepassLayer's callbacks.
    std::function<void()> prePass;
    std::function<void()> postPass;

private:
    void process(Renderer& _renderer, SortedCommands& _cmds) override;
};
}
