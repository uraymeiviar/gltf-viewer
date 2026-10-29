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
struct SceneEffectSettings;

// Phase 7: settings that trade image quality for memory, startup time or fill cost.
// They apply to every viewport. Configured targets are preserved independently of
// the visible, opt-out adaptive effective settings.
enum class QualityPreset {
    High = 0,
    Balanced = 1,
    Performance = 2,
    Custom = 3,
};

// A3.2: sun shadow filter (shadow.glsl).
enum class ShadowFilter {
    Hard = 0, // one bilinear hardware compare
    Pcf = 1,  // 3x3 bilinear compares (smooth 4x4-texel footprint)
    Soft = 2, // 16 rotated Poisson compares, radius 2.5 texels
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

    // A3.2 cascaded sun shadow maps (only while the sun is on). The quality fields (on/off, resolution,
    // cascades, distance, filter) are part of the four quality presets; the split lambda and the bias
    // values are tuning values kept across preset changes.
    bool shadows = true;
    uint32_t shadowResolution = 2048; // 1024 / 2048 / 4096 texels per cascade side
    int shadowCascades = 3;           // 2..4
    float shadowDistance = 200.0f;    // view depth (scene units, metres) where the shadows end
    ShadowFilter shadowFilter = ShadowFilter::Pcf;
    float shadowSplitLambda = 0.75f;  // practical split scheme: 0 uniform .. 1 logarithmic
    float shadowNormalBias = 1.0f;    // receiver normal offset, in shadow-map texels
    float shadowSlopeBias = 2.0f;     // caster glPolygonOffset factor
    float shadowConstantBias = 1.0f;  // caster glPolygonOffset units

    // Settings scopes (docs/SETTINGS_SCOPES.md): the cost side of the per-effect settings, renderer-wide since they
    // left the viewports. Every viewport renders with these values; the scene decides whether an effect is on, a
    // viewport may override that (Scene / Off / On). The defaults are the former per-viewport defaults.
    int msaaIdx = 0;                  // 0 off, 1/2/3 = 2x/4x/8x
    int postAAIdx = 2;                // 0 none, 1 FXAA, 2 TAA
    int ssrQuality = 1;               // 0/1/2: 256/512/1024 screen-space samples
    int ssaoQuality = 1;              // SsaoQuality: 2 x 4, 3 x 6, 4 x 8 samples per side
    bool ssaoHalfResolution = false;  // SSAO at half resolution with a depth-aware upsample
    int skyOcclusionQuality = 1;      // SkyOcclusionQuality: 64 / 96 / 128 cells per axis
    int oceanQuality = 1;             // OceanQuality: cascades x resolution x loop frames

    bool operator==(const RenderQualitySettings&) const = default;

    static RenderQualitySettings fromPreset(QualityPreset preset);
    bool sameIbl(const RenderQualitySettings& other) const;
    // Estimated GPU memory of the filtered IBL textures that stay resident.
    uint64_t estimatedIblBytes() const;
    // A3.2: GPU memory of one viewport's shadow map (32-bit depth, every cascade); 0 when shadows are off.
    uint64_t estimatedShadowBytes() const;
    // Copies the shadow tuning values (lambda, biases) that preset changes keep.
    void copyShadowTuning(const RenderQualitySettings& from);
    // Copies every shadow field.
    void copyShadows(const RenderQualitySettings& from);
    // Copies the per-effect quality fields above (AA, SSR, SSAO, sky occlusion, ocean).
    void copyEffectQuality(const RenderQualitySettings& from);
    // The legacy --aa index (0 none, 1..3 MSAA, 4 FXAA, 5 TAA) of msaaIdx/postAAIdx; -1 for a combination it has no name for.
    int legacyAAIndex() const;
};

// The four editable user profiles. Selection 0 is Auto; 1..4 address these profiles.
// render carries the profile's AA, SSR/SSAO/sky-occlusion/ocean quality; the booleans below are cost gates (an
// effect the profile does not allow stays off even when the scene turns it on, until the user pins it).
struct QualityProfile {
    RenderQualitySettings render;
    uint32_t textureMaxSize = 0;
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
    // A3.2: a user or agent edit of a shadow setting pins the shadow group for this session (Auto quality
    // then uses the configured values instead of its cheaper start).
    static void pinShadows();
    // Settings scopes: an explicit AA edit (global quality) or SSR edit (scene switch, viewport override or SSR
    // quality) pins that group for the session, as the per-viewport edits did before.
    static void pinAA();
    static void pinSsr();
    // Whether the selected profile and Auto quality allow screen-space reflections (the scene / viewport decide
    // whether they are on).
    static bool allowSsr();
    static bool validShadowResolution(uint32_t value);
    static const char* shadowFilterName(ShadowFilter filter);
    // The settings a viewport renders with: its resolved settings (resolveViewport) with the effective (Auto /
    // profile adapted) AA and SSR quality and the SSR gate.
    static ViewportSettingsState effectiveViewport(const ViewportSettingsState& configured);
    // Settings scopes: writes the resolved values into the viewport's mirror fields (AA, SSR, SSAO, shadows, ocean
    // quality from the global quality; effects from the scene through the viewport's overrides). The renderer
    // reads only these mirrors, so every read site stays as it was.
    static void resolveViewport(ViewportSettingsState& viewport, const SceneEffectSettings& effects);
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

    // Draws the Quality tab body of the Viewport Settings panel: every renderer-wide quality setting (preset, AA,
    // render scale, grid, effect quality, IBL filter, sun shadow maps). Re-filters the environment when the IBL
    // fields change.
    static void drawSettings(ux3d::renderer::GltfRenderLayer* layer, SceneRevisions& revisions);

    // Draws the IBL filter size, sample count and precision controls. The Scene Inspector
    // shows them for the selected scene while IBL is enabled; like the rest of the
    // settings they are renderer-wide, so they apply to every viewport. Re-filters the
    // environment when a value changes.
    static void drawIblSettings(ux3d::renderer::GltfRenderLayer* layer, SceneRevisions& revisions);

    static const char* presetName(QualityPreset preset);
};

} // namespace ux3d::imgui
