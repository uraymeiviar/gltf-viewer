// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Scene Hierarchy panel UI: the pinned visibility (eye) buttons, row rendering with selection, context menus and
// drag targets, and SceneHierarchyPanel::render.

#include "SceneHierarchyInternal.h"

namespace ux3d::imgui {

constexpr float kEyeButtonWidth = 20.0f;

static void beginVisibilityToggle()
{
    // Reserve room at the end of the scrollable row for the pinned eye button, and give
    // the button an opaque background so it never overlaps the label text.
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(kEyeButtonWidth + ImGui::GetStyle().ItemSpacing.x, 0.0f));
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - kEyeButtonWidth);
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.15f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.25f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
}

static void endVisibilityToggle()
{
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
}

static void renderNodeVisibilityToggle(ux3d::gltf::Node* node, SceneRevisions& revisions)
{
    beginVisibilityToggle();

    bool isVisible = node->getVisible();
    const char* icon = isVisible ? ICON_MDI_EYE : ICON_MDI_EYE_OFF_OUTLINE;
    if (!isVisible) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
    }

    std::string btnLabel = std::string(icon) + "##eye_n_" + std::to_string(node->m_ID);
    const bool nodeEyeClicked = ImGui::Button(btnLabel.c_str(), ImVec2(kEyeButtonWidth, ImGui::GetFrameHeight()));
    ux3d::imgui::UiProbe::item("hier.node_eye", static_cast<long long>(node->m_ID));
    if (nodeEyeClicked) {
        node->setVisible(!isVisible);
        revisions.mark(SceneChange::Visibility);
    }

    if (!isVisible) {
        ImGui::PopStyleColor();
    }
    endVisibilityToggle();

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(isVisible ? "Visible (click to hide)" : "Hidden (click to show)");
    }
}

static void renderMeshVisibilityToggle(ux3d::gltf::Mesh* mesh, SceneRevisions& revisions)
{
    beginVisibilityToggle();

    bool isVisible = mesh->getVisible();
    const char* icon = isVisible ? ICON_MDI_EYE : ICON_MDI_EYE_OFF_OUTLINE;
    if (!isVisible) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
    }

    std::string btnLabel = std::string(icon) + "##eye_m_" + std::to_string(mesh->m_ID);
    const bool meshEyeClicked = ImGui::Button(btnLabel.c_str(), ImVec2(kEyeButtonWidth, ImGui::GetFrameHeight()));
    ux3d::imgui::UiProbe::item("hier.mesh_eye", static_cast<long long>(mesh->m_ID));
    if (meshEyeClicked) {
        mesh->setVisible(!isVisible);
        revisions.mark(SceneChange::Visibility);
    }

    if (!isVisible) {
        ImGui::PopStyleColor();
    }
    endVisibilityToggle();

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(isVisible ? "Visible (click to hide)" : "Hidden (click to show)");
    }
}

static void renderViewportVisibilityToggle(ViewportInstance* vp)
{
    beginVisibilityToggle();

    const char* eyeIcon = vp->isOpen ? ICON_MDI_EYE : ICON_MDI_EYE_OFF_OUTLINE;
    if (!vp->isOpen) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
    }

    std::string eyeBtnId = std::string(eyeIcon) + "##vp_eye_" + std::to_string(vp->id);
    const bool vpEyeClicked = ImGui::Button(eyeBtnId.c_str(), ImVec2(kEyeButtonWidth, ImGui::GetFrameHeight()));
    ux3d::imgui::UiProbe::item("hier.vp_eye", static_cast<long long>(vp->id));
    if (vpEyeClicked) {
        vp->isOpen = !vp->isOpen;
        if (vp->isOpen) {
            vp->requestFocus = true;
        }
    }

    if (!vp->isOpen) {
        ImGui::PopStyleColor();
    }
    endVisibilityToggle();

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(vp->isOpen ? "Visible (click to hide)" : "Hidden (click to show)");
    }
}

static void focusCameraOnNode(ux3d::gltf::Node* node,
                              ux3d::gltf::Gltf& gltfModel,
                              ux3d::renderer::GltfRenderLayer* gltfLayer,
                              ux3d::editor::CameraActor& cameraActor,
                              ux3d::imgui::AntiAliasing& antiAliasing,
                              SceneRevisions& revisions)
{
    if (node == nullptr) return;
    cameraActor.presentFocusNode(gltfLayer, node, &gltfModel);
    antiAliasing.resetHistory();
    revisions.mark(SceneChange::ViewCamera);
}

