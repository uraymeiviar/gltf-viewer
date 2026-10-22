// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "imgui/Framebuffer.h"
#include "imgui/AntiAliasing.h"
#include "imgui/ViewportSettingsPanel.h"
#include "renderer/Camera.h"
#include "renderViewInput/CameraActor.h"
#include "gltf/Node.h"
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

    ViewportInstance(uint32_t vpId, const std::string& vpName, ux3d::renderer::gl::GL* glContext)
        : id(vpId), name(vpName), rendererCamera(glContext)
    {
        antiAliasing.init();
        cameraActor.setCamera(&rendererCamera);
    }
};

} // namespace ux3d::imgui
