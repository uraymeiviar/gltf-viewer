// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#pragma once

#include "renderer/RenderPass.h"
#include "renderer/gl/GL.h"

namespace ux3d::renderer {
class TransmissivePass : public RenderPass {
public:
    TransmissivePass(gl::GL* gl, uint32_t _id = RenderPassType::Transmissive, const core::string& _name = "Transmissive");
    void setTransmissiveTextureFunction(std::function<void()> _transmissiveTexture);
    virtual ~TransmissivePass() = default;

private:
    gl::GL* m_gl;
};
}
