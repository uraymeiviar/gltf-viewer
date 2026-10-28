// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "renderer/CascadedShadows.h"
#include "math/MatrixFunctions.h"
#include "math/VectorFunctions.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ux3d::renderer {

namespace {

// View-space point on the ray of an NDC corner at view depth `depth` (metres in front of the camera).
struct CornerRay {
    glm::vec3 nearPoint; // on the near plane
    bool perspective;
};

glm::vec3 pointAtDepth(const CornerRay& ray, float depth)
{
    if (ray.perspective) {
        // Through the eye: scale the near-plane point to the requested depth.
        return ray.nearPoint * (depth / std::max(-ray.nearPoint.z, 1.0e-9f));
    }
    return glm::vec3(ray.nearPoint.x, ray.nearPoint.y, -depth);
}

glm::vec3 unproject(const glm::mat4& inverseProjection, float x, float y, float z)
{
    const glm::vec4 p = inverseProjection * glm::vec4(x, y, z, 1.0f);
    return glm::vec3(p) / p.w;
}

// Rounds a positive value down / up to a multiple of 1/8 octave (a factor of 2^(1/8), about 9%).
float quantizeDown(float value)
{
    return std::exp2(std::floor(std::log2(value) * 8.0f) / 8.0f);
}

float quantizeUp(float value)
{
    return std::exp2(std::ceil(std::log2(value) * 8.0f) / 8.0f);
}

} // namespace

