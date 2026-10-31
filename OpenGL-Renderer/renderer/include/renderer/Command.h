// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "gl/Enums.h"
#include "gl/Sampler.h"
#include "gl/Uniform.h"
#include "gl/UniformBlock.h"
#include "gl/VertexLayout.h"
#include "renderer/DrawOrder.h"

#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace ux3d::renderer::gl {
class Program;
class FrameBuffer;
class Buffer;
}

namespace ux3d::renderer {

enum class CommandTag {
    Clear,
    Draw,
    Viewport,
    Framebuffer,
    Blend,
    DepthTest,
    UpdateUniformBlocks,
    ExternalFunction
};

class Command {
public:
    Command(gl::GL*, CommandTag tag);

    static Command makeExternalFunctionCommand(gl::GL*, std::function<void()> externalFunction);
    static Command makeClearCommand(gl::GL*, gl::ClearTarget target, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
    static Command makeViewportCommand(gl::GL*, int width, int height);
    static Command makeFramebufferCommand(gl::GL*, const gl::FrameBuffer* framebuffer);
    static Command makeBlendCommand(gl::GL*, bool enable);
    static Command makeDepthTestCommand(gl::GL*, bool enable);

    CommandTag tag;

    int viewportWidth = 0;
    int viewportHeight = 0;

    gl::ClearTarget clearTarget = gl::ClearTarget::All;
    std::array<GLfloat, 4> clearColor = { 0.f, 0.f, 0.f, 0.f };

    gl::Culling culling = gl::Culling::Back;
    gl::Winding winding = gl::Winding::CounterClockwise;
    gl::PolygonMode polygonMode = gl::PolygonMode::Fill;
    bool blendingEnabled = false;
    bool premultipliedAlpha = false;
    bool overrideBlend = false;
    bool customBlend = false;
    GLenum blendSrcFactor = GL_SRC_ALPHA;
    GLenum blendDstFactor = GL_ONE_MINUS_SRC_ALPHA;
    GLenum blendEquation = GL_FUNC_ADD;
    bool logicOpEnabled = false;
    GLenum logicOp = GL_XOR;
    bool depthTestEnabled = false;
    // A1.4: covered transparent draws (decals, glass, windshield) do not write depth while
    // ASOBO_material_draw_order is active, so their order decides what is on top.
    bool depthWriteEnabled = true;
    // A2.2: decal draws get a polygon offset so a decal coplanar with its receiver wins the
    // LEQUAL depth test instead of z-fighting (forward fallback and accumulation pass).
    bool polygonOffsetEnabled = false;
    GLfloat polygonOffsetFactor = 0.0f;
    GLfloat polygonOffsetUnits = 0.0f;

    gl::PrimitiveTopology primitiveTopology = gl::PrimitiveTopology::TriangleList;
    GLsizei primitiveCount = 0u;
    GLint primitiveOffset = 0u;

    gl::PrimitiveType indexFormat = gl::PrimitiveType::UnsignedShort;
    GLint indexByteOffset = 0u;
    GLint baseVertex = 0;
    // P8.3: > 0 draws this many instances (glDraw*Instanced); 0 is an ordinary draw.
    GLsizei instanceCount = 0;
    const gl::Buffer* indexBuffer = nullptr;

    std::vector<const gl::Uniform*> uniforms;
    std::vector<const gl::UniformBlock*> uniformBlocks;
    std::vector<const gl::Sampler*> samplers;
    // P8.1a: program bindings resolved when the command is built, parallel to uniforms
    // and samplers (nullptr: not active in the program). Commands built elsewhere leave
    // them empty and the executor looks the names up per draw.
    std::vector<const gl::UniformBinding*> uniformBindings;
    std::vector<const gl::UniformBinding*> samplerBindings;

    const gl::Program* program = nullptr;
    const gl::FrameBuffer* framebuffer = nullptr;

    std::shared_ptr<gl::VertexLayout> vertexLayout;

    std::function<void()> externalFunction = []() {};
};

struct SortedCommand {
    SortedCommand(Command* command, float sortKey, DrawOrderClass drawOrderClass = DrawOrderClass::None, int drawOrderOffset = 0)
        : command(command)
        , sortKey(sortKey)
        , drawOrderClass(drawOrderClass)
        , drawOrderOffset(drawOrderOffset)
    {
    }

    Command* command;
    float sortKey;
    // A1.4: ASOBO_material_draw_order, effective only for the transparent classes.
    DrawOrderClass drawOrderClass = DrawOrderClass::None;
    int drawOrderOffset = 0;
    // Material::drawFirstInPass: OpaquePass moves it to the front after sorting, leaving the others in sorted order.
    bool drawFirst = false;
};

using SortedCommands = std::vector<SortedCommand>;
}
