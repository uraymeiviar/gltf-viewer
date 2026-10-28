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
#include <algorithm>
#include <cmath>
#include <string>
#include <cstring>

namespace ux3d::imgui {

// A3.1: Sun & Time of Day -> Sun. Conventions in SceneEnvironmentState (SceneEnvironment.h).
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

void SceneInspector::renderTimeOfDay(SceneEnvironmentState& env, SceneRevisions& revisions)
{
    TimeOfDayState& clock = env.clock;
    ImGui::SeparatorText(ICON_MDI_CLOCK_OUTLINE " Time of Day");
    if (ImGui::Checkbox("Sun Follows Time of Day", &clock.enabled)) {
        revisions.mark(SceneChange::RenderSettings);
    }
    UiProbe::item("scene.tod.enabled", 0);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Sun source: off = the Manual sun above; on = the sun is on and follows the date, time,\n"
                          "place and heading below. Colour and intensity stay the Manual sun's; the sun fades out\n"
                          "between -1 and +1 degrees of elevation.");
    }

    const TimeOfDayEvaluation tod = evaluateTimeOfDay(clock);
    bool edited = false;

    int date[3] = { clock.year, clock.month, clock.day };
    if (ImGui::DragInt3("Date (Y M D)", date, 0.1f, 1, 2100)) {
        date[0] = std::clamp(date[0], 1900, 2100);
        date[1] = std::clamp(date[1], 1, 12);
        date[2] = std::clamp(date[2], 1, solar::daysInMonth(date[0], date[1]));
        clock.year = date[0];
        clock.month = date[1];
        clock.day = date[2];
        edited = true;
    }
    UiProbe::item("scene.tod.date", 0);

    float hours = static_cast<float>(tod.localHours);
    if (ImGui::SliderFloat("Time", &hours, 0.0f, 24.0f, "%.3f h")) {
        clock.hours = std::min<double>(hours, 23.9999);
        clock.preset = static_cast<int>(TimePreset::Custom);
        edited = true;
    }
    UiProbe::item("scene.tod.time", 0);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", formatTime(tod.localHours).c_str());

    static const char* const presetLabels[] = { "Dawn", "Morning", "Noon", "Afternoon", "Dusk", "Night" };
    for (int i = 0; i < 6; ++i) {
        const bool active = clock.preset == i + 1;
        if (i > 0) ImGui::SameLine();
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::SmallButton(presetLabels[i])) {
            clock.preset = i + 1;
            edited = true;
        }
        if (active) ImGui::PopStyleColor();
        UiProbe::item((std::string("scene.tod.preset.") + kTimePresetNames[static_cast<size_t>(i + 1)]).c_str(), 0);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Presets set the time for the date and place: dawn and dusk when the sun is 3 degrees up,\n"
                          "noon at solar transit, night at solar midnight, morning and afternoon halfway.");
    }

    if (ImGui::Checkbox("Play", &clock.playing)) {
        if (clock.playing) materializePreset(clock);
        edited = true;
    }
    UiProbe::item("scene.tod.play", 0);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::DragFloat("Speed", &clock.timeScale, 1.0f, 0.0f, 100000.0f, "%.0f x", ImGuiSliderFlags_Logarithmic)) {
        edited = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Simulated seconds per real second (600 = ten minutes per second).");
    }

    if (ImGui::DragFloat("UTC Offset", &clock.utcOffsetHours, 0.25f, -12.0f, 14.0f, "%.2f h")) edited = true;
    if (ImGui::DragFloat("Latitude", &clock.latitude, 0.05f, -90.0f, 90.0f, "%.4f deg")) edited = true;
    if (ImGui::DragFloat("Longitude", &clock.longitude, 0.05f, -180.0f, 180.0f, "%.4f deg")) edited = true;
    if (ImGui::SliderFloat("Heading", &clock.heading, 0.0f, 360.0f, "%.1f deg")) edited = true;
    UiProbe::item("scene.tod.heading", 0);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("True compass direction the model's nose points to (0 = north, 90 = east).\n"
                          "MSFS aircraft have their nose on glTF +Z and their left side on +X.");
    }
    if (ImGui::Checkbox("Atmospheric Refraction", &clock.refraction)) edited = true;
    if (ImGui::DragFloat("Night Below", &clock.nightThreshold, 0.1f, -90.0f, 90.0f, "%.1f deg")) edited = true;
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Emissive Day/Night = Automatic: night while the sun's elevation is below this (-6 = civil twilight).");
    }

    ImGui::TextDisabled("Sun: azimuth %.2f deg (true), elevation %.2f deg (%.2f apparent)", tod.sun.azimuth,
                        tod.sun.elevation, tod.sun.apparentElevation);
    ImGui::TextDisabled("In the model: azimuth %.1f deg, light %.0f%%%s", tod.gliderAzimuth, tod.fade * 100.0f,
                        tod.night ? ", night" : "");
    double windowStart = 0.0;
    TimeOfDayState local = clock;
    local.hours = tod.localHours;
    local.preset = static_cast<int>(TimePreset::Custom);
    local.year = tod.year;
    local.month = tod.month;
    local.day = tod.day;
    const solar::SunDay events = timeOfDaySunDay(local, windowStart);
    if (events.hasSunrise && events.hasSunset) {
        ImGui::TextDisabled("Sunrise %s, sunset %s", formatTime(hoursSince(events.sunrise, windowStart)).c_str(),
                            formatTime(hoursSince(events.sunset, windowStart)).c_str());
    } else {
        ImGui::TextDisabled("%s", events.alwaysUp ? "The sun does not set on this date." : "The sun does not rise on this date.");
    }
    ImGui::TextDisabled("Moon: azimuth %.1f deg, elevation %.1f deg, %.0f%% lit (not rendered yet)", tod.moon.horizontal.azimuth,
                        tod.moon.horizontal.apparentElevation, tod.moon.illuminatedFraction * 100.0);
    if (edited) {
        normalizeClock(clock);
        revisions.mark(SceneChange::RenderSettings);
    }
}