ShadowCascadeSetup fitShadowCascades(const ShadowCascadeInput& input)
{
    ShadowCascadeSetup setup;
    const int count = std::clamp(input.cascadeCount, 1, 4);
    const float resolution = static_cast<float>(std::max(input.resolution, 1));

    // Camera frustum: rays through the four NDC corners, and the near/far planes.
    const bool perspective = std::abs(input.projection[3][3]) < 0.5f;
    const glm::mat4 inverseProjection = glm::inverse(input.projection);
    std::array<CornerRay, 4> rays {};
    const float cornerX[4] = { -1.0f, 1.0f, 1.0f, -1.0f };
    const float cornerY[4] = { -1.0f, -1.0f, 1.0f, 1.0f };
    for (int i = 0; i < 4; ++i) {
        rays[i].nearPoint = unproject(inverseProjection, cornerX[i], cornerY[i], -1.0f);
        rays[i].perspective = perspective;
    }
    const float cameraNear = std::max(-unproject(inverseProjection, 0.0f, 0.0f, -1.0f).z, 1.0e-6f);
    float cameraFar = std::numeric_limits<float>::max();
    {
        const glm::vec4 farPoint = inverseProjection * glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        if (std::abs(farPoint.w) > 1.0e-12f) {
            const float depth = -farPoint.z / farPoint.w;
            if (std::isfinite(depth) && depth > cameraNear) {
                cameraFar = depth;
            }
        }
    }

    // Depth range: the camera range cut to the scene and to the max distance.
    float nearDepth = cameraNear;
    float farDepth = std::min(cameraFar, std::max(input.maxDistance, cameraNear * 2.0f));
    std::array<glm::vec3, 8> sceneCorners {};
    if (input.hasSceneBounds) {
        float sceneNear = std::numeric_limits<float>::max();
        float sceneFar = -std::numeric_limits<float>::max();
        for (int i = 0; i < 8; ++i) {
            sceneCorners[i] = glm::vec3((i & 1) ? input.sceneMax.x : input.sceneMin.x,
                                        (i & 2) ? input.sceneMax.y : input.sceneMin.y,
                                        (i & 4) ? input.sceneMax.z : input.sceneMin.z);
            const float depth = -(input.view * glm::vec4(sceneCorners[i], 1.0f)).z;
            sceneNear = std::min(sceneNear, depth);
            sceneFar = std::max(sceneFar, depth);
        }
        if (sceneFar <= cameraNear) {
            return setup; // the whole scene is behind the camera
        }
        nearDepth = std::max(nearDepth, sceneNear);
        farDepth = std::min(farDepth, sceneFar);
        setup.distanceCut = input.maxDistance < sceneFar && input.maxDistance < cameraFar;
    } else {
        setup.distanceCut = input.maxDistance < cameraFar;
    }
    if (!(farDepth > nearDepth)) {
        return setup;
    }
    // Stable splits: whole 1/8 octaves, never outside the camera range or the max distance. A near depth
    // far below the far depth only spends resolution on empty space, so it is kept above far / 2000.
    nearDepth = std::max({ quantizeDown(nearDepth), cameraNear, farDepth * 5.0e-4f });
    farDepth = std::min({ quantizeUp(farDepth), cameraFar, std::max(input.maxDistance, cameraNear * 2.0f) });
    if (!(farDepth > nearDepth)) {
        return setup;
    }
    if (setup.distanceCut) {
        setup.fadeEnd = farDepth;
        setup.fadeStart = farDepth - 0.1f * (farDepth - nearDepth);
    }

    // Light basis: depends only on the sun direction.
    const glm::vec3 L = glm::normalize(input.directionToSun);
    const glm::vec3 upReference = std::abs(L.y) < 0.99f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
    const glm::vec3 lightX = glm::normalize(glm::cross(upReference, L));
    const glm::vec3 lightY = glm::cross(L, lightX);

    // How far the scene reaches towards the sun (casters beyond the view frustum).
    float sceneTowardsSun = -std::numeric_limits<float>::max();
    if (input.hasSceneBounds) {
        for (const glm::vec3& corner : sceneCorners) {
            sceneTowardsSun = std::max(sceneTowardsSun, glm::dot(corner, L));
        }
    }

    const glm::mat4 cameraFromView = glm::inverse(input.view);
    float splitNear = nearDepth;
    for (int c = 0; c < count; ++c) {
        const float fraction = static_cast<float>(c + 1) / static_cast<float>(count);
        const float logarithmic = nearDepth * std::pow(farDepth / nearDepth, fraction);
        const float uniform = nearDepth + (farDepth - nearDepth) * fraction;
        const float lambda = std::clamp(input.splitLambda, 0.0f, 1.0f);
        const float splitFar = c + 1 == count ? farDepth : lambda * logarithmic + (1.0f - lambda) * uniform;

        // Bounding sphere of the slice, in view space: centre on the line between the near and far face
        // centroids, where the farthest near corner and the farthest far corner are equally distant.
        std::array<glm::vec3, 4> nearCorners {};
        std::array<glm::vec3, 4> farCorners {};
        glm::vec3 nearCentroid(0.0f);
        glm::vec3 farCentroid(0.0f);
        for (int i = 0; i < 4; ++i) {
            nearCorners[i] = pointAtDepth(rays[i], splitNear);
            farCorners[i] = pointAtDepth(rays[i], splitFar);
            nearCentroid += nearCorners[i] * 0.25f;
            farCentroid += farCorners[i] * 0.25f;
        }
        const auto farthest = [](const std::array<glm::vec3, 4>& corners, const glm::vec3& point) {
            float result = 0.0f;
            for (const glm::vec3& corner : corners) {
                result = std::max(result, glm::distance(corner, point));
            }
            return result;
        };
        float lo = 0.0f;
        float hi = 1.0f;
        for (int iteration = 0; iteration < 40; ++iteration) {
            const float t = 0.5f * (lo + hi);
            const glm::vec3 point = glm::mix(nearCentroid, farCentroid, t);
            // Moving towards the far face brings the far corners closer and the near corners farther.
            if (farthest(nearCorners, point) > farthest(farCorners, point)) {
                hi = t;
            } else {
                lo = t;
            }
        }
        const glm::vec3 centerView = glm::mix(nearCentroid, farCentroid, 0.5f * (lo + hi));
        float radius = std::max(farthest(nearCorners, centerView), farthest(farCorners, centerView));
        // Round up to 1/64 of the radius's power of two, so float noise never changes the texel size.
        const float step = std::exp2(std::floor(std::log2(std::max(radius, 1.0e-6f))) - 6.0f);
        radius = std::ceil(radius / step) * step * 1.01f;

        // Light-space centre snapped to whole texels.
        const glm::vec3 centerWorld = glm::vec3(cameraFromView * glm::vec4(centerView, 1.0f));
        const float texel = 2.0f * radius / resolution;
        const float cx = std::floor(glm::dot(centerWorld, lightX) / texel) * texel;
        const float cy = std::floor(glm::dot(centerWorld, lightY) / texel) * texel;
        const float cz = glm::dot(centerWorld, L);
        const float bottom = cz - radius;
        float top = std::max(cz + radius, sceneTowardsSun);
        top += 0.01f * radius + 1.0e-3f;
        const float depthRange = top - bottom;

        // world -> light view: x' = dot(p, X) - cx, y' = dot(p, Y) - cy, z' = dot(p, L) - top (<= 0 below the top).
        glm::mat4 lightView(1.0f);
        lightView[0] = glm::vec4(lightX.x, lightY.x, L.x, 0.0f);
        lightView[1] = glm::vec4(lightX.y, lightY.y, L.y, 0.0f);
        lightView[2] = glm::vec4(lightX.z, lightY.z, L.z, 0.0f);
        lightView[3] = glm::vec4(-cx, -cy, -top, 1.0f);
        // Orthographic: x, y in [-radius, radius] -> [-1, 1]; depth -z' in [0, depthRange] -> [-1, 1].
        glm::mat4 lightProjection(1.0f);
        lightProjection[0][0] = 1.0f / radius;
        lightProjection[1][1] = 1.0f / radius;
        lightProjection[2][2] = -2.0f / depthRange;
        lightProjection[3][2] = -1.0f;
        // Clip -> texture: xyz * 0.5 + 0.5.
        glm::mat4 bias(0.5f);
        bias[3] = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);

        ShadowCascade& cascade = setup.cascades[c];
        cascade.lightView = lightView;
        cascade.lightProjection = lightProjection;
        cascade.textureFromWorld = bias * lightProjection * lightView;
        cascade.splitNear = splitNear;
        cascade.splitFar = splitFar;
        cascade.radius = radius;
        cascade.texelWorld = texel;
        cascade.depthRange = depthRange;
        cascade.center = cx * lightX + cy * lightY + cz * L;
        cascade.lightOrigin = glm::vec2(cx, cy);
        splitNear = splitFar;
    }
    setup.valid = true;
    setup.count = count;
    setup.nearDepth = nearDepth;
    setup.farDepth = farDepth;
    return setup;
}

}
