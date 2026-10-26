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
    bool autoExposure = false; // false = Manual (default), true = Auto-Exposure
    bool autoExposureStartFromManual = false;
    float exposureCompensationEV = 0.0f;
    float autoExposureMinEV = -6.0f;
    float autoExposureMaxEV = 6.0f;
    float autoExposureSpeedUp = 3.0f;
    float autoExposureSpeedDown = 1.5f;

    bool bloomEnabled = true;
    float bloomIntensity = 0.04f;
    float bloomRadius = 1.0f;

    bool iblEnabled = true;
    bool punctualLightsEnabled = true;

    float msfsEmissiveScale = 0.001f; // default 1/1000
    int timeOfDay = 0; // 0 = Day (default), 1 = Night

    bool operator==(const SceneEnvironmentState&) const = default;
};

} // namespace ux3d::imgui
