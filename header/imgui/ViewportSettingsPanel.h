// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "renderer/CommandGenerator.h"
#include "imgui/SceneEnvironment.h"
#include "imgui/SceneRevisions.h"
#include "renderer/gltf/GltfRenderLayer.h"
#include "renderer/editor/EditorRenderLayer.h"
#include <functional>

namespace ux3d::editor {
class CameraActor;
}

namespace ux3d::imgui {

struct ViewportSettingsState {
    ux3d::renderer::RenderMode renderMode = ux3d::renderer::RenderMode::None;
    int renderModeIdx = 0; // 0 = None / Shaded
    bool showGrid = true;
    bool forceDoubleSided = false; // Disable backface culling globally
    ux3d::renderer::AsoboOptions asobo; // MSFS (ASOBO) extensions this viewport applies (A0.4)
    float decalDepthToleranceMm = 1.2f; // Receiver-normal tolerance for the decal buffer
    bool invertWinding = false;    // Invert front/back face winding order (CW vs CCW)
    int antiAliasingIdx = 2;       // 0=None, 1=MSAA 2x, 2=MSAA 4x (Default), 3=MSAA 8x, 4=FXAA, 5=TAA

    // Visual Inspection & Diagnostics
    bool wireframeOverlay = false;         // Overlay wireframe mesh edges on top of shaded model
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
