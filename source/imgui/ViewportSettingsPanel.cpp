// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/ViewportSettingsPanel.h"
#include "imgui/StdFileSystem.h"
#include <imgui.h>
#include <portable-file-dialogs.h>
#include <cmath>

namespace ux3d::imgui {

void ViewportSettingsPanel::render(ViewportSettingsState& state,
                                   ux3d::renderer::CommandGenerator* commandGenerator,
                                   ux3d::renderer::GltfRenderLayer* gltfLayer,
                                   ux3d::imgui::OrbitCamera& camera,
                                   bool& isSceneDirty,
                                   ux3d::gltf::Gltf* gltfModel)
{
    if (ImGui::BeginTabBar("ViewportSettingsTabs")) {
        // -------------------------------------------------------------
        // Tab 1: Environment
        // -------------------------------------------------------------
        if (ImGui::BeginTabItem("Environment")) {
            // Show background skybox toggle
            if (ImGui::Checkbox("Show Background Skybox", &state.showBackground)) {
                if (gltfLayer != nullptr) {
                    gltfLayer->enableCubeMap(state.showBackground);
                    isSceneDirty = true;
                }
            }

            // Clear color (used when skybox is disabled)
            if (!state.showBackground) {
                float clr[4] = { state.clearColor.r, state.clearColor.g, state.clearColor.b, state.clearColor.a };
                if (ImGui::ColorEdit4("Clear Color", clr)) {
                    state.clearColor = glm::vec4(clr[0], clr[1], clr[2], clr[3]);
                }
            }

            ImGui::Separator();

            // HDR environment map
            ImGui::Text("HDR Environment: %s", state.currentHdrPath.c_str());
            if (ImGui::Button("Load Custom HDR...")) {
                auto selection = pfd::open_file("Open HDR Panorama", "", { "HDR Environment (*.hdr *.exr)", "*.hdr *.exr" }).result();
                if (!selection.empty() && gltfLayer != nullptr) {
                    auto path = ux3d::imgui::StdFileSystem::instance().toPath(selection[0].c_str());
                    gltfLayer->setEnvironmentMap(path);
                    if (gltfLayer->filterEnvironmentMap()) {
                        state.currentHdrPath = selection[0];
                        if (commandGenerator != nullptr) {
                            commandGenerator->setIBLEnabled(true);
                        }
                        isSceneDirty = true;
                    }
                }
            }

            // HDR Blur
            if (ImGui::SliderFloat("HDR Blur", &state.hdrBlur, 0.0f, 1.0f, "%.2f")) {
                if (commandGenerator != nullptr) {
                    commandGenerator->setEnvironmentMapBlurNormalized(state.hdrBlur);
                }
            }

            // HDR Intensity
            if (ImGui::SliderFloat("HDR Intensity", &state.hdrIntensity, 0.0f, 10.0f, "%.2f")) {
                if (commandGenerator != nullptr) {
                    commandGenerator->setEnvironmentMapIntensity(state.hdrIntensity);
                }
            }

            // HDR Rotation
            if (ImGui::SliderFloat("HDR Rotation", &state.hdrRotation, 0.0f, 360.0f, "%.1f deg")) {
                if (commandGenerator != nullptr) {
                    commandGenerator->setEnvironmentMapRotation(state.hdrRotation);
                }
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------
        // Tab 2: Lighting & Tone Mapping
        // -------------------------------------------------------------
        if (ImGui::BeginTabItem("Lighting & Exposure")) {
            // Tone Mapping
            const char* toneMappingNames[] = {
                "Khronos PBR Neutral",
                "ACES Hill",
                "ACES Narkowicz"
            };
            if (ImGui::Combo("Tone Mapping", &state.toneMappingIdx, toneMappingNames, 3)) {
                if (commandGenerator != nullptr) {
                    if (state.toneMappingIdx == 0) {
                        commandGenerator->setToneMapping(ux3d::renderer::ToneMappingsEnum::PBR_NEUTRAL);
                    } else if (state.toneMappingIdx == 1) {
                        commandGenerator->setToneMapping(ux3d::renderer::ToneMappingsEnum::ACES_HILL);
                    } else if (state.toneMappingIdx == 2) {
                        commandGenerator->setToneMapping(ux3d::renderer::ToneMappingsEnum::ACES_NARKOWICZ);
                    }
                    isSceneDirty = true;
                }
            }

            // Exposure (EV slider and linear multiplier)
            if (ImGui::SliderFloat("Exposure (EV)", &state.exposureEV, -4.0f, 4.0f, "%.2f EV")) {
                float linearExposure = std::pow(2.0f, state.exposureEV);
                if (commandGenerator != nullptr) {
                    commandGenerator->setExposure(linearExposure);
                }
            }

            ImGui::Separator();

            // IBL toggle
            if (ImGui::Checkbox("Image Based Lighting (IBL)", &state.iblEnabled)) {
                if (commandGenerator != nullptr) {
                    commandGenerator->setIBLEnabled(state.iblEnabled);
                    isSceneDirty = true;
                }
            }

            // Punctual lights toggle
            if (ImGui::Checkbox("Punctual / Analytical Lights", &state.punctualLightsEnabled)) {
                if (commandGenerator != nullptr) {
                    commandGenerator->setPunctualLightsEnabled(state.punctualLightsEnabled);
                    isSceneDirty = true;
                }
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------
        // Tab 3: Debug Render Modes
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
                "Iridescence"
            };

            if (ImGui::Combo("Debug Mode", &state.renderModeIdx, renderModes, 21)) {
                isSceneDirty = true;
            }

            ImGui::TextDisabled("Allows inspecting individual PBR channels, geometry, or UVs.");

            ImGui::Separator();
            ImGui::Text("Anti-Aliasing (AA):");
            const char* aaModes[] = {
                "Off (No AA)",
                "MSAA 2x",
                "MSAA 4x (Recommended)",
                "MSAA 8x",
                "FXAA (Fast Approximate)",
                "TAA (Temporal Anti-Aliasing)"
            };
            if (ImGui::Combo("Mode##AntiAliasing", &state.antiAliasingIdx, aaModes, 6)) {
                isSceneDirty = true;
            }
            ImGui::TextDisabled("Hardware MSAA (crisp geometry) or post-processing FXAA / TAA (temporal subpixel smoothing).");

            ImGui::Separator();
            ImGui::Text("Face Culling & Winding:");
            if (ImGui::Checkbox("Force Double-Sided (Disable Backface Culling)", &state.forceDoubleSided)) {
                isSceneDirty = true;
            }
            if (ImGui::Checkbox("Invert Winding Order (Flip Front / Back Faces)", &state.invertWinding)) {
                isSceneDirty = true;
            }
            ImGui::Separator();
            ImGui::Text("Wireframe:");
            if (ImGui::Checkbox("Wireframe Overlay", &state.wireframeOverlay)) {
                isSceneDirty = true;
            }
            ImGui::TextDisabled("Superimposes wireframe mesh edges on top of shaded materials.");

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
                            isSceneDirty = true;
                        }
                    }
                    for (const auto& v : variants) {
                        bool isSel = (gltfLayer && gltfLayer->getMaterialVariant().has_value() && gltfLayer->getMaterialVariant().value() == v.m_ID);
                        std::string vLabel = v.hasName() ? v.getName().c_str() : ("Variant " + std::to_string(v.m_ID));
                        if (ImGui::Selectable(vLabel.c_str(), isSel)) {
                            if (gltfLayer) {
                                gltfLayer->setMaterialVariant(v.m_ID);
                                isSceneDirty = true;
                            }
                        }
                    }
                    ImGui::EndCombo();
                }
            }

            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------
        // Tab 4: Camera & Helpers
        // -------------------------------------------------------------
        if (ImGui::BeginTabItem("Camera & Helpers")) {
            ImGui::Text("Viewport Display:");
            ImGui::Checkbox("Show Ground Grid", &state.showGrid);
            ImGui::SameLine();
            ImGui::Checkbox("Show 3D View Compass", &state.showOrientationGizmo);

            ImGui::Separator();
            ImGui::Text("Bounding Box Visualizers:");
            ImGui::Checkbox("Scene Bounding Box (Yellow)", &state.showSceneBoundingBox);
            ImGui::SameLine();
            ImGui::Checkbox("Selected Node Bounds (Cyan)", &state.showSelectedBoundingBox);

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

            bool isOrtho = camera.isOrthographic();
            if (ImGui::Checkbox("Orthographic Projection", &isOrtho)) {
                camera.setOrthographic(isOrtho);
            }

            if (!isOrtho) {
                float fov = camera.getFoVY();
                if (ImGui::SliderFloat("Field of View (FoV)", &fov, 15.0f, 120.0f, "%.1f deg")) {
                    camera.setFoVY(fov);
                }
            }

            ImGui::Separator();
            ImGui::Text("Frustum Clipping Planes:");

            bool autoClip = camera.isAutoClip();
            if (ImGui::Checkbox("Auto Near / Far Clip Planes", &autoClip)) {
                camera.setAutoClip(autoClip);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Automatically adapts near clipping plane when zooming in so close-up objects (like cockpits) are not sliced off.");
            }

            if (autoClip) {
                ImGui::Text("Near Plane: %.4f m (Auto)", camera.getZNear());
                ImGui::Text("Far Plane: %.1f m (Auto)", camera.getZFar());
            } else {
                float nearVal = camera.getRawZNear();
                if (ImGui::DragFloat("Near Plane (m)", &nearVal, 0.002f, 0.0001f, 10.0f, "%.4f")) {
                    camera.setZNear(nearVal);
                }
                float farVal = camera.getRawZFar();
                if (ImGui::DragFloat("Far Plane (m)", &farVal, 10.0f, 1.0f, 100000.0f, "%.1f")) {
                    camera.setZFar(farVal);
                }
            }

            ImGui::Text("Camera Presets:");
            if (ImGui::Button("Front (Num 1)")) camera.setPresetView(PresetView::Front);
            ImGui::SameLine();
            if (ImGui::Button("Back (Ctrl+1)")) camera.setPresetView(PresetView::Back);
            ImGui::SameLine();
            if (ImGui::Button("Right (Num 3)")) camera.setPresetView(PresetView::Right);
            ImGui::SameLine();
            if (ImGui::Button("Left (Ctrl+3)")) camera.setPresetView(PresetView::Left);
            ImGui::SameLine();
            if (ImGui::Button("Top (Num 7)")) camera.setPresetView(PresetView::Top);
            ImGui::SameLine();
            if (ImGui::Button("Iso")) camera.setPresetView(PresetView::Isometric);

            ImGui::Separator();
            ImGui::Text("Camera Distance: %.2f", camera.getDistance());
            ImGui::Text("Yaw: %.1f deg, Pitch: %.1f deg", camera.getYaw(), camera.getPitch());
            auto target = camera.getTarget();
            ImGui::Text("Look Target: [%.2f, %.2f, %.2f]", target.x, target.y, target.z);

            if (ImGui::Button("Reset Camera (Home)")) {
                camera.reset();
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

} // namespace ux3d::imgui
