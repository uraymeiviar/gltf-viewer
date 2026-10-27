// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "imgui/SceneRevisions.h"

#include <cstdint>
#include <string>

namespace ux3d::renderer {
class GltfRenderLayer;
}

namespace ux3d::imgui {
struct ViewportSettingsState;

// Phase 7: settings that trade image quality for memory, startup time or fill cost.
// They apply to every viewport. Configured targets are preserved independently of
// the visible, opt-out adaptive effective settings.
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
    bool autoQuality = true;
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

// The four editable user profiles. Selection 0 is Auto; 1..4 address these profiles.
struct QualityProfile {
    RenderQualitySettings render;
    uint32_t textureMaxSize = 0;
    int msaaIdx = 0, postAAIdx = 0, ssrQuality = 0;
    bool skybox = true, ibl = true, lights = true, ssr = false, bloom = false;
};

class RenderQuality {
public:
    static RenderQualitySettings& settings();
    static RenderQualitySettings effective();
    static void configureAuto(int argc, char** argv);
    static void updateAuto(double frameMs, ux3d::renderer::GltfRenderLayer* layer, SceneRevisions& revisions);
    static bool autoActive();
    static bool allowIbl();
    static bool allowBloom(bool requested);
    static ViewportSettingsState effectiveViewport(ViewportSettingsState& configured);
    static const std::string& autoStatus();
    static int autoLevel();
    static bool wantsSample();
    static bool legacyCaptureUi();
    static bool qualityReached();
    static uint32_t textureMaxSize();
    // True when the selected profile (Auto targets Ultra) requests unlimited textures.
    static bool unlimitedTextures();
    static bool takeTextureRefresh();
    // Peeks without consuming, so the frame that requested the refresh can capture
    // its presented image before the blocking re-decode starts.
    static bool textureRefreshPending();
    static int selectedProfile();
    static void selectProfile(int selection);
    static const char* profileName(int selection);
    static void drawProfileSelector(SceneRevisions& revisions);
    static void drawProfilesEditor(SceneRevisions& revisions);
    static bool allowSkybox(bool requested);
    static bool allowLights(bool requested);
    static double gpuFrameMs();
    static void beginGpuSample();
    static void endGpuSample(bool rendered = true);
    static void releaseGpuSamples();

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
