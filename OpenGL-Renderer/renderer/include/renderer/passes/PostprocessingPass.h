// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#pragma once

#include "renderer/RenderPass.h"

namespace ux3d::renderer {
class PostprocessingPass : public RenderPass {
public:
    PostprocessingPass(uint32_t _id = RenderPassType::Postprocessing, const core::string& _name = "Postprocessing");
    virtual ~PostprocessingPass() = default;
};
}
