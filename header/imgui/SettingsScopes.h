// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <array>
#include <string_view>

namespace ux3d::imgui {

// Settings scopes (docs/SETTINGS_SCOPES.md). Three places own the settings:
//   * Quality (RenderQualitySettings, renderer-wide, the quality presets): everything that costs time or memory.
//   * Scene (SceneEnvironmentState::effects, one per glTF scene, shared by every viewport): whether an effect is on
//     and how it looks.
//   * Viewport (ViewportSettingsState): how this view looks at the scene (camera, shading, debug views, ground,
//     extension display switches) plus an override per effect.

// A viewport's override of one scene effect: follow the scene (default), force it off, or force it on.
enum class EffectOverride : int {
    Scene = 0,
    Off = 1,
    On = 2,
};

inline bool resolveEffect(EffectOverride value, bool scene)
{
    return value == EffectOverride::Scene ? scene : value == EffectOverride::On;
}

inline constexpr std::array<std::string_view, 3> kEffectOverrideNames = { "scene", "off", "on" };

inline const char* effectOverrideName(EffectOverride value)
{
    static constexpr const char* names[] = { "scene", "off", "on" };
    const int index = static_cast<int>(value);
    return names[index >= 0 && index <= 2 ? index : 0];
}

// The scene's effects: on/off switches and look parameters that used to be per-viewport settings (A3.2 shadows were
// renderer-wide). The defaults are the former defaults, so a default scene renders as before.
struct SceneEffectSettings {
    bool shadows = true;                // A3.2 sun shadows (drawn while the sun is on and the quality allows them)
    bool ssr = true;                    // windshield screen-space reflections (ASOBO_material_windshield_v3)
    float interiorSkyReflection = 0.5f; // windshield IBL fallback inside the environment occluders
    bool ssao = false;                  // A3.5 screen-space ambient occlusion of the sky light
    float ssaoRadius = 0.5f;            // world units
    float ssaoIntensity = 1.0f;         // exponent on the visibility
    float windshieldIcing = 0.0f;       // A4.2 icing state of the aircraft 0..1
    float skyPortalStrength = 0.35f;    // A3.7 ASOBO_sky_portal window light
    float decalDepthToleranceMm = 1.2f; // decal buffer depth tolerance along the receiver normal

    bool operator==(const SceneEffectSettings&) const = default;
};

} // namespace ux3d::imgui
