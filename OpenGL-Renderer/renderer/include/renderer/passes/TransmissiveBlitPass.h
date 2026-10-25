// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#pragma once

#include "math/VectorTypes.h"
#include "renderer/RenderPass.h"
#include "renderer/gl/GL.h"

namespace ux3d::renderer {
class TransmissiveBlitPass : public RenderPass {
public:
    TransmissiveBlitPass(gl::GL* gl, uint32_t _id = RenderPassType::TransmissiveBlit, const core::string& _name = "TransmissiveBlit");
    void setClearColor(math::vec4 c) { m_postCommands[0].clearColor = {c.r, c.g, c.b, c.a}; }
    virtual ~TransmissiveBlitPass() = default;
    void setBlitFramebufferFunction(std::function<void()> _blitFramebuffer, math::vec4 _clearColor);

private:
    gl::GL* m_gl;
};
}
