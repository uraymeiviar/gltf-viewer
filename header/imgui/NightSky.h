// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "imgui/SkyAtmosphere.h"
#include "imgui/TimeOfDay.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace ux3d::imgui {

// A6.4 exposure across the day, A6.5 emissive day/night blend and A6.6 night sky (docs/ASOBO_EXTENSIONS_PHASE_6.md).
// Header-only (the project's CMake source list is fixed). Units: the procedural sky's environment unit
// (SkyAtmosphere.h: 1 unit = sky::kLuxPerEnvironmentUnit = 12,800 lux = 12,800 cd/m^2 x sr).

// Exposure mode of a scene (SceneEnvironmentState::exposureMode). Default resolves per environment source: Manual for
// the HDR panorama (exactly the behaviour before A6.4), Auto for the procedural sky (A6.4b: metered eye adaptation with
// the highlight meter, so lit cockpit screens are not blown out; capture runs snap it to the meter every frame, so a
// capture does not depend on the frames before it). Curve (a function of the time of day only, the A6.4 default) and
// Manual stay selectable.
enum class ExposureMode : int { Default = -1, Manual = 0, Auto = 1, Curve = 2 };

namespace night {

// ---- A6.4 exposure ----
// Glider's exposure multiplies the linear radiance (environment units) before tone mapping: displayed = tonemap(2^EV x L).
// Photographic exposure (Lagarde & de Rousiers 2014, saturation-based, ISO 100): H = 1 / (1.2 x 2^EV100) per cd/m^2.
// With 1 unit = 12,800 cd/m^2 the same multiplier is 12,800 / (1.2 x 2^EV100), so
//     EV (Glider) = kEv100Offset - EV100,   kEv100Offset = log2(12,800 / 1.2) = 13.38.
// Glider's EV 0 is EV100 13.4; noon (EV100 ~15.4) is EV -2.0 before the key below, a full-moon night (EV100 ~ -3) +16.4.
inline constexpr double kLuxPerUnit = sky::kLuxPerEnvironmentUnit;
inline const float kEv100Offset = static_cast<float>(std::log2(kLuxPerUnit / 1.2));
// Light meters at ISO 100: reflected (average luminance, K = 12.5) EV100 = log2(L x 100 / 12.5); incident (flat
// receptor, C = 250) EV100 = log2(E x 100 / 250). Both put an 18% grey card at the same exposure within 0.2 EV.
inline constexpr float kReflectedMeterK = 12.5f;
inline constexpr float kIncidentMeterC = 250.0f;
// Default limits of the physically based modes (EV100): a starlit night (~0.001 lux, EV100 ~ -11) to snow at noon.
inline constexpr float kDefaultMinEv100 = -12.0f;
inline constexpr float kDefaultMaxEv100 = 18.0f;

// Key (compensation) curve over the scene's EV100: the eye does not see a moonlit field as bright as daylight
// (the Purkinje / mesopic darkening MSFS also shows), so dark scenes are shown darker than a mid-grey key.
// +0.6 EV in daylight puts an 18% grey card at 0.18 (the saturation-based exposure maps it to 0.12); -1.8 EV under
// starlight. Linear between the points, constant outside. Monotonic, so the displayed brightness still rises with
// the scene's.
inline float keyCompensationEv(float ev100)
{
    static constexpr std::array<std::pair<float, float>, 7> kPoints = {{
        {-12.0f, -1.8f}, {-8.0f, -1.6f}, {-4.0f, -1.3f}, {0.0f, -0.8f}, {4.0f, -0.2f}, {8.0f, 0.4f}, {11.0f, 0.6f}}};
    if (!(ev100 > kPoints.front().first)) return kPoints.front().second;
    if (ev100 >= kPoints.back().first) return kPoints.back().second;
    for (size_t i = 1; i < kPoints.size(); ++i) {
        if (ev100 <= kPoints[i].first) {
            const float t = (ev100 - kPoints[i - 1].first) / (kPoints[i].first - kPoints[i - 1].first);
            return kPoints[i - 1].second + t * (kPoints[i].second - kPoints[i - 1].second);
        }
    }
    return kPoints.back().second;
}

// Glider EV for a scene EV100 (clamped to the limits), with the key curve and the user's compensation.
inline float gliderEvFromEv100(float ev100, float minEv100, float maxEv100, float compensationEv)
{
    const float lo = std::min(minEv100, maxEv100), hi = std::max(minEv100, maxEv100);
    const float clamped = std::clamp(ev100, lo, hi);
    return kEv100Offset - clamped + keyCompensationEv(clamped) + compensationEv;
}

// Reflected meter: EV100 of an average luminance in environment units.
inline float ev100FromLuminance(float luminanceUnits)
{
    return std::log2(std::max(luminanceUnits, 1.0e-20f) * static_cast<float>(kLuxPerUnit) * 100.0f / kReflectedMeterK);
}

// Incident meter: EV100 of a horizontal illuminance in environment units.
inline float ev100FromIlluminance(float illuminanceUnits)
{
    return std::log2(std::max(illuminanceUnits, 1.0e-20f) * static_cast<float>(kLuxPerUnit) * 100.0f / kIncidentMeterC);
}

// ---- A6.4b highlight-aware metering (Auto with the procedural sky) ----
// The average meter (trimmed, centre-weighted log average, AntiAliasing::updateAutoExposure) exposes for the mid tones
// and the key curve. Lit screens and panels are far brighter than that average at night (the MSFS-scaled emissive of
// a cockpit display is about 3-5 environment units, 40,000-60,000 cd/m^2, against a night sky of 1e-8 units);
// exposing for the average clips them to white. The highlight meter takes the brightest channel of every meter cell
// (64 x 64 over the viewport, uniform weight) that shows a surface (scene depth < 1: the background sky, its glow
// around a low sun, the moon and the stars count as black, so they may compress in the tone mapper's shoulder as
// before), and the value below which kHighlightPercentile of the image area lies; the exposure that maps it to
// kHighlightTarget (1.05: in the tone mapper's shoulder, which starts at 0.76, displayed about 0.94 sRGB; 0.8 until A3.9)
// keeps the brightest lit surfaces out of its hard clip. The image's exposure is the darker of the two meters, so the highlights
// darken the image only while a bright surface covers at least 1 - kHighlightPercentile of the image (about 12 of
// the 4096 cells); a single LED covers a cell or two and is ignored. Both meters include the exposure compensation;
// the key curve applies to the average only (the highlight term is a display limit, not a scene key).
enum class ExposureMetering : int { Average = 0, Highlight = 1 };
inline constexpr float kHighlightPercentile = 0.997f; // 0.3% of the image area lies above the metered highlight
inline constexpr float kHighlightTarget = 1.05f;     // exposed brightest-channel value of that highlight

// Eye adaptation of the physically based Auto exposure (exponential, 1 / s): the scene's two speeds are read as the
// scene brightness going up (autoExposureSpeedUp, default 3 / s: light adaptation, the exposure goes down) and going
// down (autoExposureSpeedDown, default 1.5 / s: dark adaptation is the slower one). The HDR's Auto keeps its meaning
// (the EV going up / down).

// Time-of-day curve (ExposureMode::Curve): an incident meter reads the horizontal illuminance, but an exterior view also
// sees the sky that gives it, which at twilight (and under the night glow) is brighter than the lit ground. The curve
// therefore meters E_horizontal + kCurveSkyWeight x E_sky (sky + glow part) and shows it kCurveKeyOffsetEv darker than
// the shared key curve; calibrated against Auto on the E195 exterior at noon, sunset, dusk, full moon and starlight
// (validate_night.py).
inline constexpr float kCurveSkyWeight = 2.0f;
inline constexpr float kCurveKeyOffsetEv = -0.75f;

// Clear-sky diffuse horizontal illuminance (luminance, environment units) against the sun's elevation, integrated once
// from the A6.2 model itself (scripts/sky_reference.py: cosine-weighted mean radiance of the upper hemisphere x pi,
// Mie 1, ozone on, ground albedo 0.3; 24 x 48 directions, 300 steps). Below -10 degrees (astronomical twilight) the
// values are the rendered sky's own (measured from a grey ground plane lit by its IBL: the 32 x 32 multiple-scattering
// LUT makes it 1.5-3 x darker than the reference there); below -20 degrees the sky is black.
inline float clearSkyIlluminance(float sunElevationDeg)
{
    static constexpr std::array<std::pair<float, float>, 27> kTable = {{
        {-20.0f, 3.871e-10f}, {-18.0f, 2.114e-09f}, {-16.0f, 1.309e-08f}, {-14.0f, 9.388e-08f}, {-12.0f, 7.434e-07f},
        {-10.0f, 6.844e-06f},
        {-8.0f, 5.700e-05f},  {-6.0f, 5.709e-04f},  {-5.0f, 1.695e-03f},  {-4.0f, 4.912e-03f},  {-3.0f, 1.205e-02f},
        {-2.0f, 2.667e-02f},  {-1.0f, 4.939e-02f},  {0.0f, 8.116e-02f},   {1.0f, 1.160e-01f},   {2.0f, 1.519e-01f},
        {3.0f, 1.861e-01f},   {4.0f, 2.180e-01f},   {6.0f, 2.733e-01f},   {8.0f, 3.186e-01f},   {10.0f, 3.561e-01f},
        {15.0f, 4.274e-01f},  {20.0f, 4.800e-01f},  {30.0f, 5.580e-01f},  {45.0f, 6.414e-01f},  {60.0f, 6.995e-01f},
        {90.0f, 7.459e-01f}}};
    if (!(sunElevationDeg > kTable.front().first)) return 0.0f;
    if (sunElevationDeg >= kTable.back().first) return kTable.back().second;
    for (size_t i = 1; i < kTable.size(); ++i) {
        if (sunElevationDeg <= kTable[i].first) {
            // Interpolated in log space (the values span nine decades).
            const float t = (sunElevationDeg - kTable[i - 1].first) / (kTable[i].first - kTable[i - 1].first);
            return std::exp(std::log(kTable[i - 1].second) + t * (std::log(kTable[i].second) - std::log(kTable[i - 1].second)));
        }
    }
    return kTable.back().second;
}

// ---- A6.6 night sky glow (airglow + the integrated starlight the IBL does not get from the drawn stars) ----
// Zenith radiance of a dark moonless sky: 2.0e-4 cd/m^2 (about 21.5 mag/arcsec^2, Leinert et al. 1998 / the
// natural-sky brightness of dark sites), brighter towards the horizon by the van Rhijn factor of a thin layer at
// 90 km and dimmed by the atmosphere's transmittance (sky_passes.frag). Its colour is a slightly green-white
// (the 557.7 nm oxygen line dominates the visible airglow). It is part of the sky-view LUT, so it reaches the IBL:
// the horizontal illuminance it gives is about 0.001 lux (starlight + airglow of a moonless night).
inline constexpr double kAirglowZenithCdM2 = 2.0e-4;
inline constexpr float kAirglowLayerKm = 90.0f;
inline glm::vec3 airglowColour() { return glm::vec3(0.85f, 1.0f, 0.88f) / (0.2126f * 0.85f + 0.7152f + 0.0722f * 0.88f); }
// Zenith radiance in environment units for an airglow scale (1 = the level above).
inline glm::vec3 airglowRadiance(float scale)
{
    return airglowColour() * static_cast<float>(kAirglowZenithCdM2 / kLuxPerUnit) * std::max(scale, 0.0f);
}
// Horizontal illuminance of the glow (cosine-weighted van Rhijn factor x transmittance over the upper hemisphere,
// about 1.6 for the default atmosphere; plain integration of the shader's expression).
inline float airglowIlluminance(float scale)
{
    return static_cast<float>(3.14159265358979323846 * 1.6 * kAirglowZenithCdM2 / kLuxPerUnit) * std::max(scale, 0.0f);
}

// ---- A6.6 moon ----
inline constexpr double kMoonRadiusKm = 1737.4;
inline constexpr double kAstronomicalUnitKm = 149597870.7;
// Geometric albedo of the Moon (V band): the full Moon at phase angle 0 gives p x E_sun x (R / d)^2.
inline constexpr double kMoonGeometricAlbedo = 0.12;

// Krisciunas & Schaefer (1991) lunar phase law, relative to phase angle 0: 10^(-0.4 (0.026 |a| + 4e-9 a^4)).
inline double moonPhaseFactor(double phaseAngleDeg)
{
    const double a = std::abs(phaseAngleDeg);
    return std::pow(10.0, -0.4 * (0.026 * a + 4.0e-9 * a * a * a * a));
}

// Integral over the unit disk of the Lommel-Seeliger brightness mu0 / (mu0 + mu) seen at a phase angle (degrees).
// The drawn disk has radiance K x mu0 / (mu0 + mu) with K = E / (sin^2 R x this integral), so it integrates to E.
inline double moonDiskIntegral(double phaseAngleDeg)
{
    const double alpha = phaseAngleDeg * 3.14159265358979323846 / 180.0;
    const double sx = std::sin(alpha), sz = std::cos(alpha); // the sun in the moon frame: observer along +z
    constexpr int kSamples = 96;
    const double cell = 2.0 / kSamples;
    double sum = 0.0;
    for (int j = 0; j < kSamples; ++j) {
        const double y = -1.0 + (j + 0.5) * cell;
        for (int i = 0; i < kSamples; ++i) {
            const double x = -1.0 + (i + 0.5) * cell;
            const double q2 = x * x + y * y;
            if (q2 >= 1.0) continue;
            const double mu = std::sqrt(1.0 - q2);
            const double mu0 = x * sx + mu * sz;
            if (mu0 <= 0.0) continue;
            sum += mu0 / (mu0 + mu);
        }
    }
    return sum * cell * cell;
}

struct NightSettings {
    bool moonEnabled = true;      // moon disk and moonlight (procedural sky with the A6.1 clock)
    float moonlightScale = 1.0f;  // multiplies the moonlight (not the disk)
    bool starsEnabled = true;
    float starDensity = 1.0f;     // multiplies the number of stars (1 = the naked-eye sky down to magnitude 6.5)
    float airglow = 1.0f;         // night sky glow level (1 = kAirglowZenithCdM2)
    // Largest weight of the per-pixel rod-vision shift (desaturation towards a blue-grey below 3 cd/m^2, full at
    // 0.01 cd/m^2) of the physical exposure modes; 0 = off. The moonlight is not tinted: its blue look is this.
    float nightVision = 0.75f;

