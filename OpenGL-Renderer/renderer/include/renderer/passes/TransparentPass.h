// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#pragma once

#include "renderer/RenderPass.h"

namespace ux3d::renderer {
class TransparentPass : public RenderPass {
public:
    TransparentPass(gl::GL* gl, uint32_t _id = RenderPassType::Transparent, const core::string& _name = "Transparent");
    virtual ~TransparentPass() = default;

private:
    void process(Renderer& _renderer, SortedCommands& _cmds) override;
};
}
