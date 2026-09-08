// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include "renderer/Mapping.h"

namespace ux3d::gltf {
class Node;
class Light;
}

namespace ux3d::filesystem {
class Path;
}

namespace ux3d::renderer::gl {
struct SamplerState;
class Texture;
}

namespace ux3d::renderer {
class Mesh;
class Material;
class EditorRenderLayer;

using GltfLightTuple = std::tuple<const ux3d::gltf::Node*, const ux3d::gltf::Light*>;
using LightGizmoTuple = std::tuple<Mesh*, Material*>;
class LightGizmoMapping : public Mapping<GltfLightTuple, LightGizmoTuple> {
public:
    LightGizmoMapping(EditorRenderLayer& renderLayer, const filesystem::Path& editorTexturesPath);
    void setScale(float scale) { m_scale = scale; }

protected:
    bool onUpdateMapping(GltfLightTuple gltfLight, LightGizmoTuple gizmo) final;
    LightGizmoTuple onCreateMapping(GltfLightTuple gltfLight) final;
    void onDestroyMapping(GltfLightTuple gltfLight, LightGizmoTuple gizmo) final;

protected:
    EditorRenderLayer& m_renderLayer;
    gl::Texture* m_pointLightTexture = nullptr;
    gl::Texture* m_spotLightTexture = nullptr;
    gl::Texture* m_directionalLightTexture = nullptr;
    gl::SamplerState* m_lightSamplerState = nullptr;
    float m_scale = 1.0f;
};
}
