// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <array>
#include <glm/vec3.hpp>
#include <string_view>

namespace ux3d::imgui {

// A3.6: what a viewport draws on the ground (docs/ASOBO_EXTENSIONS_PHASE_3.md). Grid is the editor's
// infinite grid overlay (the default, unchanged). Plane is an infinite horizontal surface rendered into the
// linear HDR scene target. Ocean (A6.8) will be the third entry; it is shown disabled in the UI and has no
// registry value yet.
enum class GroundMode : int {
    Grid = 0,
    Plane = 1,
};

// Where the plane lies (glTF Y-up): at the scene's lowest point, at Y = 0, or at a chosen height.
enum class GroundHeightMode : int {
    ModelBase = 0,
    Zero = 1,
    Custom = 2,
};

enum class GroundPreset : int {
    Concrete = 0,
    Grass = 1,
    Sand = 2,
    Grey = 3,
    Custom = 4,
};

struct GroundPresetInfo {
    std::string_view key;   // registry / CLI value
    std::string_view label; // UI
    glm::vec3 albedo;       // linear base colour = average albedo
    float roughness;
    float detail;           // amount of procedural albedo variation 0..1
    float detailScale;      // size of the largest feature, scene units
    float jointSpacing;     // slab joints every this many scene units, 0 = none
};

inline const std::array<GroundPresetInfo, 4>& groundPresets()
{
    static const std::array<GroundPresetInfo, 4> presets = { {
        { "concrete", "Concrete / tarmac", glm::vec3(0.25f, 0.25f, 0.26f), 0.85f, 0.35f, 1.0f, 5.0f },
        { "grass", "Grass", glm::vec3(0.10f, 0.17f, 0.045f), 0.95f, 0.70f, 0.5f, 0.0f },
        { "sand", "Sand", glm::vec3(0.52f, 0.41f, 0.26f), 0.95f, 0.35f, 0.4f, 0.0f },
        { "grey", "Neutral grey", glm::vec3(0.18f), 0.80f, 0.0f, 1.0f, 0.0f },
    } };
    return presets;
}

struct GroundSettings {
    GroundMode mode = GroundMode::Grid;
    GroundHeightMode heightMode = GroundHeightMode::ModelBase;
    float customHeight = 0.0f; // scene units; used by GroundHeightMode::Custom
    GroundPreset preset = GroundPreset::Concrete;
    glm::vec3 albedo = groundPresets()[0].albedo; // linear; also the plane's average albedo (see below)
    float roughness = groundPresets()[0].roughness;
    float detail = groundPresets()[0].detail;
    float detailScale = groundPresets()[0].detailScale;
    float jointSpacing = groundPresets()[0].jointSpacing;
    float haze = 0.0f;      // visibility distance of the horizon fog, scene units; 0 = automatic (0.9 x far plane)
    bool showGrid = false;  // draw the editor grid on top of the plane (the viewport's Grid switch must be on too)

    // Copies a preset's values into the surface fields (Custom leaves them as they are).
    void applyPreset(GroundPreset value)
    {
        preset = value;
        if (value == GroundPreset::Custom) return;
        const GroundPresetInfo& info = groundPresets()[static_cast<size_t>(value)];
        albedo = info.albedo;
        roughness = info.roughness;
        detail = info.detail;
        detailScale = info.detailScale;
        jointSpacing = info.jointSpacing;
    }

    // The plane's average albedo: the procedural variation has mean 1, so it is the base colour (the slab joints
    // cover well under 1% of the area and are left out). A6.2
    // (docs/ASOBO_EXTENSIONS_PHASE_6.md) reads this as the ground albedo of its sky model.
    glm::vec3 averageAlbedo() const { return albedo; }

    bool operator==(const GroundSettings&) const = default;
};

} // namespace ux3d::imgui
