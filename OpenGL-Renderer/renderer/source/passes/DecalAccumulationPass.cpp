// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "renderer/passes/DecalAccumulationPass.h"

#include <algorithm>

namespace {
// A0-A4: colour = src * 1 + dst * (1 - srcAlpha), alpha (the accumulated T) = dst * (1 -
// srcAlpha). The separate alpha factors are what store T' = T * (1 - w) even though the
// shader's alpha is the weight w. A5 is the occlusion product: src * dst.
void setupDecalBlending(ux3d::renderer::gl::GL* gl)
{
    for (GLuint buffer = 0; buffer < 5; ++buffer) {
        gl->enablei(GL_BLEND, buffer);
        gl->blendEquationi(buffer, GL_FUNC_ADD);
        gl->blendFuncSeparatei(buffer, GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
    }
    gl->enablei(GL_BLEND, 5);
    gl->blendEquationi(5, GL_FUNC_ADD);
    gl->blendFunci(5, GL_DST_COLOR, GL_ZERO);
}

// Indexed blend state is sticky per draw buffer; restore the default framebuffer's
// conventional over blend so the transparent pass and the UI keep the state they had.
void restoreDefaultBlending(ux3d::renderer::gl::GL* gl)
{
    for (GLuint buffer = 0; buffer < 6; ++buffer) {
        gl->disablei(GL_BLEND, buffer);
        gl->blendEquationi(buffer, GL_FUNC_ADD);
        gl->blendFunci(buffer, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    gl->blendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
}

ux3d::renderer::DecalAccumulationPass::DecalAccumulationPass(gl::GL* gl, uint32_t _id, const core::string& _name)
    : RenderPass(_id, _name)
{
    // A2.2: no fixed-function depth test. The decal shader samples the prepass depth and
    // applies a tolerance of a few millimetres, because the legends are coplanar with their
    // button faces and a LEQUAL test against the prepass z-fights. Real occluders in front
    // are still rejected by the shader's discard.
    m_preCommands.emplace_back(Command::makeDepthTestCommand(gl, false));
    m_preCommands.emplace_back(Command::makeBlendCommand(gl, true));
    // The external pre/post commands bind the accumulation target and restore the viewport
    // target (set by the application, like the picking and prepass layers). The blend
    // setup runs after the generic Blend command so its indexed state is not overwritten.
    m_preCommands.emplace_back(Command::makeExternalFunctionCommand(gl, [this, gl] {
        if (prePass) {
            prePass();
        }
        setupDecalBlending(gl);
    }));
    m_postCommands.emplace_back(Command::makeExternalFunctionCommand(gl, [this, gl] {
        restoreDefaultBlending(gl);
        // The accumulation bind disabled depth writes; the forward opaque pass must get
        // them back even though the executor's cache says they are on.
        gl->depthMask(GL_TRUE);
        if (postPass) {
            postPass();
        }
    }));
}

void ux3d::renderer::DecalAccumulationPass::process(Renderer& renderer, SortedCommands& commands)
{
    // A1.4 decal order: drawOrderOffset ascending (higher lands on top), then far to near.
    // Stable for equal offsets and distances so the file's primitive order decides.
    std::stable_sort(commands.begin(), commands.end(), [](const SortedCommand& l, const SortedCommand& r) {
        if (l.drawOrderOffset != r.drawOrderOffset) {
            return l.drawOrderOffset < r.drawOrderOffset;
        }
        return l.sortKey > r.sortKey;
    });
}
