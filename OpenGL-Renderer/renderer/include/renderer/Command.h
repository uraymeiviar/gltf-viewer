// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "gl/Enums.h"
#include "gl/Sampler.h"
#include "gl/Uniform.h"
#include "gl/UniformBlock.h"
#include "gl/VertexLayout.h"

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
    bool depthTestEnabled = false;

    gl::PrimitiveTopology primitiveTopology = gl::PrimitiveTopology::TriangleList;
    GLsizei primitiveCount = 0u;
    GLint primitiveOffset = 0u;

    gl::PrimitiveType indexFormat = gl::PrimitiveType::UnsignedShort;
    GLint indexByteOffset = 0u;
    const gl::Buffer* indexBuffer = nullptr;

    std::vector<const gl::Uniform*> uniforms;
    std::vector<const gl::UniformBlock*> uniformBlocks;
    std::vector<const gl::Sampler*> samplers;

    const gl::Program* program = nullptr;
    const gl::FrameBuffer* framebuffer = nullptr;

    std::shared_ptr<gl::VertexLayout> vertexLayout;

    std::function<void()> externalFunction = []() {};
};

struct SortedCommand {
    SortedCommand(Command* command, float sortKey)
        : command(command)
        , sortKey(sortKey)
    {
    }

    Command* command;
    float sortKey;
};

using SortedCommands = std::vector<SortedCommand>;
}
