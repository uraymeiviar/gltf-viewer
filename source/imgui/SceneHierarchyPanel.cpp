// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/SceneHierarchyPanel.h"
#include "imgui/AppSettings.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "gltf/Gltf.h"
#include "gltf/Node.h"
#include "gltf/Mesh.h"
#include "gltf/Material.h"
#include "gltf/Camera.h"
#include "gltf/Light.h"
#include "gltf/Scene.h"
#include <imgui.h>
#include <string>
#include <algorithm>
#include <cctype>

namespace ux3d::imgui {

static char s_searchBuffer[128] = "";
static bool s_scrollToSelected = false;

void SceneHierarchyPanel::requestScrollToSelectedNode()
{
    s_scrollToSelected = true;
}

void SceneHierarchyPanel::resetSearch()
{
    s_searchBuffer[0] = '\0';
}

void SceneHierarchyPanel::setSearchFilter(const std::string& filter)
{
    std::snprintf(s_searchBuffer, sizeof(s_searchBuffer), "%s", filter.c_str());
}

static std::string toLowerString(const std::string& str)
{
    std::string lower = str;
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return lower;
}

static bool isAncestorOf(const ux3d::gltf::Node* parent, const ux3d::gltf::Node* target)
{
    if (parent == nullptr || target == nullptr) return false;
    for (const auto* child : parent->getChildren()) {
        if (child == target || isAncestorOf(child, target)) return true;
    }
    return false;
}

static bool nodeMatchesFilter(const ux3d::gltf::Node* node, const std::string& lowerFilter)
{
    if (lowerFilter.empty()) return true;
    if (node == nullptr) return false;

    // Check node's own name
    if (node->hasName()) {
        if (toLowerString(node->getName().c_str()).find(lowerFilter) != std::string::npos) {
            return true;
        }
    } else {
        std::string fallback = "node " + std::to_string(node->m_ID);
        if (fallback.find(lowerFilter) != std::string::npos) {
            return true;
        }
    }

    // Check mesh name if attached
    if (const auto* mesh = node->getMesh()) {
        if (mesh->hasName()) {
            if (toLowerString(mesh->getName().c_str()).find(lowerFilter) != std::string::npos) {
                return true;
            }
        }
    }

    // Check children recursively
    for (const auto* child : node->getChildren()) {
        if (nodeMatchesFilter(child, lowerFilter)) {
            return true;
        }
    }

    return false;
}

static std::string formatLabel(const char* icon,
                               const char* title,
                               uint32_t id,
                               const std::string& name,
                               const HierarchyDisplayOptions& opts)
{
    std::string res;
    if (opts.showIcon && icon && icon[0] != '\0') {
        res += icon;
        res += " ";
    }
    if (opts.showTitle && title && title[0] != '\0') {
        res += title;
        res += " ";
    }
    if (opts.showIndex) {
        res += std::to_string(id);
        res += " ";
    }
    if (opts.showName && !name.empty()) {
        res += "\"" + name + "\"";
    }

    // Trim trailing whitespace
    while (!res.empty() && (res.back() == ' ' || res.back() == '\t')) {
        res.pop_back();
    }

    // Fallback if all options were unchecked so item remains clickable and visible
    if (res.empty()) {
        if (!name.empty()) return "\"" + name + "\"";
        if (title) return std::string(title) + " " + std::to_string(id);
        return std::to_string(id);
    }
    return res;
}

static void renderNodeVisibilityToggle(ux3d::gltf::Node* node, bool& sceneDirty)
{
    float buttonWidth = 20.0f;
    float eyePos = ImGui::GetWindowContentRegionMax().x - buttonWidth;
    ImGui::SameLine(eyePos);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.25f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    bool isVisible = node->getVisible();
    const char* icon = isVisible ? ICON_MDI_EYE : ICON_MDI_EYE_OFF_OUTLINE;
    if (!isVisible) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
    }

    std::string btnLabel = std::string(icon) + "##eye_n_" + std::to_string(node->m_ID);
    if (ImGui::Button(btnLabel.c_str(), ImVec2(buttonWidth, ImGui::GetFrameHeight()))) {
        node->setVisible(!isVisible);
        sceneDirty = true;
    }

    if (!isVisible) {
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(isVisible ? "Visible (click to hide)" : "Hidden (click to show)");
    }
}