// A node is effectively hidden when its own flag or any ancestor's flag is false: the
// renderer applies the same inherited rule, so the hierarchy can grey the whole subtree.
static bool hierarchyNodeHidden(const ux3d::gltf::Node* node)
{
    while (node != nullptr) {
        if (!node->getVisible()) {
            return true;
        }
        const auto it = s_hierarchyCache.parentOfNode.find(node);
        node = (it != s_hierarchyCache.parentOfNode.end()) ? it->second : nullptr;
    }
    return false;
}

static bool hierarchyRowHidden(const HierarchyRow& row)
{
    switch (row.type) {
        case HierarchyRowType::Node:
            return hierarchyNodeHidden(static_cast<const ux3d::gltf::Node*>(row.object));
        case HierarchyRowType::Mesh: {
            const auto* mesh = static_cast<const ux3d::gltf::Mesh*>(row.object);
            return (mesh != nullptr && !mesh->getVisible())
                || hierarchyNodeHidden(static_cast<const ux3d::gltf::Node*>(row.parentObject));
        }
        case HierarchyRowType::Camera:
        case HierarchyRowType::Light:
            return hierarchyNodeHidden(static_cast<const ux3d::gltf::Node*>(row.parentObject));
        default:
            return false;
    }
}

static void renderRow(const HierarchyRow& row,
                      size_t rowIndex,
                      ux3d::gltf::Gltf& gltfModel,
                      ux3d::gltf::Scene* activeScene,
                      SelectionState& selection,
                      ux3d::renderer::GltfRenderLayer* gltfLayer,
                      ux3d::editor::CameraActor& cameraActor,
                      AntiAliasing& antiAliasing,
                      SceneRevisions& revisions,
                      ViewportInstance** activeViewport,
                      const std::function<void(ViewportInstance*)>& viewportMenu,
                      bool isSelected)
{
    if (row.type == HierarchyRowType::ViewportsHeader) {
        ImGui::Spacing();
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (row.type == HierarchyRowType::Scene) {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
        if (!row.isLeaf) {
            flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        }
    } else if (row.type == HierarchyRowType::ViewportsHeader) {
        flags |= ImGuiTreeNodeFlags_DefaultOpen;
    } else {
        if (!row.isLeaf) {
            flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_AllowOverlap;
        }
    }
    if (row.isLeaf) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }
    if (isSelected) {
        flags |= ImGuiTreeNodeFlags_Selected;
        if (s_pendingScroll.active) {
            if (s_pendingScroll.object == nullptr || row.object == s_pendingScroll.object) {
                ImGui::SetScrollHereY(0.5f);
                s_pendingScroll.active = false;
                s_pendingScroll.object = nullptr;
            }
        }
    }

    // Selected rows are highlighted light green; hidden rows (own flag or hidden parent)
    // are grey, matching what the renderer actually draws.
    const bool rowHidden = hierarchyRowHidden(row);
    const bool pushRowColor = isSelected || rowHidden;
    if (pushRowColor) {
        const ImVec4 color = isSelected
            ? ImVec4(0.62f, 0.95f, 0.55f, 1.0f)
            : ImVec4(0.52f, 0.55f, 0.60f, 0.8f);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
    }

    const std::string& labelStr = row.label ? *row.label : row.dynamicLabel;
    const bool open = ImGui::TreeNodeBehavior(row.imguiId, flags, labelStr.c_str());
    if (pushRowColor) {
        ImGui::PopStyleColor();
    }

    // Record probe with open/disclosure state and selection highlighting state
    switch (row.type) {
        case HierarchyRowType::Scene:
            ux3d::imgui::UiProbe::item("hier.scene", row.object ? static_cast<long long>(static_cast<ux3d::gltf::Scene*>(row.object)->m_ID) : 0, open, isSelected);
            break;
        case HierarchyRowType::Node:
            ux3d::imgui::UiProbe::item("hier.node", static_cast<long long>(static_cast<ux3d::gltf::Node*>(row.object)->m_ID), open, isSelected);
            break;
        case HierarchyRowType::Mesh:
            ux3d::imgui::UiProbe::item("hier.mesh", static_cast<long long>(static_cast<ux3d::gltf::Mesh*>(row.object)->m_ID), open, isSelected);
            break;
        case HierarchyRowType::Primitive:
            ux3d::imgui::UiProbe::item("hier.primitive", static_cast<long long>(row.subIndex), open, isSelected);
            break;
        case HierarchyRowType::Material:
            ux3d::imgui::UiProbe::item("hier.material", static_cast<long long>(static_cast<ux3d::gltf::Material*>(row.object)->m_ID), open, isSelected);
            break;
        case HierarchyRowType::Camera:
            ux3d::imgui::UiProbe::item("hier.camera", static_cast<long long>(static_cast<ux3d::gltf::Camera*>(row.object)->m_ID), open, isSelected);
            break;
        case HierarchyRowType::Light:
            ux3d::imgui::UiProbe::item("hier.light", static_cast<long long>(static_cast<ux3d::gltf::Light*>(row.object)->m_ID), open, isSelected);
            break;
        case HierarchyRowType::ViewportsHeader:
            ux3d::imgui::UiProbe::item("hier.viewports_header", 0, open, isSelected);
            break;
        case HierarchyRowType::ViewportItem:
            ux3d::imgui::UiProbe::item("hier.viewport", static_cast<long long>(static_cast<ViewportInstance*>(row.object)->id), open, isSelected);
            break;
    }

    if (!row.isLeaf && open != row.isOpen) {
        s_hierarchyCache.rowModelValid = false;
    }

    switch (row.type) {
        case HierarchyRowType::Scene: {
            auto* scene = static_cast<ux3d::gltf::Scene*>(row.object);
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                selection.selectScene(scene);
            }
            break;
        }
        case HierarchyRowType::Node: {
            auto* node = static_cast<ux3d::gltf::Node*>(row.object);
            if (ImGui::IsItemClicked()) {
                selection.selectNode(node);
                revisions.mark(SceneChange::Selection);
                focusCameraOnNode(node, gltfModel, gltfLayer, cameraActor, antiAliasing, revisions);
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                selection.selectNode(node);
                revisions.mark(SceneChange::Selection);
                focusCameraOnNode(node, gltfModel, gltfLayer, cameraActor, antiAliasing, revisions);
            }
            if (ImGui::BeginPopupContextItem(nullptr)) {
                // Right-click does not move the selection; the menu targets this row and
                // offers Select first when the row was not selected.
                const bool wasSelected = (selection.type == SelectionType::Node && selection.selectedNode == node);
                ObjectActionContext context{
                    gltfModel, activeScene, selection, gltfLayer, &cameraActor, &antiAliasing, revisions
                };
                drawObjectMenuItems(context, node, !wasSelected, true);
                ImGui::EndPopup();
            }
            renderNodeVisibilityToggle(node, revisions);
            break;
        }
        case HierarchyRowType::Mesh: {
            auto* mesh = static_cast<ux3d::gltf::Mesh*>(row.object);
            if (ImGui::IsItemClicked()) {
                selection.selectMesh(mesh);
                revisions.mark(SceneChange::Selection);
            }
            renderMeshVisibilityToggle(mesh, revisions);
            break;
        }
        case HierarchyRowType::Material: {
            auto* mat = static_cast<ux3d::gltf::Material*>(row.object);
            if (ImGui::IsItemClicked()) {
                selection.selectMaterial(mat);
                revisions.mark(SceneChange::Selection);
            }
            break;
        }
        case HierarchyRowType::Camera: {
            auto* cam = static_cast<ux3d::gltf::Camera*>(row.object);
            if (ImGui::IsItemClicked()) {
                selection.selectCamera(cam);
                revisions.mark(SceneChange::Selection);
            }
            break;
        }
        case HierarchyRowType::Light: {
            auto* light = static_cast<ux3d::gltf::Light*>(row.object);
            if (ImGui::IsItemClicked()) {
                selection.selectLight(light);
                revisions.mark(SceneChange::Selection);
            }
            break;
        }
        case HierarchyRowType::ViewportItem: {
            auto* vp = static_cast<ViewportInstance*>(row.object);
            if (ImGui::IsItemClicked()) {
                selection.selectViewport(vp->id);
                if (activeViewport != nullptr) {
                    *activeViewport = vp;
                }
                vp->requestFocus = true;
            }
            if (ImGui::BeginPopupContextItem(nullptr)) {
                if (viewportMenu) {
                    viewportMenu(vp);
                }
                ImGui::EndPopup();
            }
            renderViewportVisibilityToggle(vp);
            break;
        }
        default:
            break;
    }

    if (row.type == HierarchyRowType::Scene && row.isOpen) {
        if (s_searchBuffer[0] != '\0' && s_hierarchyCache.nodeRowCount == 0) {
            ImGui::Spacing();
            ImGui::TextDisabled("  No nodes matching \"%s\"", s_searchBuffer);
        }
    }
}

