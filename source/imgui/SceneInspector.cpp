// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/SceneInspector.h"
#include "imgui/RenderQuality.h"
#include "imgui/OceanSimulation.h"
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

namespace {
template <typename Widget> bool probed(const char* tag, Widget&& widget) {
    const bool changed = widget(); UiProbe::item(tag, 0); return changed;
}
void renderSceneGround(SceneEnvironmentState& env, SceneRevisions& revisions) {
    ImGui::PushID("scene_ground");
    // A3.6: the ground. Grid is the editor grid overlay; Plane is an infinite surface in the scene.
    ImGui::Separator();
    const bool groundOpen = ImGui::CollapsingHeader("Ground", ImGuiTreeNodeFlags_DefaultOpen);
    UiProbe::item("scene.ground",0);
    if (groundOpen) {
        GroundSettings& ground = env.ground;
        const char* groundNames[] = { "Off", "Plane", "Ocean" };
        if (probed("scene.ground.type", [&] { return ImGui::BeginCombo("Type", groundNames[static_cast<int>(ground.mode)]); })) {
            for (int i = 0; i < 3; ++i) {
                // A6.8: the ocean needs compute shaders (OpenGL 4.3).
                const bool unavailable = i == static_cast<int>(GroundMode::Ocean) && !oceanAvailable();
                ImGui::BeginDisabled(unavailable);
                if (ImGui::Selectable(groundNames[i], static_cast<int>(ground.mode) == i)) {
                    ground.mode = static_cast<GroundMode>(i);
                    revisions.mark(SceneChange::Environment);
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
            if (probed("scene.ground.height", [&] { return ImGui::Combo("Height", &heightMode, heightNames, IM_ARRAYSIZE(heightNames)); })) {
                ground.heightMode = static_cast<GroundHeightMode>(heightMode);
                revisions.mark(SceneChange::Environment);
            }
            if (ground.heightMode == GroundHeightMode::Custom) {
                if (probed("scene.ground.custom_height", [&] { return ImGui::DragFloat("Height (scene units)", &ground.customHeight, 0.01f, -1.0e6f, 1.0e6f, "%.3f"); })) {
                    revisions.mark(SceneChange::Environment);
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
            // Settings scopes: the simulation quality is renderer-wide (File > Settings > Quality); shown here for reference.
            {
                const char* qualityNames[] = { "Low", "Medium", "High" };
                probed("scene.ocean.quality", [&] {
                    ImGui::TextDisabled("Quality: %s (File > Settings > Quality, all viewports)",
                                        qualityNames[std::clamp(static_cast<int>(ocean.quality), 0, 2)]);
                    return false;
                });
            }
            if (probed("scene.ocean.wind_speed", [&] { return ImGui::SliderFloat("Wind speed (m/s)##oceanWindSpeed", &ocean.windSpeed, 0.5f, 40.0f, "%.1f"); })) {
                revisions.mark(SceneChange::Environment);
            }
            if (probed("scene.ocean.wind_dir", [&] { return ImGui::SliderFloat("Wind from (deg)##oceanWindDirection", &ocean.windDirection, 0.0f, 360.0f, "%.0f"); })) {
                revisions.mark(SceneChange::Environment);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("The direction the wind comes from, around +Y from +Z towards +X (the sun's azimuth). The waves travel the other way.");
            }
            if (probed("scene.ocean.choppiness", [&] { return ImGui::SliderFloat("Choppiness##oceanChoppiness", &ocean.choppiness, 0.0f, 2.0f, "%.2f"); })) {
                revisions.mark(SceneChange::Environment);
            }
            if (probed("scene.ocean.swell", [&] { return ImGui::SliderFloat("Swell##oceanSwell", &ocean.swell, 0.0f, 1.0f, "%.2f"); })) {
                revisions.mark(SceneChange::Environment);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Narrows the directional spread of the waves (long, parallel swell).");
            }
            if (probed("scene.ocean.foam", [&] { return ImGui::SliderFloat("Foam amount##oceanFoam", &ocean.foam, 0.0f, 1.0f, "%.2f"); })) {
                revisions.mark(SceneChange::Environment);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Whitecaps. 0.5 is the coverage observed at this wind speed (about 1%% at 10 m/s, 10%% at 20 m/s); 1 is four times that.");
            }
            if (probed("scene.ocean.color", [&] { return ImGui::ColorEdit3("Water colour##oceanColor", &ocean.color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR); })) {
                revisions.mark(SceneChange::Environment);
            }
            ImGui::BeginDisabled(ocean.live);
            if (probed("scene.ocean.period", [&] { return ImGui::DragFloat("Loop period (s)##oceanPeriod", &ocean.period, 0.5f, 5.0f, 120.0f, "%.1f"); })) {
                ocean.period = std::clamp(ocean.period, 5.0f, 120.0f);
                revisions.mark(SceneChange::Environment);
            }
            ImGui::EndDisabled();
            if (probed("scene.ocean.fetch", [&] { return ImGui::DragFloat("Fetch (m)##oceanFetch", &ocean.fetch, 100.0f, 1000.0f, 1.0e7f, "%.0f", ImGuiSliderFlags_Logarithmic); })) {
                ocean.fetch = std::clamp(ocean.fetch, 1000.0f, 1.0e7f);
                revisions.mark(SceneChange::Environment);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Distance the wind has blown over. Beyond full development (about 22000 x U^2 / g) it changes nothing.");
            }

            ImGui::SameLine();
            if (probed("scene.ocean.reset", [&] { return ImGui::Button("Reset ocean##oceanReset"); })) {
                ocean = OceanSettings();
                revisions.mark(SceneChange::Environment);
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
            if (probed("scene.ground.material", [&] { return ImGui::BeginCombo("Material", presetLabel); })) {
                for (size_t i = 0; i < groundPresets().size(); ++i) {
                    if (ImGui::Selectable(groundPresets()[i].label.data(), static_cast<size_t>(preset) == i)) {
                        ground.applyPreset(static_cast<GroundPreset>(i));
                        revisions.mark(SceneChange::Environment);
                    }
                }
                ImGui::EndCombo();
            }
            if (probed("scene.ground.albedo", [&] { return ImGui::ColorEdit3("Albedo", &ground.albedo.x, ImGuiColorEditFlags_Float); })) {
                ground.preset = GroundPreset::Custom;
                revisions.mark(SceneChange::Environment);
            }
            if (probed("scene.ground.roughness", [&] { return ImGui::SliderFloat("Roughness", &ground.roughness, 0.0f, 1.0f, "%.2f"); })) {
                ground.preset = GroundPreset::Custom;
                revisions.mark(SceneChange::Environment);
            }
            if (probed("scene.ground.detail", [&] { return ImGui::SliderFloat("Detail", &ground.detail, 0.0f, 1.0f, "%.2f"); })) {
                ground.preset = GroundPreset::Custom;
                revisions.mark(SceneChange::Environment);
            }
            if (probed("scene.ground.detail_size", [&] { return ImGui::DragFloat("Detail size", &ground.detailScale, 0.01f, 0.0001f, 1.0e6f, "%.3f", ImGuiSliderFlags_Logarithmic); })) {
                ground.preset = GroundPreset::Custom;
                revisions.mark(SceneChange::Environment);
            }
            if (probed("scene.ground.joints", [&] { return ImGui::DragFloat("Joint spacing", &ground.jointSpacing, 0.05f, 0.0f, 1.0e6f, ground.jointSpacing > 0.0f ? "%.2f" : "None"); })) {
                ground.preset = GroundPreset::Custom;
                revisions.mark(SceneChange::Environment);
            }
            if (probed("scene.ground.haze", [&] { return ImGui::DragFloat("Haze distance", &ground.haze, 1.0f, 0.0f, 1.0e9f, ground.haze > 0.0f ? "%.0f" : "Auto", ImGuiSliderFlags_Logarithmic); })) {
                revisions.mark(SceneChange::Environment);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Distance at which the plane has faded 98%% into the horizon colour. 0 = automatic (0.9 x far plane, at least 100 x the camera height).");
            }
            ImGui::TextDisabled("Lit by the environment (its own lower hemisphere) and the sun with shadows.");
        }
    }

    ImGui::PopID();
}
}

// A3.1: Sun & Time of Day -> Sun. Conventions in SceneEnvironmentState (SceneEnvironment.h).
void SceneInspector::renderSun(SceneEnvironmentState& env, ux3d::renderer::GltfRenderLayer* gltfLayer,
                               SceneRevisions& revisions, const EffectiveSun* sun)
{
    ImGui::SeparatorText(ICON_MDI_WHITE_BALANCE_SUNNY " Sun");
    const bool procedural = env.environmentSource == static_cast<int>(EnvironmentSource::ProceduralSky);
    const SunSource source = effectiveSunSource(env);
    int choice = procedural ? (source == SunSource::Clock ? 1 : 0) : static_cast<int>(source);
    if (ImGui::Combo("Sun Source##sun_source", &choice, procedural ? "Manual\0Time of Day\0" : "Off\0From HDR\0Manual\0Time of Day\0")) {
        selectSunSource(env, procedural ? (choice == 1 ? SunSource::Clock : SunSource::Manual) : static_cast<SunSource>(choice));
        revisions.mark(SceneChange::Environment);
    }
    UiProbe::item("scene.sun.source", 0);
    if (procedural) ImGui::TextDisabled("Colour and intensity come from the atmosphere.");
    if (source == SunSource::Off) return;
    if (sun != nullptr) {
        float azimuth = 0.0f, elevation = 0.0f;
        sunAnglesFromDirection(sun->direction, azimuth, elevation);
        ImGui::TextDisabled("Effective: az %.1f, el %.1f deg", azimuth, elevation);
    }

    const ux3d::renderer::EnvironmentSun* found = gltfLayer != nullptr ? &gltfLayer->getEnvironmentSun() : nullptr;
    if (source == SunSource::FromHdr) {
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
                selectSunSource(env, SunSource::Manual);
                revisions.mark(SceneChange::Environment);
            }
        }
    } else {
        if (source == SunSource::Manual) {
            if (ImGui::SliderFloat("Azimuth", &env.sunAzimuth, 0.0f, 360.0f, "%.1f deg")) {
                revisions.mark(SceneChange::Environment);
            }
            UiProbe::item("scene.sun.azimuth", 0);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Around +Y, from +Z towards +X (glTF Y-up). 0 = sun towards +Z.");
            }
            if (ImGui::SliderFloat("Elevation", &env.sunElevation, -90.0f, 90.0f, "%.1f deg")) {
                revisions.mark(SceneChange::Environment);
            }
            UiProbe::item("scene.sun.elevation", 0);
        }
        if (!procedural) {
            float colour[3] = { env.sunColor.r, env.sunColor.g, env.sunColor.b };
            if (ImGui::ColorEdit3("Sun Colour", colour, ImGuiColorEditFlags_Float)) {
                env.sunColor = glm::vec3(colour[0], colour[1], colour[2]);
                revisions.mark(SceneChange::Environment);
            }
            static float kelvin = 5778.0f;
            if (ImGui::DragFloat("Kelvin", &kelvin, 10.0f, 1667.0f, 25000.0f, "%.0f K")) {
                env.sunColor = kelvinToLinearRgb(kelvin);
                revisions.mark(SceneChange::Environment);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Sets the colour to a black body at this temperature (luminance 1).");
            }
            if (ImGui::DragFloat("Intensity", &env.sunIntensity, 0.05f, 0.0f, 100000.0f, "%.3f", ImGuiSliderFlags_Logarithmic)) {
                revisions.mark(SceneChange::Environment);
            }
            UiProbe::item("scene.sun.intensity", 0);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Illuminance in the environment's units (HDR radiance x sr).\n"
                                  "A white Lambert surface facing the sun reflects colour x intensity / pi.");
            }
        }
        if (source == SunSource::Manual && !procedural && found != nullptr && found->found && ImGui::SmallButton("Use HDR sun direction")) {
            const glm::mat3 rotation = ux3d::renderer::CommandGenerator::environmentRotationMatrix(env.hdrRotation);
            sunAnglesFromDirection(glm::transpose(rotation) * found->direction, env.sunAzimuth, env.sunElevation);
            revisions.mark(SceneChange::Environment);
        }
    }
    if (ImGui::SliderFloat("Angular Radius", &env.sunAngularRadius, 0.0f, 5.0f, "%.2f deg")) {
        revisions.mark(SceneChange::Environment);
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
    if (effectiveSunSource(env) != SunSource::Clock) ImGui::TextDisabled("The clock does not move the selected sun source.");
    if (ImGui::Checkbox("Moon & Stars Clock##clock_enabled", &clock.enabled)) revisions.mark(SceneChange::Environment);
    UiProbe::item("scene.tod.clock", 0);

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
        revisions.mark(SceneChange::Environment);
    }
}

