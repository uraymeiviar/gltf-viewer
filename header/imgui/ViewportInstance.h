// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "imgui/Framebuffer.h"
#include "imgui/AntiAliasing.h"
#include "imgui/ViewportSettingsPanel.h"
#include "imgui/SceneRevisions.h"
#include "renderer/Camera.h"
#include "renderViewInput/CameraActor.h"
#include "gltf/Node.h"
#include <array>
#include <string>
#include <memory>

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
    AntiAliasing antiAliasing;

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
        : id(vpId), name(vpName), rendererCamera(glContext)
    {
        antiAliasing.init();
        cameraActor.setCamera(&rendererCamera);
    }
};

} // namespace ux3d::imgui
