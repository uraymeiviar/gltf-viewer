// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "imgui/SceneEnvironment.h"
#include "imgui/SceneRevisions.h"

namespace ux3d::gltf {
class Scene;
}

namespace ux3d::renderer {
class CommandGenerator;
class GltfRenderLayer;
}

namespace ux3d::imgui {

class SceneInspector {
public:
    static void render(SceneEnvironmentState& env,
                       ux3d::renderer::CommandGenerator* commandGenerator,
                       ux3d::renderer::GltfRenderLayer* gltfLayer,
                       SceneRevisions& revisions,
                       ux3d::gltf::Scene* scene = nullptr);

private:
    // A3.1: Environment -> Sun section.
    static void renderSun(SceneEnvironmentState& env, ux3d::renderer::GltfRenderLayer* gltfLayer, SceneRevisions& revisions);
};

} // namespace ux3d::imgui
