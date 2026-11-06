#include "imgui/GladGL.h"
#include "imgui/ViewportSettingsPanel.h"
#include "imgui/OceanSimulation.h"
#include "imgui/RenderQuality.h"
#include "imgui/SearchPathsPanel.h"
#include "imgui/StdFileSystem.h"
#include "imgui/UiProbe.h"
#include "imgui/PropertyTree.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "imgui/AntiAliasing.h"
#include "imgui/ExtensionInspector.h"
#include "gltf/extensions/AsoboExtensions.h"
#include "gltf/extensions/AsoboSupport.h"
#include "renderViewInput/CameraActor.h"
#include <imgui.h>
#include <portable-file-dialogs.h>
#include <algorithm>
#include <cfloat>
#include <string>
#include <cmath>

namespace ux3d::imgui {

namespace {
// Runs an ImGui widget and registers its rectangle with the UI scenario harness (--ui-script: hoverp / clickp <tag>), so
// the Ground controls can be hovered one by one to check for ImGui ID conflicts.
template <typename Widget>
bool probed(const char* tag, Widget&& widget)
{
    const bool result = widget();
    UiProbe::item(tag, 0);
    return result;
}
// In the Inspector (a PropertyTree table is open) a property row with a hidden widget label; elsewhere (the texture
// legend's popup) the widget keeps its visible label.
std::string field(const char* icon, const char* label, const char* id, const char* tooltip = nullptr)
{
    if (!prop::active()) {
        return label;
    }
    prop::row(icon, label, tooltip);
    return std::string("##") + id;
}
} // namespace

void ViewportSettingsPanel::renderTextureMetricsSettings(ViewportSettingsState& state, SceneRevisions& revisions,
                                                         AntiAliasing* antiAliasing, const char* probePrefix)
{
    const auto probe = [probePrefix](const char* name) {
        UiProbe::item((std::string(probePrefix) + "." + name).c_str(), 0);
    };
    if (state.renderMode != ux3d::renderer::RenderMode::TextureDistortion &&
        state.renderMode != ux3d::renderer::RenderMode::TextureDensity) {
        return;
    }
    if (state.renderMode == ux3d::renderer::RenderMode::TextureDistortion) {
        const char* views[] = { "Both", "Shear", "Area" };
        if (ImGui::Combo(field(ICON_MDI_VECTOR_DIFFERENCE, "Distortion", "textureDistortionView",
                               "Which distortion the heatmap shows: shear, area or both.").c_str(),
                         &state.textureDistortionView, views, IM_ARRAYSIZE(views))) {
            revisions.mark(SceneChange::ViewportSettings);
        }
        probe("texture_distortion_view");
    }
    // Frame values read by the shader: redraw without a renderer sync, even while a slider is dragged.
    const auto redraw = [&]() {
        if (antiAliasing) antiAliasing->resetHistory();
        revisions.mark(SceneChange::Environment);
    };
    if (ImGui::SliderFloat(field(ICON_MDI_OPACITY, "Heatmap Opacity", "textureHeatmapOpacity").c_str(),
                           &state.textureHeatmapOpacity, 0.3f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
        redraw();
    }
    probe("texture_heatmap_opacity");
    if (ImGui::Checkbox(field(ICON_MDI_GRID, "UV Grid", "textureGrid").c_str(), &state.textureGrid)) {
        redraw();
    }
    probe("texture_grid");
    renderUvGridOptions(state, revisions, antiAliasing, probePrefix, state.textureGrid);
}

void ViewportSettingsPanel::renderUvGridOptions(ViewportSettingsState& state, SceneRevisions& revisions,
                                                AntiAliasing* antiAliasing, const char* probePrefix, bool enabled)
{
    const auto probe = [probePrefix](const char* name) {
        UiProbe::item((std::string(probePrefix) + "." + name).c_str(), 0);
    };
    const auto redraw = [&]() {
        if (antiAliasing) antiAliasing->resetHistory();
        revisions.mark(SceneChange::Environment);
    };
    ImGui::BeginDisabled(!enabled);
    const char* cells[] = { "8 x 8", "16 x 16", "32 x 32", "64 x 64" };
    if (ImGui::Combo(field(ICON_MDI_GRID_LARGE, "Grid Cells", "textureGridCells").c_str(),
                     &state.textureGridCells, cells, IM_ARRAYSIZE(cells))) {
        redraw();
    }
    probe("texture_grid_cells");
    if (ImGui::Checkbox(field(ICON_MDI_LABEL_OUTLINE, "Cell Labels", "textureGridLabels").c_str(), &state.textureGridLabels)) {
        redraw();
    }
    probe("texture_grid_labels");
    if (ImGui::SliderFloat(field(ICON_MDI_OPACITY, "Grid Opacity", "textureGridOpacity").c_str(),
                           &state.textureGridOpacity, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
        redraw();
    }
    probe("texture_grid_opacity");
    ImGui::EndDisabled();
}

void ViewportSettingsPanel::render(ViewportSettingsState& state,
                                   ux3d::renderer::CommandGenerator* commandGenerator,
                                   ux3d::renderer::GltfRenderLayer* gltfLayer,
                                   ux3d::editor::CameraActor& cameraActor,
                                   SceneRevisions& revisions,
                                   ux3d::gltf::Gltf* gltfModel,
                                   bool isAttachedToCamera,
                                   const std::string& attachedCameraName,
                                   std::function<void()> onDetachCamera,
                                   AntiAliasing* antiAliasing)
{
    (void)commandGenerator;

    // The rows go into the Inspector's key/value tree, which the caller opens (prop::begin).
    if (isAttachedToCamera) {
        prop::colorTextRow(ICON_MDI_CAMERA, "Attached Camera", "Camera transform and projection follow the glTF scene.",
                           ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", attachedCameraName.empty() ? "(Unnamed)" : attachedCameraName.c_str());
        if (onDetachCamera) {
            prop::row(ICON_MDI_CAMERA_OFF, "Detach", "Returns the viewport to its own camera.");
            if (ImGui::Button(ICON_MDI_CAMERA_OFF " Detach from Camera", ImVec2(-FLT_MIN, 0.0f))) {
                onDetachCamera();
            }
        }
    }

    // -------------------------------------------------------------
    // Render Modes (open by default, as the first tab was)
    // -------------------------------------------------------------
    const bool renderOpen = prop::group(ICON_MDI_EYE_SETTINGS_OUTLINE, "Render Modes", true);
    UiProbe::item("vp.tab.render", 0);
    if (renderOpen) {
        const char* renderModes[] = {
            "Shaded + post effects (Default)",
            "Wireframe",
            "Normal Texture",
            "Shading Normal",
            "Geometry Normal",
            "Tangent",
            "Bitangent",
            "Base Color",
            "Metallic",
            "Roughness",
            "Metallic-Roughness",
            "Occlusion",
            "Emissive",
            "Alpha",
            "TexCoord 0",
            "TexCoord 1",
            "Clearcoat",
            "Sheen",
            "Specular",
            "Transmission / Volume",
            "Iridescence",
            "Clearcoat Factor", "Clearcoat Roughness", "Clearcoat Normal",
            "Sheen Color", "Sheen Roughness", "Specular Factor", "Specular Color",
            "Transmission Factor", "Volume Thickness", "Iridescence Factor", "Iridescence Thickness",
            "SSR", "SSR Ray", "Shadow Cascades", "Shadow Visibility", "SSAO", "Sky Visibility", "Shaded", "Texture Distortion", "Texture Density"
        };

        const char* renderModeTip = "Allows inspecting individual PBR channels, geometry, or UVs.\n"
                                    "Quality levels apply to all views. Shaded omits post AA, bloom, lens effects, SSR and SSAO.";
        prop::row(ICON_MDI_EYE_SETTINGS_OUTLINE, "Render Mode", renderModeTip);
        if (ImGui::Combo("##renderMode", &state.renderModeIdx, renderModes, IM_ARRAYSIZE(renderModes))) {
            state.postEffects = state.renderModeIdx != 38;
            switch (state.renderModeIdx) {
            case 1: state.renderMode = ux3d::renderer::RenderMode::Wireframe; break;
            case 2: state.renderMode = ux3d::renderer::RenderMode::NormalTexture; break;
            case 3: state.renderMode = ux3d::renderer::RenderMode::ShadingNormal; break;
            case 4: state.renderMode = ux3d::renderer::RenderMode::NormalGeometry; break;
            case 5: state.renderMode = ux3d::renderer::RenderMode::Tangent; break;
            case 6: state.renderMode = ux3d::renderer::RenderMode::Bitangent; break;
            case 7: state.renderMode = ux3d::renderer::RenderMode::BaseColor; break;
            case 8: state.renderMode = ux3d::renderer::RenderMode::Metallic; break;
            case 9: state.renderMode = ux3d::renderer::RenderMode::Roughness; break;
            case 10: state.renderMode = ux3d::renderer::RenderMode::MetallicRoughness; break;
            case 11: state.renderMode = ux3d::renderer::RenderMode::Occlusion; break;
            case 12: state.renderMode = ux3d::renderer::RenderMode::Emissive; break;
            case 13: state.renderMode = ux3d::renderer::RenderMode::Alpha; break;
            case 14: state.renderMode = ux3d::renderer::RenderMode::TexCoord_0; break;
            case 15: state.renderMode = ux3d::renderer::RenderMode::TexCoord_1; break;
            case 16: state.renderMode = ux3d::renderer::RenderMode::Clearcoat; break;
            case 17: state.renderMode = ux3d::renderer::RenderMode::Sheen; break;
            case 18: state.renderMode = ux3d::renderer::RenderMode::Specular; break;
            case 19: state.renderMode = ux3d::renderer::RenderMode::TransmissionVolume; break;
            case 20: state.renderMode = ux3d::renderer::RenderMode::Iridescence; break;
            case 21: state.renderMode = ux3d::renderer::RenderMode::ClearcoatFactor; break;
            case 22: state.renderMode = ux3d::renderer::RenderMode::ClearcoatRoughness; break;
            case 23: state.renderMode = ux3d::renderer::RenderMode::ClearcoatNormal; break;
            case 24: state.renderMode = ux3d::renderer::RenderMode::SheenColor; break;
            case 25: state.renderMode = ux3d::renderer::RenderMode::SheenRoughness; break;
            case 26: state.renderMode = ux3d::renderer::RenderMode::SpecularFactor; break;
            case 27: state.renderMode = ux3d::renderer::RenderMode::SpecularColor; break;
            case 28: state.renderMode = ux3d::renderer::RenderMode::TransmissionFactor; break;
            case 29: state.renderMode = ux3d::renderer::RenderMode::VolumeThickness; break;
            case 30: state.renderMode = ux3d::renderer::RenderMode::IridescenceFactor; break;
            case 31: state.renderMode = ux3d::renderer::RenderMode::IridescenceThickness; break;
            case 32: state.renderMode = ux3d::renderer::RenderMode::SSR; break;
            case 33: state.renderMode = ux3d::renderer::RenderMode::SSRRay; break;
            case 34: state.renderMode = ux3d::renderer::RenderMode::ShadowCascades; break;
            case 35: state.renderMode = ux3d::renderer::RenderMode::ShadowVisibility; break;
            case 36: state.renderMode = ux3d::renderer::RenderMode::SSAO; break;
            case 37: state.renderMode = ux3d::renderer::RenderMode::SkyVisibility; break;
            case 39: state.renderMode = ux3d::renderer::RenderMode::TextureDistortion; break;
            case 40: state.renderMode = ux3d::renderer::RenderMode::TextureDensity; break;
            default: state.renderMode = ux3d::renderer::RenderMode::None; break;
            }
            if (antiAliasing) antiAliasing->resetHistory();
            revisions.mark(SceneChange::ViewportSettings);
        }
        UiProbe::item("vp.render_mode", 0);
        prop::tooltip(renderModeTip);
        ImGui::PushID("textureMetrics"); // Its "UV Grid" row sits beside the "UV Grid" group.
        renderTextureMetricsSettings(state, revisions, antiAliasing, "vp");
        ImGui::PopID();

        if (prop::group(ICON_MDI_FLIP_TO_BACK, "Face Culling & Winding", true)) {
            prop::row(ICON_MDI_FLIP_TO_BACK, "Force Double-Sided", "Disables back-face culling.");
            if (ImGui::Checkbox("##forceDoubleSided", &state.forceDoubleSided)) {
                revisions.mark(SceneChange::ViewportSettings);
            }
            UiProbe::item("vp.force_double_sided", 0);
            prop::row(ICON_MDI_SWAP_HORIZONTAL, "Invert Winding", "Flips front / back faces.");
            if (ImGui::Checkbox("##invertWinding", &state.invertWinding)) {
                revisions.mark(SceneChange::ViewportSettings);
            }
            UiProbe::item("vp.invert_winding", 0);
            prop::endGroup();
        }

        if (prop::group(ICON_MDI_PUZZLE_OUTLINE, "glTF Extensions", true)) {
            // The extension switches (ASOBO now, other vendors later) are app settings, not viewport settings: one set
            // for every viewport, saved in settings.ini. They are edited in File > Settings > glTF Extensions.
            const char* switchesTip = "Which extensions are applied is an app setting (File > Settings > glTF Extensions).";
            prop::row(ICON_MDI_COG_OUTLINE, "Extension Switches", switchesTip);
            if (ImGui::Button("glTF Extensions settings...", ImVec2(-FLT_MIN, 0.0f))) {
                requestSettingsWindow(SettingsTab::GltfExtensions);
            }
            UiProbe::item("vp.open_gltf_extensions", 0);
            prop::tooltip(switchesTip);
            // View settings that depend on the ASOBO group stay here (the effects are Quality levels; the ASOBO tuning is
            // in File > Settings > glTF Extensions); the sky-portal gizmos are a view helper.
            if (gltfLayer != nullptr && gltfLayer->hasSkyPortals()) {
                // A3.7: shown only for files with ASOBO_sky_portal nodes (the gizmos are a view helper).
                const char* portalTip = "Gizmo for every portal: disc, inner circle and the direction into the cabin.";
                prop::row(ICON_MDI_WINDOW_CLOSED_VARIANT, "Show Sky Portals", portalTip);
                ImGui::BeginDisabled(!state.asobo.enabled);
                if (ImGui::Checkbox("##showSkyPortals", &state.showSkyPortals)) {
                    revisions.mark(SceneChange::ViewportSettings);
                }
                prop::tooltip(portalTip);
                ImGui::EndDisabled();
            }
            prop::endGroup();
        }

        if (prop::group(ICON_MDI_GRID, "UV Grid", true)) {
            if (state.renderMode == ux3d::renderer::RenderMode::TextureDistortion ||
                state.renderMode == ux3d::renderer::RenderMode::TextureDensity) {
                prop::textRow(ICON_MDI_INFORMATION_OUTLINE, "UV Grid Overlay", "This view has its own UV Grid switch, above.",
                              "In the view's settings above");
            } else {
                const char* overlayTip = "Superimposes the UV grid and cell labels on any view.";
                prop::row(ICON_MDI_GRID, "UV Grid Overlay", overlayTip);
                if (ImGui::Checkbox("##uvGridOverlay", &state.uvGridOverlay)) {
                    if (antiAliasing) antiAliasing->resetHistory();
                    revisions.mark(SceneChange::Environment);
                }
                UiProbe::item("vp.uv_grid_overlay", 0);
                prop::tooltip(overlayTip);
                renderUvGridOptions(state, revisions, antiAliasing, "vp", state.uvGridOverlay);
            }
            prop::endGroup();
        }

        if (prop::group(ICON_MDI_VECTOR_TRIANGLE, "Wireframe", true)) {
            const char* overlayTip = "Superimposes wireframe mesh edges on top of shaded materials.";
            prop::row(ICON_MDI_VECTOR_TRIANGLE, "Wireframe Overlay", overlayTip);
            if (ImGui::Checkbox("##wireframeOverlay", &state.wireframeOverlay)) {
                revisions.mark(SceneChange::ViewportSettings);
            }
            UiProbe::item("vp.wireframe_overlay", 0);
            prop::tooltip(overlayTip);
            prop::row(ICON_MDI_PALETTE, "Line Color");
            if (ImGui::ColorEdit3("##wireframeColor", &state.wireframeColor.x)) {
                revisions.mark(SceneChange::ViewportSettings);
            }
            prop::row(ICON_MDI_OPACITY, "Line Opacity");
            if (ImGui::SliderFloat("##wireframeOpacity", &state.wireframeColor.w, 0.0f, 1.0f, "%.2f")) {
                revisions.mark(SceneChange::ViewportSettings);
            }
            const char* wireframeBlendNames[] = { "Normal", "Additive", "Invert", "XOR" };
            int currentWireBlend = static_cast<int>(state.wireframeBlend);
            prop::row(ICON_MDI_LAYERS_OUTLINE, "Blend Mode");
            if (ImGui::Combo("##wireframeBlend", &currentWireBlend, wireframeBlendNames, IM_ARRAYSIZE(wireframeBlendNames))) {
                state.wireframeBlend = static_cast<WireframeBlendMode>(currentWireBlend);
                revisions.mark(SceneChange::ViewportSettings);
            }
            prop::endGroup();
        }

        // Material Variants (KHR_materials_variants)
        if (gltfModel != nullptr && gltfModel->hasMaterialVariants()) {
            if (prop::group(ICON_MDI_SWAP_HORIZONTAL, "Material Variants", true, "KHR_materials_variants")) {
                auto& variants = gltfModel->getMaterialVariants();
                std::string curVarName = "Default / Original";
                if (gltfLayer && gltfLayer->getMaterialVariant().has_value()) {
                    uint32_t activeVarId = gltfLayer->getMaterialVariant().value();
                    for (const auto& v : variants) {
                        if (v.m_ID == activeVarId) {
                            curVarName = v.hasName() ? v.getName().c_str() : ("Variant " + std::to_string(v.m_ID));
                            break;
                        }
                    }
                }

                prop::row(ICON_MDI_SWAP_HORIZONTAL, "Active Variant");
                if (ImGui::BeginCombo("##activeVariant", curVarName.c_str())) {
                    bool isDef = !gltfLayer || !gltfLayer->getMaterialVariant().has_value();
                    if (ImGui::Selectable("Default / Original", isDef)) {
                        if (gltfLayer) {
                            gltfLayer->setMaterialVariant(std::nullopt);
                            revisions.mark(SceneChange::Material);
                        }
                    }
                    for (const auto& v : variants) {
                        bool isSel = (gltfLayer && gltfLayer->getMaterialVariant().has_value() && gltfLayer->getMaterialVariant().value() == v.m_ID);
                        std::string vLabel = v.hasName() ? v.getName().c_str() : ("Variant " + std::to_string(v.m_ID));
                        ImGui::PushID(static_cast<int>(v.m_ID));
                        if (ImGui::Selectable(vLabel.c_str(), isSel)) {
                            if (gltfLayer) {
                                gltfLayer->setMaterialVariant(v.m_ID);
                                revisions.mark(SceneChange::Material);
                            }
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }
                prop::endGroup();
            }
        }

        const char* qualityTip = "Quality (AA, render scale, IBL, shadow maps) is an app setting (File > Settings > Quality).";
        prop::row(ICON_MDI_TUNE, "Quality", qualityTip);
        if (ImGui::Button("Quality settings...", ImVec2(-FLT_MIN, 0.0f))) {
            requestSettingsWindow(SettingsTab::Quality);
        }
        UiProbe::item("vp.open_quality", 0);
        prop::tooltip(qualityTip);
        prop::endGroup();
    }

    // -------------------------------------------------------------
    // Camera & Helpers (closed by default, as the second tab was not selected)
    // -------------------------------------------------------------
    const bool cameraOpen = prop::group(ICON_MDI_CAMERA, "Camera & Helpers", false);
    UiProbe::item("vp.tab.camera", 0);
    if (cameraOpen) {
        if (prop::group(ICON_MDI_MONITOR, "Viewport Display", true)) {
            prop::row(ICON_MDI_GRID, "Ground Grid", "The editor grid overlay.");
            ImGui::Checkbox("##showGrid", &state.showGrid);
            UiProbe::item("vp.grid", 0);
            prop::row(ICON_MDI_COMPASS_OUTLINE, "View Compass", "The 3D orientation compass in the viewport corner.");
            ImGui::Checkbox("##showCompass", &state.showOrientationGizmo);
            prop::endGroup();
        }

        if (prop::group(ICON_MDI_SELECTION_DRAG, "Selection & Transform Visualizers", true)) {
            prop::row(ICON_MDI_CUBE_OUTLINE, "Selected OBB", "Oriented bounding box of the selection (orange).");
            ImGui::Checkbox("##selectedObb", &state.showSelectedBoundingBox);
            prop::row(ICON_MDI_CUBE_SCAN, "Scene AABB", "Axis-aligned bounding box of the scene (yellow).");
            ImGui::Checkbox("##sceneAabb", &state.showSceneBoundingBox);
            prop::row(ICON_MDI_AXIS_ARROW, "Node Gizmo", "The selected node's coordinate axes.");
            ImGui::Checkbox("##nodeGizmo", &state.showNodeCoordinateGizmo);
            if (state.showNodeCoordinateGizmo) {
                const char* conventions[] = { "Blender Source (Z-Up: X=Red, Y=Green, Z=Blue)", "glTF Native (Y-Up)" };
                prop::row(ICON_MDI_AXIS_ARROW, "Gizmo Axis Convention");
                ImGui::Combo("##gizmoAxisConvention", &state.gizmoAxisConvention, conventions, IM_ARRAYSIZE(conventions));
            }
            prop::endGroup();
        }

        if (prop::group(ICON_MDI_ROTATE_360, "Turntable", true)) {
            prop::row(ICON_MDI_ROTATE_360, "Auto-Rotate", "Turns the camera around the model.");
            ImGui::Checkbox("##turntable", &state.turntable);
            if (state.turntable) {
                prop::row(ICON_MDI_SPEEDOMETER, "Speed", "Degrees per second.");
                ImGui::SliderFloat("##turntableSpeed", &state.turntableSpeed, -180.0f, 180.0f, "%.1f deg/s");
            }
            prop::endGroup();
        }

        auto* userCam = cameraActor.getUserCamera();
        if (prop::group(ICON_MDI_CAMERA, "Camera Settings", true,
                        isAttachedToCamera ? "Follows the attached glTF camera." : nullptr)) {
            ImGui::BeginDisabled(isAttachedToCamera);
            if (userCam != nullptr) {
                bool isOrtho = (userCam->getType() == ux3d::renderer::Camera::Type::Orthographic);
                prop::row(ICON_MDI_PROJECTOR, "Orthographic", "Orthographic instead of perspective projection.");
                if (ImGui::Checkbox("##orthographic", &isOrtho)) {
                    cameraActor.changeUserCameraType(isOrtho ? ux3d::renderer::Camera::Type::Orthographic : ux3d::renderer::Camera::Type::Perspective);
                    revisions.mark(SceneChange::ViewCamera);
                }

                if (!isOrtho) {
                    float fov = glm::degrees(userCam->getYFoV());
                    prop::row(ICON_MDI_ANGLE_ACUTE, "Field of View", "Vertical field of view.");
                    if (ImGui::SliderFloat("##fov", &fov, 15.0f, 120.0f, "%.1f deg")) {
                        userCam->setPerspective(userCam->getAspectRatio(), glm::radians(fov), userCam->getZNearPlane(), userCam->getZFarPlane());
                        revisions.mark(SceneChange::ViewCamera);
                    }
                }

                float planes[2] = { userCam->getZNearPlane(), userCam->getZFarPlane() };
                const char* planesTip = "Frustum clipping planes. Drag to edit the near and far clipping planes.";
                prop::row(ICON_MDI_ARROW_EXPAND_HORIZONTAL, "Near / Far", planesTip);
                if (ImGui::DragFloat2("##nearFar", planes, 0.01f, 0.0001f, 1000000.0f, "%.4f")) {
                    const float nearPlane = (std::max)(0.0001f, planes[0]);
                    const float farPlane = (std::max)(nearPlane + 0.0001f, planes[1]);
                    if (isOrtho) {
                        userCam->setOrthographic(userCam->getXMag(), userCam->getYMag(), nearPlane, farPlane);
                    } else {
                        userCam->setPerspective(userCam->getAspectRatio(), userCam->getYFoV(), nearPlane, farPlane);
                    }
                    if (antiAliasing != nullptr) antiAliasing->resetHistory();
                    revisions.mark(SceneChange::ViewCamera);
                }
                prop::tooltip(planesTip);
            }
            ImGui::EndDisabled();
            prop::endGroup();
        }

        if (prop::group(ICON_MDI_VIDEO_3D, "Camera Presets", true)) {
            prop::row(ICON_MDI_CAMERA_SWITCH, "Views", "Orients the camera (the numpad keys do the same in the viewport).");
            ImGui::BeginDisabled(isAttachedToCamera);
            {
                const float presetWidth = (std::max)(1.0f, (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f);
                auto presetButton = [&](const char* label, ux3d::editor::CameraOrientation orientation) {
                    if (ImGui::Button(label, ImVec2(presetWidth, 0.0f))) {
                        cameraActor.orientUserCamera(orientation);
                        if (antiAliasing != nullptr) antiAliasing->resetHistory();
                        revisions.mark(SceneChange::ViewCamera);
                    }
                };
                presetButton("Front (Num 1)", ux3d::editor::CameraOrientation::Front);
                ImGui::SameLine();
                presetButton("Back (Ctrl+1)", ux3d::editor::CameraOrientation::Back);
                presetButton("Right (Num 3)", ux3d::editor::CameraOrientation::Right);
                ImGui::SameLine();
                presetButton("Left (Ctrl+3)", ux3d::editor::CameraOrientation::Left);
                presetButton("Top (Num 7)", ux3d::editor::CameraOrientation::Top);
                ImGui::SameLine();
                presetButton("Iso", ux3d::editor::CameraOrientation::Isometric);
            }
            ImGui::EndDisabled();
            prop::endGroup();
        }

        if (prop::group(ICON_MDI_ORBIT, "Camera Navigation", true)) {
            ImGui::BeginDisabled(isAttachedToCamera);
            {
                float zoom = cameraActor.getZoom();
                prop::row(ICON_MDI_MAGNIFY, "Zoom", "Distance to the look target.");
                if (ImGui::DragFloat("##zoom", &zoom, 0.01f, 0.0000001f, 100000.0f, "%.2f", ImGuiSliderFlags_Logarithmic)) {
                    cameraActor.setZoom(zoom);
                    cameraActor.updateUserCameraTransform();
                    if (antiAliasing != nullptr) antiAliasing->resetHistory();
                    revisions.mark(SceneChange::ViewCamera);
                }

                float rotation[2] = { glm::degrees(cameraActor.getRotationX()), glm::degrees(cameraActor.getRotationY()) };
                prop::row(ICON_MDI_ROTATE_3D_VARIANT, "Rotation", "Degrees.");
                if (ImGui::DragFloat2("##rotation", rotation, 0.5f)) {
                    cameraActor.setCameraRotation(glm::radians(rotation[0]), glm::radians(rotation[1]));
                    cameraActor.updateUserCameraTransform();
                    if (antiAliasing != nullptr) antiAliasing->resetHistory();
                    revisions.mark(SceneChange::ViewCamera);
                }

                auto target = cameraActor.getLookTarget();
                float targetValues[3] = { target.x, target.y, target.z };
                prop::row(ICON_MDI_CROSSHAIRS_GPS, "Look Target");
                if (ImGui::DragFloat3("##lookTarget", targetValues, 0.01f)) {
                    cameraActor.setTarget(ux3d::math::vec3(targetValues[0], targetValues[1], targetValues[2]));
                    cameraActor.updateUserCameraTransform();
                    if (antiAliasing != nullptr) antiAliasing->resetHistory();
                    revisions.mark(SceneChange::ViewCamera);
                }
            }
            ImGui::EndDisabled();

            prop::row(ICON_MDI_HOME_OUTLINE, "Reset", "Fits the camera to the default scene.");
            if (ImGui::Button("Reset Camera (Home)", ImVec2(-FLT_MIN, 0.0f))) {
                if (gltfModel != nullptr && gltfLayer != nullptr) {
                    auto* defScene = gltfModel->getDefaultScene();
                    if (defScene) {
                        cameraActor.fitCameraToScene(*defScene, gltfModel);
                    }
                }
            }
            prop::endGroup();
        }
        prop::endGroup();
    }
}

} // namespace ux3d::imgui