void SceneInspector::render(SceneEnvironmentState& env,
                            ux3d::renderer::CommandGenerator* commandGenerator,
                            ux3d::renderer::GltfRenderLayer* gltfLayer,
                            SceneRevisions& revisions,
                            ux3d::gltf::Scene* scene,
                            const SkyStatus* sky,
                            const EffectiveSun* sun,
                            float outdoorIlluminance)
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

    const bool environmentOpen = ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen);
    UiProbe::item("scene.environment", 0);
    if (environmentOpen) {
        // Show background skybox toggle
        if (ImGui::Checkbox("Background", &env.showBackground)) {
            if (gltfLayer != nullptr) {
                gltfLayer->enableCubeMap(env.showBackground);
            }
            // The background switches the layer's cube map and gates the environment load: renderer sync (C2).
            revisions.mark(SceneChange::RenderSettings);
        }

        UiProbe::item("scene.background", 0);
        // Clear color (used when skybox is disabled)
        if (!env.showBackground) {
            float clr[4] = { env.clearColor.r, env.clearColor.g, env.clearColor.b, env.clearColor.a };
            if (ImGui::ColorEdit4("Clear Color", clr)) {
                env.clearColor = glm::vec4(clr[0], clr[1], clr[2], clr[3]);
                revisions.mark(SceneChange::Environment);
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
                    if (commandGenerator != nullptr) {
                        commandGenerator->setIBLEnabled(RenderQuality::allowIbl());
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
            revisions.mark(SceneChange::Environment);
        }

        // HDR Intensity
        if (ImGui::SliderFloat("HDR Intensity", &env.hdrIntensity, 0.0f, 10.0f, "%.2f")) {
            if (commandGenerator != nullptr) {
                commandGenerator->setEnvironmentMapIntensity(env.hdrIntensity);
            }
            revisions.mark(SceneChange::Environment);
        }

        // HDR Rotation
        if (ImGui::SliderFloat("HDR Rotation", &env.hdrRotation, 0.0f, 360.0f, "%.1f deg")) {
            if (commandGenerator != nullptr) {
                commandGenerator->setEnvironmentMapRotation(env.hdrRotation);
            }
            revisions.mark(SceneChange::Environment);
        }

        ImGui::Separator();

        // Tone Mapping
        const char* toneMappingNames[] = {
            "Khronos PBR Neutral",
            "ACES Hill",
            "ACES Narkowicz"
        };
        // A6.4: an unset choice shows the source's default (ACES Hill with the HDR, PBR Neutral with the sky).
        int toneMappingIdx = effectiveToneMapping(env);
        if (ImGui::Combo("Tone Mapping", &toneMappingIdx, toneMappingNames, 3)) {
            env.toneMappingIdx = toneMappingIdx;
            if (commandGenerator != nullptr) {
                if (env.toneMappingIdx == 0) {
                    commandGenerator->setToneMapping(ux3d::renderer::ToneMappingsEnum::PBR_NEUTRAL);
                } else if (env.toneMappingIdx == 1) {
                    commandGenerator->setToneMapping(ux3d::renderer::ToneMappingsEnum::ACES_HILL);
                } else if (env.toneMappingIdx == 2) {
                    commandGenerator->setToneMapping(ux3d::renderer::ToneMappingsEnum::ACES_NARKOWICZ);
                }
            }
            revisions.mark(SceneChange::Environment);
        }

        // Exposure mode (Manual / Auto / Time-of-day curve, A6.4) and controls. An unset mode shows the source's
        // default (Manual with the HDR, Auto with the procedural sky).
        const char* exposureModeNames[] = { "Manual", "Auto (eye adaptation)", "Time-of-day curve" };
        const ExposureMode effectiveMode = effectiveExposureMode(env);
        int exposureModeIdx = static_cast<int>(effectiveMode);
        if (ImGui::Combo("Exposure Mode", &exposureModeIdx, exposureModeNames, 3)) {
            env.exposureMode = exposureModeIdx;
            env.autoExposureStartFromManual = exposureModeIdx == static_cast<int>(ExposureMode::Auto);
            revisions.mark(SceneChange::Environment);
        }
        UiProbe::item("scene.exposure.mode", 0);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Manual: the EV below. Auto: metered eye adaptation (with the procedural sky in EV100 with a key\n"
                              "curve that keeps night darker than day). Time-of-day curve: the procedural sky's exposure from\n"
                              "the sun, sky, moon and night glow illuminance (Manual with the HDR). Default: Manual with the\n"
                              "HDR, Auto with the procedural sky.");
        }
        const bool physicalExposure = env.environmentSource == static_cast<int>(EnvironmentSource::ProceduralSky) &&
                                      effectiveMode != ExposureMode::Manual;
        if (physicalExposure) {
            if (ImGui::SliderFloat("EV Compensation", &env.exposureCompensationEV, -4.0f, 4.0f, "%+.2f EV")) {
                revisions.mark(SceneChange::Environment);
            }
            UiProbe::item("scene.exposure.compensation", 0);
            if (ImGui::SliderFloat("Darkest Scene (EV100)", &env.exposureMinEV100, -20.0f, 10.0f, "%.1f")) {
                env.exposureMaxEV100 = (std::max)(env.exposureMaxEV100, env.exposureMinEV100);
                revisions.mark(SceneChange::Environment);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Scenes darker than this are not exposed further (-12: a starlit night, about 0.001 lux).\n"
                                  "Glider EV = 13.38 - EV100 + the key curve.");
            }
            if (ImGui::SliderFloat("Brightest Scene (EV100)", &env.exposureMaxEV100, 0.0f, 25.0f, "%.1f")) {
                env.exposureMinEV100 = (std::min)(env.exposureMinEV100, env.exposureMaxEV100);
                revisions.mark(SceneChange::Environment);
            }
            if (effectiveMode == ExposureMode::Auto) {
                // A6.4b: the physically based speeds are the scene brightness going up (light adaptation) / down.
                if (ImGui::SliderFloat("Adapt to Brighter", &env.autoExposureSpeedUp, 0.2f, 20.0f, "%.1f /s")) {
                    revisions.mark(SceneChange::Environment);
                }
                if (ImGui::SliderFloat("Adapt to Darker", &env.autoExposureSpeedDown, 0.2f, 20.0f, "%.1f /s")) {
                    revisions.mark(SceneChange::Environment);
                }
                // A6.4b metering (NightSky.h), after the existing controls.
                const char* meteringNames[] = { "Average", "Average + highlights" };
                int metering = std::clamp(env.exposureMetering, 0, 1);
                if (ImGui::Combo("Metering", &metering, meteringNames, 2)) {
                    env.exposureMetering = metering;
                    revisions.mark(SceneChange::Environment);
                }
                UiProbe::item("scene.exposure.metering", 0);
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Average: trimmed centre-weighted log average with the key curve.\n"
                                      "Average + highlights (default): the darker of the average and an exposure that keeps the\n"
                                      "brightest lit surfaces (0.3%% of the image: screens, panels) below the tone mapper's shoulder.\n"
                                      "The sky, the moon, the stars and single lights do not count.");
                }
            }
        } else if (effectiveMode != ExposureMode::Auto) {
            // Manual Exposure (EV slider and linear multiplier)
            if (ImGui::SliderFloat("Exposure (EV)", &env.exposureEV, -4.0f, 4.0f, "%.2f EV")) {
                float linearExposure = std::pow(2.0f, env.exposureEV);
                if (commandGenerator != nullptr) {
                    commandGenerator->setExposure(linearExposure);
                }
                revisions.mark(SceneChange::Environment);
            }
        } else {
            if (ImGui::SliderFloat("EV Compensation", &env.exposureCompensationEV, -4.0f, 4.0f, "%+.2f EV")) {
                revisions.mark(SceneChange::Environment);
            }
            if (ImGui::SliderFloat("Min Exposure (EV)", &env.autoExposureMinEV, -10.0f, 0.0f, "%.2f EV")) {
                if (env.autoExposureMinEV > env.autoExposureMaxEV) {
                    env.autoExposureMaxEV = env.autoExposureMinEV;
                }
                revisions.mark(SceneChange::Environment);
            }
            if (ImGui::SliderFloat("Max Exposure (EV)", &env.autoExposureMaxEV, 0.0f, 10.0f, "%.2f EV")) {
                if (env.autoExposureMaxEV < env.autoExposureMinEV) {
                    env.autoExposureMinEV = env.autoExposureMaxEV;
                }
                revisions.mark(SceneChange::Environment);
            }
            if (ImGui::SliderFloat("Adapt Speed Up", &env.autoExposureSpeedUp, 0.2f, 20.0f, "%.1f /s")) {
                revisions.mark(SceneChange::Environment);
            }
            if (ImGui::SliderFloat("Adapt Speed Down", &env.autoExposureSpeedDown, 0.2f, 20.0f, "%.1f /s")) {
                revisions.mark(SceneChange::Environment);
            }
        }

        ImGui::Separator();

        // Physically based bloom (Jimenez 2014)
        { // Bloom appearance stays editable while the Quality level is off.
            if (ImGui::SliderFloat("Bloom Intensity", &env.bloomIntensity, 0.0f, 0.5f, "%.3f")) {
                revisions.mark(SceneChange::Environment);
            }
            UiProbe::item("scene.bloom.intensity", 0);
            if (ImGui::SliderFloat("Bloom Radius", &env.bloomRadius, 0.25f, 3.0f, "%.2f")) {
                revisions.mark(SceneChange::Environment);
            }
            UiProbe::item("scene.bloom.radius", 0);
        }

        ImGui::Separator();

        // MSFS Emissive Scale & Time of Day
        float emissiveScale = effectiveMsfsEmissiveScale(env);
        if (ImGui::DragFloat("MSFS Emissive: 1 unit = x", &emissiveScale, 0.0001f, 0.0f, 10.0f, "%.8f", ImGuiSliderFlags_Logarithmic)) {
            env.msfsEmissiveScale = emissiveScale;
            env.msfsEmissiveScaleOverride = true;
            revisions.mark(SceneChange::Environment);
        }
        UiProbe::item("scene.msfs.emissive.scale", 0);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Effective ASOBO emissive scale. Sky: cd/m^2 / 12800; HDR: 1/1000.\n"
                              "Editing overrides both sources. Right-click to restore source defaults.\n"
                              "Legacy MSFS 2020 units cannot be reliably identified from the glTF.\n"
                              "Core glTF (including KHR emissive_strength) is unchanged.");
        }
        if (ImGui::BeginPopupContextItem("msfs_emissive_scale_default")) {
            if (ImGui::MenuItem("Use source default", nullptr, !env.msfsEmissiveScaleOverride)) {
                env.msfsEmissiveScaleOverride = false;
                revisions.mark(SceneChange::Environment);
            }
            UiProbe::item("scene.msfs.emissive.default", 0);
            ImGui::EndPopup();
        }

        const char* timeOfDayNames[] = { "Day", "Night", "Automatic" };
        if (ImGui::Combo("Emissive Day/Night", &env.timeOfDay, timeOfDayNames, 3)) {
            revisions.mark(SceneChange::Environment);
        }
        UiProbe::item("scene.time_of_day", 0);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Which ASOBO_material_emissive multiplier applies. Automatic follows the sun's elevation\n"
                              "from the Time of Day clock (night below the threshold there).");
        }

        ImGui::Separator();

        // Settings scopes: the IBL filter is quality (renderer-wide) and is edited in File > Settings > Quality.
        // While IBL is on, the scene shows what it gets (the probe tag stays on the heading).
        if (env.iblEnabled) {
            ImGui::SeparatorText("IBL Filter Settings");
            UiProbe::item("scene.ibl.filter", 0);
            const RenderQualitySettings& quality = RenderQuality::settings();
            ImGui::TextDisabled("Cube %u, irradiance %s, LUT %u, %u samples, %s", quality.iblCubeSize,
                                quality.iblIrradianceSize == 0 ? "full" : std::to_string(quality.iblIrradianceSize).c_str(),
                                quality.iblLutSize, quality.iblSpecularSamples, quality.iblHalfFloat ? "16-bit" : "32-bit");
            ImGui::TextDisabled("Quality (all viewports): File > Settings > Quality.");
        }
    }

    // Sun and time of day get their own header below Environment, so the environment controls
    // keep their place in the panel.
    const bool sunTimeOpen = ImGui::CollapsingHeader("Sun & Time of Day", ImGuiTreeNodeFlags_DefaultOpen);
    UiProbe::item("scene.sun_time", 0);
    if (sunTimeOpen) {
        renderSun(env, gltfLayer, revisions, sun);
        renderTimeOfDay(env, revisions);
    }

    // Settings scopes: the scene's effect switches and look parameters (formerly per viewport; the shadow switch was
    // renderer-wide). Every viewport follows them unless its Viewport Settings override shadows, SSR or SSAO.
    renderSceneGround(env, revisions);
    const bool effectsOpen = ImGui::CollapsingHeader("Ambient Occlusion & Aircraft", ImGuiTreeNodeFlags_DefaultOpen);
    UiProbe::item("scene.effects", 0);
    if (effectsOpen) {
        renderEffects(env, gltfLayer, revisions);
    }

    // A6.2 / A6.3: after the existing headers (collapsed by default), so their controls keep their places.
    const bool skyOpen = ImGui::CollapsingHeader("Sky & Atmosphere");
    UiProbe::item("scene.sky", 0);
    if (skyOpen) {
        renderSky(env, sky, revisions);
    }

    // A6.7: after Sky & Atmosphere (collapsed by default), so every existing control keeps its place.
    const bool diskOpen = ImGui::CollapsingHeader("Sun Disk");
    UiProbe::item("scene.sun_disk", 0);
    if (diskOpen) {
        renderSunDisk(env, revisions);
    }

    // A6.6: after Sun Disk (collapsed by default).
    const bool nightOpen = ImGui::CollapsingHeader("Night Sky");
    UiProbe::item("scene.night", 0);
    if (nightOpen) {
        renderNightSky(env, revisions);
    }

    // A3.8: after Night Sky (collapsed by default), so every existing control keeps its place.
    const bool ambientOpen = ImGui::CollapsingHeader("Ambient Light");
    UiProbe::item("scene.ambient", 0);
    if (ambientOpen) {
        renderAmbient(env, revisions, outdoorIlluminance);
    }

    // A3.9: after Ambient Light (collapsed by default), so every existing control keeps its place.
    const bool skyOcclusionOpen = ImGui::CollapsingHeader("Sky Occlusion");
    UiProbe::item("scene.sky_occlusion", 0);
    if (skyOcclusionOpen) {
        renderSkyOcclusion(env, sky, revisions);
    }
}