    bool operator==(const NightSettings&) const = default;
};

struct MoonEvaluation {
    bool available = false;        // procedural sky, clock on, moon enabled
    bool aboveHorizon = false;
    glm::vec3 direction = glm::vec3(0.0f, 1.0f, 0.0f); // world, unit, towards the moon (lighting elevation)
    glm::vec3 sunFromMoon = glm::vec3(0.0f, 1.0f, 0.0f); // world, unit, from the moon towards the sun
    float angularRadius = 0.0f;    // radians
    double phaseAngle = 180.0;     // degrees
    double illuminatedFraction = 0.0;
    double distanceKm = 384400.0;
    glm::vec3 transmittance = glm::vec3(0.0f);
    glm::vec3 illuminanceTop = glm::vec3(0.0f); // above the atmosphere, at normal incidence (environment units)
    glm::vec3 illuminance = glm::vec3(0.0f);    // at the ground (x transmittance), at normal incidence
    glm::vec3 diskScale = glm::vec3(0.0f);      // K: disk radiance = K x mu0 / (mu0 + mu) (x transmittance included)
};

// The moon of a time-of-day evaluation (world directions through the heading, A3.1 convention).
inline MoonEvaluation evaluateMoon(const TimeOfDayState& clock, const TimeOfDayEvaluation& tod,
                                   const AtmosphereParameters& atmosphere, bool enabled)
{
    MoonEvaluation out;
    out.available = enabled;
    const solar::MoonState& moon = tod.moon;
    const double elevation = clock.refraction ? moon.horizontal.apparentElevation : moon.horizontal.elevation;
    out.direction = glm::normalize(glm::vec3(
        std::cos(glm::radians(static_cast<float>(elevation))) * std::sin(glm::radians(tod.moonGliderAzimuth)),
        std::sin(glm::radians(static_cast<float>(elevation))),
        std::cos(glm::radians(static_cast<float>(elevation))) * std::cos(glm::radians(tod.moonGliderAzimuth))));
    out.distanceKm = moon.distanceKm;
    out.angularRadius = static_cast<float>(std::asin(std::min(kMoonRadiusKm / std::max(moon.distanceKm, kMoonRadiusKm * 2.0), 1.0)));
    out.phaseAngle = moon.phaseAngle;
    out.illuminatedFraction = moon.illuminatedFraction;
    // Sun direction from the moon: the sun's world direction at its distance minus the moon's position (both seen
    // from the observer); the sun's direction is the clock's (also below the horizon).
    const glm::dvec3 sunWorld = glm::dvec3(glm::normalize(glm::vec3(
        std::cos(glm::radians(tod.gliderElevation)) * std::sin(glm::radians(tod.gliderAzimuth)), std::sin(glm::radians(tod.gliderElevation)),
        std::cos(glm::radians(tod.gliderElevation)) * std::cos(glm::radians(tod.gliderAzimuth)))));
    const double sunDistance = tod.sunCoordinates.distanceAu * kAstronomicalUnitKm;
    out.sunFromMoon = glm::vec3(glm::normalize(sunWorld * sunDistance - glm::dvec3(out.direction) * moon.distanceKm));
    out.aboveHorizon = out.direction.y > 0.0f;
    if (!enabled) return out;
    const double ratio = kMoonRadiusKm / moon.distanceKm;
    const double sunScale = 1.0 / (tod.sunCoordinates.distanceAu * tod.sunCoordinates.distanceAu);
    const double top = kMoonGeometricAlbedo * sky::kSolarIlluminance * sunScale * ratio * ratio * moonPhaseFactor(moon.phaseAngle);
    out.illuminanceTop = glm::vec3(static_cast<float>(top));
    out.transmittance = atmosphereTransmittance(atmosphere, out.direction);
    out.illuminance = out.illuminanceTop * out.transmittance;
    const double integral = moonDiskIntegral(moon.phaseAngle);
    const double sin2 = std::sin(out.angularRadius) * std::sin(out.angularRadius);
    if (integral > 1.0e-9 && sin2 > 0.0) {
        out.diskScale = out.illuminance / static_cast<float>(sin2 * integral);
    }
    return out;
}

// ---- A6.6 stars ----
// Rotation from world directions to the equatorial frame of date (x towards RA 0 / Dec 0, y towards RA 6 h, z the north
// celestial pole), from the clock's local sidereal time and latitude through the heading (A3.1 world convention).
inline glm::mat3 worldToEquatorial(const TimeOfDayState& clock, const TimeOfDayEvaluation& tod)
{
    auto world = [&](double ra, double dec) {
        const solar::Horizontal h = solar::horizontalFromEquatorial(ra, dec, tod.julianDayUt, clock.latitude, clock.longitude);
        const float az = glm::radians(static_cast<float>(gliderAzimuthFromBearing(h.azimuth, clock.heading)));
        const float el = glm::radians(static_cast<float>(h.elevation));
        return glm::vec3(std::cos(el) * std::sin(az), std::sin(el), std::cos(el) * std::cos(az));
    };
    const glm::vec3 x = world(0.0, 0.0), y = world(90.0, 0.0), z = world(0.0, 90.0);
    // Columns map equatorial to world; the transpose maps world to equatorial.
    return glm::transpose(glm::mat3(x, y, z));
}

// Stars fade in as the sky darkens below civil twilight: 0 with the sun at -6 degrees, 1 at -12 (nautical dusk).
inline float starFade(double sunLightElevationDeg)
{
    const double x = std::clamp((-6.0 - sunLightElevationDeg) / 6.0, 0.0, 1.0);
    return static_cast<float>(x * x * (3.0 - 2.0 * x));
}

// Illuminance of a magnitude-0 star (V band), environment units: 2.54e-6 lux.
inline constexpr float kStarMagnitudeZeroUnits = static_cast<float>(2.54e-6 / kLuxPerUnit);
inline constexpr float kStarFaintestMagnitude = 6.5f;
// Stars per steradian down to the faintest magnitude: about 8,900 on the whole sky (N(<m) ~ 10^(0.5 m)).
inline constexpr float kStarsPerSteradian = 8900.0f / (4.0f * 3.14159265f);

// ---- A6.5 emissive day/night blend ----
// Automatic day/night (DayNightMode::Automatic) blends the ASOBO_material_emissive multipliers over twilight: the night
// weight is a smoothstep from 0 at threshold + kNightBlendHalfWidth to 1 at threshold - kNightBlendHalfWidth (0.5 at the
// threshold, where the A6.1 state flips).
inline constexpr double kNightBlendHalfWidth = 3.0;
inline float nightBlend(double lightElevationDeg, double thresholdDeg)
{
    const double x = std::clamp((thresholdDeg + kNightBlendHalfWidth - lightElevationDeg) / (2.0 * kNightBlendHalfWidth), 0.0, 1.0);
    return static_cast<float>(x * x * (3.0 - 2.0 * x));
}

} // namespace night

inline const char* exposureModeName(ExposureMode mode)
{
    switch (mode) {
    case ExposureMode::Manual: return "manual";
    case ExposureMode::Auto: return "auto";
    case ExposureMode::Curve: return "curve";
    default: return "default";
    }
}

} // namespace ux3d::imgui
