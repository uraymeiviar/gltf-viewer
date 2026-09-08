// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "renderer/gl/Enums.h"
#include "renderer/gl/GL.h"

namespace ux3d::renderer {
class Command;

class CommandExecutor {
public:
    CommandExecutor(gl::GL*);
    CommandExecutor(CommandExecutor&&) noexcept = default;
    CommandExecutor& operator=(CommandExecutor&&) = default;

    void init();
    void resetState();
    void execute(const Command& command);

private:
    void draw(const Command& command);
    void clear(const Command& command) const;
    bool bindFrameBuffer(const Command& command) const;
    bool activateProgram(const Command& command);

    void setupVertexArray(const Command& command);
    void setupBlendFunc(const Command& command) const;
    void setupCulling(const Command& command) const;
    void setupPolygonMode(const Command& command) const;
    void setupFrontFace(const Command& command) const;
    void setupDepthTest(const Command& command) const;
    void setupScissorTest(const Command& command) const;
    void readPixel(const Command& command) const;

private:
    gl::GL* m_gl;
    GLint m_maxTextureUnits;
    GLuint m_activeVertexArray = gl::invalid;
};
}
