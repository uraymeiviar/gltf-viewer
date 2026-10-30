// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace ux3d::imgui {

// A6.2 procedural sky (docs/ASOBO_EXTENSIONS_PHASE_6.md, A6.2): the parameters, units and the CPU half of the model.
// The GPU half (LUTs, sky cube, IBL filtering) is SkyEnvironment.h; the shaders are sky_common.glsl / sky_passes.frag.
//
// Model: Hillaire 2020, "A Scalable and Production Ready Sky and Atmosphere Rendering Technique" (EGSR): transmittance
// LUT, multiple-scattering LUT (Psi_ms, isotropic second order + geometric series), sky-view LUT. Atmosphere: Bruneton
// 2017 / Hillaire 2020 Earth: ground radius 6360 km, top 6460 km; Rayleigh scattering (5.802, 13.558, 33.1) e-3 /km,
// scale height 8 km; Mie scattering 3.996e-3 /km, extinction 4.440e-3 /km, scale height 1.2 km, Cornette-Shanks
// phase g = 0.8; ozone absorption (0.650, 1.881, 0.085) e-3 /km in a tent layer 10..40 km peaking at 25 km.
// The observer stands kObserverAltitudeKm above the ground; world +Y (glTF up) is its zenith.
//
// Units: the model works per unit top-of-atmosphere sun illuminance. Glider's environment unit is fixed by one
// constant, kLuxPerEnvironmentUnit: 1 environment unit = 12,800 lux (illuminance) = 12,800 cd/m^2 (radiance x sr).
// The top-of-atmosphere solar illuminance kSolarIlluminanceLux = 128,000 lux is therefore 10 units; at noon on a clear
// day the ground receives about 8.5-9.3 units from the sun (110-120 klux, close to the A3.1 default 8), and a white
// Lambert surface facing it reflects about 2.8. The clear sky is about 0.16 units at the zenith with the sun 60 degrees
// up (2.1 kcd/m^2) and 0.6-0.9 near the horizon (8-12 kcd/m^2); a surface facing away from the sun receives about 0.5
// (irradiance / pi). A6.4 refines the exposure; the default exposure (EV 0, ACES Hill) keeps noon readable.
namespace sky {
inline constexpr double kBottomRadiusKm = 6360.0;
inline constexpr double kTopRadiusKm = 6460.0;
inline constexpr double kObserverAltitudeKm = 0.05;
inline constexpr double kSolarIlluminanceLux = 128000.0;
inline constexpr double kLuxPerEnvironmentUnit = 12800.0;
inline constexpr float kSolarIlluminance = static_cast<float>(kSolarIlluminanceLux / kLuxPerEnvironmentUnit); // 10
inline constexpr float kRayleighScaleHeightKm = 8.0f;
inline constexpr float kMieScaleHeightKm = 1.2f;
inline constexpr float kMiePhaseG = 0.8f;
inline constexpr float kOzonePeakKm = 25.0f;
inline constexpr float kOzoneHalfWidthKm = 15.0f;
// Ground albedo of the lower hemisphere while the active viewport's ground is the Grid (no A3.6 plane): Earth's
// mean (Bond) albedo, grey.
inline constexpr float kNeutralGroundAlbedo = 0.3f;

inline glm::vec3 rayleighScattering() { return glm::vec3(5.802e-3f, 13.558e-3f, 33.1e-3f); }
inline glm::vec3 mieScattering() { return glm::vec3(3.996e-3f); }
inline glm::vec3 mieExtinction() { return glm::vec3(4.440e-3f); }
inline glm::vec3 ozoneAbsorption() { return glm::vec3(0.650e-3f, 1.881e-3f, 0.085e-3f); }
} // namespace sky

// Environment source of a scene (SceneEnvironmentState::environmentSource).
enum class EnvironmentSource : int { Hdr = 0, ProceduralSky = 1 };

// What the sky LUTs depend on, besides the sun direction.
struct AtmosphereParameters {
    float mieDensity = 1.0f;                // multiplies the Mie (aerosol) density: 1 = clear, larger = hazier
    bool ozone = true;                      // ozone absorption (the blue of the twilight sky)
    glm::vec3 groundAlbedo = glm::vec3(sky::kNeutralGroundAlbedo);
    float airglow = 1.0f;                   // A6.6 night sky glow level (NightSky.h; 1 = a dark moonless sky)

    bool operator==(const AtmosphereParameters&) const = default;
};

// Extinction coefficient (1/km) at a height above the ground, per channel (the shaders' sampleMedium).
inline glm::dvec3 atmosphereExtinction(const AtmosphereParameters& p, double heightKm)
{
    heightKm = std::max(heightKm, 0.0);
    const double rayleigh = std::exp(-heightKm / sky::kRayleighScaleHeightKm);
    const double mie = std::exp(-heightKm / sky::kMieScaleHeightKm) * std::max(p.mieDensity, 0.0f);
    const double ozone = p.ozone ? std::max(0.0, 1.0 - std::abs(heightKm - sky::kOzonePeakKm) / sky::kOzoneHalfWidthKm) : 0.0;
    return glm::dvec3(sky::rayleighScattering()) * rayleigh + glm::dvec3(sky::mieExtinction()) * mie +
           glm::dvec3(sky::ozoneAbsorption()) * ozone;
}

