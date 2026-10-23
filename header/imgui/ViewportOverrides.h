// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <cstdint>

namespace ux3d::renderer {
class GltfRenderLayer;
}

namespace ux3d::imgui {

// Per-viewport overrides that persist on the render layer's shared objects
// (P4.5). The layer owns the glTF-derived base values of its materials and
// meshes; a viewport only overrides them. Material double-sidedness and mesh
// front-face winding survive between createScene calls, so they are written
// here. The debug-mode DEBUG define and polygon mode are reset by every
// createScene and are applied per drawable after it, in main.cpp.
struct ViewportDrawOverrides {
    bool forceDoubleSided = false;
    bool invertWinding = false;

    bool operator==(const ViewportDrawOverrides&) const = default;
};

// Tracks which overrides are currently written into the layer's shared
// materials and meshes, so consecutive viewports with equal overrides, and
// unchanged frames, skip the per-material and per-mesh loops. The record is
// keyed on the layer's glTF/scene/pose generations: updateGltf rewrites
// isDoubleSided, updateScene recreates meshes, and any pose change can flip a
// node's world-matrix determinant and therefore its winding.
class SharedOverrideState {
public:
    // Returns true when the shared objects were (re)written.
    bool apply(renderer::GltfRenderLayer& layer, const ViewportDrawOverrides& overrides);
    void invalidate() { m_valid = false; }

private:
    bool needsApply(const renderer::GltfRenderLayer& layer, const ViewportDrawOverrides& overrides) const;

    bool m_valid = false;
    const renderer::GltfRenderLayer* m_layer = nullptr;
    ViewportDrawOverrides m_applied;
    uint64_t m_gltfGeneration = 0;
    uint64_t m_sceneGeneration = 0;
    uint64_t m_poseGeneration = 0;
};

} // namespace ux3d::imgui