void SceneInspector::renderEffects(SceneEnvironmentState& env, ux3d::renderer::GltfRenderLayer* gltfLayer,
                                   SceneRevisions& revisions)
{
    SceneEffectSettings& fx = env.effects;
    if (ImGui::DragFloat("AO radius##sceneSsaoRadius", &fx.ssaoRadius, 0.01f, 0.01f, 100.0f, "%.2f",
                         ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic)) {
        revisions.mark(SceneChange::Environment);
    }
    UiProbe::item("scene.effects.ssao_radius", 0);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("World units (metres in MSFS assets): occluders farther away do not count.");
    }
    if (ImGui::SliderFloat("AO intensity##sceneSsaoIntensity", &fx.ssaoIntensity, 0.0f, 4.0f, "%.2f")) {
        revisions.mark(SceneChange::Environment);
    }
    UiProbe::item("scene.effects.ssao_intensity", 0);
    ImGui::SeparatorText("Aircraft state (MSFS)");
    if (ImGui::SliderFloat("Icing##sceneIcing", &fx.windshieldIcing, 0.0f, 1.0f, "%.2f")) {
        revisions.mark(SceneChange::Environment);
    }
    UiProbe::item("scene.effects.icing", 0);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Windshield icing and frosted geometry decals (0 = clean aircraft).");
    }

}

