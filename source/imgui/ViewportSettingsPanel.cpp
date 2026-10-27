#include "imgui/GladGL.h"
#include "imgui/ViewportSettingsPanel.h"
#include "imgui/RenderQuality.h"
#include "imgui/StdFileSystem.h"
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
        if (ImGui::BeginTabItem("Render Modes")) {
            const char* renderModes[] = {
                "None / Shaded (Default)",
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
                "SSR", "SSR Ray"
            };

            if (ImGui::Combo("Debug Mode", &state.renderModeIdx, renderModes, 34)) {
                revisions.mark(SceneChange::ViewportSettings);
            }

            ImGui::TextDisabled("Allows inspecting individual PBR channels, geometry, or UVs.");

            ImGui::Separator();
            ImGui::Text("Anti-Aliasing (AA):");
            const char* msaaModes[] = { "Off", "2x", "4x", "8x" };
            int msaa = state.effectiveMsaaIdx();
            if (ImGui::Combo("MSAA", &msaa, msaaModes, 4)) {
                state.msaaIdx = msaa;
                if (antiAliasing) antiAliasing->resetHistory();
                revisions.mark(SceneChange::ViewportSettings);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Crisp geometry edges; costs memory and fill rate.");
            const char* postModes[] = { "None", "FXAA", "TAA" };
            int post = state.effectivePostAAIdx();
            if (ImGui::Combo("Post AA", &post, postModes, 3)) {
                state.postAAIdx = post;
                if (antiAliasing) antiAliasing->resetHistory();
                revisions.mark(SceneChange::ViewportSettings);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(
                "FXAA: cheap, slightly soft.\nTAA: stable, best on shader aliasing and decals; can ghost.");

            ImGui::Separator();
            ImGui::Text("Face Culling & Winding:");
            if (ImGui::Checkbox("Force Double-Sided (Disable Backface Culling)", &state.forceDoubleSided)) {
                revisions.mark(SceneChange::ViewportSettings);
            }
            if (ImGui::Checkbox("Invert Winding Order (Flip Front / Back Faces)", &state.invertWinding)) {
                revisions.mark(SceneChange::ViewportSettings);
            }
            ImGui::Separator();
            ImGui::Text("MSFS Extensions:");
            // The master switch keeps this row: validate_p4_exit_gate.py clicks it.
            if (ImGui::Checkbox("Apply MSFS (ASOBO) Extensions", &state.asobo.enabled)) {
                revisions.mark(SceneChange::ViewportSettings);
            }
            // One switch per extension Glider renders, from the A0.2 support table.
            ImGui::BeginDisabled(!state.asobo.enabled);
            if (ImGui::Checkbox("Screen-space reflections", &state.screenSpaceReflections)) {
                revisions.mark(SceneChange::ViewportSettings);
            }
            if (ImGui::Combo("SSR quality", &state.ssrQuality, "Low\0Medium\0High\0")) {
                revisions.mark(SceneChange::ViewportSettings);
            }
            if (ImGui::SliderFloat("Interior sky reflection", &state.interiorSkyReflection, 0.0f, 1.0f)) {
                revisions.mark(SceneChange::ViewportSettings);
            }
            if (ImGui::SliderFloat("Windshield icing", &state.windshieldIcing, 0.0f, 1.0f, "%.2f")) {
                revisions.mark(SceneChange::ViewportSettings);
            }
            ImGui::Indent();
            for (const ux3d::gltf::asobo::SupportInfo& info : ux3d::gltf::asobo::supportTable()) {
                if (!ux3d::gltf::asobo::isAsoboExtension(info.name)) {
                    continue;
                }
                if (info.status != ux3d::gltf::asobo::Support::Rendered && info.status != ux3d::gltf::asobo::Support::Partial) {
                    continue;
                }
                if (!info.viewportSwitch) {
                    continue;
                }
                const std::string extension(info.name);
                const std::string label = ExtensionInspector::formatExtensionLabel(extension) + "##asobo_" + extension;
                bool applied = std::find(state.asobo.disabled.begin(), state.asobo.disabled.end(), extension) == state.asobo.disabled.end();
                if (ImGui::Checkbox(label.c_str(), &applied)) {
                    state.asobo.setApplied(extension, applied);
                    revisions.mark(SceneChange::ViewportSettings);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s%s%.*s", extension.c_str(), info.note.empty() ? "" : ": ",
                                      static_cast<int>(info.note.size()), info.note.data());
                }
            }
            const bool usesDecalBuffer = gltfLayer != nullptr && gltfLayer->getIsolationRoot() == nullptr
                && state.renderMode != ux3d::renderer::RenderMode::Wireframe
                && ((gltfLayer->hasGeometryDecalMaterials() && state.asobo.applies("ASOBO_material_geometry_decal"))
                    || (gltfLayer->hasBlendGbufferMaterials() && state.asobo.applies("ASOBO_material_blend_gbuffer")));
            if (usesDecalBuffer) {
                if (ImGui::DragFloat("Decal depth tolerance (mm)", &state.decalDepthToleranceMm,
                                     0.05f, 0.0f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
                    revisions.mark(SceneChange::ViewportSettings);
                }
            }
            ImGui::Unindent();
            ImGui::EndDisabled();
            ImGui::TextDisabled("Off: this viewport renders the file as if it had no ASOBO extensions.");
            ImGui::Separator();
            ImGui::Text("Wireframe:");
            if (ImGui::Checkbox("Wireframe Overlay", &state.wireframeOverlay)) {
                revisions.mark(SceneChange::ViewportSettings);
            }
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

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------
        // Tab 2: Camera & Helpers
        // -------------------------------------------------------------
        if (ImGui::BeginTabItem("Camera & Helpers")) {
            ImGui::Text("Viewport Display:");
            ImGui::Checkbox("Show Ground Grid", &state.showGrid);
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

        // -------------------------------------------------------------
        // Tab 3: Render Quality (all viewports)
        // -------------------------------------------------------------
        if (ImGui::BeginTabItem("Quality")) {
            RenderQuality::drawSettings(gltfLayer, revisions);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

} // namespace ux3d::imgui
