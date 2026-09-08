// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#pragma once

#include "renderer/RenderPass.h"
#include "renderer/gl/GL.h"

namespace ux3d::renderer {
class OpaquePass : public RenderPass {
public:
    OpaquePass(gl::GL* gl, uint32_t _id = RenderPassType::Opaque, const core::string& _name = "Opaque");
    virtual ~OpaquePass() = default;

private:
    void process(Renderer& _renderer, SortedCommands& _cmds) override;
};
}
