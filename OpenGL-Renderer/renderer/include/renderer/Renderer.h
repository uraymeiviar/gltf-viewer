// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "Command.h"
#include "CommandExecutor.h"
#include "core/Vector.h"
#include "math/VectorTypes.h"

#include <vector>

namespace ux3d::renderer {
class CommandGenerator;

class Renderer {
public:
    Renderer(gl::GL*);
    Renderer(Renderer&&) = default;

    bool init(math::vec4 clearColor);
    bool draw(const core::Vector<CommandGenerator*>& scenes);
    void uninit();

    std::vector<Command> clearCommands;
    std::vector<Command> preRenderCommands;
    std::vector<Command> postRenderCommands;

    bool setViewportAndClear(int width, int height);

private:
    bool preDrawScenes();
    bool onDrawScenes(const core::Vector<CommandGenerator*>& scenes);
    bool postDrawScenes();

    gl::GL* m_gl;
    CommandExecutor m_commandExecutor;
};
}
