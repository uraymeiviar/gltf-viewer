// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include "GltfMapping.h"

#include <tuple>

namespace ux3d::gltf {
class Node;
class Light;
}

namespace ux3d::renderer {
struct Light;

using GltfLightTuple = std::tuple<const ux3d::gltf::Node*, const ux3d::gltf::Light*>;
using RendererLightTuple = std::tuple<Light*>;
class GltfLightMapping : public GltfMapping<GltfLightTuple, RendererLightTuple> {
public:
    GltfLightMapping(GltfRenderLayer& scene);

protected:
    bool onUpdateMapping(GltfLightTuple gltfLight, RendererLightTuple light) final;
    RendererLightTuple onCreateMapping(GltfLightTuple gltfLight) final;
    void onDestroyMapping(GltfLightTuple gltfLight, RendererLightTuple light) final;
};
}
