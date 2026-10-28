// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/SceneInspector.h"
#include "imgui/RenderQuality.h"
#include "imgui/StdFileSystem.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "imgui/UiProbe.h"
#include "renderer/CommandGenerator.h"
#include "renderer/gltf/GltfRenderLayer.h"
#include "gltf/Scene.h"
#include <imgui.h>
#include <portable-file-dialogs.h>
#include <cmath>
#include <cstring>

namespace ux3d::imgui {

// A3.1: Environment -> Sun. Conventions in SceneEnvironmentState (SceneEnvironment.h).
void SceneInspector::renderSun(SceneEnvironmentState& env, ux3d::renderer::GltfRenderLayer* gltfLayer,
                               SceneRevisions& revisions)
{
    ImGui::SeparatorText(ICON_MDI_WHITE_BALANCE_SUNNY " Sun");
    if (ImGui::Checkbox("Sun", &env.sunEnabled)) {
        revisions.mark(SceneChange::RenderSettings);
    }
    UiProbe::item("scene.sun.on", 0);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("A directional light through the same BRDF as punctual lights.\n"
                          "While it is on, the HDR's own sun is removed from the IBL and the background\n"
                          "(in both modes), so there is never a second sun. Toggling re-filters the IBL once.");
    }
    if (!env.sunEnabled) {
        return;
    }

    const char* modeNames[] = { "Manual", "From HDR" };
    if (ImGui::Combo("Sun Mode", &env.sunMode, modeNames, 2)) {
        revisions.mark(SceneChange::RenderSettings);
    }
    UiProbe::item("scene.sun.mode", 0);

    const ux3d::renderer::EnvironmentSun* found = gltfLayer != nullptr ? &gltfLayer->getEnvironmentSun() : nullptr;
    if (env.sunMode == static_cast<int>(SunMode::FromHdr)) {
        if (found == nullptr || !found->found) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "No sun found in the HDR: the sun is off.");
        } else {
            const glm::mat3 rotation = ux3d::renderer::CommandGenerator::environmentRotationMatrix(env.hdrRotation);
            float azimuth = 0.0f, elevation = 0.0f;
            sunAnglesFromDirection(glm::transpose(rotation) * found->direction, azimuth, elevation);
            ImGui::Text("Detected: azimuth %.2f deg, elevation %.2f deg", azimuth, elevation);
            ImGui::Text("Illuminance %.4g (x HDR intensity %.2f)", found->illuminance, env.hdrIntensity);
            float colour[3] = { found->color.r, found->color.g, found->color.b };
            ImGui::ColorEdit3("Detected Colour", colour, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker |
                                                         ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
            ImGui::TextDisabled("%u texels, %.3g sr, peak %.4g, sky %.4g", found->texelCount, found->solidAngle,
                                found->peakLuminance, found->skyLevel);
            if (ImGui::Button("Copy to Manual")) {
                env.sunAzimuth = azimuth;
                env.sunElevation = elevation;
                env.sunColor = found->color;
                env.sunIntensity = found->illuminance * env.hdrIntensity;
                env.sunMode = static_cast<int>(SunMode::Manual);
                revisions.mark(SceneChange::RenderSettings);
            }
        }
    } else {
        if (ImGui::SliderFloat("Azimuth", &env.sunAzimuth, 0.0f, 360.0f, "%.1f deg")) {
            revisions.mark(SceneChange::RenderSettings);
        }
        UiProbe::item("scene.sun.azimuth", 0);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Around +Y, from +Z towards +X (glTF Y-up). 0 = sun towards +Z.");
        }
        if (ImGui::SliderFloat("Elevation", &env.sunElevation, -90.0f, 90.0f, "%.1f deg")) {
            revisions.mark(SceneChange::RenderSettings);
        }
        UiProbe::item("scene.sun.elevation", 0);
        float colour[3] = { env.sunColor.r, env.sunColor.g, env.sunColor.b };
        if (ImGui::ColorEdit3("Sun Colour", colour, ImGuiColorEditFlags_Float)) {
            env.sunColor = glm::vec3(colour[0], colour[1], colour[2]);
            revisions.mark(SceneChange::RenderSettings);
        }
        static float kelvin = 5778.0f;
        if (ImGui::DragFloat("Kelvin", &kelvin, 10.0f, 1667.0f, 25000.0f, "%.0f K")) {
            env.sunColor = kelvinToLinearRgb(kelvin);
            revisions.mark(SceneChange::RenderSettings);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Sets the colour to a black body at this temperature (luminance 1).");
        }
        if (ImGui::DragFloat("Intensity", &env.sunIntensity, 0.05f, 0.0f, 100000.0f, "%.3f", ImGuiSliderFlags_Logarithmic)) {
            revisions.mark(SceneChange::RenderSettings);
        }
        UiProbe::item("scene.sun.intensity", 0);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Illuminance in the environment's units (HDR radiance x sr).\n"
                              "A white Lambert surface facing the sun reflects colour x intensity / pi.");
        }
        if (found != nullptr && found->found && ImGui::SmallButton("Use HDR sun direction")) {
            const glm::mat3 rotation = ux3d::renderer::CommandGenerator::environmentRotationMatrix(env.hdrRotation);
            sunAnglesFromDirection(glm::transpose(rotation) * found->direction, env.sunAzimuth, env.sunElevation);
            revisions.mark(SceneChange::RenderSettings);
        }
    }
    if (ImGui::SliderFloat("Angular Radius", &env.sunAngularRadius, 0.0f, 5.0f, "%.2f deg")) {
        revisions.mark(SceneChange::RenderSettings);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Widens the specular highlight to the sun's disk (0.27 deg = the real sun).");
    }
    if (found != nullptr && found->removed) {
        ImGui::TextDisabled("HDR sun removed from IBL and background (%u texels).", found->texelCount);
    }
}

