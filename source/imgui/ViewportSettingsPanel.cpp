#include "imgui/GladGL.h"
#include "imgui/ViewportSettingsPanel.h"
#include "imgui/OceanSimulation.h"
#include "imgui/RenderQuality.h"
#include "imgui/SearchPathsPanel.h"
#include "imgui/StdFileSystem.h"
#include "imgui/UiProbe.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "imgui/AntiAliasing.h"
#include "imgui/ExtensionInspector.h"
#include "gltf/extensions/AsoboExtensions.h"
#include "gltf/extensions/AsoboSupport.h"
#include "renderViewInput/CameraActor.h"
#include <imgui.h>
#include <portable-file-dialogs.h>
#include <algorithm>
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
} // namespace

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

    if (isAttachedToCamera) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
        ImGui::Text(ICON_MDI_CAMERA " Attached to glTF Camera: %s", attachedCameraName.empty() ? "(Unnamed)" : attachedCameraName.c_str());
        ImGui::PopStyleColor();
        ImGui::TextDisabled("Camera transform and projection follow the glTF scene.");
        if (onDetachCamera) {
            if (ImGui::Button(ICON_MDI_CAMERA_OFF " Detach from Camera")) {
                onDetachCamera();
            }
        }
        ImGui::Separator();
    }

    if (ImGui::BeginTabBar("ViewportSettingsTabs")) {
        // -------------------------------------------------------------
        // Tab 1: Debug Render Modes
        // -------------------------------------------------------------
        const bool renderTabOpen = ImGui::BeginTabItem("Render Modes");
        UiProbe::item("vp.tab.render", 0);
        if (renderTabOpen) {
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
                "SSR", "SSR Ray", "Shadow Cascades", "Shadow Visibility", "SSAO", "Sky Visibility", "Shaded"
            };

            if (ImGui::Combo("Render Mode", &state.renderModeIdx, renderModes, IM_ARRAYSIZE(renderModes))) {
                state.postEffects = state.renderModeIdx != 38;
    switch (state.renderModeIdx) {
    case 1:
        state.renderMode = ux3d::renderer::RenderMode::Wireframe; break;
    case 2:
        state.renderMode = ux3d::renderer::RenderMode::NormalTexture; break;
    case 3:
        state.renderMode = ux3d::renderer::RenderMode::ShadingNormal; break;
    case 4:
        state.renderMode = ux3d::renderer::RenderMode::NormalGeometry; break;
    case 5:
        state.renderMode = ux3d::renderer::RenderMode::Tangent; break;
    case 6:
        state.renderMode = ux3d::renderer::RenderMode::Bitangent; break;
    case 7:
        state.renderMode = ux3d::renderer::RenderMode::BaseColor; break;
    case 8:
        state.renderMode = ux3d::renderer::RenderMode::Metallic; break;
    case 9:
        state.renderMode = ux3d::renderer::RenderMode::Roughness; break;
    case 10:
        state.renderMode = ux3d::renderer::RenderMode::MetallicRoughness; break;
    case 11:
        state.renderMode = ux3d::renderer::RenderMode::Occlusion; break;
    case 12:
        state.renderMode = ux3d::renderer::RenderMode::Emissive; break;
    case 13:
        state.renderMode = ux3d::renderer::RenderMode::Alpha; break;
    case 14:
        state.renderMode = ux3d::renderer::RenderMode::TexCoord_0; break;
    case 15:
        state.renderMode = ux3d::renderer::RenderMode::TexCoord_1; break;
    case 16:
        state.renderMode = ux3d::renderer::RenderMode::Clearcoat; break;
    case 17:
        state.renderMode = ux3d::renderer::RenderMode::Sheen; break;
    case 18:
        state.renderMode = ux3d::renderer::RenderMode::Specular; break;
    case 19:
        state.renderMode = ux3d::renderer::RenderMode::TransmissionVolume; break;
    case 20:
        state.renderMode = ux3d::renderer::RenderMode::Iridescence; break;
    case 21:
        state.renderMode = ux3d::renderer::RenderMode::ClearcoatFactor; break;
    case 22:
        state.renderMode = ux3d::renderer::RenderMode::ClearcoatRoughness; break;
    case 23:
        state.renderMode = ux3d::renderer::RenderMode::ClearcoatNormal; break;
    case 24:
        state.renderMode = ux3d::renderer::RenderMode::SheenColor; break;
    case 25:
        state.renderMode = ux3d::renderer::RenderMode::SheenRoughness; break;
    case 26:
        state.renderMode = ux3d::renderer::RenderMode::SpecularFactor; break;
    case 27:
        state.renderMode = ux3d::renderer::RenderMode::SpecularColor; break;
    case 28:
        state.renderMode = ux3d::renderer::RenderMode::TransmissionFactor; break;
    case 29:
        state.renderMode = ux3d::renderer::RenderMode::VolumeThickness; break;
    case 30:
        state.renderMode = ux3d::renderer::RenderMode::IridescenceFactor; break;
    case 31:
        state.renderMode = ux3d::renderer::RenderMode::IridescenceThickness; break;
    case 32:
        state.renderMode = ux3d::renderer::RenderMode::SSR; break;
    case 33:
        state.renderMode = ux3d::renderer::RenderMode::SSRRay; break;
    case 34:
        state.renderMode = ux3d::renderer::RenderMode::ShadowCascades; break;
    case 35:
        state.renderMode = ux3d::renderer::RenderMode::ShadowVisibility; break;
    case 36:
        state.renderMode = ux3d::renderer::RenderMode::SSAO; break;
    case 37:
        state.renderMode = ux3d::renderer::RenderMode::SkyVisibility; break;
    default:
        state.renderMode = ux3d::renderer::RenderMode::None; break;
    }
                if (antiAliasing) antiAliasing->resetHistory();
                revisions.mark(SceneChange::ViewportSettings);
            }

            UiProbe::item("vp.render_mode", 0);
            ImGui::TextDisabled("Allows inspecting individual PBR channels, geometry, or UVs.");

            ImGui::TextWrapped("Quality levels apply to all views. Shaded omits post AA, bloom, lens effects, SSR and SSAO.");
            ImGui::Separator();
            ImGui::Text("Face Culling & Winding:");
            if (ImGui::Checkbox("Force Double-Sided (Disable Backface Culling)", &state.forceDoubleSided)) {
                revisions.mark(SceneChange::ViewportSettings);
            }
            UiProbe::item("vp.force_double_sided", 0);
            if (ImGui::Checkbox("Invert Winding Order (Flip Front / Back Faces)", &state.invertWinding)) {
                revisions.mark(SceneChange::ViewportSettings);
            }
            UiProbe::item("vp.invert_winding", 0);
            ImGui::Separator();
            ImGui::Text("glTF Extensions:");
            // The extension switches (ASOBO now, other vendors later) are app settings, not viewport settings: one set
            // for every viewport, saved in settings.ini. They are edited in File > Settings > glTF Extensions.
            ImGui::TextDisabled("Which extensions are applied is an app setting.");
            if (ImGui::SmallButton("glTF Extensions settings...")) {
                requestSettingsWindow(SettingsTab::GltfExtensions);
            }
            UiProbe::item("vp.open_gltf_extensions", 0);
            // View settings that depend on the ASOBO group stay here (the effects are Quality levels; the ASOBO tuning is
            // in File > Settings > glTF Extensions); the sky-portal gizmos are a view helper.
            if (gltfLayer != nullptr && gltfLayer->hasSkyPortals()) {
                // A3.7: shown only for files with ASOBO_sky_portal nodes (the gizmos are a view helper).
                ImGui::BeginDisabled(!state.asobo.enabled);
                if (ImGui::Checkbox("Show sky portals", &state.showSkyPortals)) {
                    revisions.mark(SceneChange::ViewportSettings);
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip("Gizmo for every portal: disc, inner circle and the direction into the cabin.");
                }
                ImGui::EndDisabled();
            }
            ImGui::Separator();
            ImGui::Text("Wireframe:");
            if (ImGui::Checkbox("Wireframe Overlay", &state.wireframeOverlay)) {
                revisions.mark(SceneChange::ViewportSettings);
            }
            UiProbe::item("vp.wireframe_overlay", 0);
            ImGui::TextDisabled("Superimposes wireframe mesh edges on top of shaded materials.");
            if (ImGui::ColorEdit3("Line Color", &state.wireframeColor.x)) {
                revisions.mark(SceneChange::ViewportSettings);
            }
            if (ImGui::SliderFloat("Line Opacity", &state.wireframeColor.w, 0.0f, 1.0f, "%.2f")) {
                revisions.mark(SceneChange::ViewportSettings);
            }
            const char* wireframeBlendNames[] = { "Normal", "Additive", "Invert", "XOR" };
            int currentWireBlend = static_cast<int>(state.wireframeBlend);
            if (ImGui::Combo("Blend Mode", &currentWireBlend, wireframeBlendNames, IM_ARRAYSIZE(wireframeBlendNames))) {
                state.wireframeBlend = static_cast<WireframeBlendMode>(currentWireBlend);
                revisions.mark(SceneChange::ViewportSettings);
            }

            // Material Variants (KHR_materials_variants)
            if (gltfModel != nullptr && gltfModel->hasMaterialVariants()) {
                ImGui::Separator();
                ImGui::Text("Material Variants (KHR_materials_variants):");
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

                if (ImGui::BeginCombo("Active Variant", curVarName.c_str())) {
                    bool isDef = !gltfLayer || !gltfLayer->getMaterialVariant().has_value();
                    if (ImGui::Selectable("Default / Original", isDef)) {
                        if (gltfLayer) {
                            gltfLayer->setMaterialVariant(std::nullopt);
                            revisions.mark(SceneChange::Material);
                        }
                    }
                    for (const auto& v : variants) {
                        bool isSel = (gltfLayer && gltfLayer->getMaterialVariant().has_value() && gltfLayer->getMaterialVariant().value() == v.m_ID);
                        std::string vLabel = v.hasName() ? v.hasName() ? v.getName().c_str() : ("Variant " + std::to_string(v.m_ID)) : ("Variant " + std::to_string(v.m_ID));
                        if (ImGui::Selectable(vLabel.c_str(), isSel)) {
                            if (gltfLayer) {
                                gltfLayer->setMaterialVariant(v.m_ID);
                                revisions.mark(SceneChange::Material);
                            }
                        }
                    }
                    ImGui::EndCombo();
                }
            }

            ImGui::Separator();
            ImGui::TextDisabled("Quality (AA, render scale, IBL, shadow maps) is an app setting.");
            if (ImGui::SmallButton("Quality settings...")) {
                requestSettingsWindow(SettingsTab::Quality);
            }
            UiProbe::item("vp.open_quality", 0);

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------
        // Tab 2: Camera & Helpers
        // -------------------------------------------------------------
        const bool cameraTabOpen = ImGui::BeginTabItem("Camera & Helpers");
        UiProbe::item("vp.tab.camera", 0);
        if (cameraTabOpen) {
            ImGui::Text("Viewport Display:");
            ImGui::Checkbox("Show Ground Grid", &state.showGrid);
            UiProbe::item("vp.grid",0);
            ImGui::SameLine();
            ImGui::Checkbox("Show 3D View Compass", &state.showOrientationGizmo);

            ImGui::Separator();
            ImGui::Text("Selection & Transform Visualizers:");
            ImGui::Checkbox("Selected Oriented Bounding Box (Orange OBB)", &state.showSelectedBoundingBox);
            ImGui::Checkbox("Scene Bounding Box (Yellow AABB)", &state.showSceneBoundingBox);
            ImGui::Checkbox("Show Node Coordinate Gizmo", &state.showNodeCoordinateGizmo);
            if (state.showNodeCoordinateGizmo) {
                const char* conventions[] = { "Blender Source (Z-Up: X=Red, Y=Green, Z=Blue)", "glTF Native (Y-Up)" };
                ImGui::SetNextItemWidth(320.0f);
                ImGui::Combo("Gizmo Axis Convention", &state.gizmoAxisConvention, conventions, IM_ARRAYSIZE(conventions));
            }

            ImGui::Separator();
            ImGui::Text("Turntable Auto-Rotate:");
            ImGui::Checkbox("Enable Turntable", &state.turntable);
            if (state.turntable) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(150.0f);
                ImGui::SliderFloat("Speed (deg/s)", &state.turntableSpeed, -180.0f, 180.0f, "%.1f");
            }

            ImGui::Separator();
            ImGui::Text("Camera Settings:");

            auto* userCam = cameraActor.getUserCamera();
            ImGui::BeginDisabled(isAttachedToCamera);
            if (userCam != nullptr) {
                bool isOrtho = (userCam->getType() == ux3d::renderer::Camera::Type::Orthographic);
                if (ImGui::Checkbox("Orthographic Projection", &isOrtho)) {
                    cameraActor.changeUserCameraType(isOrtho ? ux3d::renderer::Camera::Type::Orthographic : ux3d::renderer::Camera::Type::Perspective);
                    revisions.mark(SceneChange::ViewCamera);
                }

                if (!isOrtho) {
                    float fov = glm::degrees(userCam->getYFoV());
                    if (ImGui::SliderFloat("Field of View (FoV)", &fov, 15.0f, 120.0f, "%.1f deg")) {
                        userCam->setPerspective(userCam->getAspectRatio(), glm::radians(fov), userCam->getZNearPlane(), userCam->getZFarPlane());
                        revisions.mark(SceneChange::ViewCamera);
                    }
                }

                ImGui::Separator();
                ImGui::Text("Frustum Clipping Planes:");
                float planes[2] = { userCam->getZNearPlane(), userCam->getZFarPlane() };
                if (ImGui::DragFloat2("Near / Far", planes, 0.01f, 0.0001f, 1000000.0f, "%.4f")) {
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
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Drag to edit the near and far clipping planes");
                }
            }
            ImGui::EndDisabled();

            ImGui::Separator();
            ImGui::Text("Camera Presets:");
            ImGui::BeginDisabled(isAttachedToCamera);
            {
                const float presetWidth = (std::max)(60.0f, (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f);
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

            ImGui::Separator();
            ImGui::Text("Camera Navigation:");
            ImGui::BeginDisabled(isAttachedToCamera);
            {
                float zoom = cameraActor.getZoom();
                if (ImGui::DragFloat("Zoom", &zoom, 0.01f, 0.0000001f, 100000.0f, "%.2f", ImGuiSliderFlags_Logarithmic)) {
                    cameraActor.setZoom(zoom);
                    cameraActor.updateUserCameraTransform();
                    if (antiAliasing != nullptr) antiAliasing->resetHistory();
                    revisions.mark(SceneChange::ViewCamera);
                }

                float rotation[2] = { glm::degrees(cameraActor.getRotationX()), glm::degrees(cameraActor.getRotationY()) };
                if (ImGui::DragFloat2("Rotation (deg)", rotation, 0.5f)) {
                    cameraActor.setCameraRotation(glm::radians(rotation[0]), glm::radians(rotation[1]));
                    cameraActor.updateUserCameraTransform();
                    if (antiAliasing != nullptr) antiAliasing->resetHistory();
                    revisions.mark(SceneChange::ViewCamera);
                }

                auto target = cameraActor.getLookTarget();
                float targetValues[3] = { target.x, target.y, target.z };
                if (ImGui::DragFloat3("Look Target", targetValues, 0.01f)) {
                    cameraActor.setTarget(ux3d::math::vec3(targetValues[0], targetValues[1], targetValues[2]));
                    cameraActor.updateUserCameraTransform();
                    if (antiAliasing != nullptr) antiAliasing->resetHistory();
                    revisions.mark(SceneChange::ViewCamera);
                }
            }
            ImGui::EndDisabled();

            if (ImGui::Button("Reset Camera (Home)")) {
                if (gltfModel != nullptr && gltfLayer != nullptr) {
                    auto* defScene = gltfModel->getDefaultScene();
                    if (defScene) {
                        cameraActor.fitCameraToScene(*defScene, gltfModel);
                    }
                }
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

} // namespace ux3d::imgui