void SceneInspector::renderSkyOcclusion(SceneEnvironmentState& env, const SkyStatus* sky, SceneRevisions& revisions)
{
    ImGui::TextDisabled("Mode and bake quality: File > Settings > Quality.");
    if (ImGui::Checkbox("Environment occluders block the sky", &env.skyOcclusionOccluders)) {
        revisions.mark(SceneChange::Environment);
    }
    UiProbe::item("scene.sky_occlusion.occluders", 0);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("ASOBO_material_environment_occluder geometry (hidden in the image) closes a cabin for the\n"
                          "environment; its sky portals then bring the window light.");
    }
    if (sky != nullptr) {
        if (sky->skyOcclusionBaking) {
            ImGui::TextDisabled("Baking... %.0f%%", sky->skyOcclusionProgress * 100.0f);
        } else if (sky->skyOcclusionActive) {
            ImGui::TextDisabled("Volume %d x %d x %d, cell %.3g, %.1f MiB, baked in %.0f ms", sky->skyOcclusionDims[0],
                                sky->skyOcclusionDims[1], sky->skyOcclusionDims[2], sky->skyOcclusionCell,
                                sky->skyOcclusionMemoryMiB, sky->skyOcclusionBakeMs);
        } else {
            ImGui::TextDisabled("Not in use");
        }
    }
}

