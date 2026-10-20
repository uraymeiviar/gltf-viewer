// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "Command.h"
#include "CommandExecutor.h"
#include "GpuProfiling.h"
#include "core/Vector.h"
#include "math/VectorTypes.h"

#include <cstdint>
#include <string>
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

    void setGpuProfilingEnabled(bool enabled) { m_gpuProfiler.setEnabled(enabled); }
    bool gpuProfilingEnabled() const { return m_gpuProfiler.enabled(); }
    void setGpuFrameContext(uint64_t frame, uint32_t viewport, const std::string& label)
    {
        m_gpuFrame = frame;
        m_gpuViewport = viewport;
        m_gpuLabel = label;
    }
    const GpuProfiler& getGpuProfiler() const { return m_gpuProfiler; }

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
    GpuProfiler m_gpuProfiler;
    uint64_t m_gpuFrame = 0;
    uint32_t m_gpuViewport = 0;
    std::string m_gpuLabel = "main";
};
}
