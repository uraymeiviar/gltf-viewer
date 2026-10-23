// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/ObjectActions.h"
#include "imgui/AntiAliasing.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "imgui/SceneHierarchyPanel.h"
#include "renderViewInput/CameraActor.h"
#include "renderer/gltf/GltfRenderLayer.h"
#include "gltf/Gltf.h"
#include "gltf/Node.h"
#include "gltf/Scene.h"

#include <imgui.h>

#include <string>

namespace ux3d::imgui {

namespace {

ux3d::gltf::Node* findParentRecursive(const ux3d::gltf::Node* parent, const ux3d::gltf::Node* node)
{
    if (parent == nullptr) {
        return nullptr;
    }
    for (const auto* child : parent->getChildren()) {
        if (child == node) {
            return const_cast<ux3d::gltf::Node*>(parent);
        }
        if (auto* found = findParentRecursive(child, node); found != nullptr) {
            return found;
        }
    }
    return nullptr;
}

} // namespace

ux3d::gltf::Node* parentNode(const ObjectActionContext& context, const ux3d::gltf::Node* node)
{
    if (node == nullptr || context.scene == nullptr) {
        return nullptr;
    }
    for (const auto* root : context.scene->getNodes()) {
        if (auto* parent = findParentRecursive(root, node); parent != nullptr) {
            return parent;
        }
    }
    return nullptr;
}

ux3d::gltf::Node* firstChildNode(const ux3d::gltf::Node* node)
{
    if (node == nullptr) {
        return nullptr;
    }
    for (auto* child : node->getChildren()) {
        if (child != nullptr) {
            return child;
        }
    }
    return nullptr;
}

ux3d::gltf::Node* selectedNode(const ObjectActionContext& context)
{
    return context.selection.type == SelectionType::Node ? context.selection.selectedNode : nullptr;
}

void selectNode(const ObjectActionContext& context, ux3d::gltf::Node* node)
{
    if (node == nullptr) {
        return;
    }
    context.selection.selectNode(node);
    SceneHierarchyPanel::requestScrollToSelectedNode();
    context.revisions.mark(SceneChange::Selection);
}

void focusNode(const ObjectActionContext& context, ux3d::gltf::Node* node)
{
    if (node == nullptr) {
        return;
    }
    if (context.camera != nullptr) {
        context.camera->presentFocusNode(context.layer, node, &context.gltfModel);
    }
    if (context.antiAliasing != nullptr) {
        context.antiAliasing->resetHistory();
    }
    context.revisions.mark(SceneChange::ViewCamera);
}

void toggleNodeVisibility(const ObjectActionContext& context, ux3d::gltf::Node* node)
{
    if (node == nullptr) {
        return;
    }
    node->setVisible(!node->getVisible());
    context.revisions.mark(SceneChange::Visibility);
}

void setIsolation(const ObjectActionContext& context, ux3d::gltf::Node* node, bool includeChildren)
{
    if (context.layer == nullptr) {
        return;
    }
    context.layer->setIsolation(node, includeChildren);
    context.revisions.mark(SceneChange::Visibility);
}

void exitIsolation(const ObjectActionContext& context)
{
    if (context.layer == nullptr) {
        return;
    }
    context.layer->setIsolation(nullptr, false);
    context.revisions.mark(SceneChange::Visibility);
}

bool isolationActive(const ObjectActionContext& context)
{
    return isolationRoot(context) != nullptr;
}

ux3d::gltf::Node* isolationRoot(const ObjectActionContext& context)
{
    return context.layer != nullptr ? const_cast<ux3d::gltf::Node*>(context.layer->getIsolationRoot()) : nullptr;
}

void selectParent(const ObjectActionContext& context, ux3d::gltf::Node* node)
{
    selectNode(context, parentNode(context, node));
}

void selectFirstChild(const ObjectActionContext& context, ux3d::gltf::Node* node)
{
    selectNode(context, firstChildNode(node));
}

void drawObjectMenuItems(const ObjectActionContext& context, ux3d::gltf::Node* node, bool includeSelect, bool showHeader)
{
    const bool hasNode = node != nullptr;
    const bool isolated = isolationActive(context);
    auto* parent = hasNode ? parentNode(context, node) : nullptr;
    auto* child = hasNode ? firstChildNode(node) : nullptr;

    if (showHeader && hasNode) {
        const std::string name = node->hasName() ? node->getName() : ("Node " + std::to_string(node->m_ID));
        ImGui::TextColored(ImVec4(0.35f, 0.78f, 1.0f, 1.0f), ICON_MDI_CUBE_OUTLINE " %s", name.c_str());
        ImGui::Separator();
    }

    if (includeSelect) {
        if (ImGui::MenuItem(ICON_MDI_CURSOR_DEFAULT " Select")) {
            selectNode(context, node);
        }
        ImGui::Separator();
    }

    if (ImGui::MenuItem(ICON_MDI_CROSSHAIRS_GPS " Focus", "F", false, hasNode && context.camera != nullptr)) {
        focusNode(context, node);
    }
    const bool visible = hasNode && node->getVisible();
    if (ImGui::MenuItem(hasNode && !visible ? ICON_MDI_EYE " Unhide" : ICON_MDI_EYE_OFF_OUTLINE " Hide", "H", false, hasNode)) {
        toggleNodeVisibility(context, node);
    }

    ImGui::Separator();

    if (isolated) {
        if (ImGui::MenuItem(ICON_MDI_CLOSE " Exit Isolate", "/")) {
            exitIsolation(context);
        }
    } else {
        if (ImGui::MenuItem(ICON_MDI_FILTER " Isolate", "/", false, hasNode)) {
            setIsolation(context, node, false);
        }
        if (ImGui::MenuItem(ICON_MDI_FILE_TREE " Isolate with Childs", "Shift+/", false, hasNode)) {
            setIsolation(context, node, true);
        }
    }

    ImGui::Separator();

    if (ImGui::MenuItem(ICON_MDI_ARROW_UP_BOLD " Select Parent", "[", false, parent != nullptr)) {
        selectNode(context, parent);
    }
    if (ImGui::MenuItem(ICON_MDI_ARROW_DOWN_BOLD " Select First Child", "]", false, child != nullptr)) {
        selectNode(context, child);
    }
}

} // namespace ux3d::imgui
