// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// The Scene Inspector as a key/value tree (PropertyTree.h): groups are collapsible tree nodes, every property is one
// row with an icon and a label on the left and the widget on the right; descriptions are tooltips.

#include "imgui/SceneInspector.h"
#include "imgui/PropertyTree.h"
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
#include <cfloat>
#include <cmath>
#include <filesystem>
#include <string>
#include <cstring>

namespace ux3d::imgui {

namespace {

void markEnv(SceneRevisions& revisions) { revisions.mark(SceneChange::Environment); }

void renderSceneGround(SceneEnvironmentState& env, SceneRevisions& revisions) {
    ImGui::PushID("scene_ground");
    // A3.6: the ground. Grid is the editor grid overlay; Plane is an infinite surface in the scene.
    const bool groundOpen = prop::group(ICON_MDI_TERRAIN, "Ground", true);
    UiProbe::item("scene.ground", 0);
    if (groundOpen) {
        GroundSettings& ground = env.ground;
        const char* groundNames[] = { "Off", "Plane", "Ocean" };
        prop::row(ICON_MDI_SHAPE_OUTLINE, "Type", "Off, an infinite plane lit by the environment and the sun with shadows, or an ocean.");
        const bool typeOpen = ImGui::BeginCombo("##groundType", groundNames[static_cast<int>(ground.mode)]);
        UiProbe::item("scene.ground.type", 0);
        if (typeOpen) {
            for (int i = 0; i < 3; ++i) {
                // A6.8: the ocean needs compute shaders (OpenGL 4.3).
                const bool unavailable = i == static_cast<int>(GroundMode::Ocean) && !oceanAvailable();
                ImGui::BeginDisabled(unavailable);
                if (ImGui::Selectable(groundNames[i], static_cast<int>(ground.mode) == i)) {
                    ground.mode = static_cast<GroundMode>(i);
                    markEnv(revisions);
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
            prop::row(ICON_MDI_ARROW_EXPAND_VERTICAL, "Height");
            if (ImGui::Combo("##groundHeight", &heightMode, heightNames, IM_ARRAYSIZE(heightNames))) {
                ground.heightMode = static_cast<GroundHeightMode>(heightMode);
                markEnv(revisions);
            }
            UiProbe::item("scene.ground.height", 0);
            if (ground.heightMode == GroundHeightMode::Custom) {
                prop::row(ICON_MDI_RULER, "Custom Height", "In scene units.");
                if (ImGui::DragFloat("##groundCustomHeight", &ground.customHeight, 0.01f, -1.0e6f, 1.0e6f, "%.3f")) markEnv(revisions);
                UiProbe::item("scene.ground.custom_height", 0);
            }
        }
        if (ground.mode == GroundMode::Ocean) {
            // A6.8: the ocean. Wind, fetch, choppiness, swell, quality and period recompute the simulation
            // (a looping set in the background, a live set at once); foam, colour and play only change the shading.
            OceanSettings& ocean = ground.ocean;
            const OceanUiStatus& status = oceanUiStatus();
            if (prop::group(ICON_MDI_WAVES, "Ocean", true, "Reflections: sky and environment only (no ocean SSR).")) {
                // Settings scopes: the simulation quality is renderer-wide (File > Settings > Quality); shown here for reference.
                const char* qualityNames[] = { "Low", "Medium", "High" };
                prop::textRow(ICON_MDI_TUNE, "Quality", "Renderer-wide: File > Settings > Quality (all viewports).", "%s",
                              qualityNames[std::clamp(static_cast<int>(ocean.quality), 0, 2)]);
                UiProbe::item("scene.ocean.quality", 0);
                prop::row(ICON_MDI_WEATHER_WINDY, "Wind Speed", "Metres per second.");
                if (ImGui::SliderFloat("##oceanWindSpeed", &ocean.windSpeed, 0.5f, 40.0f, "%.1f m/s")) markEnv(revisions);
                UiProbe::item("scene.ocean.wind_speed", 0);
                const char* windTip = "The direction the wind comes from, around +Y from +Z towards +X (the sun's azimuth). The waves travel the other way.";
                prop::row(ICON_MDI_NAVIGATION, "Wind From", windTip);
                if (ImGui::SliderFloat("##oceanWindDirection", &ocean.windDirection, 0.0f, 360.0f, "%.0f deg")) markEnv(revisions);
                UiProbe::item("scene.ocean.wind_dir", 0);
                prop::tooltip(windTip);
                prop::row(ICON_MDI_WAVES_ARROW_UP, "Choppiness");
                if (ImGui::SliderFloat("##oceanChoppiness", &ocean.choppiness, 0.0f, 2.0f, "%.2f")) markEnv(revisions);
                UiProbe::item("scene.ocean.choppiness", 0);
                const char* swellTip = "Narrows the directional spread of the waves (long, parallel swell).";
                prop::row(ICON_MDI_WAVES, "Swell", swellTip);
                if (ImGui::SliderFloat("##oceanSwell", &ocean.swell, 0.0f, 1.0f, "%.2f")) markEnv(revisions);
                UiProbe::item("scene.ocean.swell", 0);
                prop::tooltip(swellTip);
                const char* foamTip = "Whitecaps. 0.5 is the coverage observed at this wind speed (about 1% at 10 m/s, 10% at 20 m/s); 1 is four times that.";
                prop::row(ICON_MDI_WATER_OUTLINE, "Foam Amount", foamTip);
                if (ImGui::SliderFloat("##oceanFoam", &ocean.foam, 0.0f, 1.0f, "%.2f")) markEnv(revisions);
                UiProbe::item("scene.ocean.foam", 0);
                prop::tooltip(foamTip);
                prop::row(ICON_MDI_PALETTE, "Water Colour");
                if (ImGui::ColorEdit3("##oceanColor", &ocean.color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR)) markEnv(revisions);
                UiProbe::item("scene.ocean.color", 0);
                prop::row(ICON_MDI_TIMER_OUTLINE, "Loop Period", "Seconds; the precomputed waves repeat after this (not used while live).");
                ImGui::BeginDisabled(ocean.live);
                if (ImGui::DragFloat("##oceanPeriod", &ocean.period, 0.5f, 5.0f, 120.0f, "%.1f s")) {
                    ocean.period = std::clamp(ocean.period, 5.0f, 120.0f);
                    markEnv(revisions);
                }
                ImGui::EndDisabled();
                UiProbe::item("scene.ocean.period", 0);
                const char* fetchTip = "Distance the wind has blown over. Beyond full development (about 22000 x U^2 / g) it changes nothing.";
                prop::row(ICON_MDI_RULER, "Fetch", fetchTip);
                if (ImGui::DragFloat("##oceanFetch", &ocean.fetch, 100.0f, 1000.0f, 1.0e7f, "%.0f m", ImGuiSliderFlags_Logarithmic)) {
                    ocean.fetch = std::clamp(ocean.fetch, 1000.0f, 1.0e7f);
                    markEnv(revisions);
                }
                UiProbe::item("scene.ocean.fetch", 0);
                prop::tooltip(fetchTip);
                if (ocean.time >= 0.0f) {
                    prop::textRow(ICON_MDI_CLOCK_OUTLINE, "Time", "Pinned by --ocean-time.", "%.2f s (pinned)", ocean.time);
                }
                if (status.busy) {
                    prop::row(ICON_MDI_INFORMATION_OUTLINE, "Simulation");
                    ImGui::ProgressBar(status.progress, ImVec2(-FLT_MIN, 0.0f), "Recomputing the waves");
                } else if (status.ready) {
                    const char* simTip = status.live ? "Hs = significant wave height; simulated every frame."
                                                     : "Hs = significant wave height; precomputed, sampled per frame.";
                    if (status.live) {
                        prop::textRow(ICON_MDI_INFORMATION_OUTLINE, "Simulation", simTip, "Hs %.2f m, %.1f MiB, %.2f ms GPU (live)",
                                      status.significantWaveHeight, static_cast<double>(status.memoryBytes) / 1048576.0, status.liveGpuMs);
                    } else {
                        prop::textRow(ICON_MDI_INFORMATION_OUTLINE, "Simulation", simTip, "Hs %.2f m, %.1f MiB (precomputed)",
                                      status.significantWaveHeight, static_cast<double>(status.memoryBytes) / 1048576.0);
                    }
                }
                prop::row(ICON_MDI_TUNE, "Defaults");
                if (ImGui::Button("Reset ocean##oceanReset", ImVec2(-FLT_MIN, 0.0f))) {
                    ocean = OceanSettings();
                    markEnv(revisions);
                }
                UiProbe::item("scene.ocean.reset", 0);
                prop::endGroup();
            }
        }
        if (ground.mode == GroundMode::Plane) {
            int preset = static_cast<int>(ground.preset);
            const char* presetLabel =
                ground.preset == GroundPreset::Custom ? "Custom" : groundPresets()[static_cast<size_t>(preset)].label.data();
            prop::row(ICON_MDI_TEXTURE, "Material", "Lit by the environment (its own lower hemisphere) and the sun with shadows.");
            const bool materialOpen = ImGui::BeginCombo("##groundMaterial", presetLabel);
            UiProbe::item("scene.ground.material", 0);
            if (materialOpen) {
                for (size_t i = 0; i < groundPresets().size(); ++i) {
                    if (ImGui::Selectable(groundPresets()[i].label.data(), static_cast<size_t>(preset) == i)) {
                        ground.applyPreset(static_cast<GroundPreset>(i));
                        markEnv(revisions);
                    }
                }
                ImGui::EndCombo();
            }
            const auto custom = [&] { ground.preset = GroundPreset::Custom; markEnv(revisions); };
            prop::row(ICON_MDI_PALETTE, "Albedo");
            if (ImGui::ColorEdit3("##groundAlbedo", &ground.albedo.x, ImGuiColorEditFlags_Float)) custom();
            UiProbe::item("scene.ground.albedo", 0);
            prop::row(ICON_MDI_GRADIENT_VERTICAL, "Roughness");
            if (ImGui::SliderFloat("##groundRoughness", &ground.roughness, 0.0f, 1.0f, "%.2f")) custom();
            UiProbe::item("scene.ground.roughness", 0);
            prop::row(ICON_MDI_TEXTURE, "Detail");
            if (ImGui::SliderFloat("##groundDetail", &ground.detail, 0.0f, 1.0f, "%.2f")) custom();
            UiProbe::item("scene.ground.detail", 0);
            prop::row(ICON_MDI_RULER, "Detail Size");
            if (ImGui::DragFloat("##groundDetailSize", &ground.detailScale, 0.01f, 0.0001f, 1.0e6f, "%.3f", ImGuiSliderFlags_Logarithmic)) custom();
            UiProbe::item("scene.ground.detail_size", 0);
            prop::row(ICON_MDI_VIEW_GRID_OUTLINE, "Joint Spacing");
            if (ImGui::DragFloat("##groundJoints", &ground.jointSpacing, 0.05f, 0.0f, 1.0e6f, ground.jointSpacing > 0.0f ? "%.2f" : "None")) custom();
            UiProbe::item("scene.ground.joints", 0);
            const char* hazeTip = "Distance at which the plane has faded 98% into the horizon colour. 0 = automatic (0.9 x far plane, at least 100 x the camera height).";
            prop::row(ICON_MDI_WEATHER_HAZY, "Haze Distance", hazeTip);
            if (ImGui::DragFloat("##groundHaze", &ground.haze, 1.0f, 0.0f, 1.0e9f, ground.haze > 0.0f ? "%.0f" : "Auto", ImGuiSliderFlags_Logarithmic)) markEnv(revisions);
            UiProbe::item("scene.ground.haze", 0);
            prop::tooltip(hazeTip);
        }
        prop::endGroup();
    }
    ImGui::PopID();
}

} // namespace

// A3.1: Sun & Time of Day -> Sun. Conventions in SceneEnvironmentState (SceneEnvironment.h).
void SceneInspector::renderSun(SceneEnvironmentState& env, ux3d::renderer::GltfRenderLayer* gltfLayer,
                               SceneRevisions& revisions, const EffectiveSun* sun)
{
    const bool procedural = env.environmentSource == static_cast<int>(EnvironmentSource::ProceduralSky);
    if (!prop::group(ICON_MDI_WHITE_BALANCE_SUNNY, "Sun", true,
                     procedural ? "With the procedural sky the sun's colour and intensity come from the atmosphere." : nullptr)) {
        return;
    }
    const SunSource source = effectiveSunSource(env);
    int choice = procedural ? (source == SunSource::Clock ? 1 : 0) : static_cast<int>(source);
    prop::row(ICON_MDI_LIGHTBULB_ON_OUTLINE, "Sun Source",
              procedural ? "Colour and intensity come from the atmosphere." : nullptr);
    if (ImGui::Combo("##sun_source", &choice, procedural ? "Manual\0Time of Day\0" : "Off\0From HDR\0Manual\0Time of Day\0")) {
        selectSunSource(env, procedural ? (choice == 1 ? SunSource::Clock : SunSource::Manual) : static_cast<SunSource>(choice));
        markEnv(revisions);
    }
    UiProbe::item("scene.sun.source", 0);
    if (source == SunSource::Off) {
        prop::endGroup();
        return;
    }
    if (sun != nullptr) {
        float azimuth = 0.0f, elevation = 0.0f;
        sunAnglesFromDirection(sun->direction, azimuth, elevation);
        prop::textRow(ICON_MDI_COMPASS_OUTLINE, "Effective", "The sun direction in use this frame.", "az %.1f, el %.1f deg", azimuth, elevation);
    }

    const ux3d::renderer::EnvironmentSun* found = gltfLayer != nullptr ? &gltfLayer->getEnvironmentSun() : nullptr;
    if (source == SunSource::FromHdr) {
        if (found == nullptr || !found->found) {
            prop::row(ICON_MDI_ALERT_OUTLINE, "Detected");
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "No sun found in the HDR: the sun is off.");
        } else {
            const glm::mat3 rotation = ux3d::renderer::CommandGenerator::environmentRotationMatrix(env.hdrRotation);
            float azimuth = 0.0f, elevation = 0.0f;
            sunAnglesFromDirection(glm::transpose(rotation) * found->direction, azimuth, elevation);
            char detail[160];
            std::snprintf(detail, sizeof(detail), "%u texels, %.3g sr, peak %.4g, sky %.4g", found->texelCount,
                          found->solidAngle, found->peakLuminance, found->skyLevel);
            prop::textRow(ICON_MDI_COMPASS_OUTLINE, "Detected Direction", detail, "az %.2f, el %.2f deg", azimuth, elevation);
            prop::textRow(ICON_MDI_BRIGHTNESS_6, "Detected Illuminance", detail, "%.4g (x HDR intensity %.2f)",
                          found->illuminance, env.hdrIntensity);
            float colour[3] = { found->color.r, found->color.g, found->color.b };
            prop::row(ICON_MDI_PALETTE, "Detected Colour");
            ImGui::ColorEdit3("##detectedColour", colour, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoPicker |
                                                          ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float);
            prop::row(ICON_MDI_TUNE, "Manual Sun", "Copies the detected direction, colour and illuminance to a Manual sun.");
            if (ImGui::Button("Copy to Manual", ImVec2(-FLT_MIN, 0.0f))) {
                env.sunAzimuth = azimuth;
                env.sunElevation = elevation;
                env.sunColor = found->color;
                env.sunIntensity = found->illuminance * env.hdrIntensity;
                selectSunSource(env, SunSource::Manual);
                markEnv(revisions);
            }
        }
    } else {
        if (source == SunSource::Manual) {
            const char* azTip = "Around +Y, from +Z towards +X (glTF Y-up). 0 = sun towards +Z.";
            prop::row(ICON_MDI_COMPASS_OUTLINE, "Azimuth", azTip);
            if (ImGui::SliderFloat("##sunAzimuth", &env.sunAzimuth, 0.0f, 360.0f, "%.1f deg")) markEnv(revisions);
            UiProbe::item("scene.sun.azimuth", 0);
            prop::tooltip(azTip);
            prop::row(ICON_MDI_ANGLE_ACUTE, "Elevation");
            if (ImGui::SliderFloat("##sunElevation", &env.sunElevation, -90.0f, 90.0f, "%.1f deg")) markEnv(revisions);
            UiProbe::item("scene.sun.elevation", 0);
        }
        if (!procedural) {
            float colour[3] = { env.sunColor.r, env.sunColor.g, env.sunColor.b };
            prop::row(ICON_MDI_PALETTE, "Sun Colour");
            if (ImGui::ColorEdit3("##sunColour", colour, ImGuiColorEditFlags_Float)) {
                env.sunColor = glm::vec3(colour[0], colour[1], colour[2]);
                markEnv(revisions);
            }
            static float kelvin = 5778.0f;
            const char* kelvinTip = "Sets the colour to a black body at this temperature (luminance 1).";
            prop::row(ICON_MDI_THEME_LIGHT_DARK, "Kelvin", kelvinTip);
            if (ImGui::DragFloat("##sunKelvin", &kelvin, 10.0f, 1667.0f, 25000.0f, "%.0f K")) {
                env.sunColor = kelvinToLinearRgb(kelvin);
                markEnv(revisions);
            }
            prop::tooltip(kelvinTip);
            const char* intensityTip = "Illuminance in the environment's units (HDR radiance x sr).\n"
                                       "A white Lambert surface facing the sun reflects colour x intensity / pi.";
            prop::row(ICON_MDI_BRIGHTNESS_6, "Intensity", intensityTip);
            if (ImGui::DragFloat("##sunIntensity", &env.sunIntensity, 0.05f, 0.0f, 100000.0f, "%.3f", ImGuiSliderFlags_Logarithmic)) markEnv(revisions);
            UiProbe::item("scene.sun.intensity", 0);
            prop::tooltip(intensityTip);
        }
        if (source == SunSource::Manual && !procedural && found != nullptr && found->found) {
            prop::row(ICON_MDI_IMAGE_FILTER_HDR, "HDR Sun", "Points the manual sun where the HDR's sun is.");
            if (ImGui::Button("Use HDR sun direction", ImVec2(-FLT_MIN, 0.0f))) {
                const glm::mat3 rotation = ux3d::renderer::CommandGenerator::environmentRotationMatrix(env.hdrRotation);
                sunAnglesFromDirection(glm::transpose(rotation) * found->direction, env.sunAzimuth, env.sunElevation);
                markEnv(revisions);
            }
        }
    }
    std::string radiusTip = "Widens the specular highlight to the sun's disk (0.27 deg = the real sun).";
    if (found != nullptr && found->removed) {
        radiusTip += "\nHDR sun removed from IBL and background (" + std::to_string(found->texelCount) + " texels).";
    }
    prop::row(ICON_MDI_CIRCLE_OUTLINE, "Angular Radius", radiusTip.c_str());
    if (ImGui::SliderFloat("##sunAngularRadius", &env.sunAngularRadius, 0.0f, 5.0f, "%.2f deg")) markEnv(revisions);
    prop::tooltip(radiusTip.c_str());
    prop::endGroup();
}

void SceneInspector::renderTimeOfDay(SceneEnvironmentState& env, SceneRevisions& revisions)
{
    TimeOfDayState& clock = env.clock;
    const bool clockDrivesSun = effectiveSunSource(env) == SunSource::Clock;
    if (!prop::group(ICON_MDI_CLOCK_OUTLINE, "Time of Day", true,
                     clockDrivesSun ? nullptr : "The clock does not move the selected sun source.")) {
        return;
    }
    const char* clockTip = "Runs the clock for the moon and the stars (and the sun when Sun Source is Time of Day).";
    prop::row(ICON_MDI_WEATHER_NIGHT, "Moon & Stars Clock", clockTip);
    if (ImGui::Checkbox("##clock_enabled", &clock.enabled)) markEnv(revisions);
    UiProbe::item("scene.tod.clock", 0);

    const TimeOfDayEvaluation tod = evaluateTimeOfDay(clock);
    bool edited = false;

    int date[3] = { clock.year, clock.month, clock.day };
    prop::row(ICON_MDI_CALENDAR, "Date", "Year, month, day.");
    if (ImGui::DragInt3("##todDate", date, 0.1f, 1, 2100)) {
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
    const std::string timeFormat = "%.3f h  (" + formatTime(tod.localHours) + ")";
    prop::row(ICON_MDI_CLOCK_OUTLINE, "Time", "Local time at the place below.");
    if (ImGui::SliderFloat("##todTime", &hours, 0.0f, 24.0f, timeFormat.c_str())) {
        clock.hours = std::min<double>(hours, 23.9999);
        clock.preset = static_cast<int>(TimePreset::Custom);
        edited = true;
    }
    UiProbe::item("scene.tod.time", 0);

    const char* presetTip = "Presets set the time for the date and place: dawn and dusk when the sun is 3 degrees up,\n"
                            "noon at solar transit, night at solar midnight, morning and afternoon halfway.";
    prop::row(ICON_MDI_WEATHER_SUNSET_UP, "Presets", presetTip);
    static const char* const presetLabels[] = { "Dawn", "Morning", "Noon", "Afternoon", "Dusk", "Night" };
    const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    for (int i = 0; i < 6; ++i) {
        const bool active = clock.preset == i + 1;
        if (i > 0) {
            // Wrap to the next line when the button does not fit in the value cell.
            const float width = ImGui::CalcTextSize(presetLabels[i]).x + ImGui::GetStyle().FramePadding.x * 2.0f;
            ImGui::SameLine();
            if (ImGui::GetCursorScreenPos().x + width > right) ImGui::NewLine();
        }
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(presetLabels[i])) {
            clock.preset = i + 1;
            edited = true;
        }
        if (active) ImGui::PopStyleColor();
        UiProbe::item((std::string("scene.tod.preset.") + kTimePresetNames[static_cast<size_t>(i + 1)]).c_str(), 0);
        prop::tooltip(presetTip);
    }

    prop::row(ICON_MDI_PLAY_CIRCLE_OUTLINE, "Play", "Advances the clock.");
    if (ImGui::Checkbox("##todPlay", &clock.playing)) {
        if (clock.playing) materializePreset(clock);
        edited = true;
    }
    UiProbe::item("scene.tod.play", 0);
    const char* speedTip = "Simulated seconds per real second (600 = ten minutes per second).";
    prop::row(ICON_MDI_SPEEDOMETER, "Speed", speedTip);
    if (ImGui::DragFloat("##todSpeed", &clock.timeScale, 1.0f, 0.0f, 100000.0f, "%.0f x", ImGuiSliderFlags_Logarithmic)) edited = true;
    prop::tooltip(speedTip);

    prop::row(ICON_MDI_EARTH, "UTC Offset");
    if (ImGui::DragFloat("##todUtc", &clock.utcOffsetHours, 0.25f, -12.0f, 14.0f, "%.2f h")) edited = true;
    prop::row(ICON_MDI_LATITUDE, "Latitude");
    if (ImGui::DragFloat("##todLatitude", &clock.latitude, 0.05f, -90.0f, 90.0f, "%.4f deg")) edited = true;
    prop::row(ICON_MDI_LONGITUDE, "Longitude");
    if (ImGui::DragFloat("##todLongitude", &clock.longitude, 0.05f, -180.0f, 180.0f, "%.4f deg")) edited = true;
    const char* headingTip = "True compass direction the model's nose points to (0 = north, 90 = east).\n"
                             "MSFS aircraft have their nose on glTF +Z and their left side on +X.";
    prop::row(ICON_MDI_NAVIGATION, "Heading", headingTip);
    if (ImGui::SliderFloat("##todHeading", &clock.heading, 0.0f, 360.0f, "%.1f deg")) edited = true;
    UiProbe::item("scene.tod.heading", 0);
    prop::tooltip(headingTip);

    const char* nightTip = "Emissive Day/Night = Automatic: night while the sun's elevation is below this (-6 = civil twilight).";
    prop::row(ICON_MDI_WEATHER_SUNSET_DOWN, "Night Below", nightTip);
    if (ImGui::DragFloat("##todNightBelow", &clock.nightThreshold, 0.1f, -90.0f, 90.0f, "%.1f deg")) edited = true;
    prop::tooltip(nightTip);

    if (prop::group(ICON_MDI_INFORMATION_OUTLINE, "Positions", true)) {
        prop::textRow(ICON_MDI_WHITE_BALANCE_SUNNY, "Sun", "True azimuth; elevation (apparent elevation with refraction).",
                      "az %.2f, el %.2f deg (%.2f apparent)", tod.sun.azimuth, tod.sun.elevation, tod.sun.apparentElevation);
        prop::textRow(ICON_MDI_CUBE_OUTLINE, "In the Model", "The sun's azimuth in the model's frame (after Heading) and its light.",
                      "az %.1f deg, light %.0f%%%s", tod.gliderAzimuth, tod.fade * 100.0f, tod.night ? ", night" : "");
        double windowStart = 0.0;
        TimeOfDayState local = clock;
        local.hours = tod.localHours;
        local.preset = static_cast<int>(TimePreset::Custom);
        local.year = tod.year;
        local.month = tod.month;
        local.day = tod.day;
        const solar::SunDay events = timeOfDaySunDay(local, windowStart);
        if (events.hasSunrise && events.hasSunset) {
            prop::textRow(ICON_MDI_WEATHER_SUNSET_UP, "Sunrise / Sunset", nullptr, "%s / %s",
                          formatTime(hoursSince(events.sunrise, windowStart)).c_str(),
                          formatTime(hoursSince(events.sunset, windowStart)).c_str());
        } else {
            prop::textRow(ICON_MDI_WEATHER_SUNSET_UP, "Sunrise / Sunset", nullptr, "%s",
                          events.alwaysUp ? "The sun does not set on this date." : "The sun does not rise on this date.");
        }
        prop::textRow(ICON_MDI_MOON_WAXING_CRESCENT, "Moon", nullptr, "az %.1f, el %.1f deg, %.0f%% lit",
                      tod.moon.horizontal.azimuth, tod.moon.horizontal.apparentElevation, tod.moon.illuminatedFraction * 100.0);
        prop::endGroup();
    }
    if (edited) {
        normalizeClock(clock);
        markEnv(revisions);
    }
    prop::endGroup();
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
    if (!prop::begin("Inspector")) return;

    if (scene != nullptr) {
        prop::textRow(ICON_MDI_IMAGE_FILTER_HDR, "Scene", nullptr, "ID %u", scene->m_ID);
        prop::textRow(ICON_MDI_TAG_OUTLINE, "Name", "The name in the glTF (read only).", "%s",
                      scene->hasName() ? scene->getName().c_str() : "(unnamed)");
    }

    const bool environmentOpen = prop::group(ICON_MDI_EARTH, "Environment", true);
    UiProbe::item("scene.environment", 0);
    if (environmentOpen) {
        prop::row(ICON_MDI_IMAGE, "Background", "Shows the environment (HDR or sky) behind the model; off shows the clear colour.");
        if (ImGui::Checkbox("##background", &env.showBackground)) {
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
            prop::row(ICON_MDI_FORMAT_COLOR_FILL, "Clear Color");
            if (ImGui::ColorEdit4("##clearColor", clr)) {
                env.clearColor = glm::vec4(clr[0], clr[1], clr[2], clr[3]);
                markEnv(revisions);
            }
        }

        // HDR environment map: the file name is the button; the full path is its tooltip.
        const std::u8string hdrName8 = std::filesystem::u8path(env.currentHdrPath).filename().u8string();
        const std::string hdrName(hdrName8.begin(), hdrName8.end());
        prop::row(ICON_MDI_IMAGE_FILTER_HDR, "HDR / EXR", "The environment panorama. Click to load another HDR / EXR file.");
        if (ImGui::Button((ICON_MDI_FOLDER_OPEN " " + (hdrName.empty() ? std::string("Load...") : hdrName) + "##loadHdr").c_str(),
                          ImVec2(-FLT_MIN, 0.0f))) {
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
        prop::tooltip((env.currentHdrPath + "\nClick to load another HDR / EXR panorama.").c_str());

        prop::row(ICON_MDI_BLUR, "HDR Blur");
        if (ImGui::SliderFloat("##hdrBlur", &env.hdrBlur, 0.0f, 1.0f, "%.2f")) {
            if (commandGenerator != nullptr) commandGenerator->setEnvironmentMapBlurNormalized(env.hdrBlur);
            markEnv(revisions);
        }
        prop::row(ICON_MDI_BRIGHTNESS_6, "HDR Intensity");
        if (ImGui::SliderFloat("##hdrIntensity", &env.hdrIntensity, 0.0f, 10.0f, "%.2f")) {
            if (commandGenerator != nullptr) commandGenerator->setEnvironmentMapIntensity(env.hdrIntensity);
            markEnv(revisions);
        }
        prop::row(ICON_MDI_ROTATE_3D_VARIANT, "HDR Rotation");
        if (ImGui::SliderFloat("##hdrRotation", &env.hdrRotation, 0.0f, 360.0f, "%.1f deg")) {
            if (commandGenerator != nullptr) commandGenerator->setEnvironmentMapRotation(env.hdrRotation);
            markEnv(revisions);
        }

        if (prop::group(ICON_MDI_CAMERA_IRIS, "Tone & Exposure", true)) {
            const char* toneMappingNames[] = { "Khronos PBR Neutral", "ACES Hill", "ACES Narkowicz" };
            // A6.4: an unset choice shows the source's default (ACES Hill with the HDR, PBR Neutral with the sky).
            int toneMappingIdx = effectiveToneMapping(env);
            prop::row(ICON_MDI_CONTRAST_CIRCLE, "Tone Mapping", "Default: ACES Hill with the HDR, PBR Neutral with the procedural sky.");
            if (ImGui::Combo("##toneMapping", &toneMappingIdx, toneMappingNames, 3)) {
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
                markEnv(revisions);
            }

            // Exposure mode (Manual / Auto / Time-of-day curve, A6.4) and controls. An unset mode shows the source's
            // default (Manual with the HDR, Auto with the procedural sky).
            const char* exposureModeNames[] = { "Manual", "Auto (eye adaptation)", "Time-of-day curve" };
            const ExposureMode effectiveMode = effectiveExposureMode(env);
            int exposureModeIdx = static_cast<int>(effectiveMode);
            const char* modeTip = "Manual: the EV below. Auto: metered eye adaptation (with the procedural sky in EV100 with a key\n"
                                  "curve that keeps night darker than day). Time-of-day curve: the procedural sky's exposure from\n"
                                  "the sun, sky, moon and night glow illuminance (Manual with the HDR). Default: Manual with the\n"
                                  "HDR, Auto with the procedural sky.";
            prop::row(ICON_MDI_CAMERA_IRIS, "Exposure Mode", modeTip);
            if (ImGui::Combo("##exposureMode", &exposureModeIdx, exposureModeNames, 3)) {
                env.exposureMode = exposureModeIdx;
                env.autoExposureStartFromManual = exposureModeIdx == static_cast<int>(ExposureMode::Auto);
                markEnv(revisions);
            }
            UiProbe::item("scene.exposure.mode", 0);
            prop::tooltip(modeTip);
            const bool physicalExposure = env.environmentSource == static_cast<int>(EnvironmentSource::ProceduralSky) &&
                                          effectiveMode != ExposureMode::Manual;
            if (physicalExposure) {
                prop::row(ICON_MDI_TUNE, "EV Compensation");
                if (ImGui::SliderFloat("##evCompensation", &env.exposureCompensationEV, -4.0f, 4.0f, "%+.2f EV")) markEnv(revisions);
                UiProbe::item("scene.exposure.compensation", 0);
                const char* darkTip = "Scenes darker than this are not exposed further (-12: a starlit night, about 0.001 lux).\n"
                                      "Glider EV = 13.38 - EV100 + the key curve.";
                prop::row(ICON_MDI_WEATHER_NIGHT, "Darkest Scene", darkTip);
                if (ImGui::SliderFloat("##darkestScene", &env.exposureMinEV100, -20.0f, 10.0f, "%.1f EV100")) {
                    env.exposureMaxEV100 = (std::max)(env.exposureMaxEV100, env.exposureMinEV100);
                    markEnv(revisions);
                }
                prop::tooltip(darkTip);
                prop::row(ICON_MDI_WEATHER_SUNNY_ALERT, "Brightest Scene");
                if (ImGui::SliderFloat("##brightestScene", &env.exposureMaxEV100, 0.0f, 25.0f, "%.1f EV100")) {
                    env.exposureMinEV100 = (std::min)(env.exposureMinEV100, env.exposureMaxEV100);
                    markEnv(revisions);
                }
                if (effectiveMode == ExposureMode::Auto) {
                    // A6.4b: the physically based speeds are the scene brightness going up (light adaptation) / down.
                    prop::row(ICON_MDI_TRANSITION, "Adapt to Brighter");
                    if (ImGui::SliderFloat("##adaptBrighter", &env.autoExposureSpeedUp, 0.2f, 20.0f, "%.1f /s")) markEnv(revisions);
                    prop::row(ICON_MDI_TRANSITION, "Adapt to Darker");
                    if (ImGui::SliderFloat("##adaptDarker", &env.autoExposureSpeedDown, 0.2f, 20.0f, "%.1f /s")) markEnv(revisions);
                    // A6.4b metering (NightSky.h), after the existing controls.
                    const char* meteringNames[] = { "Average", "Average + highlights" };
                    int metering = std::clamp(env.exposureMetering, 0, 1);
                    const char* meterTip = "Average: trimmed centre-weighted log average with the key curve.\n"
                                           "Average + highlights (default): the darker of the average and an exposure that keeps the\n"
                                           "brightest lit surfaces (0.3% of the image: screens, panels) below the tone mapper's shoulder.\n"
                                           "The sky, the moon, the stars and single lights do not count.";
                    prop::row(ICON_MDI_BULLSEYE, "Metering", meterTip);
                    if (ImGui::Combo("##metering", &metering, meteringNames, 2)) {
                        env.exposureMetering = metering;
                        markEnv(revisions);
                    }
                    UiProbe::item("scene.exposure.metering", 0);
                    prop::tooltip(meterTip);
                }
            } else if (effectiveMode != ExposureMode::Auto) {
                prop::row(ICON_MDI_CAMERA_IRIS, "Exposure");
                if (ImGui::SliderFloat("##exposureEV", &env.exposureEV, -4.0f, 4.0f, "%.2f EV")) {
                    if (commandGenerator != nullptr) commandGenerator->setExposure(std::pow(2.0f, env.exposureEV));
                    markEnv(revisions);
                }
            } else {
                prop::row(ICON_MDI_TUNE, "EV Compensation");
                if (ImGui::SliderFloat("##evCompensation", &env.exposureCompensationEV, -4.0f, 4.0f, "%+.2f EV")) markEnv(revisions);
                prop::row(ICON_MDI_WEATHER_NIGHT, "Min Exposure");
                if (ImGui::SliderFloat("##minExposure", &env.autoExposureMinEV, -10.0f, 0.0f, "%.2f EV")) {
                    if (env.autoExposureMinEV > env.autoExposureMaxEV) env.autoExposureMaxEV = env.autoExposureMinEV;
                    markEnv(revisions);
                }
                prop::row(ICON_MDI_WEATHER_SUNNY_ALERT, "Max Exposure");
                if (ImGui::SliderFloat("##maxExposure", &env.autoExposureMaxEV, 0.0f, 10.0f, "%.2f EV")) {
                    if (env.autoExposureMaxEV < env.autoExposureMinEV) env.autoExposureMinEV = env.autoExposureMaxEV;
                    markEnv(revisions);
                }
                prop::row(ICON_MDI_TRANSITION, "Adapt Speed Up");
                if (ImGui::SliderFloat("##adaptUp", &env.autoExposureSpeedUp, 0.2f, 20.0f, "%.1f /s")) markEnv(revisions);
                prop::row(ICON_MDI_TRANSITION, "Adapt Speed Down");
                if (ImGui::SliderFloat("##adaptDown", &env.autoExposureSpeedDown, 0.2f, 20.0f, "%.1f /s")) markEnv(revisions);
            }
            prop::endGroup();
        }

        // Physically based bloom (Jimenez 2014). Bloom appearance stays editable while the Quality level is off.
        if (prop::group(ICON_MDI_FLARE, "Bloom", true)) {
            prop::row(ICON_MDI_BRIGHTNESS_6, "Intensity");
            if (ImGui::SliderFloat("##bloomIntensity", &env.bloomIntensity, 0.0f, 0.5f, "%.3f")) markEnv(revisions);
            UiProbe::item("scene.bloom.intensity", 0);
            prop::row(ICON_MDI_CIRCLE_OUTLINE, "Radius");
            if (ImGui::SliderFloat("##bloomRadius", &env.bloomRadius, 0.25f, 3.0f, "%.2f")) markEnv(revisions);
            UiProbe::item("scene.bloom.radius", 0);
            prop::endGroup();
        }

        if (prop::group(ICON_MDI_AIRPLANE, "MSFS Emissive", true)) {
            float emissiveScale = effectiveMsfsEmissiveScale(env);
            const char* scaleTip = "Effective ASOBO emissive scale (1 unit = x). Sky: cd/m^2 / 12800; HDR: 1/1000.\n"
                                   "Editing overrides both sources. Right-click to restore source defaults.\n"
                                   "Legacy MSFS 2020 units cannot be reliably identified from the glTF.\n"
                                   "Core glTF (including KHR emissive_strength) is unchanged.";
            prop::row(ICON_MDI_LIGHTBULB_ON_OUTLINE, "Emissive Scale", scaleTip);
            if (ImGui::DragFloat("##msfsEmissiveScale", &emissiveScale, 0.0001f, 0.0f, 10.0f, "%.8f", ImGuiSliderFlags_Logarithmic)) {
                env.msfsEmissiveScale = emissiveScale;
                env.msfsEmissiveScaleOverride = true;
                markEnv(revisions);
            }
            UiProbe::item("scene.msfs.emissive.scale", 0);
            prop::tooltip(scaleTip);
            if (ImGui::BeginPopupContextItem("msfs_emissive_scale_default")) {
                if (ImGui::MenuItem("Use source default", nullptr, !env.msfsEmissiveScaleOverride)) {
                    env.msfsEmissiveScaleOverride = false;
                    markEnv(revisions);
                }
                UiProbe::item("scene.msfs.emissive.default", 0);
                ImGui::EndPopup();
            }

            const char* timeOfDayNames[] = { "Day", "Night", "Automatic" };
            const char* dayNightTip = "Which ASOBO_material_emissive multiplier applies. Automatic follows the sun's elevation\n"
                                      "from the Time of Day clock (night below the threshold there).";
            prop::row(ICON_MDI_THEME_LIGHT_DARK, "Day / Night", dayNightTip);
            if (ImGui::Combo("##emissiveDayNight", &env.timeOfDay, timeOfDayNames, 3)) markEnv(revisions);
            UiProbe::item("scene.time_of_day", 0);
            prop::tooltip(dayNightTip);
            prop::endGroup();
        }

        // Settings scopes: the IBL filter is quality (renderer-wide) and is edited in File > Settings > Quality.
        // While IBL is on, the scene shows what it gets (the probe tag stays on the row).
        if (env.iblEnabled) {
            const RenderQualitySettings& quality = RenderQuality::settings();
            prop::textRow(ICON_MDI_TUNE, "IBL Filter", "Quality (all viewports): File > Settings > Quality.",
                          "Cube %u, irr. %s, LUT %u, %u spp, %s", quality.iblCubeSize,
                          quality.iblIrradianceSize == 0 ? "full" : std::to_string(quality.iblIrradianceSize).c_str(),
                          quality.iblLutSize, quality.iblSpecularSamples, quality.iblHalfFloat ? "16-bit" : "32-bit");
            UiProbe::item("scene.ibl.filter", 0);
        }
        prop::endGroup();
    }

    // Sun and time of day get their own group below Environment.
    const bool sunTimeOpen = prop::group(ICON_MDI_WEATHER_SUNSET_UP, "Sun & Time of Day", true);
    UiProbe::item("scene.sun_time", 0);
    if (sunTimeOpen) {
        renderSun(env, gltfLayer, revisions, sun);
        renderTimeOfDay(env, revisions);
        prop::endGroup();
    }

    // Settings scopes: the scene's effect switches and look parameters. Every viewport follows them unless its
    // Viewport Settings override shadows, SSR or SSAO.
    renderSceneGround(env, revisions);
    const bool effectsOpen = prop::group(ICON_MDI_AIRPLANE, "Ambient Occlusion & Aircraft", true);
    UiProbe::item("scene.effects", 0);
    if (effectsOpen) {
        renderEffects(env, gltfLayer, revisions);
        prop::endGroup();
    }

    // A6.2 / A6.3 and the following groups are collapsed by default.
    const bool skyOpen = prop::group(ICON_MDI_WEATHER_FOG, "Sky & Atmosphere", false);
    UiProbe::item("scene.sky", 0);
    if (skyOpen) {
        renderSky(env, sky, revisions);
        prop::endGroup();
    }

    const bool diskOpen = prop::group(ICON_MDI_WHITE_BALANCE_SUNNY, "Sun Disk", false);
    UiProbe::item("scene.sun_disk", 0);
    if (diskOpen) {
        renderSunDisk(env, revisions);
        prop::endGroup();
    }

    const bool nightOpen = prop::group(ICON_MDI_WEATHER_NIGHT, "Night Sky", false);
    UiProbe::item("scene.night", 0);
    if (nightOpen) {
        renderNightSky(env, revisions);
        prop::endGroup();
    }

    const bool ambientOpen = prop::group(ICON_MDI_LIGHTBULB_OUTLINE, "Ambient Light", false);
    UiProbe::item("scene.ambient", 0);
    if (ambientOpen) {
        renderAmbient(env, revisions, outdoorIlluminance);
        prop::endGroup();
    }

    const bool skyOcclusionOpen = prop::group(ICON_MDI_HOME_OUTLINE, "Sky Occlusion", false,
                                              "Mode and bake quality: File > Settings > Quality.");
    UiProbe::item("scene.sky_occlusion", 0);
    if (skyOcclusionOpen) {
        renderSkyOcclusion(env, sky, revisions);
        prop::endGroup();
    }

    prop::end();
}

void SceneInspector::renderEffects(SceneEnvironmentState& env, ux3d::renderer::GltfRenderLayer* /*gltfLayer*/,
                                   SceneRevisions& revisions)
{
    SceneEffectSettings& fx = env.effects;
    const char* radiusTip = "World units (metres in MSFS assets): occluders farther away do not count.";
    prop::row(ICON_MDI_CIRCLE_OUTLINE, "AO Radius", radiusTip);
    if (ImGui::DragFloat("##sceneSsaoRadius", &fx.ssaoRadius, 0.01f, 0.01f, 100.0f, "%.2f",
                         ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic)) {
        markEnv(revisions);
    }
    UiProbe::item("scene.effects.ssao_radius", 0);
    prop::tooltip(radiusTip);
    prop::row(ICON_MDI_CONTRAST_CIRCLE, "AO Intensity");
    if (ImGui::SliderFloat("##sceneSsaoIntensity", &fx.ssaoIntensity, 0.0f, 4.0f, "%.2f")) markEnv(revisions);
    UiProbe::item("scene.effects.ssao_intensity", 0);
    if (prop::group(ICON_MDI_AIRPLANE, "Aircraft State (MSFS)", true)) {
        const char* icingTip = "Windshield icing and frosted geometry decals (0 = clean aircraft).";
        prop::row(ICON_MDI_SNOWFLAKE, "Icing", icingTip);
        if (ImGui::SliderFloat("##sceneIcing", &fx.windshieldIcing, 0.0f, 1.0f, "%.2f")) markEnv(revisions);
        UiProbe::item("scene.effects.icing", 0);
        prop::tooltip(icingTip);
        prop::endGroup();
    }
}

void SceneInspector::renderSkyOcclusion(SceneEnvironmentState& env, const SkyStatus* sky, SceneRevisions& revisions)
{
    const char* occluderTip = "ASOBO_material_environment_occluder geometry (hidden in the image) closes a cabin for the\n"
                              "environment; its sky portals then bring the window light.";
    prop::row(ICON_MDI_HOME_OUTLINE, "Occluders Block Sky", occluderTip);
    if (ImGui::Checkbox("##skyOcclusionOccluders", &env.skyOcclusionOccluders)) markEnv(revisions);
    UiProbe::item("scene.sky_occlusion.occluders", 0);
    prop::tooltip(occluderTip);
    if (sky != nullptr) {
        const char* statusTip = "Mode and bake quality: File > Settings > Quality.";
        if (sky->skyOcclusionBaking) {
            prop::textRow(ICON_MDI_INFORMATION_OUTLINE, "Status", statusTip, "Baking... %.0f%%", sky->skyOcclusionProgress * 100.0f);
        } else if (sky->skyOcclusionActive) {
            prop::textRow(ICON_MDI_INFORMATION_OUTLINE, "Status", statusTip, "%d x %d x %d, cell %.3g, %.1f MiB, %.0f ms",
                          sky->skyOcclusionDims[0], sky->skyOcclusionDims[1], sky->skyOcclusionDims[2], sky->skyOcclusionCell,
                          sky->skyOcclusionMemoryMiB, sky->skyOcclusionBakeMs);
        } else {
            prop::textRow(ICON_MDI_INFORMATION_OUTLINE, "Status", statusTip, "Not in use");
        }
    }
}

void SceneInspector::renderAmbient(SceneEnvironmentState& env, SceneRevisions& revisions, float outdoorIlluminance)
{
    int enabled = env.ambient < 0 ? 0 : env.ambient != 0 ? 1 : 2;
    const char* ambientTip = "A viewer light, not part of the glTF: a uniform radiance from every direction, with the HDR or\n"
                             "the procedural sky. Manual is uniform; Auto estimates outdoor bounce where the sky is hidden.\n"
                             "Authored occlusion and SSAO apply; the background and emissive surfaces are unchanged.";
    prop::row(ICON_MDI_LIGHTBULB_OUTLINE, "Ambient Light", ambientTip);
    if (ImGui::Combo("##ambientEnabled", &enabled, "Auto (sky on, HDR off)\0On\0Off\0")) {
        env.ambient = enabled == 0 ? -1 : enabled == 1 ? 1 : 0;
        markEnv(revisions);
    }
    UiProbe::item("scene.ambient.on", 0);
    prop::tooltip(ambientTip);
    ImGui::BeginDisabled(!effectiveAmbientEnabled(env));
    int mode = effectiveAmbientAuto(env) ? 0 : 1;
    prop::row(ICON_MDI_TUNE, "Intensity Mode");
    if (ImGui::Combo("##ambient_mode", &mode, "Auto\0Manual\0")) {
        env.ambientMode = mode == 0 ? 1 : 0;
        markEnv(revisions);
    }
    UiProbe::item("scene.ambient.mode", 0);
    if (effectiveAmbientAuto(env)) {
        const float luminance = env.ambientFill * outdoorIlluminance / 3.14159265358979323846f;
        char fillTip[200];
        std::snprintf(fillTip, sizeof(fillTip), "White fill where the sky is hidden; uniform without a volume.\n"
                                                "Outdoor %.4g lux; fill %.4g cd/m^2.",
                      outdoorIlluminance * kAmbientUnitLuminance, luminance * kAmbientUnitLuminance);
        prop::row(ICON_MDI_FORMAT_COLOR_FILL, "Fill Fraction", fillTip);
        if (ImGui::SliderFloat("##ambient_fill", &env.ambientFill, 0.0f, 1.0f, "%.4f")) markEnv(revisions);
        UiProbe::item("scene.ambient.fill", 0);
        prop::tooltip(fillTip);
    } else {
        prop::row(ICON_MDI_PALETTE, "Ambient Color");
        if (ImGui::ColorEdit3("##ambientColor", &env.ambientColor.x, ImGuiColorEditFlags_Float)) {
            env.ambientColor = (glm::max)(env.ambientColor, glm::vec3(0.0f));
            markEnv(revisions);
        }
        UiProbe::item("scene.ambient.color", 0);
        char intensityTip[600];
        std::snprintf(intensityTip, sizeof(intensityTip),
                      "Luminance of the ambient light in cd/m^2 (times the colour; white = luminance 1); %.3g environment units.\n"
                      "1 environment unit = 12,800 cd/m^2, the procedural sky's calibrated unit. An HDR panorama has no\n"
                      "absolute scale, so the same unit is used with it: a value adds the same radiance with both sources.\n"
                      "For scale: a dim cockpit 1-3, an office wall about 50-100, the clear zenith sky about 2,000.",
                      env.ambientLuminance / kAmbientUnitLuminance);
        prop::row(ICON_MDI_BRIGHTNESS_6, "Ambient Intensity", intensityTip);
        if (ImGui::SliderFloat("##ambientIntensity", &env.ambientLuminance, 0.0f, kAmbientMaxLuminance, "%.3g cd/m^2",
                               ImGuiSliderFlags_Logarithmic)) {
            markEnv(revisions);
        }
        UiProbe::item("scene.ambient.intensity", 0);
        prop::tooltip(intensityTip);
    }
    ImGui::EndDisabled();
}

void SceneInspector::renderNightSky(SceneEnvironmentState& env, SceneRevisions& revisions)
{
    night::NightSettings& n = env.nightSky;
    const char* availability = nullptr;
    if (env.environmentSource != static_cast<int>(EnvironmentSource::ProceduralSky)) {
        availability = "The moon, the stars and the night glow belong to the procedural sky.";
    } else if (!env.clock.enabled) {
        availability = "The moon and the stars follow the Time of Day clock (switch it on).";
    }
    std::string moonTip = "The moon disk at the clock's moon position with its phase, and the moonlight: the directional\n"
                          "light (with the sun's shadows) while it is brighter than the sun. Full moon about 0.25 lux.";
    if (availability != nullptr) moonTip += std::string("\n") + availability;
    prop::row(ICON_MDI_MOON_WAXING_CRESCENT, "Moon", moonTip.c_str());
    if (ImGui::Checkbox("##moon", &n.moonEnabled)) markEnv(revisions);
    UiProbe::item("scene.night.moon", 0);
    prop::tooltip(moonTip.c_str());
    ImGui::BeginDisabled(!n.moonEnabled);
    prop::row(ICON_MDI_BRIGHTNESS_6, "Moonlight");
    if (ImGui::SliderFloat("##moonlight", &n.moonlightScale, 0.0f, 100.0f, "%.2f x", ImGuiSliderFlags_Logarithmic)) markEnv(revisions);
    UiProbe::item("scene.night.moonlight", 0);
    ImGui::EndDisabled();
    std::string starsTip = "Procedural stars (magnitudes down to 6.5) turning with the sidereal time; they fade in once the\n"
                           "sun is below civil twilight. Background only.";
    if (availability != nullptr) starsTip += std::string("\n") + availability;
    prop::row(ICON_MDI_STAR_OUTLINE, "Stars", starsTip.c_str());
    if (ImGui::Checkbox("##stars", &n.starsEnabled)) markEnv(revisions);
    UiProbe::item("scene.night.stars", 0);
    prop::tooltip(starsTip.c_str());
    ImGui::BeginDisabled(!n.starsEnabled);
    prop::row(ICON_MDI_STAR_OUTLINE, "Star Density");
    if (ImGui::SliderFloat("##starDensity", &n.starDensity, 0.0f, 10.0f, "%.2f x", ImGuiSliderFlags_Logarithmic)) markEnv(revisions);
    UiProbe::item("scene.night.star_density", 0);
    ImGui::EndDisabled();
    const char* glowTip = "Airglow and integrated starlight of the procedural sky (1 = a dark moonless sky, 2e-4 cd/m^2 at\n"
                          "the zenith); it lights the scene through the IBL.";
    prop::row(ICON_MDI_WEATHER_NIGHT, "Night Glow", glowTip);
    if (ImGui::SliderFloat("##airglow", &n.airglow, 0.0f, 100.0f, "%.2f x", ImGuiSliderFlags_Logarithmic)) markEnv(revisions);
    UiProbe::item("scene.night.airglow", 0);
    prop::tooltip(glowTip);
    const char* visionTip = "Rod vision of the Auto / Time-of-day exposure: dim pixels lose colour towards a blue-grey (below\n"
                            "3 cd/m^2, fully at 0.01 cd/m^2), bright ones (screens, the moon) keep it. 0 = off.";
    prop::row(ICON_MDI_EYE_OUTLINE, "Night Vision", visionTip);
    if (ImGui::SliderFloat("##nightVision", &n.nightVision, 0.0f, 1.0f, "%.2f")) markEnv(revisions);
    UiProbe::item("scene.night.vision", 0);
    prop::tooltip(visionTip);
    if (env.clock.enabled) {
        const TimeOfDayEvaluation tod = evaluateTimeOfDay(env.clock);
        prop::textRow(ICON_MDI_INFORMATION_OUTLINE, "Moon Now", nullptr, "el %.1f deg, %.0f%% lit",
                      tod.moon.horizontal.apparentElevation, 100.0 * tod.moon.illuminatedFraction);
    }
}

void SceneInspector::renderSunDisk(SceneEnvironmentState& env, SceneRevisions& revisions)
{
    SunDiskSettings& disk = env.sunDisk;
    std::string diskTip = "The sun's disk in the background (HDR or procedural sky) while the sun is on, with the sun's\n"
                          "illuminance and colour. Only the visible background: the IBL never sees it (the sun light\n"
                          "already comes from the directional light). Its glare is the scene's bloom.";
    if (effectiveSunSource(env) == SunSource::Off) diskTip += "\nDrawn while the sun is on (it is off now).";
    prop::row(ICON_MDI_WHITE_BALANCE_SUNNY, "Sun Disk", diskTip.c_str());
    if (ImGui::Checkbox("##sunDiskEnabled", &disk.enabled)) markEnv(revisions);
    UiProbe::item("scene.sun_disk.on", 0);
    prop::tooltip(diskTip.c_str());
    ImGui::BeginDisabled(!disk.enabled);
    const char* sizeTip = "Artistic multiplier of the angular radius (Sun -> Angular Radius). The disk keeps the sun's\n"
                          "illuminance, so a larger disk is dimmer; the specular highlight is not scaled.";
    prop::row(ICON_MDI_CIRCLE_OUTLINE, "Disk Size", sizeTip);
    if (ImGui::SliderFloat("##sunDiskScale", &disk.scale, 0.0f, sundisk::kMaxScale, "%.2f x", ImGuiSliderFlags_Logarithmic)) markEnv(revisions);
    UiProbe::item("scene.sun_disk.scale", 0);
    prop::tooltip(sizeTip);
    const char* limbTip = "Darker, redder edge (Hestroffer & Magnan power law); the disk's mean radiance is unchanged.";
    prop::row(ICON_MDI_GRADIENT_VERTICAL, "Limb Darkening", limbTip);
    if (ImGui::Checkbox("##limbDarkening", &disk.limbDarkening)) markEnv(revisions);
    UiProbe::item("scene.sun_disk.limb", 0);
    prop::tooltip(limbTip);
    ImGui::EndDisabled();
}

void SceneInspector::renderSky(SceneEnvironmentState& env, const SkyStatus* sky, SceneRevisions& revisions)
{
    prop::row(ICON_MDI_ANGLE_ACUTE, "Atmospheric Refraction", "Raises the apparent sun and moon near the horizon.");
    if (ImGui::Checkbox("##skyRefraction", &env.clock.refraction)) markEnv(revisions);
    UiProbe::item("scene.sky.refraction", 0);

    const char* sourceNames[] = { "HDR panorama", "Procedural sky" };
    const char* sourceTip = "Procedural sky: a physically based atmosphere (Hillaire 2020) as the background and the\n"
                            "source of the image-based lighting, re-filtered as the sun moves. The HDR stays available.\n"
                            "With the procedural sky the sun is always on (Time of Day clock or Sun angles) and its\n"
                            "colour and intensity come from the atmosphere.";
    prop::row(ICON_MDI_EARTH, "Environment Source", sourceTip);
    if (ImGui::Combo("##environmentSource", &env.environmentSource, sourceNames, 2)) {
        revisions.mark(SceneChange::RenderSettings);
    }
    UiProbe::item("scene.sky.source", 0);
    prop::tooltip(sourceTip);
    if (env.environmentSource != static_cast<int>(EnvironmentSource::ProceduralSky)) {
        return;
    }

    prop::row(ICON_MDI_INFORMATION_OUTLINE, "Lighting");
    ImGui::AlignTextToFramePadding();
    if (sky != nullptr && sky->active) {
        const ImVec4 green(0.13f, 0.67f, 0.42f, 1.0f), amber(1.0f, 0.8f, 0.2f, 1.0f);
        switch (sky->state) {
        case SkyRefineState::Converged:
            ImGui::TextColored(green, ICON_MDI_CHECK " Converged (%u px cubes)", sky->cubeSize);
            break;
        case SkyRefineState::Updating:
            ImGui::TextColored(amber, ICON_MDI_REFRESH " Updating (%u px, %.0f%%)", sky->cubeSize, sky->progress * 100.0f);
            break;
        case SkyRefineState::WaitingToRefine:
            ImGui::TextColored(amber, ICON_MDI_REFRESH " Reduced (%u px), refining soon", sky->cubeSize);
            break;
        case SkyRefineState::Refining:
            ImGui::TextColored(amber, ICON_MDI_REFRESH " Refining to %u px: %.0f%%", sky->targetSize, sky->progress * 100.0f);
            break;
        default:
            ImGui::TextDisabled("-");
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

    const char* mieTip = "Aerosol (haze) density relative to a clear sky: larger values whiten the sky and widen the glow\n"
                         "around the sun.";
    prop::row(ICON_MDI_WEATHER_HAZY, "Mie Density", mieTip);
    if (ImGui::SliderFloat("##mieDensity", &env.skyMieDensity, 0.0f, 10.0f, "%.2f x", ImGuiSliderFlags_Logarithmic)) markEnv(revisions);
    UiProbe::item("scene.sky.mie", 0);
    prop::tooltip(mieTip);
    const char* ozoneTip = "Ozone absorption keeps the twilight sky blue overhead.";
    prop::row(ICON_MDI_EARTH, "Ozone", ozoneTip);
    if (ImGui::Checkbox("##ozone", &env.skyOzone)) markEnv(revisions);
    UiProbe::item("scene.sky.ozone", 0);
    prop::tooltip(ozoneTip);
    bool overrideAlbedo = env.skyGroundAlbedo >= 0.0f;
    std::string albedoTip = "Overrides the ground albedo the atmosphere sees below the horizon.";
    if (!overrideAlbedo && sky != nullptr) {
        char buf[160];
        std::snprintf(buf, sizeof(buf), "\nIn use: %.2f %.2f %.2f (%s).", sky->groundAlbedo.r, sky->groundAlbedo.g, sky->groundAlbedo.b,
                      sky->groundFromPlane ? "the viewport's ground plane" : "neutral, no ground plane");
        albedoTip += buf;
    }
    prop::row(ICON_MDI_TERRAIN, "Override Ground Albedo", albedoTip.c_str());
    if (ImGui::Checkbox("##overrideGroundAlbedo", &overrideAlbedo)) {
        env.skyGroundAlbedo = overrideAlbedo ? (sky != nullptr ? sky->groundAlbedo.g : sky::kNeutralGroundAlbedo) : -1.0f;
        markEnv(revisions);
    }
    UiProbe::item("scene.sky.ground_albedo", 0);
    prop::tooltip(albedoTip.c_str());
    if (overrideAlbedo) {
        prop::row(ICON_MDI_PALETTE, "Ground Albedo");
        if (ImGui::SliderFloat("##groundAlbedo", &env.skyGroundAlbedo, 0.0f, 1.0f, "%.2f")) markEnv(revisions);
    } else if (sky != nullptr) {
        prop::textRow(ICON_MDI_PALETTE, "Ground Albedo", sky->groundFromPlane ? "From the viewport's ground plane." : "Neutral, no ground plane.",
                      "%.2f %.2f %.2f", sky->groundAlbedo.r, sky->groundAlbedo.g, sky->groundAlbedo.b);
    }
}

} // namespace ux3d::imgui