void SceneInspector::render(SceneEnvironmentState& env,
                            ux3d::renderer::CommandGenerator* commandGenerator,
                            ux3d::renderer::GltfRenderLayer* gltfLayer,
                            SceneRevisions& revisions,
                            ux3d::gltf::Scene* scene,
                            const SkyStatus* sky)
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
                    // A6.2: loading a panorama selects it as the environment source.
                    env.environmentSource = static_cast<int>(EnvironmentSource::Hdr);
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

        const char* timeOfDayNames[] = { "Day", "Night", "Automatic" };
        if (ImGui::Combo("Emissive Day/Night", &env.timeOfDay, timeOfDayNames, 3)) {
            revisions.mark(SceneChange::RenderSettings);
        }
        UiProbe::item("scene.time_of_day", 0);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Which ASOBO_material_emissive multiplier applies. Automatic follows the sun's elevation\n"
                              "from the Time of Day clock (night below the threshold there).");
        }

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

    // Sun and time of day get their own header below Environment, so the environment controls
    // keep their place in the panel.
    if (ImGui::CollapsingHeader("Sun & Time of Day", ImGuiTreeNodeFlags_DefaultOpen)) {
        renderSun(env, gltfLayer, revisions);
        renderTimeOfDay(env, revisions);
    }

    // A6.2 / A6.3: after the existing headers (collapsed by default), so their controls keep their places.
    const bool skyOpen = ImGui::CollapsingHeader("Sky & Atmosphere");
    UiProbe::item("scene.sky", 0);
    if (skyOpen) {
        renderSky(env, sky, revisions);
    }
}