void SceneInspector::renderAmbient(SceneEnvironmentState& env, SceneRevisions& revisions, float outdoorIlluminance)
{
    int enabled = env.ambient < 0 ? 0 : env.ambient != 0 ? 1 : 2;
    if (ImGui::Combo("Ambient Light##enabled", &enabled, "Auto (sky on, HDR off)\0On\0Off\0")) {
        env.ambient = enabled == 0 ? -1 : enabled == 1 ? 1 : 0;
        revisions.mark(SceneChange::Environment);
    }
    UiProbe::item("scene.ambient.on", 0);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("A viewer light, not part of the glTF: a uniform radiance from every direction, with the HDR or\n"
                          "the procedural sky. Manual is uniform; Auto estimates outdoor bounce where the sky is hidden.\n"
                          "Authored occlusion and SSAO apply; the background and emissive surfaces are unchanged.");
    }
    ImGui::BeginDisabled(!effectiveAmbientEnabled(env));
    int mode = effectiveAmbientAuto(env) ? 0 : 1;
    if (ImGui::Combo("Intensity Mode##ambient_mode", &mode, "Auto\0Manual\0")) {
        env.ambientMode = mode == 0 ? 1 : 0;
        revisions.mark(SceneChange::Environment);
    }
    UiProbe::item("scene.ambient.mode", 0);
    if (effectiveAmbientAuto(env)) {
        if (ImGui::SliderFloat("Fill Fraction##ambient_fill", &env.ambientFill, 0.0f, 1.0f, "%.4f")) {
            revisions.mark(SceneChange::Environment);
        }
        UiProbe::item("scene.ambient.fill", 0);
        const float luminance = env.ambientFill * outdoorIlluminance / 3.14159265358979323846f;
        ImGui::TextDisabled("Outdoor %.4g lux; fill %.4g cd/m^2", outdoorIlluminance * kAmbientUnitLuminance,
                            luminance * kAmbientUnitLuminance);
        ImGui::TextDisabled("White fill where sky is hidden; uniform without a volume.");
    } else {
        if (ImGui::ColorEdit3("Ambient Color", &env.ambientColor.x, ImGuiColorEditFlags_Float)) {
            env.ambientColor = (glm::max)(env.ambientColor, glm::vec3(0.0f));
            revisions.mark(SceneChange::Environment);
        }
        UiProbe::item("scene.ambient.color", 0);
        if (ImGui::SliderFloat("Ambient Intensity", &env.ambientLuminance, 0.0f, kAmbientMaxLuminance, "%.3g cd/m^2",
                               ImGuiSliderFlags_Logarithmic)) {
            revisions.mark(SceneChange::Environment);
        }
        UiProbe::item("scene.ambient.intensity", 0);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Luminance of the ambient light in cd/m^2 (times the colour; white = luminance 1).\n"
                              "1 environment unit = 12,800 cd/m^2, the procedural sky's calibrated unit. An HDR panorama has no\n"
                              "absolute scale, so the same unit is used with it: a value adds the same radiance with both sources.\n"
                              "For scale: a dim cockpit 1-3, an office wall about 50-100, the clear zenith sky about 2,000.");
        }
        ImGui::TextDisabled("%.3g environment units", env.ambientLuminance / kAmbientUnitLuminance);
    }
    ImGui::EndDisabled();
}

