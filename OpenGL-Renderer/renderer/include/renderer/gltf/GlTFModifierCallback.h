// Copyright (c) 2020 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Vector.h"
#include <functional>

// forward decls
namespace ux3d::gltf {
class Node;
class Mesh;
class Material;
class Primitive;
} //! ux3d::gltf

namespace ux3d::renderer {
class GltfRenderLayer;
struct CommandModifier;

// return true to render generate a draw call for the _pNode, _pMesh, _pPrimitive (and its Material) combination
// _pScene is the the scene calling this callback
// pass a out reference to a vector of modifiers
// getModifier implementation should reteurn a valid pointer to a CommandModifier object via _pOutModifier (created by scene::addFrameCommandModifier())
using GlTFModifierCallback = std::function<bool(renderer::GltfRenderLayer* _pScene, const gltf::Node* _pNode, std::optional<CommandModifier>& _modifier)>;
} // !ux3d::renderer
