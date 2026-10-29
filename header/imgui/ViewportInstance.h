// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "imgui/Framebuffer.h"
#include "imgui/AntiAliasing.h"
#include "imgui/AmbientOcclusion.h"
#include "imgui/ViewportSettingsPanel.h"
#include "imgui/SceneRevisions.h"
#include "imgui/SunShadowMap.h"
#include "renderer/CascadedShadows.h"
#include "renderer/Camera.h"
#include "renderer/PickingRenderLayer.h"
#include "renderViewInput/CameraActor.h"
#include "gltf/Node.h"
#include <array>
#include <string>
#include <memory>

namespace ux3d::imgui {
class OceanSimulation;
}
#include <unordered_map>

namespace ux3d::gltf {
class Node;
class Gltf;
class Scene;
}

namespace ux3d::renderer {
class GltfRenderLayer;
}

namespace ux3d::imgui {

// Everything the viewport's scene image is drawn from (P5.3). While it is unchanged,
// the last image is shown again instead of being redrawn.
struct ViewportRenderKey {
    const void* layer = nullptr;
    const void* scene = nullptr;
    std::array<uint64_t, kSceneChangeCount> revisions{};
    // Layer generations (P4.4): the status bar's Stop/End/Rewind apply a pose
    // without marking a revision.
    uint64_t layerSceneGeneration = 0;
    uint64_t layerPoseGeneration = 0;
    glm::mat4 view{ 1.0f };
    glm::mat4 projection{ 1.0f };
    glm::vec3 eye{ 0.0f };
    float zoom = 0.0f;
    int width = 0;
    int height = 0;
    int samples = 0;
    int aaMode = -1;
    uint64_t aaTargetGeneration = 0;
    bool attached = false;
    const void* attachedCameraNode = nullptr;
    ViewportSettingsState settings;
    SceneEnvironmentState environment;
    uint64_t skyOcclusion = 0; // A3.9: key of the sky-visibility volume the render reads (0 = none)
    // A6.8: the ocean's precomputed set and time (both 0 unless the viewport's ground is the ocean).
    uint64_t oceanVersion = 0;
    double oceanTime = 0.0;

    bool operator==(const ViewportRenderKey&) const = default;
};

struct ViewportInstance {
    uint32_t id = 1;
    std::string name = "3D Viewport";
    bool isOpen = true;
    bool requestFocus = false;

    // glTF Camera attachment
    bool isAttachedToCamera = false;
    const ux3d::gltf::Node* attachedCameraNode = nullptr;
    uint32_t attachedCameraNodeId = 0;
    std::string attachedCameraNodeName = "";

    // Rendering resources
    Framebuffer sceneFbo;
    Framebuffer pickingFbo;
    Framebuffer overlayFbo; // TAA display composite; never fed back into history
    ux3d::renderer::PickingRenderLayer overlayDepthLayer;
    AntiAliasing antiAliasing;
    // SSR overlays meter a normally shaded scene, so diagnostic colours never drive EV.
    Framebuffer ssrExposureFbo;
    std::unique_ptr<AntiAliasing> ssrExposureAa;
    // A3.2: the sun's cascaded shadow map, fitted to this viewport's frustum (released while the sun or its
    // shadows are off), and the cascades of its last render (reports and get_state).
    SunShadowMap sunShadowMap;
    ux3d::renderer::ShadowCascadeSetup sunShadowSetup;
    bool sunShadowActive = false;

    // A3.6: the ground plane of this viewport's last render (the plane is drawn only in shaded mode). The height
    // is the resolved plane height in scene units. groundAlbedo is the plane's average albedo, the value the A6.2
    // sky reads as its ground albedo (lower hemisphere of the environment lighting): A6.2 takes it from the
    // active viewport when groundPlaneActive, otherwise it keeps the HDR's own lower half.
    // A3.5: this viewport's SSAO targets (nothing allocated while its SSAO is off) and whether its last render
    // computed and applied the term.
    AmbientOcclusion ambientOcclusion;
    bool ssaoActive = false;
    // A3.9: whether this viewport's last render read the scene's sky-visibility volume.
    bool skyOcclusionActive = false;
    // A3.7: the number of sky portals that lit this viewport's last render (0 when none or switched off).
    int skyPortalCount = 0;
    // A6.4: the pre-exposure this viewport's last render wrote its scene target with (1 = none) and its exposure.
    float preExposure = 1.0f;
    bool physicalExposure = false; // the last render used the procedural sky's physically based exposure
    // A6.4b: the pre-exposure the physically based Auto exposure asks for (2^round(adapted EV); 0 = none yet): the
    // scene target is written near the displayed range, so lit screens far above the time-of-day curve's exposure
    // do not saturate the half-float target.
    float autoPreExposure = 0.0f;
    // A6.6: the light the shadow map was rendered for (the moon at night) and its direction.
    bool sunShadowFromMoon = false;
    glm::vec3 sunShadowDirection = glm::vec3(0.0f, 1.0f, 0.0f);
    float displayedExposureEv = 0.0f;

    bool groundPlaneActive = false;
    float groundPlaneHeight = 0.0f;
    glm::vec3 groundAlbedo = glm::vec3(0.0f);
    // A6.8: this viewport's ocean simulation (created while its ground is the ocean, released otherwise; shared_ptr so
    // the type stays incomplete here) and whether its last render drew the ocean (groundPlaneActive is then true too,
    // groundPlaneHeight is the sea level).
    std::shared_ptr<OceanSimulation> ocean;
    bool oceanActive = false;
    double oceanTime = 0.0;  // the ocean time of the last render, seconds
    double oceanClock = 0.0; // the viewport's ocean clock, seconds (Application::updateOceanClock)

    // Material variants live as long as this viewport. The model generation covers
    // full reloads and material-only Inspector/ASOBO syncs; viewport settings cover
    // per-view defines and render policy. Shared source materials have one variant.
    const ux3d::renderer::GltfRenderLayer* wireMaterialLayer = nullptr;
    uint64_t wireMaterialGeneration = 0;
    uint64_t wireRenderSettingsRevision = 0;
    ViewportSettingsState wireMaterialSettings;
    std::unordered_map<ux3d::renderer::Material*, std::unique_ptr<ux3d::renderer::Material>> wireMaterials;

    // Viewport camera
    ux3d::renderer::Camera rendererCamera;
    ux3d::editor::CameraActor cameraActor;

    // Per-viewport settings
    ViewportSettingsState settings;

    // Last executed pass names
    std::vector<std::string> lastExecutedPasses;
    uint64_t renderCount = 0;

    // Cached scene redraw (P5.3): the key and display texture of the last drawn image.
    // hasCachedImage is cleared whenever the AA targets that may hold it are released.
    ViewportRenderKey lastRenderKey;
    GLuint cachedDisplayTexture = 0;
    bool hasCachedImage = false;
    uint32_t taaStableRenders = 0;  // consecutive TAA renders with an unchanged key
    uint64_t sceneRenders = 0;
    uint64_t sceneSkips = 0;

    ViewportInstance(uint32_t vpId, const std::string& vpName, ux3d::renderer::gl::GL* glContext)
        : id(vpId), name(vpName), overlayDepthLayer(glContext), rendererCamera(glContext)
    {
        antiAliasing.init();
        cameraActor.setCamera(&rendererCamera);
    }
};

} // namespace ux3d::imgui
