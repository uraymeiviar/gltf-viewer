// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include <tuple>
#include <vector>

#include "core/LinkedList.h"
#include "math/VectorTypes.h"

namespace ux3d::renderer {

class Mesh;
class Material;
struct Light;
class Camera;
class EnvironmentMap;

// A3.7: one ASOBO_sky_portal disc in world space (docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.7).
struct SkyPortalLight {
    math::vec3 position = math::vec3(0.0f); // disc centre
    math::vec3 normal = math::vec3(0.0f, 0.0f, 1.0f); // unit, pointing into the cabin (node +Z)
    float radius = 0.0f; // disc radius in world units (source_radius cm / 100 x node scale)
    float cosInner = 0.0f; // cos(inner_cone_angle / 2): full emission inside this angle from the normal
    float cosOuter = 0.0f; // cos(outer_cone_angle / 2): no emission beyond it
};

// At most this many portals light a viewport (the nearest to its camera); pbr.frag loops over them.
inline constexpr size_t kMaxSkyPortals = 8;

struct RendererScene {
    std::vector<Light*> lights;
    core::LinkedList<std::tuple<Mesh*, Material*>> drawables;
    // A4: schedule the behind copy after decal accumulation, before forward shading.
    bool asoboGlassCopy = false;
    bool windshieldSsr = false;
    Camera* camera = nullptr;
    EnvironmentMap* environmentMap = nullptr;
    // A3.7: the sky portals this viewport applies (empty when the file has none or the viewport's
    // ASOBO_sky_portal switch is off), nearest to the camera first, at most kMaxSkyPortals.
    std::vector<SkyPortalLight> skyPortals;
};

}