void SceneInspector::render(SceneEnvironmentState& env,
                            ux3d::renderer::CommandGenerator* commandGenerator,
                            ux3d::renderer::GltfRenderLayer* gltfLayer,
                            SceneRevisions& revisions,
                            ux3d::gltf::Scene* scene)
{
    if (scene != nullptr) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), ICON_MDI_IMAGE_FILTER_HDR " Scene ID: %u", scene->m_ID);

        char nameBuf[256] = {};
        if (scene->hasName()) {
            std::strncpy(nameBuf, scene->getName().c_str(), sizeof(nameBuf) - 1);
        }
        if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
            scene->setName(nameBuf);
            revisions.mark(SceneChange::Metadata);
        }

        ImGui::Separator();
    }

    if (ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Show background skybox toggle
        if (ImGui::Checkbox("Show Background Skybox", &env.showBackground)) {
            if (gltfLayer != nullptr) {
                gltfLayer->enableCubeMap(env.showBackground);
            }
            revisions.mark(SceneChange::RenderSettings);
        }

        // Clear color (used when skybox is disabled)
        if (!env.showBackground) {
            float clr[4] = { env.clearColor.r, env.clearColor.g, env.clearColor.b, env.clearColor.a };
            if (ImGui::ColorEdit4("Clear Color", clr)) {
                env.clearColor = glm::vec4(clr[0], clr[1], clr[2], clr[3]);
                revisions.mark(SceneChange::RenderSettings);
            }
        }

        ImGui::Separator();

        // HDR environment map
        ImGui::Text("HDR / EXR Environment: %s", env.currentHdrPath.c_str());
        if (ImGui::Button("Load Custom HDR / EXR...")) {
            auto selection = pfd::open_file("Open HDR / EXR Panorama", "", { "HDR / OpenEXR Environment (*.hdr *.exr)", "*.hdr *.exr", "All Files (*.*)", "*.*" }).result();
            if (!selection.empty() && gltfLayer != nullptr) {
                auto path = ux3d::imgui::StdFileSystem::instance().toPath(selection[0].c_str());
                gltfLayer->setEnvironmentMap(path);
                if (gltfLayer->filterEnvironmentMap()) {
                    env.currentHdrPath = selection[0];
                    env.iblEnabled = true;
                    if (commandGenerator != nullptr) {
                        commandGenerator->setIBLEnabled(true);
                    }
                    revisions.mark(SceneChange::RenderSettings);
                }
            }
        }

        // HDR Blur
        if (ImGui::SliderFloat("HDR Blur", &env.hdrBlur, 0.0f, 1.0f, "%.2f")) {
            if (commandGenerator != nullptr) {
                commandGenerator->setEnvironmentMapBlurNormalized(env.hdrBlur);
            }
            revisions.mark(SceneChange::RenderSettings);
        }

        // HDR Intensity
        if (ImGui::SliderFloat("HDR Intensity", &env.hdrIntensity, 0.0f, 10.0f, "%.2f")) {
            if (commandGenerator != nullptr) {
                commandGenerator->setEnvironmentMapIntensity(env.hdrIntensity);
            }
            revisions.mark(SceneChange::RenderSettings);
        }

        // HDR Rotation
        if (ImGui::SliderFloat("HDR Rotation", &env.hdrRotation, 0.0f, 360.0f, "%.1f deg")) {
            if (commandGenerator != nullptr) {
                commandGenerator->setEnvironmentMapRotation(env.hdrRotation);
            }
            revisions.mark(SceneChange::RenderSettings);
        }

        ImGui::Separator();

        // Tone Mapping
        const char* toneMappingNames[] = {
            "Khronos PBR Neutral",
            "ACES Hill",
            "ACES Narkowicz"
        };
        if (ImGui::Combo("Tone Mapping", &env.toneMappingIdx, toneMappingNames, 3)) {
            if (commandGenerator != nullptr) {
                if (env.toneMappingIdx == 0) {
                    commandGenerator->setToneMapping(ux3d::renderer::ToneMappingsEnum::PBR_NEUTRAL);
                } else if (env.toneMappingIdx == 1) {
                    commandGenerator->setToneMapping(ux3d::renderer::ToneMappingsEnum::ACES_HILL);
                } else if (env.toneMappingIdx == 2) {
                    commandGenerator->setToneMapping(ux3d::renderer::ToneMappingsEnum::ACES_NARKOWICZ);
                }
            }
            revisions.mark(SceneChange::RenderSettings);
        }

        // Exposure mode (Manual / Auto) and controls
        const char* exposureModeNames[] = { "Manual", "Auto" };
        int exposureModeIdx = env.autoExposure ? 1 : 0;
        if (ImGui::Combo("Exposure Mode", &exposureModeIdx, exposureModeNames, 2)) {
            env.autoExposure = (exposureModeIdx == 1);
            env.autoExposureStartFromManual = env.autoExposure;
            revisions.mark(SceneChange::RenderSettings);
        }
        UiProbe::item("scene.exposure.mode", 0);

        if (!env.autoExposure) {
            // Manual Exposure (EV slider and linear multiplier)
            if (ImGui::SliderFloat("Exposure (EV)", &env.exposureEV, -4.0f, 4.0f, "%.2f EV")) {
                float linearExposure = std::pow(2.0f, env.exposureEV);
                if (commandGenerator != nullptr) {
                    commandGenerator->setExposure(linearExposure);
                }
                revisions.mark(SceneChange::RenderSettings);
            }
        } else {
            if (ImGui::SliderFloat("EV Compensation", &env.exposureCompensationEV, -4.0f, 4.0f, "%+.2f EV")) {
                revisions.mark(SceneChange::RenderSettings);
            }
            if (ImGui::SliderFloat("Min Exposure (EV)", &env.autoExposureMinEV, -10.0f, 0.0f, "%.2f EV")) {
                if (env.autoExposureMinEV > env.autoExposureMaxEV) {
                    env.autoExposureMaxEV = env.autoExposureMinEV;
                }
                revisions.mark(SceneChange::RenderSettings);
            }
            if (ImGui::SliderFloat("Max Exposure (EV)", &env.autoExposureMaxEV, 0.0f, 10.0f, "%.2f EV")) {
                if (env.autoExposureMaxEV < env.autoExposureMinEV) {
                    env.autoExposureMinEV = env.autoExposureMaxEV;
                }
                revisions.mark(SceneChange::RenderSettings);
            }
            if (ImGui::SliderFloat("Adapt Speed Up", &env.autoExposureSpeedUp, 0.2f, 20.0f, "%.1f /s")) {
                revisions.mark(SceneChange::RenderSettings);
            }
            if (ImGui::SliderFloat("Adapt Speed Down", &env.autoExposureSpeedDown, 0.2f, 20.0f, "%.1f /s")) {
                revisions.mark(SceneChange::RenderSettings);
            }
        }

        ImGui::Separator();

        // Physically based bloom (Jimenez 2014)
        if (ImGui::Checkbox("Bloom", &env.bloomEnabled)) {
            revisions.mark(SceneChange::RenderSettings);
        }
        UiProbe::item("scene.bloom.on", 0);
        if (env.bloomEnabled) {
            if (ImGui::SliderFloat("Bloom Intensity", &env.bloomIntensity, 0.0f, 0.5f, "%.3f")) {
                revisions.mark(SceneChange::RenderSettings);
            }
            UiProbe::item("scene.bloom.intensity", 0);
            if (ImGui::SliderFloat("Bloom Radius", &env.bloomRadius, 0.25f, 3.0f, "%.2f")) {
                revisions.mark(SceneChange::RenderSettings);
            }
            UiProbe::item("scene.bloom.radius", 0);
        }

        ImGui::Separator();

        // MSFS Emissive Scale & Time of Day
        if (ImGui::DragFloat("MSFS Emissive: 1 unit = x", &env.msfsEmissiveScale, 0.0001f, 0.0f, 10.0f, "%.5f", ImGuiSliderFlags_Logarithmic)) {
            revisions.mark(SceneChange::RenderSettings);
        }
        UiProbe::item("scene.msfs.emissive.scale", 0);

        const char* timeOfDayNames[] = { "Day", "Night" };
        if (ImGui::Combo("Time of Day", &env.timeOfDay, timeOfDayNames, 2)) {
            revisions.mark(SceneChange::RenderSettings);
        }
        UiProbe::item("scene.time_of_day", 0);

        renderSun(env, gltfLayer, revisions);

        ImGui::Separator();

        // IBL toggle
        if (ImGui::Checkbox("Image Based Lighting (IBL)", &env.iblEnabled)) {
            if (commandGenerator != nullptr) {
                commandGenerator->setIBLEnabled(env.iblEnabled);
            }
            revisions.mark(SceneChange::RenderSettings);
        }
        UiProbe::item("scene.ibl.on", 0);

        // Punctual lights toggle
        if (ImGui::Checkbox("Punctual / Analytical Lights", &env.punctualLightsEnabled)) {
            if (commandGenerator != nullptr) {
                commandGenerator->setPunctualLightsEnabled(env.punctualLightsEnabled);
            }
            revisions.mark(SceneChange::RenderSettings);
        }
        UiProbe::item("scene.punctual.on", 0);

        // IBL filter settings live with the scene environment now, but they configure the
        // one filtered environment every viewport draws, so they apply globally. They only
        // matter while IBL is on, so the panel is hidden when it is off.
        if (env.iblEnabled) {
            ImGui::SeparatorText("IBL Filter Settings");
            UiProbe::item("scene.ibl.filter", 0);
            ImGui::TextDisabled("Applies to every viewport.");
            RenderQuality::drawIblSettings(gltfLayer, revisions);
        }
    }
}

} // namespace ux3d::imgui
