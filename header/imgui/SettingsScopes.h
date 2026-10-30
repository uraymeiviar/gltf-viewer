// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

namespace ux3d::imgui {

// Quality owns enablement and budgets; Scene owns shared appearance and ground;
// Viewport owns camera, overlays and Shaded/Shaded+post. Vendor tuning lives in
// GltfExtensionSettings. This enum survives only for deprecated no-op keys/reports.
enum class EffectOverride : int {
    Scene = 0,
    Off = 1,
    On = 2,
};

inline const char* effectOverrideName(EffectOverride value)
{
    static constexpr const char* names[] = { "scene", "off", "on" };
    const int index = static_cast<int>(value);
    return names[index >= 0 && index <= 2 ? index : 0];
}

// Session-only appearance values shared by the scene's viewports.
struct SceneEffectSettings {
    float ssaoRadius = 0.5f;            // world units
    float ssaoIntensity = 1.0f;         // exponent on the visibility
    float windshieldIcing = 0.0f;       // A4.2 icing state of the aircraft 0..1
    float skyPortalStrength = 0.35f;    // hidden compatibility override; ordinary portals use 0.35

    bool operator==(const SceneEffectSettings&) const = default;
};

} // namespace ux3d::imgui