static void renderMeshVisibilityToggle(ux3d::gltf::Mesh* mesh, bool& sceneDirty)
{
    float buttonWidth = 20.0f;
    float eyePos = ImGui::GetWindowContentRegionMax().x - buttonWidth;
    ImGui::SameLine(eyePos);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.25f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    bool isVisible = mesh->getVisible();
    const char* icon = isVisible ? ICON_MDI_EYE : ICON_MDI_EYE_OFF_OUTLINE;
    if (!isVisible) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
    }

    std::string btnLabel = std::string(icon) + "##eye_m_" + std::to_string(mesh->m_ID);
    if (ImGui::Button(btnLabel.c_str(), ImVec2(buttonWidth, ImGui::GetFrameHeight()))) {
        mesh->setVisible(!isVisible);
        sceneDirty = true;
    }

    if (!isVisible) {
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(isVisible ? "Visible (click to hide)" : "Hidden (click to show)");
    }
}

static void focusCameraOnNode(ux3d::gltf::Node* node,
                              ux3d::gltf::Gltf& gltfModel,
                              ux3d::renderer::GltfRenderLayer* gltfLayer,
                              ux3d::editor::CameraActor& cameraActor,
                              ux3d::imgui::AntiAliasing& antiAliasing,
                              bool& sceneDirty)
{
    if (node == nullptr) return;
    cameraActor.presentFocusNode(gltfLayer, node, &gltfModel);
    antiAliasing.resetHistory();
    sceneDirty = true;
}

