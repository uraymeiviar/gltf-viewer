// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include <vector>

#include "GltfType.h"
#include "core/Vector.h"
#include "math/VectorTypes.h"

#include <string>

namespace ux3d::gltf {
class Scene;
class Node;
class Gltf;
class Accessor;
class Image;
class ChildOfRoot;

struct Extends {
    math::dvec3 min;
    math::dvec3 max;
    math::dvec3 center;
};

struct ClippingPlanes {
    double zNear = 0;
    double zFar = -1;
};
}

namespace ux3d::gltf::utils {
using NodeTransforms = core::Vector<std::tuple<const Node*, math::dmat4>>;

double getFittingCameraZoom(const Extends& _Extends, double _yFov, double _AspectRatio);
ClippingPlanes getFittingClippingPlanes(const Extends& _Extends, double _Zoom);
// _geometryOnly: only drawn mesh geometry counts (no camera, light or joint positions, no primitives of MSFS
// invisible / environment-occluder materials), unless the nodes have none.
// _points (optional) receives every point the box was made from: the 8 world corners of each primitive's
// POSITION box (per GPU instance) and the counted camera, light and joint positions. Framing fits these, which
// follow the model's shape far more closely than the 8 corners of the overall box.
Extends getExtends(const core::Vector<Node*>& _nodes, const Gltf* _pGltf, const math::dmat4& _rootTransform = math::identity<math::dmat4>(),
    bool _geometryOnly = false, std::vector<math::dvec3>* _points = nullptr);
Extends getAccessorExtends(const Accessor& _Accessor, const math::dmat4& _Transform);
void fillNodeTransforms(const core::Vector<Node*>& _Nodes, NodeTransforms& _NodeTransforms, const math::dmat4& _Root = math::identity<math::dmat4>());
bool isNodeUsedAsJointInSkin(const Gltf* _pGltf, const Node* _pNode);
void searchAndReplace(std::string& str, const std::string& search, const std::string& replace);
bool isURIUsed(const std::string& uri, const Gltf& gltf);
bool hasURICollision(const ChildOfRoot& object, GltfType type, const std::string& uri, const Gltf& gltf);

static uint32_t m_uniqueIdCounter;
}
