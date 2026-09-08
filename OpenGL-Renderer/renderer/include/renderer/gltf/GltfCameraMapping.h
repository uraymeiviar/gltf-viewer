// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include <tuple>

#include "GltfMapping.h"
#include "renderer/gl/GL.h"

namespace ux3d::gltf {
class Node;
class Camera;
}

namespace ux3d::filesystem {
class Path;
}

namespace ux3d::renderer::gl {
struct SamplerState;
class Texture;
}

namespace ux3d::renderer {
class Camera;
class Material;
class Mesh;

using GltfCameraTuple = std::tuple<const ux3d::gltf::Node*, const ux3d::gltf::Camera*>;
using RendererCameraTuple = std::tuple<Camera*>;
class GltfCameraMapping : public GltfMapping<GltfCameraTuple, RendererCameraTuple> {
public:
    GltfCameraMapping(GltfRenderLayer& scene);

protected:
    bool onUpdateMapping(GltfCameraTuple gltfTuple, RendererCameraTuple rendererTuple) final;
    RendererCameraTuple onCreateMapping(GltfCameraTuple gltfTuple) final;
    void onDestroyMapping(GltfCameraTuple gltfTuple, RendererCameraTuple rendereTuple) final;
};
}