static void renderNodeRecursive(ux3d::gltf::Node* node,
                                ux3d::gltf::Gltf& gltfModel,
                                SelectionState& selection,
                                ux3d::renderer::GltfRenderLayer* gltfLayer,
                                ux3d::editor::CameraActor& cameraActor,
                                AntiAliasing& antiAliasing,
                                bool& sceneDirty,
                                const HierarchyDisplayOptions& opts,
                                const std::string& lowerFilter)
{
    if (node == nullptr) return;

    // Filter check: skip if neither this node nor its descendants match
    if (!lowerFilter.empty() && !nodeMatchesFilter(node, lowerFilter)) {
        return;
    }

    ImGui::PushID(static_cast<int>(node->m_ID));

    // Auto-expand if search query matches a descendant
    if (!lowerFilter.empty()) {
        ImGui::SetNextItemOpen(true);
    }

    // Auto-expand parents if selected element is a descendant
    if (selection.type == SelectionType::Node && selection.selectedNode != nullptr) {
        if (isAncestorOf(node, selection.selectedNode)) {
            ImGui::SetNextItemOpen(true);
        }
    } else if (selection.type == SelectionType::Mesh && selection.selectedMesh != nullptr) {
        if (node->getMesh() == selection.selectedMesh) {
            ImGui::SetNextItemOpen(true);
        }
    } else if (selection.type == SelectionType::Material && selection.selectedMaterial != nullptr) {
        if (node->getMesh() != nullptr) {
            for (const auto& prim : node->getMesh()->getPrimitives()) {
                if (prim.getMaterial() == selection.selectedMaterial) {
                    ImGui::SetNextItemOpen(true);
                    break;
                }
            }
        }
    }

    bool hasMesh = (node->getMesh() != nullptr);
    bool hasCamera = (node->getCamera() != nullptr);
    bool hasLight = (node->getLight() != nullptr);
    bool hasChildren = !node->getChildren().empty();
    bool isLeaf = (!hasMesh && !hasCamera && !hasLight && !hasChildren);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap;
    bool isSelected = (selection.type == SelectionType::Node && selection.selectedNode == node);
    if (isSelected) {
        flags |= ImGuiTreeNodeFlags_Selected;
        if (s_scrollToSelected) {
            ImGui::SetScrollHereY(0.5f);
            s_scrollToSelected = false;
        }
    }
    if (isLeaf) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    std::string name = node->hasName() ? node->getName().c_str() : "";
    std::string label = formatLabel(ICON_MDI_SOURCE_BRANCH, "Node", node->m_ID, name, opts);

    bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(node->m_ID)), flags, "%s", label.c_str());

    // Single-click selection & focus
    if (ImGui::IsItemClicked()) {
        selection.selectNode(node);
        focusCameraOnNode(node, gltfModel, gltfLayer, cameraActor, antiAliasing, sceneDirty);
    }
    // Double-click focus
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        selection.selectNode(node);
        focusCameraOnNode(node, gltfModel, gltfLayer, cameraActor, antiAliasing, sceneDirty);
    }

    // Right-aligned eye toggle button on the node row
    renderNodeVisibilityToggle(node, sceneDirty);

    // If open and has children elements
    if (open && !isLeaf) {
        // 1. Mesh child element
        if (hasMesh) {
            auto* mesh = node->getMesh();
            std::string meshName = mesh->hasName() ? mesh->getName().c_str() : "";
            std::string meshLabel = formatLabel(ICON_MDI_CUBE_OUTLINE, "Mesh", mesh->m_ID, meshName, opts);

            bool meshHasPrims = !mesh->getPrimitives().empty();
            ImGuiTreeNodeFlags meshFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap;
            if (selection.type == SelectionType::Mesh && selection.selectedMesh == mesh) {
                meshFlags |= ImGuiTreeNodeFlags_Selected;
            }
            if (!meshHasPrims) {
                meshFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            }

            // Auto-expand mesh if selected material belongs to one of its primitives
            if (selection.type == SelectionType::Material && selection.selectedMaterial != nullptr) {
                for (const auto& prim : mesh->getPrimitives()) {
                    if (prim.getMaterial() == selection.selectedMaterial) {
                        ImGui::SetNextItemOpen(true);
                        break;
                    }
                }
            }

            bool meshOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(mesh->m_ID + 2000000)), meshFlags, "%s", meshLabel.c_str());
            if (ImGui::IsItemClicked()) {
                selection.selectMesh(mesh);
                sceneDirty = true;
            }

            // Right-aligned eye toggle button for mesh
            renderMeshVisibilityToggle(mesh, sceneDirty);

            if (meshOpen && meshHasPrims) {
                uint32_t primIdx = 0;
                for (auto& prim : mesh->getPrimitives()) {
                    auto* mat = prim.getMaterial();
                    bool primHasMat = (mat != nullptr);

                    std::string primLabel = formatLabel(ICON_MDI_VECTOR_TRIANGLE, "Primitive", primIdx, "", opts);
                    ImGuiTreeNodeFlags primFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

                    if (!primHasMat) {
                        primFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    }

                    if (selection.type == SelectionType::Material && selection.selectedMaterial == mat) {
                        ImGui::SetNextItemOpen(true);
                    }

                    bool primOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(mesh->m_ID * 1000 + primIdx + 3000000)), primFlags, "%s", primLabel.c_str());

                    if (primOpen && primHasMat) {
                        std::string matName = mat->hasName() ? mat->getName().c_str() : "";
                        std::string matLabel = formatLabel(ICON_MDI_CIRCLE_HALF_FULL, "Material", mat->m_ID, matName, opts);

                        ImGuiTreeNodeFlags matFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
                        if (selection.type == SelectionType::Material && selection.selectedMaterial == mat) {
                            matFlags |= ImGuiTreeNodeFlags_Selected;
                        }

                        ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(mat->m_ID + 4000000)), matFlags, "%s", matLabel.c_str());
                        if (ImGui::IsItemClicked()) {
                            selection.selectMaterial(mat);
                            sceneDirty = true;
                        }

                        ImGui::TreePop(); // pop primitive
                    }
                    primIdx++;
                }
                ImGui::TreePop(); // pop mesh
            }
        }

        // 2. Camera child element
        if (hasCamera) {
            auto* cam = node->getCamera();
            std::string camName = cam->hasName() ? cam->getName().c_str() : "";
            std::string camLabel = formatLabel(ICON_MDI_CAMERA, "Camera", cam->m_ID, camName, opts);

            ImGuiTreeNodeFlags camFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (selection.type == SelectionType::Camera && selection.selectedCamera == cam) {
                camFlags |= ImGuiTreeNodeFlags_Selected;
            }

            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(cam->m_ID + 5000000)), camFlags, "%s", camLabel.c_str());
            if (ImGui::IsItemClicked()) {
                selection.selectCamera(cam);
                sceneDirty = true;
            }
        }

        // 3. Light child element
        if (hasLight) {
            auto* light = node->getLight();
            std::string lightName = light->hasName() ? light->getName().c_str() : "";
            std::string lightLabel = formatLabel(ICON_MDI_LIGHTBULB_OUTLINE, "Light", light->m_ID, lightName, opts);

            ImGuiTreeNodeFlags lightFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (selection.type == SelectionType::Light && selection.selectedLight == light) {
                lightFlags |= ImGuiTreeNodeFlags_Selected;
            }

            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(light->m_ID + 6000000)), lightFlags, "%s", lightLabel.c_str());
            if (ImGui::IsItemClicked()) {
                selection.selectLight(light);
                sceneDirty = true;
            }
        }

        // 4. Child Nodes
        for (auto* child : node->getChildren()) {
            renderNodeRecursive(child, gltfModel, selection, gltfLayer, cameraActor, antiAliasing, sceneDirty, opts, lowerFilter);
        }

        ImGui::TreePop(); // pop node
    }

    ImGui::PopID();
}

