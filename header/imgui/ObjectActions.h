// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "imgui/ResourcesPanel.h"
#include "imgui/SceneRevisions.h"

namespace ux3d::gltf {
class Gltf;
class Scene;
class Node;
}

namespace ux3d::renderer {
class GltfRenderLayer;
}

namespace ux3d::editor {
class CameraActor;
}

namespace ux3d::imgui {

class AntiAliasing;

// Everything the Object actions need. Assembled at each call site from the current
// selection, the active scene and the viewport the action should affect.
struct ObjectActionContext {
    ux3d::gltf::Gltf& gltfModel;
    ux3d::gltf::Scene* scene = nullptr;
    SelectionState& selection;
    ux3d::renderer::GltfRenderLayer* layer = nullptr;
    ux3d::editor::CameraActor* camera = nullptr;
    AntiAliasing* antiAliasing = nullptr;
    SceneRevisions& revisions;
};

// Active-scene hierarchy navigation. parentNode() returns null for scene roots.
ux3d::gltf::Node* parentNode(const ObjectActionContext& context, const ux3d::gltf::Node* node);
ux3d::gltf::Node* firstChildNode(const ux3d::gltf::Node* node);

// Object actions. Node arguments may be null; the functions then do nothing.
void selectNode(const ObjectActionContext& context, ux3d::gltf::Node* node);
ux3d::gltf::Node* selectedNode(const ObjectActionContext& context);
void focusNode(const ObjectActionContext& context, ux3d::gltf::Node* node);
void toggleNodeVisibility(const ObjectActionContext& context, ux3d::gltf::Node* node);
void setIsolation(const ObjectActionContext& context, ux3d::gltf::Node* node, bool includeChildren);
void exitIsolation(const ObjectActionContext& context);
bool isolationActive(const ObjectActionContext& context);
ux3d::gltf::Node* isolationRoot(const ObjectActionContext& context);
void selectParent(const ObjectActionContext& context, ux3d::gltf::Node* node);
void selectFirstChild(const ObjectActionContext& context, ux3d::gltf::Node* node);

// Draws the six Object menu items for node: Focus, Hide/Unhide, Isolate or Exit
// Isolate, Isolate with Childs (while not isolated), Select Parent and Select First
// Child. includeSelect adds a leading Select item (hierarchy context menu); showHeader
// adds a top label with the node's name in the accent color (context menus).
void drawObjectMenuItems(const ObjectActionContext& context, ux3d::gltf::Node* node, bool includeSelect,
                         bool showHeader = false);

} // namespace ux3d::imgui
