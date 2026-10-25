// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include <tuple>
#include <vector>

#include "core/LinkedList.h"

namespace ux3d::renderer {

class Mesh;
class Material;
struct Light;
class Camera;
class EnvironmentMap;

struct RendererScene {
    std::vector<Light*> lights;
    core::LinkedList<std::tuple<Mesh*, Material*>> drawables;
    // A4: schedule the behind copy after decal accumulation, before forward shading.
    bool asoboGlassCopy = false;
    bool windshieldSsr = false;
    Camera* camera = nullptr;
    EnvironmentMap* environmentMap = nullptr;
};

}