void SceneInspector::renderSky(SceneEnvironmentState& env, const SkyStatus* sky, SceneRevisions& revisions)
{
    const char* sourceNames[] = { "HDR panorama", "Procedural sky" };
    if (ImGui::Combo("Environment Source", &env.environmentSource, sourceNames, 2)) {
        revisions.mark(SceneChange::RenderSettings);
    }
    UiProbe::item("scene.sky.source", 0);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Procedural sky: a physically based atmosphere (Hillaire 2020) as the background and the\n"
                          "source of the image-based lighting, re-filtered as the sun moves. The HDR stays available.");
    }
    if (env.environmentSource != static_cast<int>(EnvironmentSource::ProceduralSky)) {
        return;
    }

    if (sky != nullptr && sky->active) {
        const ImVec4 green(0.13f, 0.67f, 0.42f, 1.0f), amber(1.0f, 0.8f, 0.2f, 1.0f);
        switch (sky->state) {
        case SkyRefineState::Converged:
            ImGui::TextColored(green, ICON_MDI_CHECK " Lighting converged (%u px cubes)", sky->cubeSize);
            break;
        case SkyRefineState::Updating:
            ImGui::TextColored(amber, ICON_MDI_REFRESH " Updating lighting (%u px, %.0f%%)", sky->cubeSize, sky->progress * 100.0f);
            break;
        case SkyRefineState::WaitingToRefine:
            ImGui::TextColored(amber, ICON_MDI_REFRESH " Reduced lighting (%u px), refining soon", sky->cubeSize);
            break;
        case SkyRefineState::Refining:
            ImGui::TextColored(amber, ICON_MDI_REFRESH " Refining lighting to %u px: %.0f%%", sky->targetSize, sky->progress * 100.0f);
            break;
        default:
            break;
        }
        UiProbe::item("scene.sky.refine", 0);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("While the sky changes, the IBL cubes are re-filtered at a reduced size, spread over frames,\n"
                              "and swapped in when complete; once it is still they are refined to the quality preset's sizes.\n"
                              "Extra GPU time per frame: last %.2f ms, max %.2f ms (aim %.1f ms).",
                              sky->gpuMsLast, sky->gpuMsMax, sky->budgetMs);
        }
    } else {
        ImGui::TextDisabled("Starting the procedural sky...");
    }

    if (ImGui::SliderFloat("Mie Density", &env.skyMieDensity, 0.0f, 10.0f, "%.2f x", ImGuiSliderFlags_Logarithmic)) {
        revisions.mark(SceneChange::RenderSettings);
    }
    UiProbe::item("scene.sky.mie", 0);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Aerosol (haze) density relative to a clear sky: larger values whiten the sky and widen the glow\n"
                          "around the sun.");
    }
    if (ImGui::Checkbox("Ozone", &env.skyOzone)) {
        revisions.mark(SceneChange::RenderSettings);
    }
    UiProbe::item("scene.sky.ozone", 0);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Ozone absorption keeps the twilight sky blue overhead.");
    }
    bool overrideAlbedo = env.skyGroundAlbedo >= 0.0f;
    if (ImGui::Checkbox("Override Ground Albedo", &overrideAlbedo)) {
        env.skyGroundAlbedo = overrideAlbedo ? (sky != nullptr ? sky->groundAlbedo.g : sky::kNeutralGroundAlbedo) : -1.0f;
        revisions.mark(SceneChange::RenderSettings);
    }
    UiProbe::item("scene.sky.ground_albedo", 0);
    if (overrideAlbedo) {
        if (ImGui::SliderFloat("Ground Albedo", &env.skyGroundAlbedo, 0.0f, 1.0f, "%.2f")) {
            revisions.mark(SceneChange::RenderSettings);
        }
    } else if (sky != nullptr) {
        ImGui::TextDisabled("Ground albedo %.2f %.2f %.2f (%s)", sky->groundAlbedo.r, sky->groundAlbedo.g, sky->groundAlbedo.b,
                            sky->groundFromPlane ? "the viewport's ground plane" : "neutral, no ground plane");
    }
    ImGui::TextDisabled("Sun: always on, from the Time of Day clock or the Sun angles;");
    ImGui::TextDisabled("its colour and intensity come from the atmosphere.");
}

} // namespace ux3d::imgui
