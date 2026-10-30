// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// The viewport toolbar strip: projection, grid, bounding box and turntable toggles, material variants, the
// camera attachment and shading-mode pulldowns.

#include "Application.h"
#include "imgui/OceanSimulation.h"
#include "imgui/RenderQuality.h"
#include "imgui/SearchPathsPanel.h"
#include "imgui/Theme.h"
#include "imgui/ObjectActions.h"
#include "imgui/SceneHierarchyPanel.h"
#include "renderer/editor/EditorRenderLayer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace ux3d::imgui::application {

void Application::drawViewportToolbar(ux3d::imgui::ViewportInstance* vp)
{
    // Quick Navigation & Inspection Toolbar Strip. It keeps the default frame
    // padding, so it is exactly as tall as a collapsing header.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 2.0f));
    ImGui::BeginGroup();

    if (!ux3d::imgui::RenderQuality::legacyCaptureUi()) {
        if (ImGui::SmallButton(ICON_MDI_COG "##ViewportSettingsGear")) {
            activeViewport = vp;
            selection.selectViewport(vp->id);
            showInspector = true;
        }
        ux3d::imgui::UiProbe::item("vp.toolbar.settings", vp->id);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Viewport settings in the Inspector");
        ImGui::SameLine();
    }
    // 1. Perspective / Orthographic Toggle (E)
    bool isOrtho = (vp->rendererCamera.getType() == ux3d::renderer::Camera::Type::Orthographic);
    const char* projIcon = isOrtho ? ICON_MDI_PERSPECTIVE_LESS : ICON_MDI_PERSPECTIVE_MORE;
    if (ImGui::SmallButton(projIcon)) {
        if (!vp->isAttachedToCamera) {
            vp->cameraActor.changeUserCameraType(isOrtho ? ux3d::renderer::Camera::Type::Perspective
                                                         : ux3d::renderer::Camera::Type::Orthographic);
            vp->antiAliasing.resetHistory();
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(isOrtho ? "Orthographic (Click to switch to Perspective)"
                                  : "Perspective (Click to switch to Orthographic)");
    }

    // 2. Toggle Floor Grid (D)
    ImGui::SameLine();
    bool gridActive = vp->settings.showGrid;
    if (gridActive) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.67f, 0.42f, 0.4f));
    }
    if (ImGui::SmallButton(ICON_MDI_GRID "##GridToggle")) {
        vp->settings.showGrid = !vp->settings.showGrid;
    }
    if (gridActive) {
        ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(vp->settings.showGrid ? "Floor Grid (Active - click to hide)"
                                                : "Floor Grid (Hidden - click to show)");
    }

    // 3. Toggle Bounding Box (E)
    ImGui::SameLine();
    bool bboxActive = vp->settings.showSceneBoundingBox || vp->settings.showSelectedBoundingBox;
    if (bboxActive) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.67f, 0.42f, 0.4f));
    }
    if (ImGui::SmallButton(ICON_MDI_CUBE_OUTLINE "##BBoxToggle")) {
        bool next = !bboxActive;
        vp->settings.showSceneBoundingBox = next;
        vp->settings.showSelectedBoundingBox = next;
    }
    if (bboxActive) {
        ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(bboxActive ? "Bounding Boxes (Active - click to hide)"
                                     : "Bounding Boxes (Hidden - click to show)");
    }

    // 4. Toggle Orbit (Turntable)
    ImGui::SameLine();
    bool orbitActive = vp->settings.turntable;
    if (orbitActive) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.67f, 0.42f, 0.4f));
    }
    if (ImGui::SmallButton(ICON_MDI_ROTATE_ORBIT "##OrbitToggle")) {
        vp->settings.turntable = !vp->settings.turntable;
    }
    if (orbitActive) {
        ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(vp->settings.turntable ? "Turntable Orbit (Active - click to stop)"
                                                 : "Turntable Orbit (Stopped - click to start)");
    }

    // Material Variants (if model has variants)
    if (gltfModel.hasMaterialVariants()) {
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        auto& variants = gltfModel.getMaterialVariants();
        std::string curVarName = "Default";
        if (gltfLayer && gltfLayer->getMaterialVariant().has_value()) {
            uint32_t activeVarId = gltfLayer->getMaterialVariant().value();
            for (const auto& v : variants) {
                if (v.m_ID == activeVarId) {
                    curVarName = v.hasName() ? v.getName().c_str() : ("Var " + std::to_string(v.m_ID));
                    break;
                }
            }
        }
        std::string varComboId = "##ToolbarVariant_" + std::to_string(vp->id);
        if (ImGui::BeginCombo(varComboId.c_str(), curVarName.c_str())) {
            if (ImGui::Selectable("Default", !gltfLayer || !gltfLayer->getMaterialVariant().has_value())) {
                if (gltfLayer) {
                    gltfLayer->setMaterialVariant(std::nullopt);
                    sceneRevisions.mark(ux3d::imgui::SceneChange::Material);
                }
            }
            for (const auto& v : variants) {
                bool isSel = (gltfLayer && gltfLayer->getMaterialVariant().has_value() &&
                              gltfLayer->getMaterialVariant().value() == v.m_ID);
                std::string vLabel = v.hasName() ? v.getName().c_str() : ("Var " + std::to_string(v.m_ID));
                if (ImGui::Selectable(vLabel.c_str(), isSel)) {
                    if (gltfLayer) {
                        gltfLayer->setMaterialVariant(v.m_ID);
                        sceneRevisions.mark(ux3d::imgui::SceneChange::Material);
                    }
                }
            }
            ImGui::EndCombo();
        }
    }

    // Where the left-hand items end (window-local x). After an item the cursor already sits on the
    // next line, so GetCursorPosX() cannot tell how much of the row is taken.
    const float leftEndX = ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x;

    // Right-corner pulldowns: [ Free Look | <camera-list> ] and [ Shading Mode ]
    std::string camName;
    if (vp->isAttachedToCamera) camName = vp->attachedCameraNodeName.empty() ? "Camera" : vp->attachedCameraNodeName;
    const std::string camIcon = vp->isAttachedToCamera ? ICON_MDI_CAMERA : ICON_MDI_EYE_OUTLINE;
    const std::string camFullLabel = camIcon + " " + (vp->isAttachedToCamera ? camName : std::string("Free Look")) +
                                     " " ICON_MDI_MENU_DOWN;
    const std::string camCompactLabel = camIcon + " " ICON_MDI_MENU_DOWN;

    // The toolbar uses a short name for the default mode; the Inspector combo keeps the full one.
    const char* curModeLabel = getRenderModeLabel(vp->settings.renderMode);
    if (vp->settings.renderMode == ux3d::renderer::RenderMode::None)
        curModeLabel = vp->settings.postEffects ? "Shaded + Post" : "Shaded";
    std::string shadingPulldownLabel = std::string(curModeLabel) + " " ICON_MDI_MENU_DOWN;
    const ImGuiStyle& style = ImGui::GetStyle();
    auto buttonWidth = [&](const std::string& label) {
        return ImGui::CalcTextSize(label.c_str(), nullptr, true).x + style.FramePadding.x * 2.0f;
    };
    const float shadingPulldownWidth = buttonWidth(shadingPulldownLabel);
    const float rightEdge = ImGui::GetWindowContentRegionMax().x - 8.0f;
    const float minGap = style.ItemSpacing.x + 6.0f;

    // Right-aligned when there is room; in a narrow viewport the camera button drops its text, and
    // if that is still too wide the pulldowns follow the left items (clipped at the edge, never on top).
    std::string camPulldownLabel = camFullLabel;
    float rightX = rightEdge - (buttonWidth(camPulldownLabel) + style.ItemSpacing.x + shadingPulldownWidth);
    if (rightX < leftEndX + minGap) {
        camPulldownLabel = camCompactLabel;
        rightX = rightEdge - (buttonWidth(camPulldownLabel) + style.ItemSpacing.x + shadingPulldownWidth);
    }
    if (rightX >= leftEndX + minGap) {
        ImGui::SameLine(rightX);
    } else {
        ImGui::SameLine(leftEndX + minGap);
    }

    // 1. Camera attachment pulldown [ Free Look | <camera-list> ]
    std::string camPopupId = "##CameraMenuPopup_" + std::to_string(vp->id);
    if (vp->isAttachedToCamera) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.78f, 1.0f, 1.0f));
    }
    if (ImGui::SmallButton(camPulldownLabel.c_str())) {
        ImGui::OpenPopup(camPopupId.c_str());
    }
    if (vp->isAttachedToCamera) {
        ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(vp->isAttachedToCamera ? "Camera Attached (Click to switch camera or Free Look)"
                                                 : "Free Look (Click to attach to a glTF camera)");
    }

    if (ImGui::BeginPopup(camPopupId.c_str())) {
        bool isFreeLook = !vp->isAttachedToCamera;
        if (ImGui::MenuItem(ICON_MDI_EYE_OUTLINE " Free Look", nullptr, isFreeLook)) {
            vp->isAttachedToCamera = false;
            vp->attachedCameraNode = nullptr;
            vp->attachedCameraNodeId = 0;
            vp->attachedCameraNodeName = "";
            sceneRevisions.mark(ux3d::imgui::SceneChange::ViewCamera);
        }
        ImGui::Separator();

        auto cameraNodes = collectSceneCameraNodes(activeScene);
        if (cameraNodes.empty()) {
            ImGui::TextDisabled("  (No Cameras in Scene)");
        } else {
            for (const auto* camNode : cameraNodes) {
                std::string cName =
                    camNode->hasName() ? camNode->getName() : ("Camera Node " + std::to_string(camNode->m_ID));
                std::string camItemLabel = ICON_MDI_CAMERA " " + cName;
                bool isSelected = vp->isAttachedToCamera &&
                                  (vp->attachedCameraNodeId == camNode->m_ID || vp->attachedCameraNode == camNode);
                if (ImGui::MenuItem(camItemLabel.c_str(), nullptr, isSelected)) {
                    vp->isAttachedToCamera = true;
                    vp->attachedCameraNode = camNode;
                    vp->attachedCameraNodeId = camNode->m_ID;
                    vp->attachedCameraNodeName = cName;
                    sceneRevisions.mark(ux3d::imgui::SceneChange::ViewCamera);
                }
            }
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();

    // 2. Shading Mode pulldown
    std::string popupId = "##ShadingModePopup_" + std::to_string(vp->id);
    if (ImGui::SmallButton(shadingPulldownLabel.c_str())) {
        ImGui::OpenPopup(popupId.c_str());
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Shading & Debug Render Modes");
    }

    if (ImGui::BeginPopup(popupId.c_str())) {
        drawShadingMenu(*vp);
        ImGui::EndPopup();
    }

    ImGui::EndGroup();
    ImGui::PopStyleVar();
}

} // namespace ux3d::imgui::application
