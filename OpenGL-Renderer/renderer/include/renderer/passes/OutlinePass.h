// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#pragma once

#include "math/VectorTypes.h"
#include "renderer/RenderPass.h"
#include "renderer/gl/GL.h"

namespace ux3d::renderer {
class OutlinePass : public RenderPass {
public:
    OutlinePass(gl::GL* gl, uint32_t _id = RenderPassType::Outline, const core::string& _name = "Outline");
    virtual ~OutlinePass() = default;
    void setBlitFramebufferFunction(std::function<void()> _blitFramebuffer, math::vec4 _clearColor);

private:
    gl::GL* m_gl;
};
}