// Transmittance from the observer to the top of the atmosphere towards `direction` (world, unit, +Y up); zero when the
// ray hits the ground. Midpoint quadrature in double precision (512 steps). atmosphereTransmittance below is the same
// value through a small exact-input cache.
inline glm::vec3 atmosphereTransmittanceUncached(const AtmosphereParameters& p, const glm::vec3& direction)
{
    const double bottom = sky::kBottomRadiusKm, top = sky::kTopRadiusKm;
    const double h0 = sky::kObserverAltitudeKm;
    const double r0 = bottom + h0;
    const glm::dvec3 d = glm::normalize(glm::dvec3(direction));
    const double mu = std::clamp(d.y, -1.0, 1.0);
    // Below the observer's horizon the planet blocks the ray.
    if (mu < 0.0 && r0 * r0 * mu * mu - h0 * (2.0 * bottom + h0) >= 0.0) {
        return glm::vec3(0.0f);
    }
    const double length = -r0 * mu + std::sqrt(std::max(r0 * r0 * (mu * mu - 1.0) + top * top, 0.0));
    constexpr int kSteps = 512;
    const double dt = length / kSteps;
    glm::dvec3 opticalDepth(0.0);
    for (int i = 0; i < kSteps; ++i) {
        const double t = (i + 0.5) * dt;
        const double r = std::sqrt(r0 * r0 + t * t + 2.0 * r0 * mu * t);
        const double h = (h0 * (2.0 * bottom + h0) + t * t + 2.0 * r0 * mu * t) / (r + bottom);
        opticalDepth += atmosphereExtinction(p, h) * dt;
    }
    return glm::vec3(glm::exp(-opticalDepth));
}

// C7 (round 11): the sun, the moon, the exposure plan, the sun disk and the star extinction ask for the same few
// transmittances many times per frame (about 27 with three viewports, 8 us each). The function is pure, so a cache
// keyed on the exact bits of every input it reads (mieDensity, ozone, the direction; not groundAlbedo or airglow)
// returns bit-identical results. Per thread, 8 entries, round-robin replacement; nothing is tolerance-matched.
inline glm::vec3 atmosphereTransmittance(const AtmosphereParameters& p, const glm::vec3& direction)
{
    struct Entry {
        std::array<uint32_t, 5> key{};
        glm::vec3 value{0.0f};
        bool valid = false;
    };
    thread_local std::array<Entry, 8> cache{};
    thread_local size_t next = 0;
    const std::array<uint32_t, 5> key = {std::bit_cast<uint32_t>(p.mieDensity), p.ozone ? 1u : 0u,
                                         std::bit_cast<uint32_t>(direction.x), std::bit_cast<uint32_t>(direction.y),
                                         std::bit_cast<uint32_t>(direction.z)};
    for (const Entry& entry : cache) {
        if (entry.valid && entry.key == key) return entry.value;
    }
    Entry& slot = cache[next];
    next = (next + 1) % cache.size();
    slot.value = atmosphereTransmittanceUncached(p, direction);
    slot.key = key;
    slot.valid = true;
    return slot.value;
}

// A3.1 sun in procedural-sky mode: the top-of-atmosphere sun (white, kSolarIlluminance) through the atmosphere towards
// the sun, i.e. linear RGB illuminance in environment units (colour x intensity of SceneEnvironmentState).
inline glm::vec3 skySunIlluminance(const AtmosphereParameters& p, const glm::vec3& sunDirection)
{
    return atmosphereTransmittance(p, sunDirection) * sky::kSolarIlluminance;
}

// Refinement state of the procedural environment's IBL cubes (UI, get_state, profile reports).
enum class SkyRefineState : int {
    Inactive = 0,   // HDR source
    Converged,      // the filtered cubes are the configured quality for the current sky
    Updating,       // the sky changed: a reduced-size filter runs (time-sliced) and replaces the cubes when complete
    WaitingToRefine,// reduced-size cubes are current; refinement starts once the sky has been still for a moment
    Refining,       // a configured-quality filter runs (time-sliced) and replaces the cubes when complete
};

inline const char* skyRefineStateName(SkyRefineState state)
{
    switch (state) {
    case SkyRefineState::Converged: return "converged";
    case SkyRefineState::Updating: return "updating";
    case SkyRefineState::WaitingToRefine: return "waiting";
    case SkyRefineState::Refining: return "refining";
    default: return "inactive";
    }
}

// What the Scene Inspector shows about the procedural environment.
struct SkyStatus {
    bool active = false;
    SkyRefineState state = SkyRefineState::Inactive;
    float progress = 0.0f;          // of the running job, 0..1
    uint32_t cubeSize = 0;          // the displayed (front) cubes
    uint32_t targetSize = 0;        // the configured quality
    double gpuMsLast = 0.0;         // extra GPU time of the last measured frame with sky work
    double gpuMsMax = 0.0;          // largest time-sliced frame
    double budgetMs = 0.0;
    glm::vec3 groundAlbedo = glm::vec3(0.0f);
    bool groundFromPlane = false;
    // A3.9 sky occlusion (shown in the Scene Inspector's Sky Occlusion section).
    bool skyOcclusionActive = false;   // the volume is baked and used
    bool skyOcclusionBaking = false;
    float skyOcclusionProgress = 0.0f; // of the running bake, 0..1
    int skyOcclusionDims[3] = { 0, 0, 0 };
    float skyOcclusionCell = 0.0f;
    double skyOcclusionMemoryMiB = 0.0;
    double skyOcclusionBakeMs = 0.0;   // CPU time of the last bake
};

} // namespace ux3d::imgui
