// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

// A6.7 sun disk (docs/ASOBO_EXTENSIONS_PHASE_6.md, A6.7): the sun drawn in the visible background only
// (cubemap.frag SUN_DISK), never in the cube the IBL filters read, so the sun is not counted twice (the A3.1
// directional light already lights the scene with it).
//
// Radiance: a uniform disk of angular radius R that gives the illuminance E on a surface facing it has the mean
// radiance E / (pi sin^2 R) (the disk's projected solid angle is pi sin^2 R). E is the A3.1 sun illuminance the
// renderer uses (in the procedural sky: 10 x the atmosphere transmittance towards the sun, so the disk carries the
// transmittance colour). Limb darkening: Hestroffer & Magnan's power law I(mu) / I(1) = mu^alpha with
// mu = sqrt(1 - (r / R)^2) and alpha = (0.397, 0.503, 0.652) at 680 / 550 / 440 nm (Hestroffer & Magnan 1998,
// also Hillaire 2016 "Physically Based Sky, Atmosphere and Cloud Rendering in Frostbite"). The disk mean of mu^alpha
// is 2 / (alpha + 2), so the centre radiance is mean x (alpha + 2) / 2 and the disk still integrates to E.
//
// Half-float range: the scene target is RGBA16F (largest finite value 65504). The real sun's disk (R = 0.27 deg,
// E = 8..10) has a mean radiance of about 1.2e5..1.5e5, so the shader's colour is scaled down (hue kept) until its
// largest channel at the centre is kMaxDiskRadiance; half the half-float range is left for blending on top of it.

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

namespace ux3d::imgui {

struct SunDiskSettings {
    bool enabled = true;       // drawn whenever the sun is on and the background is shown
    float scale = 1.0f;        // artistic size multiplier of the disk's angular radius (the illuminance is kept)
    bool limbDarkening = true; // Hestroffer & Magnan power law (off: a uniform disk)

    bool operator==(const SunDiskSettings&) const = default;
};

struct SunDiskEvaluation {
    bool visible = false;
    float angularRadius = 0.0f;           // radians, after the scale
    glm::vec3 meanRadiance = glm::vec3(0.0f);   // physical, before the half-float limit
    glm::vec3 centreRadiance = glm::vec3(0.0f); // physical, before the half-float limit
    float rangeScale = 1.0f;              // <= 1: the factor the drawn disk is scaled by to stay in range
};

namespace sundisk {
inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kMaxRadiance = 32000.0f;   // largest drawn channel (scene units, before exposure)
inline constexpr float kMaxScale = 20.0f;
inline const glm::vec3 kLimbAlpha = glm::vec3(0.397f, 0.503f, 0.652f);
} // namespace sundisk

// sunVisible: the A3.1 sun is on and the background is drawn; angularRadiusDeg: the A3.1 sun angular radius.
inline SunDiskEvaluation evaluateSunDisk(const SunDiskSettings& settings, bool sunVisible, const glm::vec3& illuminance,
                                         float angularRadiusDeg)
{
    SunDiskEvaluation out;
    const float radius = glm::radians(std::clamp(angularRadiusDeg, 0.0f, 10.0f)) * std::clamp(settings.scale, 0.0f, sundisk::kMaxScale);
    out.angularRadius = std::min(radius, glm::radians(45.0f));
    const float sinR = std::sin(out.angularRadius);
    const float projectedSolidAngle = sundisk::kPi * sinR * sinR;
    if (!settings.enabled || !sunVisible || projectedSolidAngle <= 1.0e-12f || !(std::max({illuminance.r, illuminance.g, illuminance.b}) > 0.0f)) {
        return out;
    }
    out.visible = true;
    out.meanRadiance = glm::max(illuminance, glm::vec3(0.0f)) / projectedSolidAngle;
    out.centreRadiance = settings.limbDarkening ? out.meanRadiance * (sundisk::kLimbAlpha + glm::vec3(2.0f)) * 0.5f : out.meanRadiance;
    const float peak = std::max({out.centreRadiance.r, out.centreRadiance.g, out.centreRadiance.b});
    out.rangeScale = peak > sundisk::kMaxRadiance ? sundisk::kMaxRadiance / peak : 1.0f;
    return out;
}

} // namespace ux3d::imgui
