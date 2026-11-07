// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "renderer/CommandGenerator.h"
#include "imgui/GroundSettings.h"
#include "imgui/SceneEnvironment.h"
#include "imgui/SceneRevisions.h"
#include "imgui/SsaoSettings.h"
#include "renderer/gltf/GltfRenderLayer.h"
#include "renderer/editor/EditorRenderLayer.h"
#include <functional>
#include <glm/vec4.hpp>

namespace ux3d::editor {
class CameraActor;
}

namespace ux3d::imgui {

enum class WireframeBlendMode : int {
    Normal = 0,
    Additive = 1,
    Invert = 2,
    XOR = 3
};

struct ViewportSettingsState {
    ux3d::renderer::RenderMode renderMode = ux3d::renderer::RenderMode::None;
    int renderModeIdx = 0; // 0 = Shaded + post; 38 = Shaded; historic debug indices preserved
    bool postEffects = true; // Shaded + post effects (default); Shaded keeps shadows and tone mapping
    bool showGrid = true;
    GroundSettings ground; // resolved scene surface plus global ocean Quality mirrors
    bool forceDoubleSided = false; // Disable backface culling globally
    ux3d::renderer::AsoboOptions asobo; // MSFS (ASOBO) extensions this viewport applies (A0.4)
    ux3d::renderer::MaterialExtensionOptions materialExtensions;
    bool showSkyPortals = false; // A3.7: draw every portal's gizmo (a selected portal node always shows its own)
    // Resolved Quality lens settings (0 off, 1 ghosts+halo, 2 streak, 3 ghosts+streak).
    int lensFlare = 0;
    float lensFlareIntensity = 1.0f;
    bool invertWinding = false;    // Invert front/back face winding order (CW vs CCW)

    // Deprecated no-op compatibility fields. The renderer does not resolve through these.
    EffectOverride shadowsOverride = EffectOverride::Scene;
    EffectOverride ssrOverride = EffectOverride::Scene;
    EffectOverride ssaoOverride = EffectOverride::Scene;

    // ---- Resolved mirrors ----
    // Written every frame by RenderQuality::resolveViewport from the global quality (AA, SSR / SSAO / ocean quality)
    // and scene appearance. Shaded omits post effects. The renderer, the reports and get_state read them; nothing
    // edits them directly (the UI and the registry edit the global quality, the scene or the overrides).
    bool sunShadows = true;        // Quality shadow-map enablement
    bool screenSpaceReflections = true;
    int ssrQuality = 1; // Low/Medium/High: 256/512/1024 pixel-step budgets
    float interiorSkyReflection = 0.5f;
    float decalDepthToleranceMm = 1.2f; // Receiver-normal tolerance for the decal buffer
    float windshieldIcing = 0.0f; // Windshield icing factor 0..1 (A4.2)
    float skyPortalStrength = 0.35f; // A3.7: scale of the ASOBO_sky_portal window light (0..4); 0.35 inferred from the SDK pair
    SsaoSettings ssao; // A3.5: screen-space ambient occlusion of the sky light (off by default)
    int antiAliasingIdx = 5;       // legacy --aa index of the global AA (-1: no legacy name)
    int msaaIdx = -1;             // 0=Off, 1/2/3=2/4/8x (-1 before the first resolve: antiAliasingIdx)
    int postAAIdx = -1;           // 0=None, 1=FXAA, 2=TAA (-1 before the first resolve: antiAliasingIdx)

    int effectiveMsaaIdx() const { return msaaIdx >= 0 ? msaaIdx :
        (antiAliasingIdx >= 1 && antiAliasingIdx <= 3 ? antiAliasingIdx : 0); }
    int effectivePostAAIdx() const { return postAAIdx >= 0 ? postAAIdx :
        (antiAliasingIdx >= 4 ? antiAliasingIdx - 3 : 0); }

