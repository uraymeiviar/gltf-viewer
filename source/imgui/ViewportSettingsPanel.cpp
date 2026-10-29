#include "imgui/GladGL.h"
#include "imgui/ViewportSettingsPanel.h"
#include "imgui/OceanSimulation.h"
#include "imgui/RenderQuality.h"
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
                "SSR", "SSR Ray", "Shadow Cascades", "Shadow Visibility", "SSAO", "Sky Visibility"
            };

            if (ImGui::Combo("Debug Mode", &state.renderModeIdx, renderModes, IM_ARRAYSIZE(renderModes))) {
                revisions.mark(SceneChange::ViewportSettings);
            }

            ImGui::TextDisabled("Allows inspecting individual PBR channels, geometry, or UVs.");

            // Settings scopes: anti-aliasing is renderer-wide (Quality tab); in its place, this viewport's overrides of
            // the scene's effects. The block keeps the rows' heights (a text and two rows of combos), so the controls
            // below keep their places (validate_p4_exit_gate.py clicks them by position).
            ImGui::Separator();
            ImGui::Text("Effect overrides (Scene / Off / On):");
            const char* overrideNames[] = { "Scene", "Off", "On" };
            const float overrideWidth = 70.0f;
            const auto overrideCombo = [&](const char* label, const char* tag, EffectOverride& value, bool effective,
                                           const char* tooltip) {
                int index = static_cast<int>(value);
                ImGui::SetNextItemWidth(overrideWidth);
                const bool changed = ImGui::Combo(label, &index, overrideNames, 3);
                UiProbe::item(tag, 0);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s\nScene: follow the Scene Inspector. Now %s in this viewport.", tooltip,
                                      effective ? "on" : "off");
                }
                if (changed) {
                    value = static_cast<EffectOverride>(index);
                    if (antiAliasing) antiAliasing->resetHistory();
                    revisions.mark(SceneChange::ViewportSettings);
                }
                return changed;
            };
            overrideCombo("Shadows##vpShadowsOverride", "vp.override.shadows", state.shadowsOverride, state.sunShadows,
                          "Sun shadows in this viewport.");
            ImGui::SameLine();
            if (overrideCombo("SSR##vpSsrOverride", "vp.override.ssr", state.ssrOverride, state.screenSpaceReflections,
                              "Windshield screen-space reflections in this viewport.")) {
                RenderQuality::pinSsr();
            }
            overrideCombo("SSAO##vpSsaoOverride", "vp.override.ssao", state.ssaoOverride, state.ssao.enabled,
                          "Screen-space ambient occlusion in this viewport.");
            ImGui::SameLine();
            ImGui::TextDisabled("Now: shadows %s, SSR %s, SSAO %s", state.sunShadows ? "on" : "off",
                                state.screenSpaceReflections ? "on" : "off", state.ssao.enabled ? "on" : "off");

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
            // Settings scopes: SSR, interior sky reflection, icing, sky portal strength and the decal tolerance are
            // scene settings now (Scene Inspector > Shadows, Reflections & AO); SSR quality is in the Quality tab.
            if (gltfLayer != nullptr && gltfLayer->hasSkyPortals()) {
                // A3.7: shown only for files with ASOBO_sky_portal nodes (the gizmos are a view helper).
                if (ImGui::Checkbox("Show sky portals", &state.showSkyPortals)) {
                    revisions.mark(SceneChange::ViewportSettings);
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Gizmo for every portal: disc, inner circle and the direction into the cabin.");
                }
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
        const bool cameraTabOpen = ImGui::BeginTabItem("Camera & Helpers");
        UiProbe::item("vp.tab.camera", 0);
        if (cameraTabOpen) {
            ImGui::Text("Viewport Display:");
            ImGui::Checkbox("Show Ground Grid", &state.showGrid);
            ImGui::SameLine();
            ImGui::Checkbox("Show 3D View Compass", &state.showOrientationGizmo);

            // A3.6: the ground. Grid is the editor grid overlay; Plane is an infinite surface in the scene.
            ImGui::Separator();
            if (ImGui::CollapsingHeader("Ground", ImGuiTreeNodeFlags_DefaultOpen)) {
                GroundSettings& ground = state.ground;
                const char* groundNames[] = { "Grid", "Plane", "Ocean" };
                if (probed("vp.ground.type", [&] { return ImGui::BeginCombo("Type", groundNames[static_cast<int>(ground.mode)]); })) {
                    for (int i = 0; i < 3; ++i) {
                        // A6.8: the ocean needs compute shaders (OpenGL 4.3).
                        const bool unavailable = i == static_cast<int>(GroundMode::Ocean) && !oceanAvailable();
                        ImGui::BeginDisabled(unavailable);
                        if (ImGui::Selectable(groundNames[i], static_cast<int>(ground.mode) == i)) {
                            ground.mode = static_cast<GroundMode>(i);
                            revisions.mark(SceneChange::ViewportSettings);
                        }
                        ImGui::EndDisabled();
                        if (unavailable && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                            ImGui::SetTooltip("The ocean needs an OpenGL 4.3 context (compute shaders).");
                        }
                    }
                    ImGui::EndCombo();
                }
                if (ground.mode == GroundMode::Plane || ground.mode == GroundMode::Ocean) {
                    const char* heightNames[] = { "Model base (lowest point)", "Y = 0", "Custom" };
                    int heightMode = static_cast<int>(ground.heightMode);
                    if (probed("vp.ground.height", [&] { return ImGui::Combo("Height", &heightMode, heightNames, IM_ARRAYSIZE(heightNames)); })) {
                        ground.heightMode = static_cast<GroundHeightMode>(heightMode);
                        revisions.mark(SceneChange::ViewportSettings);
                    }
                    if (ground.heightMode == GroundHeightMode::Custom) {
                        if (probed("vp.ground.custom_height", [&] { return ImGui::DragFloat("Height (scene units)", &ground.customHeight, 0.01f, -1.0e6f, 1.0e6f, "%.3f"); })) {
                            revisions.mark(SceneChange::ViewportSettings);
                        }
                    }
                }
                if (ground.mode == GroundMode::Ocean) {
                    // A6.8: the ocean. Wind, fetch, choppiness, swell, quality and period recompute the simulation
                    // (a looping set in the background, a live set at once); foam, colour and play only change the
                    // shading. Every ID carries an "##ocean" suffix (no clash with the plane's or other tabs' labels).
                    OceanSettings& ocean = ground.ocean;
                    const OceanUiStatus& status = oceanUiStatus();
                    ImGui::Spacing();
                    ImGui::Text("Ocean");
                    int mode = ocean.live ? 1 : 0;
                    if (probed("vp.ocean.mode", [&] { return ImGui::Combo("Mode##oceanMode", &mode, "Looping (precomputed)\0Live (FFT every frame)\0"); })) {
                        ocean.live = mode == 1;
                        revisions.mark(SceneChange::ViewportSettings);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Looping: a precomputed set that repeats every Period; the per-frame cost is texture sampling.\n"
                                          "Live: the FFT runs every frame (512x512 at High quality); it costs GPU time each frame.");
                    }
                    // Settings scopes: the simulation quality is renderer-wide (Quality tab); shown here for reference.
                    {
                        const char* qualityNames[] = { "Low", "Medium", "High" };
                        probed("vp.ocean.quality", [&] {
                            ImGui::TextDisabled("Quality: %s (Quality tab, all viewports)",
                                                qualityNames[std::clamp(static_cast<int>(ocean.quality), 0, 2)]);
                            return false;
                        });
                    }
                    if (probed("vp.ocean.wind_speed", [&] { return ImGui::SliderFloat("Wind speed (m/s)##oceanWindSpeed", &ocean.windSpeed, 0.5f, 40.0f, "%.1f"); })) {
                        revisions.mark(SceneChange::ViewportSettings);
                    }
                    if (probed("vp.ocean.wind_dir", [&] { return ImGui::SliderFloat("Wind from (deg)##oceanWindDirection", &ocean.windDirection, 0.0f, 360.0f, "%.0f"); })) {
                        revisions.mark(SceneChange::ViewportSettings);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("The direction the wind comes from, around +Y from +Z towards +X (the sun's azimuth). The waves travel the other way.");
                    }
                    if (probed("vp.ocean.choppiness", [&] { return ImGui::SliderFloat("Choppiness##oceanChoppiness", &ocean.choppiness, 0.0f, 2.0f, "%.2f"); })) {
                        revisions.mark(SceneChange::ViewportSettings);
                    }
                    if (probed("vp.ocean.swell", [&] { return ImGui::SliderFloat("Swell##oceanSwell", &ocean.swell, 0.0f, 1.0f, "%.2f"); })) {
                        revisions.mark(SceneChange::ViewportSettings);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Narrows the directional spread of the waves (long, parallel swell).");
                    }
                    if (probed("vp.ocean.foam", [&] { return ImGui::SliderFloat("Foam amount##oceanFoam", &ocean.foam, 0.0f, 1.0f, "%.2f"); })) {
                        revisions.mark(SceneChange::ViewportSettings);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Whitecaps. 0.5 is the coverage observed at this wind speed (about 1%% at 10 m/s, 10%% at 20 m/s); 1 is four times that.");
                    }
                    if (probed("vp.ocean.color", [&] { return ImGui::ColorEdit3("Water colour##oceanColor", &ocean.color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR); })) {
                        revisions.mark(SceneChange::ViewportSettings);
                    }
                    ImGui::BeginDisabled(ocean.live);
                    if (probed("vp.ocean.period", [&] { return ImGui::DragFloat("Loop period (s)##oceanPeriod", &ocean.period, 0.5f, 5.0f, 120.0f, "%.1f"); })) {
                        ocean.period = std::clamp(ocean.period, 5.0f, 120.0f);
                        revisions.mark(SceneChange::ViewportSettings);
                    }
                    ImGui::EndDisabled();
                    if (probed("vp.ocean.fetch", [&] { return ImGui::DragFloat("Fetch (m)##oceanFetch", &ocean.fetch, 100.0f, 1000.0f, 1.0e7f, "%.0f", ImGuiSliderFlags_Logarithmic); })) {
                        ocean.fetch = std::clamp(ocean.fetch, 1000.0f, 1.0e7f);
                        revisions.mark(SceneChange::ViewportSettings);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Distance the wind has blown over. Beyond full development (about 22000 x U^2 / g) it changes nothing.");
                    }
                    if (probed("vp.ocean.play", [&] { return ImGui::Checkbox("Animate##oceanPlay", &ocean.play); })) {
                        revisions.mark(SceneChange::ViewportSettings);
                    }
                    ImGui::SameLine();
                    if (probed("vp.ocean.reset", [&] { return ImGui::Button("Reset ocean##oceanReset"); })) {
                        ocean = OceanSettings();
                        revisions.mark(SceneChange::ViewportSettings);
                    }
                    if (ocean.time >= 0.0f) {
                        ImGui::TextDisabled("Time pinned at %.2f s (--ocean-time).", ocean.time);
                    }
                    if (status.busy) {
                        ImGui::ProgressBar(status.progress, ImVec2(-FLT_MIN, 0.0f), "Recomputing the waves");
                    } else if (status.ready) {
                        if (status.live) {
                            ImGui::TextDisabled("Hs %.2f m, %.1f MiB, %.2f ms GPU per frame (live)", status.significantWaveHeight,
                                                static_cast<double>(status.memoryBytes) / 1048576.0, status.liveGpuMs);
                        } else {
                            ImGui::TextDisabled("Hs %.2f m, %.1f MiB (precomputed, sampled per frame)", status.significantWaveHeight,
                                                static_cast<double>(status.memoryBytes) / 1048576.0);
                        }
                    }
                    ImGui::TextDisabled("Reflections: sky and environment only (no ocean SSR).");
                }
                if (ground.mode == GroundMode::Plane) {
                    int preset = static_cast<int>(ground.preset);
                    const char* presetLabel =
                        ground.preset == GroundPreset::Custom ? "Custom" : groundPresets()[static_cast<size_t>(preset)].label.data();
                    if (probed("vp.ground.material", [&] { return ImGui::BeginCombo("Material", presetLabel); })) {
                        for (size_t i = 0; i < groundPresets().size(); ++i) {
                            if (ImGui::Selectable(groundPresets()[i].label.data(), static_cast<size_t>(preset) == i)) {
                                ground.applyPreset(static_cast<GroundPreset>(i));
                                revisions.mark(SceneChange::ViewportSettings);
                            }
                        }
                        ImGui::EndCombo();
                    }
                    if (probed("vp.ground.albedo", [&] { return ImGui::ColorEdit3("Albedo", &ground.albedo.x, ImGuiColorEditFlags_Float); })) {
                        ground.preset = GroundPreset::Custom;
                        revisions.mark(SceneChange::ViewportSettings);
                    }
                    if (probed("vp.ground.roughness", [&] { return ImGui::SliderFloat("Roughness", &ground.roughness, 0.0f, 1.0f, "%.2f"); })) {
                        ground.preset = GroundPreset::Custom;
                        revisions.mark(SceneChange::ViewportSettings);
                    }
                    if (probed("vp.ground.detail", [&] { return ImGui::SliderFloat("Detail", &ground.detail, 0.0f, 1.0f, "%.2f"); })) {
                        ground.preset = GroundPreset::Custom;
                        revisions.mark(SceneChange::ViewportSettings);
                    }
                    if (probed("vp.ground.detail_size", [&] { return ImGui::DragFloat("Detail size", &ground.detailScale, 0.01f, 0.0001f, 1.0e6f, "%.3f", ImGuiSliderFlags_Logarithmic); })) {
                        ground.preset = GroundPreset::Custom;
                        revisions.mark(SceneChange::ViewportSettings);
                    }
                    if (probed("vp.ground.joints", [&] { return ImGui::DragFloat("Joint spacing", &ground.jointSpacing, 0.05f, 0.0f, 1.0e6f, ground.jointSpacing > 0.0f ? "%.2f" : "None"); })) {
                        ground.preset = GroundPreset::Custom;
                        revisions.mark(SceneChange::ViewportSettings);
                    }
                    if (probed("vp.ground.haze", [&] { return ImGui::DragFloat("Haze distance", &ground.haze, 1.0f, 0.0f, 1.0e9f, ground.haze > 0.0f ? "%.0f" : "Auto", ImGuiSliderFlags_Logarithmic); })) {
                        revisions.mark(SceneChange::ViewportSettings);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Distance at which the plane has faded 98% into the horizon colour. 0 = automatic (0.9 x far plane).");
                    }
                    if (probed("vp.ground.show_grid", [&] { return ImGui::Checkbox("Show grid on ground", &ground.showGrid); })) {
                        revisions.mark(SceneChange::ViewportSettings);
                    }
                    ImGui::TextDisabled("Lit by the environment (its own lower hemisphere) and the sun with shadows.");
                }
            }

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
        // Tab 3: Render Quality (all viewports): AA, resolution, effect quality, IBL filter, sun shadow maps
        // -------------------------------------------------------------
        const bool qualityTabOpen = ImGui::BeginTabItem("Quality");
        UiProbe::item("vp.tab.quality", 0);
        if (qualityTabOpen) {
            RenderQuality::drawSettings(gltfLayer, revisions);
            ImGui::EndTabItem();
        }

        // -------------------------------------------------------------
        // Tab 4: Post (A6.7 lens effect; after the existing tabs so they keep their places)
        // -------------------------------------------------------------
        const bool postTabOpen = ImGui::BeginTabItem("Post");
        UiProbe::item("vp.tab.post", 0);
        if (postTabOpen) {
            ImGui::SeparatorText(ICON_MDI_FLARE " Lens Effect");
            const char* lensModes[] = { "Off", "Ghosts + Halo", "Anamorphic Streak", "Ghosts + Streak" };
            int lens = std::clamp(state.lensFlare, 0, 3);
            if (ImGui::Combo("Lens Effect", &lens, lensModes, IM_ARRAYSIZE(lensModes))) {
                state.lensFlare = lens;
                revisions.mark(SceneChange::ViewportSettings);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Screen-space lens ghosts, a halo and/or a horizontal streak from very bright light\n"
                                  "(the sun disk, its mirror highlights), built from the bloom mips.\n"
                                  "Part of the bloom chain: Scene Inspector -> Bloom off removes it.");
            }
            ImGui::BeginDisabled(state.lensFlare == 0);
            if (ImGui::SliderFloat("Lens Intensity", &state.lensFlareIntensity, 0.0f, 10.0f, "%.2f", ImGuiSliderFlags_Logarithmic)) {
                revisions.mark(SceneChange::ViewportSettings);
            }
            ImGui::EndDisabled();
            ImGui::TextDisabled("Glare itself is the scene's bloom (Scene Inspector -> Environment).");
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

} // namespace ux3d::imgui