void SceneInspector::renderNightSky(SceneEnvironmentState& env, SceneRevisions& revisions)
{
    night::NightSettings& n = env.nightSky;
    if (env.environmentSource != static_cast<int>(EnvironmentSource::ProceduralSky)) {
        ImGui::TextDisabled("The moon, the stars and the night glow belong to the procedural sky.");
    } else if (!env.clock.enabled) {
        ImGui::TextDisabled("The moon and the stars follow the Time of Day clock (switch it on).");
    }
    if (ImGui::Checkbox("Moon", &n.moonEnabled)) {
        revisions.mark(SceneChange::Environment);
    }
    UiProbe::item("scene.night.moon", 0);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("The moon disk at the clock's moon position with its phase, and the moonlight: the directional\n"
                          "light (with the sun's shadows) while it is brighter than the sun. Full moon about 0.25 lux.");
    }
    ImGui::BeginDisabled(!n.moonEnabled);
    if (ImGui::SliderFloat("Moonlight", &n.moonlightScale, 0.0f, 100.0f, "%.2f x", ImGuiSliderFlags_Logarithmic)) {
        revisions.mark(SceneChange::Environment);
    }
    UiProbe::item("scene.night.moonlight", 0);
    ImGui::EndDisabled();
    if (ImGui::Checkbox("Stars", &n.starsEnabled)) {
        revisions.mark(SceneChange::Environment);
    }
    UiProbe::item("scene.night.stars", 0);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Procedural stars (magnitudes down to 6.5) turning with the sidereal time; they fade in once the\n"
                          "sun is below civil twilight. Background only.");
    }
    ImGui::BeginDisabled(!n.starsEnabled);
    if (ImGui::SliderFloat("Star Density", &n.starDensity, 0.0f, 10.0f, "%.2f x", ImGuiSliderFlags_Logarithmic)) {
        revisions.mark(SceneChange::Environment);
    }
    UiProbe::item("scene.night.star_density", 0);
    ImGui::EndDisabled();
    if (ImGui::SliderFloat("Night Glow", &n.airglow, 0.0f, 100.0f, "%.2f x", ImGuiSliderFlags_Logarithmic)) {
        revisions.mark(SceneChange::Environment);
    }
    UiProbe::item("scene.night.airglow", 0);
    if (ImGui::SliderFloat("Night Vision", &n.nightVision, 0.0f, 1.0f, "%.2f")) {
        revisions.mark(SceneChange::Environment);
    }
    UiProbe::item("scene.night.vision", 0);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Rod vision of the Auto / Time-of-day exposure: dim pixels lose colour towards a blue-grey (below\n"
                          "3 cd/m^2, fully at 0.01 cd/m^2), bright ones (screens, the moon) keep it. 0 = off.");
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Airglow and integrated starlight of the procedural sky (1 = a dark moonless sky, 2e-4 cd/m^2 at\n"
                          "the zenith); it lights the scene through the IBL.");
    }
    if (env.clock.enabled) {
        const TimeOfDayEvaluation tod = evaluateTimeOfDay(env.clock);
        ImGui::TextDisabled("Moon: elevation %.1f deg, %.0f%% lit", tod.moon.horizontal.apparentElevation,
                            100.0 * tod.moon.illuminatedFraction);
    }
}