    // Visual Inspection & Diagnostics
    bool wireframeOverlay = false;         // Overlay wireframe mesh edges on top of shaded model
    glm::vec4 wireframeColor = glm::vec4(1.0f, 1.0f, 1.0f, 0.5f); // alpha: the overlay's; the Wireframe mode draws opaque
    bool uvGridOverlay = false;            // UV grid over any view but the two texture views, which have textureGrid
    WireframeBlendMode wireframeBlend = WireframeBlendMode::Normal;
    bool showSceneBoundingBox = false;     // Yellow AABB box for entire scene
    bool showSelectedBoundingBox = true;   // Orange OBB box for selected node (matching Blender)
    bool showNodeCoordinateGizmo = true;   // 3D coordinate axis gizmo at selected node origin
    int gizmoAxisConvention = 0;           // 0 = Blender Source (Z-Up), 1 = glTF Native (Y-Up)
    bool turntable = false;                // Auto-rotate turntable
    float turntableSpeed = 30.0f;          // deg/sec
    bool showOrientationGizmo = true;      // 3D navigation axes compass in viewport corner
    int textureDistortionView = 0;         // Texture Distortion debug view: 0 both, 1 shear, 2 area
    float textureHeatmapOpacity = 0.75f;   // Both texture debug views: heat colour over the grey surface (the add-on's 0.3 to 1)
    bool textureGrid = true;               // ... UV grid in them
    int textureGridCells = 1;              // ... cells across the UV square, also of the overlay: 0 8, 1 16, 2 32, 3 64
    bool textureGridLabels = true;         // ... a label in each cell, also in the overlay
    float textureGridOpacity = 0.5f;       // ... lines and labels, also of the overlay; 1 is the add-on's strength
    // Luminance debug view (imgui/LuminanceView.h): session-only view settings, never saved.
    int luminanceScale = 0;                // 0 Absolute (log10 cd/m^2), 1 Exposure-relative (EV around middle grey)
    float luminanceMin = 0.01f;            // Absolute: lower end of the scale in cd/m^2 (max > min > 0)
    float luminanceMax = 100000.0f;        // Absolute: upper end in cd/m^2
    float luminanceEvMin = -6.0f;          // Relative: lower end in EV relative to middle grey at the displayed exposure
    float luminanceEvMax = 6.0f;           // Relative: upper end
    bool luminanceBands = true;            // one band per decade (Absolute) or stop (Relative); off: continuous
    bool luminanceEmissiveOnly = false;    // the emitted luminance alone (no lighting, black background)

    // The scene is shaded like Shaded + post effects: the default view, or the Luminance view without "emissive only".
    bool shadesNormally() const
    {
        return renderMode == ux3d::renderer::RenderMode::None ||
               (renderMode == ux3d::renderer::RenderMode::Luminance && !luminanceEmissiveOnly);
    }

    // Compared whole by the cached scene redraw (P5.3): some controls change these
    // fields without marking a revision.
    bool operator==(const ViewportSettingsState&) const = default;
};

class AntiAliasing;

class ViewportSettingsPanel {
public:
    // The Texture Distortion / Density settings, nothing in other views: in this panel and in the view's legend, tagged
    // for --ui-script with the prefix.
    static void renderTextureMetricsSettings(ViewportSettingsState& state, SceneRevisions& revisions,
                                             AntiAliasing* antiAliasing, const char* probePrefix);
    // The Luminance view's scale, ends, bands and emissive-only switch, nothing in other views: in this panel and in
    // the view's legend, tagged for --ui-script with the prefix.
    static void renderLuminanceSettings(ViewportSettingsState& state, SceneRevisions& revisions,
                                        AntiAliasing* antiAliasing, const char* probePrefix);
    // The UV grid's cells, labels and opacity, shared by the texture views' grid and the overlay.
    static void renderUvGridOptions(ViewportSettingsState& state, SceneRevisions& revisions, AntiAliasing* antiAliasing,
                                    const char* probePrefix, bool enabled);
    static void render(ViewportSettingsState& state,
                       ux3d::renderer::CommandGenerator* commandGenerator,
                       ux3d::renderer::GltfRenderLayer* gltfLayer,
                       ux3d::editor::CameraActor& cameraActor,
                       SceneRevisions& revisions,
                       ux3d::gltf::Gltf* gltfModel = nullptr,
                       bool isAttachedToCamera = false,
                       const std::string& attachedCameraName = "",
                       std::function<void()> onDetachCamera = nullptr,
                       AntiAliasing* antiAliasing = nullptr);
};

} // namespace ux3d::imgui
