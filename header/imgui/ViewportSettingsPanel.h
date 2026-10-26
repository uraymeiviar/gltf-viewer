// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "renderer/CommandGenerator.h"
#include "imgui/SceneEnvironment.h"
#include "imgui/SceneRevisions.h"
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
    bool forceDoubleSided = false; // Disable backface culling globally
    ux3d::renderer::AsoboOptions asobo; // MSFS (ASOBO) extensions this viewport applies (A0.4)
    bool screenSpaceReflections = true;
    int ssrQuality = 1; // Low/Medium/High: 256/512/1024 pixel-step budgets
    float interiorSkyReflection = 0.5f;
    float decalDepthToleranceMm = 1.2f; // Receiver-normal tolerance for the decal buffer
    bool invertWinding = false;    // Invert front/back face winding order (CW vs CCW)
    // Legacy mode remains the fallback for saved state and existing automation.
    int antiAliasingIdx = 5;       // 0=None, 1/2/3=MSAA 2/4/8x, 4=FXAA, 5=TAA
    int msaaIdx = -1;             // -1=legacy, 0=Off, 1/2/3=2/4/8x
    int postAAIdx = -1;           // -1=legacy, 0=None, 1=FXAA, 2=TAA

    int effectiveMsaaIdx() const { return msaaIdx >= 0 ? msaaIdx :
        (antiAliasingIdx >= 1 && antiAliasingIdx <= 3 ? antiAliasingIdx : 0); }
    int effectivePostAAIdx() const { return postAAIdx >= 0 ? postAAIdx :
        (antiAliasingIdx >= 4 ? antiAliasingIdx - 3 : 0); }
    void setLegacyAA(int mode) { antiAliasingIdx = mode; msaaIdx = postAAIdx = -1; }

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