void SceneHierarchyPanel::render(ux3d::gltf::Gltf& gltfModel,
                                 ux3d::gltf::Scene* activeScene,
                                 SelectionState& selection,
                                 ux3d::renderer::GltfRenderLayer* gltfLayer,
                                 ux3d::editor::CameraActor& cameraActor,
                                 AntiAliasing& antiAliasing,
                                 SceneRevisions& revisions,
                                 bool* pOpen,
                                 std::vector<std::unique_ptr<ViewportInstance>>* viewports,
                                 ViewportInstance** activeViewport,
                                 std::function<void(ViewportInstance*)> viewportMenu)
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
    ux3d::imgui::UiProbe::item("hier.search", 0);

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

    // Isolation indicator and exit action, above the list (always visible while active).
    if (gltfLayer != nullptr && gltfLayer->getIsolationRoot() != nullptr) {
        const auto* isolatedRoot = gltfLayer->getIsolationRoot();
        const std::string isolatedName = isolatedRoot->hasName()
            ? isolatedRoot->getName()
            : ("Node " + std::to_string(isolatedRoot->m_ID));
        if (ImGui::Button(ICON_MDI_CLOSE " Exit Isolate")) {
            ObjectActionContext context{
                gltfModel, activeScene, selection, gltfLayer, &cameraActor, &antiAliasing, revisions
            };
            exitIsolation(context);
        }
        ux3d::imgui::UiProbe::item("hier.exit_isolate", 0);
        ImGui::SameLine();
        ImGui::TextDisabled("Isolated: %s%s", isolatedName.c_str(),
                            gltfLayer->getIsolationIncludeChildren() ? " (with children)" : "");
        ImGui::Separator();
    }

    const std::string lowerFilter = toLowerString(s_searchBuffer);
    if (activeScene != nullptr) {
        ensureHierarchyCache(gltfModel, activeScene, opts, lowerFilter);
    }

    // -----------------------------------------------------------------
    // Hierarchy Tree View (Child window so top search bar never scrolls)
    // -----------------------------------------------------------------
    ImGui::BeginChild("##HierarchyTreeScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    ImGuiWindow* childWindow = ImGui::GetCurrentWindow();
    ImGuiStorage* storage = (childWindow && childWindow->DC.StateStorage) ? childWindow->DC.StateStorage : &s_hierarchyCache.standaloneStorage;
    const ImGuiID baseSeed = childWindow ? childWindow->ID : 0x12345678;

    // Dependency tracking for virtualized row model
    size_t vpCount = viewports ? viewports->size() : 0;
    const ViewportInstance* curActiveVp = activeViewport ? *activeViewport : nullptr;
    uint64_t vpHash = 0;
    if (viewports) {
        for (const auto& vp : *viewports) {
            vpHash = vpHash * 31 + (vp->isOpen ? 1 : 0) + vp->id * 7;
        }
    }

    const bool viewportsStale = (vpCount != s_hierarchyCache.lastViewportCount)
                             || (curActiveVp != s_hierarchyCache.lastActiveViewport)
                             || (vpHash != s_hierarchyCache.lastViewportStateHash);

    const auto* isolationRoot = gltfLayer != nullptr ? gltfLayer->getIsolationRoot() : nullptr;
    const bool isolationIncludeChildren = gltfLayer != nullptr && gltfLayer->getIsolationIncludeChildren();
    const bool isolationStale = (isolationRoot != s_hierarchyCache.isolationRoot)
                             || (isolationIncludeChildren != s_hierarchyCache.isolationIncludeChildren);

    const void* curSelectedObj = nullptr;
    if (selection.type == SelectionType::Node) curSelectedObj = selection.selectedNode;
    else if (selection.type == SelectionType::Mesh) curSelectedObj = selection.selectedMesh;
    else if (selection.type == SelectionType::Material) curSelectedObj = selection.selectedMaterial;
    else if (selection.type == SelectionType::Camera) curSelectedObj = selection.selectedCamera;
    else if (selection.type == SelectionType::Light) curSelectedObj = selection.selectedLight;
    else if (selection.type == SelectionType::Scene) curSelectedObj = selection.selectedScene;
    else if (selection.type == SelectionType::Viewport) curSelectedObj = reinterpret_cast<void*>(static_cast<intptr_t>(selection.selectedViewportId));

    const bool selectionStale = (selection.type != s_hierarchyCache.lastSelectionType)
                             || (curSelectedObj != s_hierarchyCache.lastSelectedObject)
                             || (selection.revision != s_hierarchyCache.lastSelectionRevision);

    if (s_pendingScroll.active) {
        if (s_pendingScroll.object != nullptr && curSelectedObj != s_pendingScroll.object) {
            // Selection changed to a different object while scroll was pending: cancel stale request
            s_pendingScroll = PendingScroll{};
        } else if (s_pendingScroll.object == nullptr && curSelectedObj != nullptr) {
            s_pendingScroll.type = selection.type;
            s_pendingScroll.object = const_cast<void*>(curSelectedObj);
        }
    }

    if (!s_hierarchyCache.rowModelValid || viewportsStale || selectionStale || isolationStale) {
        s_hierarchyCache.lastViewportCount = vpCount;
        s_hierarchyCache.lastActiveViewport = curActiveVp;
        s_hierarchyCache.lastViewportStateHash = vpHash;
        s_hierarchyCache.lastSelectionType = selection.type;
        s_hierarchyCache.lastSelectedObject = curSelectedObj;
        s_hierarchyCache.lastSelectionRevision = selection.revision;
        s_hierarchyCache.isolationRoot = isolationRoot;
        s_hierarchyCache.isolationIncludeChildren = isolationIncludeChildren;

        buildRowModel(gltfModel, activeScene, selection, lowerFilter, viewports, curActiveVp, *storage, baseSeed, isolationRoot, isolationIncludeChildren, selectionStale);
    }

    if (activeScene == nullptr) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "No active glTF scene.");
    }

    if (!s_hierarchyCache.rows.empty()) {
        auto isRowSelected = [&](const HierarchyRow& r) -> bool {
            if (curSelectedObj == nullptr) return false;
            if (r.type == HierarchyRowType::ViewportItem) {
                return selection.type == SelectionType::Viewport && selection.selectedViewportId == static_cast<ViewportInstance*>(r.object)->id;
            }
            return r.object == curSelectedObj;
        };

        const bool useUnclippedFallback = s_hierarchyCache.hasMultiline
            || (!lowerFilter.empty() && s_hierarchyCache.nodeRowCount == 0);

        const float indentSpacing = ImGui::GetStyle().IndentSpacing;
        if (useUnclippedFallback) {
            // Unclipped fallback path for multiline content or no-match search results
            int currentDepth = 0;
            for (size_t i = 0; i < s_hierarchyCache.rows.size(); ++i) {
                const auto& row = s_hierarchyCache.rows[i];
                if (row.depth != currentDepth) {
                    ImGui::Indent((row.depth - currentDepth) * indentSpacing);
                    currentDepth = row.depth;
                }
                renderRow(row, i, gltfModel, activeScene, selection, gltfLayer, cameraActor, antiAliasing, revisions, activeViewport, viewportMenu, isRowSelected(row));
            }
            if (currentDepth > 0) {
                ImGui::Unindent(currentDepth * indentSpacing);
            }
        } else {
            // Virtualized clipped path for fixed-height single-line rows
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(s_hierarchyCache.rows.size()));

            ImGuiContext& g = *GImGui;
            if (g.NavId != 0) {
                auto it = s_hierarchyCache.rowIdToIndex.find(g.NavId);
                if (it != s_hierarchyCache.rowIdToIndex.end()) {
                    int navIdx = it->second;
                    clipper.IncludeItemsByIndex(std::max(0, navIdx - 1),
                                                std::min(static_cast<int>(s_hierarchyCache.rows.size()), navIdx + 2));
                }
            }

            if (s_pendingScroll.active && s_hierarchyCache.selectedRowIndex >= 0) {
                clipper.IncludeItemByIndex(s_hierarchyCache.selectedRowIndex);
            }

            bool viewportsHeaderRendered = false;
            while (clipper.Step()) {
                int currentDepth = 0;
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                    const auto& row = s_hierarchyCache.rows[static_cast<size_t>(i)];
                    if (row.type == HierarchyRowType::ViewportsHeader) {
                        viewportsHeaderRendered = true;
                    }
                    if (row.depth != currentDepth) {
                        ImGui::Indent((row.depth - currentDepth) * indentSpacing);
                        currentDepth = row.depth;
                    }
                    renderRow(row, static_cast<size_t>(i), gltfModel, activeScene, selection, gltfLayer, cameraActor, antiAliasing, revisions, activeViewport, viewportMenu, isRowSelected(row));
                }
                if (currentDepth > 0) {
                    ImGui::Unindent(currentDepth * indentSpacing);
                }
            }

            if (!viewportsHeaderRendered && (viewports != nullptr && !viewports->empty())) {
                ImGui::Dummy(ImVec2(0.0f, 0.0f));
            }
        }
    }

    ImGui::EndChild();
    ImGui::End();
}

} // namespace ux3d::imgui
