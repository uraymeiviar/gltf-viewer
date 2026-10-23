// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <glm/glm.hpp>
#include <string>

namespace ux3d::imgui {

// Default IBL/skybox environment, embedded under resources/environments/.
inline constexpr const char* kDefaultEnvironmentFile = "default_hdr_env4k.hdr";

struct SceneEnvironmentState {
    bool showBackground = true;
    glm::vec4 clearColor = glm::vec4(0.12f, 0.12f, 0.14f, 1.0f);
    float hdrBlur = 0.0f;
    float hdrIntensity = 1.0f;
    float hdrRotation = 0.0f;
    std::string currentHdrPath = kDefaultEnvironmentFile;

    int toneMappingIdx = 1; // 0 = PBR Neutral, 1 = ACES Hill, 2 = ACES Narkowicz
    float exposureEV = 0.0f; // EV value, exposure = 2^EV
    bool iblEnabled = true;
    bool punctualLightsEnabled = true;

    bool operator==(const SceneEnvironmentState&) const = default;
};

} // namespace ux3d::imgui
