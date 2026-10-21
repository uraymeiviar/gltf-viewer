// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include "gl/GL.h"

#include "core/Hash.h"
#include "core/LinkedList.h"
#include "core/TupleHash.h"
#include <optional>
#include <vector>

namespace ux3d::renderer::gl {
class ShaderCache;
}

namespace ux3d::renderer {
class CommandGenerator;
struct CmdDesc;
class Mesh;
class Material;
using MeshMaterialTuple = std::tuple<Mesh*, Material*>;
class CommandMapping {
public:
    CommandMapping(gl::GL* gl, CommandGenerator& scene, gl::ShaderCache& shaderCache);
    void update(const core::LinkedList<MeshMaterialTuple>& drawables, std::vector<CmdDesc>& commands);

protected:
    std::optional<CmdDesc> makeCommand(MeshMaterialTuple meshMat);

protected:
    gl::GL* m_gl;
    gl::ShaderCache& m_shaderCache;
    CommandGenerator& m_scene; // TODO: Rename
};
}