void SceneInspector::renderSunDisk(SceneEnvironmentState& env, SceneRevisions& revisions)
{
    SunDiskSettings& disk = env.sunDisk;
    if (ImGui::Checkbox("Sun Disk##enabled", &disk.enabled)) {
        revisions.mark(SceneChange::Environment);
    }
    UiProbe::item("scene.sun_disk.on", 0);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("The sun's disk in the background (HDR or procedural sky) while the sun is on, with the sun's\n"
                          "illuminance and colour. Only the visible background: the IBL never sees it (the sun light\n"
                          "already comes from the directional light). Its glare is the scene's bloom.");
    }
    ImGui::BeginDisabled(!disk.enabled);
    if (ImGui::SliderFloat("Disk Size", &disk.scale, 0.0f, sundisk::kMaxScale, "%.2f x", ImGuiSliderFlags_Logarithmic)) {
        revisions.mark(SceneChange::Environment);
    }
    UiProbe::item("scene.sun_disk.scale", 0);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Artistic multiplier of the angular radius (Sun -> Angular Radius). The disk keeps the sun's\n"
                          "illuminance, so a larger disk is dimmer; the specular highlight is not scaled.");
    }
    if (ImGui::Checkbox("Limb Darkening", &disk.limbDarkening)) {
        revisions.mark(SceneChange::Environment);
    }
    UiProbe::item("scene.sun_disk.limb", 0);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Darker, redder edge (Hestroffer & Magnan power law); the disk's mean radiance is unchanged.");
    }
    ImGui::EndDisabled();
    if (effectiveSunSource(env) == SunSource::Off) {
        ImGui::TextDisabled("Drawn while the sun is on.");
    }
}

