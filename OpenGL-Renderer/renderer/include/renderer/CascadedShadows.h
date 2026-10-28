// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "math/MatrixTypes.h"
#include "math/VectorTypes.h"

#include <array>

namespace ux3d::renderer {

// A3.2: fitting of the sun's cascaded shadow maps to one viewport's view frustum
// (docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.2). Pure math, no GL.
//
// * Depth range: the camera's [near, far] cut to the scene's bounds (the eight corners of its world AABB in
//   view space) and to maxDistance, then widened to the next 1/8 octave (near down, far up) so the splits
//   stay put while the camera moves a little.
// * Splits: the practical split scheme, lambda * logarithmic + (1 - lambda) * uniform.
// * Each cascade is a bounding sphere of its frustum slice, computed in view space (so its radius does not
//   change when the camera moves or turns), in a light basis that depends only on the sun direction. The
//   sphere centre is snapped to whole shadow-map texels: the map moves in texel steps and a static scene
//   rasterizes identically, so shadow edges do not shimmer.
// * Depth: from the far side of the sphere to beyond the scene's last point towards the sun, so casters
//   outside the view frustum (and outside the sphere) still cast.
struct ShadowCascadeInput {
    glm::mat4 view = glm::mat4(1.0f);        // camera view matrix (world -> view, camera looks down -z)
    glm::mat4 projection = glm::mat4(1.0f);  // unjittered camera projection (perspective or orthographic)
    glm::vec3 directionToSun = glm::vec3(0.0f, 1.0f, 0.0f);
    int cascadeCount = 3;                    // 1..4
    int resolution = 2048;                   // texels per side of every cascade
    float maxDistance = 200.0f;              // view depth where the shadows end
    float splitLambda = 0.75f;               // 0 uniform .. 1 logarithmic
    bool hasSceneBounds = false;
    glm::vec3 sceneMin = glm::vec3(0.0f);
    glm::vec3 sceneMax = glm::vec3(0.0f);
};

struct ShadowCascade {
    glm::mat4 lightView = glm::mat4(1.0f);       // world -> light view (light looks down -z)
    glm::mat4 lightProjection = glm::mat4(1.0f); // light view -> clip (orthographic)
    glm::mat4 textureFromWorld = glm::mat4(1.0f); // world -> shadow-map uv (xy) and window depth (z), all 0..1
    float splitNear = 0.0f; // view depth range this cascade covers
    float splitFar = 0.0f;
    float radius = 0.0f;     // bounding sphere radius (world units); the map covers 2 * radius
    float texelWorld = 0.0f; // world size of one texel
    float depthRange = 0.0f; // light-space depth covered by the map
    glm::vec3 center = glm::vec3(0.0f); // snapped sphere centre, world space
    glm::vec2 lightOrigin = glm::vec2(0.0f); // snapped centre in the light basis (whole multiples of texelWorld)
};

struct ShadowCascadeSetup {
    bool valid = false; // false when no scene geometry lies in front of the camera
    int count = 0;
    std::array<ShadowCascade, 4> cascades {};
    float nearDepth = 0.0f; // view depth where cascade 0 starts
    float farDepth = 0.0f;  // view depth where the last cascade ends
    // The max distance cuts the scene: the shadow fades out between fadeStart and fadeEnd.
    bool distanceCut = false;
    float fadeStart = 3.0e38f;
    float fadeEnd = 3.0e38f;
};

ShadowCascadeSetup fitShadowCascades(const ShadowCascadeInput& input);

}
