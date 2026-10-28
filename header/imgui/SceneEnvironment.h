// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <string>

namespace ux3d::imgui {

// Default IBL/skybox environment, embedded under resources/environments/.
inline constexpr const char* kDefaultEnvironmentFile = "default_hdr_env4k.hdr";

// A3.1 sun modes. Manual: azimuth, elevation, colour and intensity as set. FromHdr: the sun found in the
// HDR panorama (IBLSampler's EnvironmentSun), turned with the HDR rotation and scaled by the HDR intensity.
enum class SunMode : int { Manual = 0, FromHdr = 1 };

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

    // A3.1 sun: a directional light through the punctual-light BRDF path (pbr.frag), scene-level like the
    // exposure. Conventions (docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.1):
    //  * direction: glTF world space, Y up. Azimuth in degrees around +Y, measured from +Z towards +X
    //    (0 = sun in the +Z direction, 90 = +X); elevation in degrees above the horizon (XZ plane).
    //    sunDirection() gives the unit vector from the surface towards the sun.
    //  * intensity: illuminance on a surface facing the sun, in the environment's radiance units times
    //    steradians (an HDR texel of value L covering solid angle w contributes L * w). A white Lambert
    //    surface facing the sun then reflects radiance colour * intensity / pi, exactly as the IBL would
    //    for that much light in the HDR. The colour is linear RGB with luminance 1 for white (the Kelvin
    //    helper and From HDR produce luminance-1 colours), so intensity is the illuminance's luminance.
    //  * while the sun is on (either mode) the HDR's own sun is removed from the panorama before IBL
    //    filtering and the background (IBLSampler sun removal), so there is never a second sun.
    bool sunEnabled = false;
    int sunMode = static_cast<int>(SunMode::Manual);
    float sunAzimuth = 135.0f;
    float sunElevation = 45.0f;
    glm::vec3 sunColor = glm::vec3(1.0f);
    float sunIntensity = 8.0f;
    float sunAngularRadius = 0.27f; // degrees; the real sun is 0.2666 deg

    bool operator==(const SceneEnvironmentState&) const = default;
};

// Unit vector from the surface towards the sun for the A3.1 convention above.
inline glm::vec3 sunDirectionFromAngles(float azimuthDeg, float elevationDeg)
{
    const float az = glm::radians(azimuthDeg);
    const float el = glm::radians(elevationDeg);
    return glm::vec3(std::cos(el) * std::sin(az), std::sin(el), std::cos(el) * std::cos(az));
}

// Inverse of sunDirectionFromAngles: azimuth in [0, 360), elevation in [-90, 90].
inline void sunAnglesFromDirection(const glm::vec3& direction, float& azimuthDeg, float& elevationDeg)
{
    const glm::vec3 d = glm::normalize(direction);
    elevationDeg = glm::degrees(std::asin(std::clamp(d.y, -1.0f, 1.0f)));
    float az = glm::degrees(std::atan2(d.x, d.z));
    if (az < 0.0f) az += 360.0f;
    azimuthDeg = az;
}

// Linear-sRGB colour of a black body at `kelvin` (1667..25000 K, clamped), luminance 1 (CIE Y = 1).
// Kang et al. 2002 chromaticity fit, XYZ to linear sRGB (D65); negative components are clamped to 0.
inline glm::vec3 kelvinToLinearRgb(float kelvin)
{
    const double t = std::clamp(static_cast<double>(kelvin), 1667.0, 25000.0);
    const double t2 = t * t, t3 = t2 * t;
    const double x = t <= 4000.0 ? -0.2661239e9 / t3 - 0.2343589e6 / t2 + 0.8776956e3 / t + 0.179910
                                 : -3.0258469e9 / t3 + 2.1070379e6 / t2 + 0.2226347e3 / t + 0.240390;
    const double x2 = x * x, x3 = x2 * x;
    double y;
    if (t <= 2222.0) y = -1.1063814 * x3 - 1.34811020 * x2 + 2.18555832 * x - 0.20219683;
    else if (t <= 4000.0) y = -0.9549476 * x3 - 1.37418593 * x2 + 2.09137015 * x - 0.16748867;
    else y = 3.0817580 * x3 - 5.87338670 * x2 + 3.75112997 * x - 0.37001483;
    const double X = x / y, Y = 1.0, Z = (1.0 - x - y) / y;
    const double r = 3.2404542 * X - 1.5371385 * Y - 0.4985314 * Z;
    const double g = -0.9692660 * X + 1.8760108 * Y + 0.0415560 * Z;
    const double b = 0.0556434 * X - 0.2040259 * Y + 1.0572252 * Z;
    glm::vec3 rgb(static_cast<float>(std::max(r, 0.0)), static_cast<float>(std::max(g, 0.0)), static_cast<float>(std::max(b, 0.0)));
    const float luminance = 0.2126f * rgb.r + 0.7152f * rgb.g + 0.0722f * rgb.b;
    return luminance > 0.0f ? rgb / luminance : glm::vec3(1.0f);
}

} // namespace ux3d::imgui
