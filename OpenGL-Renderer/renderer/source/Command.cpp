// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/Command.h"
#include "renderer/gl/Program.h"

ux3d::renderer::Command::Command(gl::GL* gl, CommandTag tag)
    : tag(tag)
{
}

auto ux3d::renderer::Command::makeExternalFunctionCommand(gl::GL* gl, std::function<void()> externalFunction) -> Command
{
    Command cmd(gl, CommandTag::ExternalFunction);
    cmd.externalFunction = externalFunction;
    return cmd;
}

auto ux3d::renderer::Command::makeClearCommand(gl::GL* gl, gl::ClearTarget target, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) -> Command
{
    Command cmd(gl, CommandTag::Clear);
    cmd.clearTarget = target;
    cmd.clearColor = { red, green, blue, alpha };
    return cmd;
}

auto ux3d::renderer::Command::makeViewportCommand(gl::GL* gl, int width, int height) -> Command
{
    Command cmd(gl, CommandTag::Viewport);
    cmd.viewportWidth = width;
    cmd.viewportHeight = height;
    return cmd;
}

auto ux3d::renderer::Command::makeFramebufferCommand(gl::GL* gl, const gl::FrameBuffer* framebuffer) -> Command
{
    Command cmd(gl, CommandTag::Framebuffer);
    cmd.framebuffer = framebuffer;
    return cmd;
}

auto ux3d::renderer::Command::makeBlendCommand(gl::GL* gl, bool enable) -> Command
{
    Command cmd(gl, CommandTag::Blend);
    cmd.blendingEnabled = enable;
    return cmd;
}

auto ux3d::renderer::Command::makeDepthTestCommand(gl::GL* gl, bool enable) -> Command
{
    Command cmd(gl, CommandTag::DepthTest);
    cmd.depthTestEnabled = enable;
    return cmd;
}
