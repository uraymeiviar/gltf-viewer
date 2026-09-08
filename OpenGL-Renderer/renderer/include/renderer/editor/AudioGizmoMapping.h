// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include "renderer/Mapping.h"

#include <memory>
#include <tuple>

namespace ux3d::gltf {
class Node;
class AudioEmitter;
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

using AudioGizmoTuple = std::tuple<Mesh*, Material*>;

class AudioGizmoMapping : public Mapping<const ux3d::gltf::Node*, AudioGizmoTuple> {
public:
    AudioGizmoMapping(EditorRenderLayer& renderLayer, const filesystem::Path& editorTexturesPath);
    float scale;

protected:
    bool onUpdateMapping(const ux3d::gltf::Node* gltfTuple, AudioGizmoTuple gizmo) final;
    AudioGizmoTuple onCreateMapping(const ux3d::gltf::Node* gltfTuple) final;
    void onDestroyMapping(const ux3d::gltf::Node* gltfTuple, AudioGizmoTuple gizmo) final;

    EditorRenderLayer& m_renderLayer;
    renderer::gl::Texture* m_texture = nullptr;
    std::unique_ptr<renderer::gl::SamplerState> m_samplerState = nullptr;
};
}
