// Copyright (c) 2019 UX3D GmbH. All rights reserved.

// Private to the GltfRenderLayer*.cpp files: their shared includes and the helpers one of them defines and
// another calls. animation/Sampler.h is included by GltfRenderLayerNodes.cpp only (its explicit
// specialisations are not inline).

#pragma once

#include "renderer/gltf/GltfRenderLayer.h"
#include "animation/Animation.h"
#include "core/Result.h"
#include "imageformats/imageformats.h"
#include "core/LoadProgress.h"
#include "renderer/CommandGenerator.h"
#include "renderer/CommandMapping.h"
#include "renderer/EnvironmentMap.h"
#include "renderer/Renderer.h"
#include "renderer/editor/EditorRenderLayer.h"
#include "renderer/gl/Attribute.h"
#include "renderer/gl/Util.h"
#include "renderer/passes/OpaquePass.h"
#include "renderer/passes/TransmissivePass.h"
#include "renderer/passes/TransparentPass.h"

#include "gltf/extensions/AsoboExtensions.h"
#include "gltf/GltfUtils.h"
#include "gltf/mikktspace/mikktspace.h"
#include "gltf/mesh/Primitive.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <future>
#include <thread>
#include <chrono>
#include <memory>
#include <limits>
#include <vector>
#include <variant>

using namespace ux3d;
using namespace ux3d::renderer;

namespace ux3d::renderer {

// Accessor data as floats (GltfRenderLayerMeshes.cpp); also used by the animation channels.
bool convert(const gltf::Accessor& accessor, std::vector<float>& output, bool applyPadding);

// The A2.2 material-level tangent key (GltfRenderLayerMeshes.cpp).
uint32_t materialNormalUvSet(const gltf::Material& material, bool imagesOmitted);

}
