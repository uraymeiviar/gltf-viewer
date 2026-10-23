// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "imgui/SceneRevisions.h"

#include <cstdint>
#include <string>

namespace ux3d::renderer {
class GltfRenderLayer;
}

namespace ux3d::imgui {

// Phase 7: settings that trade image quality for memory, startup time or fill cost.
// They apply to every viewport. The High preset keeps the original renderer behavior,
// so lower quality is always an explicit choice.
enum class QualityPreset {
    High = 0,
    Balanced = 1,
    Performance = 2,
    Custom = 3,
};

enum class GridMode {
    ScreenRay = 0, // full-screen ray cast with gl_FragDepth (original)
    PlaneMesh = 1, // rasterized plane around the camera; keeps early depth testing
};

struct RenderQualitySettings {
    QualityPreset preset = QualityPreset::High;

    // Image-based lighting filter outputs (shared by all viewports).
    uint32_t iblCubeSize = 1024;      // specular, sheen and skybox cube face size
    uint32_t iblIrradianceSize = 0;   // Lambertian cube face size; 0 = same as iblCubeSize
    uint32_t iblLutSize = 1024;
    bool iblHalfFloat = false;        // RGBA16F instead of RGBA32F
    uint32_t iblSpecularSamples = 1024;

    GridMode gridMode = GridMode::ScreenRay;
    float renderScale = 1.0f;         // viewport render resolution relative to its panel size

    bool operator==(const RenderQualitySettings&) const = default;

    static RenderQualitySettings fromPreset(QualityPreset preset);
    bool sameIbl(const RenderQualitySettings& other) const;
    // Estimated GPU memory of the filtered IBL textures that stay resident.
    uint64_t estimatedIblBytes() const;
};

class RenderQuality {
public:
    static RenderQualitySettings& settings();

    // Copies the IBL fields into the layer; they take effect on the next setEnvironmentMap.
    static void applyIblParameters(ux3d::renderer::GltfRenderLayer& layer);

    // Persisted under %APPDATA%/Glider. Disabled for isolated runs, where load and save
    // do nothing.
    static void setPersistenceEnabled(bool enabled);
    static void load();
    static void save();

    // Parses one quality command-line flag starting at argv[i]; advances i past its value.
    // Returns false when argv[i] is not a quality flag. Sets error on a bad value.
    static bool parseArgument(int argc, char** argv, int& i, std::string& error);

    // Draws the Quality tab body of the Viewport Settings panel: preset, ground grid and
    // render scale. Re-filters the environment when the preset changes the IBL fields.
    static void drawSettings(ux3d::renderer::GltfRenderLayer* layer, SceneRevisions& revisions);

    // Draws the IBL filter size, sample count and precision controls. The Scene Inspector
    // shows them for the selected scene while IBL is enabled; like the rest of the
    // settings they are renderer-wide, so they apply to every viewport. Re-filters the
    // environment when a value changes.
    static void drawIblSettings(ux3d::renderer::GltfRenderLayer* layer, SceneRevisions& revisions);

    static const char* presetName(QualityPreset preset);
};

} // namespace ux3d::imgui
