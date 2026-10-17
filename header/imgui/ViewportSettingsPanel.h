// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "renderer/CommandGenerator.h"
#include "renderer/gltf/GltfRenderLayer.h"
#include "imgui/OrbitCamera.h"
#include <glm/glm.hpp>
#include <string>

namespace ux3d::imgui {

struct ViewportSettingsState {
    bool showBackground = false;
    glm::vec4 clearColor = glm::vec4(0.12f, 0.12f, 0.14f, 1.0f);
    float hdrBlur = 0.0f;
    float hdrIntensity = 1.0f;
    float hdrRotation = 0.0f;
    std::string currentHdrPath = "footprint_court.hdr";

    int toneMappingIdx = 1; // 0 = PBR Neutral, 1 = ACES Hill, 2 = ACES Narkowicz
    float exposureEV = 0.0f; // EV value, exposure = 2^EV
    bool iblEnabled = true;
    bool punctualLightsEnabled = true;

    int renderModeIdx = 0; // 0 = None / Shaded
    bool showGrid = true;
    bool forceDoubleSided = false; // Disable backface culling globally
    bool invertWinding = false;    // Invert front/back face winding order (CW vs CCW)
    int antiAliasingIdx = 2;       // 0=None, 1=MSAA 2x, 2=MSAA 4x (Default), 3=MSAA 8x, 4=FXAA, 5=TAA
};

class ViewportSettingsPanel {
public:
    static void render(ViewportSettingsState& state,
                       ux3d::renderer::CommandGenerator* commandGenerator,
                       ux3d::renderer::GltfRenderLayer* gltfLayer,
                       ux3d::imgui::OrbitCamera& camera,
                       bool& isSceneDirty);
};

} // namespace ux3d::imgui