void SceneInspector::renderSky(SceneEnvironmentState& env, const SkyStatus* sky, SceneRevisions& revisions)
{
    if (ImGui::Checkbox("Atmospheric Refraction##sky", &env.clock.refraction)) revisions.mark(SceneChange::Environment);
    UiProbe::item("scene.sky.refraction", 0);

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
        revisions.mark(SceneChange::Environment);
    }
    UiProbe::item("scene.sky.mie", 0);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Aerosol (haze) density relative to a clear sky: larger values whiten the sky and widen the glow\n"
                          "around the sun.");
    }
    if (ImGui::Checkbox("Ozone", &env.skyOzone)) {
        revisions.mark(SceneChange::Environment);
    }
    UiProbe::item("scene.sky.ozone", 0);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Ozone absorption keeps the twilight sky blue overhead.");
    }
    bool overrideAlbedo = env.skyGroundAlbedo >= 0.0f;
    if (ImGui::Checkbox("Override Ground Albedo", &overrideAlbedo)) {
        env.skyGroundAlbedo = overrideAlbedo ? (sky != nullptr ? sky->groundAlbedo.g : sky::kNeutralGroundAlbedo) : -1.0f;
        revisions.mark(SceneChange::Environment);
    }
    UiProbe::item("scene.sky.ground_albedo", 0);
    if (overrideAlbedo) {
        if (ImGui::SliderFloat("Ground Albedo", &env.skyGroundAlbedo, 0.0f, 1.0f, "%.2f")) {
            revisions.mark(SceneChange::Environment);
        }
    } else if (sky != nullptr) {
        ImGui::TextDisabled("Ground albedo %.2f %.2f %.2f (%s)", sky->groundAlbedo.r, sky->groundAlbedo.g, sky->groundAlbedo.b,
                            sky->groundFromPlane ? "the viewport's ground plane" : "neutral, no ground plane");
    }
    ImGui::TextDisabled("Sun: always on, from the Time of Day clock or the Sun angles;");
    ImGui::TextDisabled("its colour and intensity come from the atmosphere.");
}

} // namespace ux3d::imgui
