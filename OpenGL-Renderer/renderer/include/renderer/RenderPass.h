// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#pragma once

#include "Command.h"
#include "RenderPassDefines.h"
#include "core/String.h"
#include <vector>

namespace ux3d::renderer {
class Renderer;

class RenderPass {
public:
    RenderPass(uint32_t _id = RenderPassType::MaxRenderPasses, const core::string& _name = "Invalid");
    virtual ~RenderPass() = default;

    // maybe make them private and use friend class with Scene
    virtual void process(Renderer& _renderer, SortedCommands& _cmds); // default sort by l.sortKey < r.sortKey;
    virtual void deinit(Renderer& _renderer) { }

    uint32_t getId() const { return m_id; }
    void setId(uint32_t _id) { m_id = _id; }

    const core::string& getName() const { return m_name; }
    void setName(const core::string& _name) { m_name = _name; }

    std::vector<Command>& getPreCommands() { return m_preCommands; }
    const std::vector<Command>& getPreCommands() const { return m_preCommands; }
    std::vector<Command>& getPostCommands() { return m_postCommands; }
    const std::vector<Command>& getPostCommands() const { return m_postCommands; }

    // call order:
    // for each Command& cmd : getPreCommands() {render(cmd);}
    // RenderPass::process(scene->pass.commands)
    // render sorted pass commands (mostly just draw calls)
    // for each Command& cmd : m_postCommands() {render(cmd);}
    // RenderPass::deinit()

protected:
    uint32_t m_id;
    core::string m_name; // just for debugging

    std::vector<Command> m_preCommands;
    std::vector<Command> m_postCommands;
};
}