void SceneHierarchyPanel::render(ux3d::gltf::Gltf& gltfModel,
                                 ux3d::gltf::Scene* activeScene,
                                 SelectionState& selection,
                                 ux3d::renderer::GltfRenderLayer* gltfLayer,
                                 ux3d::editor::CameraActor& cameraActor,
                                 AntiAliasing& antiAliasing,
                                 bool& isSceneDirty,
                                 bool* pOpen)
{
    static const char* const WND_HIERARCHY = ICON_MDI_FILE_TREE " Scene Hierarchy";

    if (!ImGui::Begin(WND_HIERARCHY, pOpen)) {
        ImGui::End();
        return;
    }

    HierarchyDisplayOptions opts = AppSettings::instance().getHierarchyOptions();

    // -----------------------------------------------------------------
    // Top Bar: Search Input (Left) + Options Burger Menu (Right)
    // -----------------------------------------------------------------
    float burgerBtnWidth = 26.0f;
    float clearBtnWidth = (s_searchBuffer[0] != '\0') ? 24.0f : 0.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float availWidth = ImGui::GetContentRegionAvail().x;
    float searchWidth = availWidth - burgerBtnWidth - spacing;
    if (clearBtnWidth > 0.0f) {
        searchWidth -= (clearBtnWidth + spacing);
    }
    if (searchWidth < 60.0f) searchWidth = 60.0f;

    ImGui::SetNextItemWidth(searchWidth);
    ImGui::InputTextWithHint("##HierarchySearch", ICON_MDI_MAGNIFY " Filter nodes...", s_searchBuffer, sizeof(s_searchBuffer));

    if (clearBtnWidth > 0.0f) {
        ImGui::SameLine();
        if (ImGui::Button(ICON_MDI_CLOSE "##ClearSearch", ImVec2(clearBtnWidth, 0.0f))) {
            s_searchBuffer[0] = '\0';
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Clear filter");
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_MENU "##HierarchyBurger", ImVec2(burgerBtnWidth, 0.0f))) {
        ImGui::OpenPopup("HierarchyBurgerMenu");
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Display Options");

    if (ImGui::BeginPopup("HierarchyBurgerMenu")) {
        bool changed = false;
        if (ImGui::MenuItem("Show Icon", nullptr, &opts.showIcon)) changed = true;
        if (ImGui::MenuItem("Show Title", nullptr, &opts.showTitle)) changed = true;
        if (ImGui::MenuItem("Show Index", nullptr, &opts.showIndex)) changed = true;
        if (ImGui::MenuItem("Show Name", nullptr, &opts.showName)) changed = true;

        if (changed) {
            AppSettings::instance().setHierarchyOptions(opts);
            AppSettings::instance().saveSettings();
        }
        ImGui::EndPopup();
    }

    ImGui::Separator();

    // -----------------------------------------------------------------
    // Hierarchy Tree View (Child window so top search bar never scrolls)
    // -----------------------------------------------------------------
    ImGui::BeginChild("##HierarchyTreeScrollRegion", ImVec2(0, 0), false);

    if (activeScene != nullptr) {
        std::string lowerFilter = toLowerString(s_searchBuffer);

        std::string sceneName = activeScene->hasName() ? activeScene->getName().c_str() : "";
        std::string sceneLabel = formatLabel(ICON_MDI_IMAGE_FILTER_HDR, "Scene", activeScene->m_ID, sceneName, opts);

        if (ImGui::TreeNodeEx(sceneLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            size_t renderedCount = 0;
            for (auto* rootNode : activeScene->getNodes()) {
                if (lowerFilter.empty() || nodeMatchesFilter(rootNode, lowerFilter)) {
                    renderNodeRecursive(rootNode, gltfModel, selection, gltfLayer, cameraActor, antiAliasing, isSceneDirty, opts, lowerFilter);
                    renderedCount++;
                }
            }

            if (!lowerFilter.empty() && renderedCount == 0) {
                ImGui::Spacing();
                ImGui::TextDisabled("  No nodes matching \"%s\"", s_searchBuffer);
            }

            ImGui::TreePop();
        }
    } else {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No active glTF scene.");
    }

    ImGui::EndChild();

    ImGui::End();
}

} // namespace ux3d::imgui
