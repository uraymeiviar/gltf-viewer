// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include "renderer/Mapping.h"

#include <tuple>

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
class Material;
class Mesh;
class EditorRenderLayer;

using GltfCameraTuple = std::tuple<const ux3d::gltf::Node*, const ux3d::gltf::Camera*>;
using CameraGizmoTuple = std::tuple<Mesh*, Material*>;

class CameraGizmoMapping : public Mapping<GltfCameraTuple, CameraGizmoTuple> {
public:
    CameraGizmoMapping(EditorRenderLayer& renderLayer, const filesystem::Path& editorTexturesPath);
    float scale;

protected:
    bool onUpdateMapping(GltfCameraTuple gltfTuple, CameraGizmoTuple gizmo) final;
    CameraGizmoTuple onCreateMapping(GltfCameraTuple gltfTuple) final;
    void onDestroyMapping(GltfCameraTuple gltfTuple, CameraGizmoTuple gizmo) final;

    EditorRenderLayer& m_renderLayer;
    renderer::gl::Texture* m_cameraTexture = nullptr;
    renderer::gl::SamplerState* m_cameraSamplerState = nullptr;
};
}
