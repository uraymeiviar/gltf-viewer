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
    int renderModeIdx = 0; // 0 = None / Shaded
    bool showGrid = true;
    GroundSettings ground; // A3.6: grid (default) or a ground plane (its ocean quality is a resolved mirror, below)
    bool forceDoubleSided = false; // Disable backface culling globally
    ux3d::renderer::AsoboOptions asobo; // MSFS (ASOBO) extensions this viewport applies (A0.4)
    ux3d::renderer::MaterialExtensionOptions materialExtensions;
    bool showSkyPortals = false; // A3.7: draw every portal's gizmo (a selected portal node always shows its own)
    // A6.7 lens effect from the bloom mips (AntiAliasing::renderLensFlare): 0 off (default), 1 ghosts + halo,
    // 2 anamorphic streak, 3 both. Runs inside the bloom chain, so bloom off removes it. A property of this view's
    // camera lens, so it stays per viewport.
    int lensFlare = 0;
    float lensFlareIntensity = 1.0f;
    bool invertWinding = false;    // Invert front/back face winding order (CW vs CCW)

    // Settings scopes (docs/SETTINGS_SCOPES.md): this viewport's overrides of the scene's effects.
    EffectOverride shadowsOverride = EffectOverride::Scene;
    EffectOverride ssrOverride = EffectOverride::Scene;
    EffectOverride ssaoOverride = EffectOverride::Scene;

    // ---- Resolved mirrors ----
    // Written every frame by RenderQuality::resolveViewport from the global quality (AA, SSR / SSAO / ocean quality)
    // and the scene's effects through the overrides above. The renderer, the reports and get_state read them; nothing
    // edits them directly (the UI and the registry edit the global quality, the scene or the overrides).
    bool sunShadows = true;        // scene shadows through shadowsOverride (the quality may still disallow them)
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
    glm::vec4 wireframeColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    WireframeBlendMode wireframeBlend = WireframeBlendMode::Normal;
    bool showSceneBoundingBox = false;     // Yellow AABB box for entire scene
    bool showSelectedBoundingBox = true;   // Orange OBB box for selected node (matching Blender)
    bool showNodeCoordinateGizmo = true;   // 3D coordinate axis gizmo at selected node origin
    int gizmoAxisConvention = 0;           // 0 = Blender Source (Z-Up), 1 = glTF Native (Y-Up)
    bool turntable = false;                // Auto-rotate turntable
    float turntableSpeed = 30.0f;          // deg/sec
    bool showOrientationGizmo = true;      // 3D navigation axes compass in viewport corner

    // Compared whole by the cached scene redraw (P5.3): some controls change these
    // fields without marking a revision.
    bool operator==(const ViewportSettingsState&) const = default;
};

class AntiAliasing;

class ViewportSettingsPanel {
public:
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
