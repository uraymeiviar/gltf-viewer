// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Agent settings registry: the per-viewport keys (effect overrides, shading and overlays, A3.6 ground and A6.8
// ocean, MSFS view helpers, A6.7 lens effect).

#include "CommandsInternal.h"

namespace ux3d::imgui::application {

void Application::registerAgentViewportSettings()
{
    using VS = ux3d::imgui::ViewportSettingsState;
    using ES = ux3d::imgui::SceneEnvironmentState;
    constexpr SettingScope V = SettingScope::Viewport;
    constexpr SettingScope E = SettingScope::Environment;
    // Defaults come from default-constructed state, so they never drift from the structs.
    auto addSetting = [this](SettingSpec spec) {
        if (spec.scope == SettingScope::Viewport) {
            VS defaults;
            spec.defaultValue = spec.get(&defaults);
        } else if (spec.scope == SettingScope::Environment) {
            ES defaults;
            spec.defaultValue = spec.get(&defaults);
        }
        agentRegistry.addSetting(std::move(spec));
    };
    using RQ = ux3d::imgui::RenderQuality;

    // --- Viewport: overrides of the scene's effects (settings scopes) ---
    const auto overrideSetting = [&](std::string key, ux3d::imgui::EffectOverride VS::*member) {
        SettingSpec spec;
        spec.key = std::move(key);
        spec.scope = V;
        spec.type = SettingType::Enum;
        spec.help = "Compatibility key: warned no-op. Use global Quality levels or render_mode shaded for a view without post effects.";
        spec.compat = true;
        spec.enumValues = {"scene", "off", "on"};
        spec.enumAliases = {{"default", "scene"}, {"auto", "scene"}, {"false", "off"}, {"true", "on"}};
        spec.get = [member](void* t) { return json(ux3d::imgui::effectOverrideName(static_cast<VS*>(t)->*member)); };
        spec.set = [](void*, const json&, std::string&) {
            ux3d::core::logWarning("viewport effect override is a compatibility no-op; use Quality levels or Shaded");
            return true;
        };
        addSetting(std::move(spec));
    };
    overrideSetting("viewport_shadows", &VS::shadowsOverride);
    overrideSetting("viewport_ssr", &VS::ssrOverride);
    overrideSetting("viewport_ssao", &VS::ssaoOverride);

    // --- Viewport: shading and overlays ---
    {
        SettingSpec spec;
        spec.key = "render_mode";
        spec.aliases = {"debug_view"};
        spec.scope = V;
        spec.type = SettingType::Enum;
        spec.help = "Shaded + post effects (shaded_post, default), Shaded without post effects, wireframe or a debug render mode (View menu names; spaces, _ and - are ignored).";
        spec.cliFlag = "--debug-view <name> / --render-mode <shaded_post|shaded|wireframe>";
        spec.enumValues = renderModeNames();
        spec.enumValues.push_back("shaded_post");
        spec.enumAliases = {{"default", "shaded_post"}, {"none", "shaded_post"}, {"shaded+posteffects", "shaded_post"}};
        spec.get = [this](void* t) { return json(static_cast<VS*>(t)->renderMode == ux3d::renderer::RenderMode::None && static_cast<VS*>(t)->postEffects ? "shaded_post" : renderModeName(static_cast<VS*>(t)->renderMode)); };
        spec.set = [this](void* t, const json& v, std::string& error) {
            const auto mode = getRenderModeFromName(v == "shaded_post" ? "shaded" : v.get<std::string>());
            if (!mode.has_value()) {
                error = "unknown render mode " + v.get<std::string>();
                return false;
            }
            static_cast<VS*>(t)->postEffects = v != "shaded";
            static_cast<VS*>(t)->renderMode = mode.value();
            static_cast<VS*>(t)->renderModeIdx = v == "shaded" ? 38 : getIndexFromRenderMode(mode.value());
            return true;
        };
        addSetting(std::move(spec));
    }
    addSetting(indexEnumSetting<VS>("texture_distortion_view", V, &VS::textureDistortionView, {"both", "shear", "area"},
                                    "What the Texture Distortion debug view shows: both (default; shear blue, area green, cyan together), shear or area.",
                                    "--texture-distortion-view <both|shear|area>"));
    // The Luminance view (LuminanceView.h): session-only view settings of the luminance render mode.
    addSetting(indexEnumSetting<VS>("luminance_scale", V, &VS::luminanceScale, {"absolute", "relative"},
                                    "Luminance view scale: absolute (default; log10 cd/m^2 from luminance_min to luminance_max) or relative "
                                    "(EV from middle grey 0.18 at the displayed exposure, luminance_ev_min to luminance_ev_max).",
                                    "--luminance-scale <absolute|relative>"));
    {
        // The ends of a scale: high > low (and, in cd/m^2, low > 0); a value that breaks it is refused.
        const auto end = [&](const char* key, float VS::*member, float VS::*other, bool isLow, bool positive, std::string help,
                             std::string cli, std::pair<double, double> range) {
            SettingSpec spec = floatSetting<VS>(key, V, member, std::move(help), std::move(cli), range);
            spec.set = [member, other, isLow, positive, key = std::string(key)](void* t, const json& v, std::string& error) {
                auto* state = static_cast<VS*>(t);
                const float value = static_cast<float>(v.get<double>());
                const float otherValue = state->*other;
                if (!std::isfinite(value) || (positive && !(value > 0.0f)) || (isLow ? !(value < otherValue) : !(value > otherValue))) {
                    error = key + " must be " + (positive && isLow ? "> 0 and " : "") + (isLow ? "below " : "above ") +
                            "the other end (" + std::to_string(otherValue) + ")";
                    return false;
                }
                state->*member = value;
                return true;
            };
            addSetting(std::move(spec));
        };
        end("luminance_min", &VS::luminanceMin, &VS::luminanceMax, true, true,
            "Luminance view, absolute scale: lower end in cd/m^2 (default 0.01; > 0 and below luminance_max). Below it: black.",
            "--luminance-min <cd/m^2>", std::pair{1.0e-9, 1.0e12});
        end("luminance_max", &VS::luminanceMax, &VS::luminanceMin, false, true,
            "Luminance view, absolute scale: upper end in cd/m^2 (default 100000; above luminance_min). Above it: magenta.",
            "--luminance-max <cd/m^2>", std::pair{1.0e-9, 1.0e12});
        end("luminance_ev_min", &VS::luminanceEvMin, &VS::luminanceEvMax, true, false,
            "Luminance view, relative scale: lower end in EV from middle grey (default -6; below luminance_ev_max). Below it: black.",
            "--luminance-ev-min <ev>", std::pair{-40.0, 40.0});
        end("luminance_ev_max", &VS::luminanceEvMax, &VS::luminanceEvMin, false, false,
            "Luminance view, relative scale: upper end in EV from middle grey (default +6; above luminance_ev_min). Above it: magenta.",
            "--luminance-ev-max <ev>", std::pair{-40.0, 40.0});
    }
    addSetting(boolSetting<VS>("luminance_bands", V, &VS::luminanceBands,
                               "Luminance view: one band per decade (absolute) or stop (relative), each in the colour of its centre (default on); off: continuous.",
                               "--luminance-bands <on|off>"));
    addSetting(boolSetting<VS>("luminance_emissive_only", V, &VS::luminanceEmissiveOnly,
                               "Luminance view: the materials' emitted luminance alone, no lighting, black background (default off).",
                               "--luminance-emissive-only <on|off>"));
    addSetting(floatSetting<VS>("texture_heatmap_opacity", V, &VS::textureHeatmapOpacity,
                                "Texture Distortion / Density: how strongly the heat colours cover the grey surface (0.3 to 1, default 0.75).",
                                "--texture-heatmap-opacity <0.3..1>", std::pair{0.3, 1.0}));
    addSetting(boolSetting<VS>("uv_grid_overlay", V, &VS::uvGridOverlay,
                               "UV grid and cell labels over any view but Texture Distortion / Density, which use texture_grid (default off).",
                               "--uv-grid-overlay"));
    addSetting(boolSetting<VS>("texture_grid", V, &VS::textureGrid,
                               "Texture Distortion / Density: UV grid over the surface (default on).", "--texture-grid <on|off>"));
    addSetting(indexEnumSetting<VS>("texture_grid_cells", V, &VS::textureGridCells, {"8", "16", "32", "64"},
                                    "UV grid, in Texture Distortion / Density and the overlay: cells across the UV square (default 16).",
                                    "--texture-grid-cells <8|16|32|64>"));
    addSetting(boolSetting<VS>("texture_grid_labels", V, &VS::textureGridLabels,
                               "UV grid, in Texture Distortion / Density and the overlay: a label in each cell, column letter and row number; A1 is the image's lower left (default on).",
                               "--texture-grid-labels <on|off>"));
    addSetting(floatSetting<VS>("texture_grid_opacity", V, &VS::textureGridOpacity,
                                "UV grid, in Texture Distortion / Density and the overlay: strength of the lines and labels (0 to 1, default 0.5; 1 is the FSS add-on's).",
                                "--texture-grid-opacity <0..1>", std::pair{0.0, 1.0}));
    {
        SettingSpec spec = boolSetting<VS>("grid", V, &VS::showGrid, "Ground grid.", "--hide-grid");
        spec.aliases = {"show_grid"};
        addSetting(std::move(spec));
    }
    // --- Viewport: ground (A3.6: grid or a ground plane; docs/ASOBO_EXTENSIONS_PHASE_3.md) ---
    {
        using GS = ux3d::imgui::GroundSettings;
        SettingSpec spec;
        spec.key = "ground";
        spec.scope = E;
        spec.type = SettingType::Enum;
        spec.help = "Shared scene ground: off (default), an infinite ground plane "
                    "that receives IBL and the sun's shadow, or an animated FFT ocean (A6.8; needs OpenGL 4.3, "
                    "otherwise refused with a warning and the ground is kept).";
        spec.cliFlag = "--ground <off|grid|plane|ocean>";
        spec.enumValues = {"off", "plane", "ocean"};
        spec.enumAliases = {{"grid", "off"}, {"none", "off"}};
        spec.get = [values = spec.enumValues](void* t) {
            return json(values[static_cast<size_t>(static_cast<ES*>(t)->ground.mode)]);
        };
        spec.set = [this, values = spec.enumValues](void* t, const json& v, std::string&) {
            const auto mode = static_cast<ux3d::imgui::GroundMode>(
                std::find(values.begin(), values.end(), v.get<std::string>()) - values.begin());
            // A6.8: with a known context below 4.3 the ocean is refused here (before the context exists, the
            // viewport refuses it at its first render, Viewports.cpp).
            if (mode == ux3d::imgui::GroundMode::Ocean && ux3d::imgui::glCaps().major > 0 && !ux3d::imgui::oceanAvailable()) {
                if (!oceanUnavailableWarned) {
                    ux3d::core::logWarning("Ocean ground needs an OpenGL 4.3 context (compute shaders); keeping the current ground");
                    oceanUnavailableWarned = true;
                }
                return true;
            }
            static_cast<ES*>(t)->ground.mode = mode;
            return true;
        };
        addSetting(std::move(spec));

        SettingSpec heightMode;
        heightMode.key = "ground_height_mode";
        heightMode.scope = E;
        heightMode.type = SettingType::Enum;
        heightMode.help = "Plane height: the scene's lowest point (model_base; static pose, animation does not move it), "
                          "Y = 0 (zero) or ground_height (custom).";
        heightMode.cliFlag = "--ground-height <model-base|zero|number>";
        heightMode.enumValues = {"model_base", "zero", "custom"};
        heightMode.enumAliases = {{"model-base", "model_base"}, {"modelbase", "model_base"}, {"base", "model_base"}};
        heightMode.get = [values = heightMode.enumValues](void* t) {
            return json(values[static_cast<size_t>(static_cast<ES*>(t)->ground.heightMode)]);
        };
        heightMode.set = [values = heightMode.enumValues](void* t, const json& v, std::string&) {
            static_cast<ES*>(t)->ground.heightMode = static_cast<ux3d::imgui::GroundHeightMode>(
                std::find(values.begin(), values.end(), v.get<std::string>()) - values.begin());
            return true;
        };
        addSetting(std::move(heightMode));

        const auto groundFloat = [&](const char* key, float GS::*member, const char* help, const char* cli,
                                     std::optional<std::pair<double, double>> range, bool editsSurface) {
            SettingSpec f;
            f.key = key;
            f.scope = E;
            f.type = SettingType::Float;
            f.help = help;
            f.cliFlag = cli;
            if (range) {
                f.hasRange = true;
                f.minimum = range->first;
                f.maximum = range->second;
            }
            f.get = [member](void* t) { return agent::floatJson(static_cast<ES*>(t)->ground.*member); };
            f.set = [member, editsSurface](void* t, const json& v, std::string&) {
                auto& ground = static_cast<ES*>(t)->ground;
                ground.*member = static_cast<float>(v.get<double>());
                if (editsSurface) ground.preset = ux3d::imgui::GroundPreset::Custom;
                return true;
            };
            addSetting(std::move(f));
        };
        groundFloat("ground_height", &GS::customHeight, "Plane height in scene units for ground_height_mode custom.",
                    "--ground-height <number>", std::nullopt, false);
        groundFloat("ground_roughness", &GS::roughness, "Plane perceptual roughness 0..1 (marks the preset custom).",
                    "--ground-roughness <0..1>", std::pair{0.0, 1.0}, true);
        groundFloat("ground_detail", &GS::detail, "Amount of procedural albedo variation 0..1 (marks the preset custom).",
                    "--ground-detail <0..1>", std::pair{0.0, 1.0}, true);
        groundFloat("ground_detail_scale", &GS::detailScale,
                    "Size of the largest procedural feature in scene units (marks the preset custom).",
                    "--ground-detail-scale <units>", std::pair{0.0001, 1000000.0}, true);
        groundFloat("ground_joint_spacing", &GS::jointSpacing,
                    "Slab joint spacing in scene units, 0 = none (marks the preset custom).",
                    "--ground-joints <units>", std::pair{0.0, 1000000.0}, true);
        groundFloat("ground_haze", &GS::haze,
                    "Horizon fog visibility distance in scene units (distance of 2% contrast); 0 = automatic (0.9 x far plane, at least 100 x the camera height above the ground).",
                    "--ground-haze <units>", std::pair{0.0, 1.0e9}, false);

        SettingSpec preset;
        preset.key = "ground_preset";
        preset.scope = E;
        preset.type = SettingType::Enum;
        preset.help = "Plane surface preset: sets colour, roughness and detail (concrete, grass, sand, grey); "
                      "custom keeps the current values.";
        preset.cliFlag = "--ground-preset <concrete|grass|sand|grey>";
        preset.enumValues = {"concrete", "grass", "sand", "grey", "custom"};
        preset.enumAliases = {{"tarmac", "concrete"}, {"gray", "grey"}};
        preset.get = [values = preset.enumValues](void* t) {
            return json(values[static_cast<size_t>(static_cast<ES*>(t)->ground.preset)]);
        };
        preset.set = [values = preset.enumValues](void* t, const json& v, std::string&) {
            static_cast<ES*>(t)->ground.applyPreset(static_cast<ux3d::imgui::GroundPreset>(
                std::find(values.begin(), values.end(), v.get<std::string>()) - values.begin()));
            return true;
        };
        addSetting(std::move(preset));

        SettingSpec color;
        color.key = "ground_color";
        color.scope = E;
        color.type = SettingType::Color3;
        color.help = "Plane albedo (linear base colour, also the average albedo A6.2 reads); marks the preset custom.";
        color.cliFlag = "--ground-color <r,g,b>";
        color.get = [](void* t) {
            const glm::vec3& c = static_cast<ES*>(t)->ground.albedo;
            return json{agent::floatJson(c.r), agent::floatJson(c.g), agent::floatJson(c.b)};
        };
        color.set = [](void* t, const json& v, std::string&) {
            auto& ground = static_cast<ES*>(t)->ground;
            ground.albedo = glm::vec3(v[0].get<float>(), v[1].get<float>(), v[2].get<float>());
            ground.preset = ux3d::imgui::GroundPreset::Custom;
            return true;
        };
        addSetting(std::move(color));

        using OS = ux3d::imgui::OceanSettings;
        const auto oceanFloat = [&](const char* key, float OS::*member, const char* help, const char* cli,
                                    std::pair<double, double> range) {
            SettingSpec f;
            f.key = key;
            f.scope = E;
            f.type = SettingType::Float;
            f.help = help;
            f.cliFlag = cli;
            f.hasRange = true;
            f.minimum = range.first;
            f.maximum = range.second;
            f.get = [member](void* t) { return agent::floatJson(static_cast<ES*>(t)->ground.ocean.*member); };
            f.set = [member](void* t, const json& v, std::string&) {
                static_cast<ES*>(t)->ground.ocean.*member = static_cast<float>(v.get<double>());
                return true;
            };
            addSetting(std::move(f));
        };
        oceanFloat("ocean_wind_speed", &OS::windSpeed, "Ocean wind speed 10 m above the sea, m/s (recomputes the loop).",
                   "--ocean-wind-speed <m/s>", {0.5, 40.0});
        oceanFloat("ocean_wind_dir", &OS::windDirection,
                   "Direction the wind comes from, degrees (the sun's azimuth convention: around +Y from +Z towards +X); "
                   "the waves travel the opposite way (recomputes the loop).",
                   "--ocean-wind-dir <deg>", {0.0, 360.0});
        oceanFloat("ocean_time", &OS::time,
                   "Pinned ocean time in seconds for captures; -1 follows the application clock.",
                   "--ocean-time <s>", {-1.0, 1.0e6});
        oceanFloat("ocean_fetch", &OS::fetch,
                   "Distance in metres over which the wind has blown (JONSWAP); beyond full development (about 22000 "
                   "x U^2 / g) it changes nothing (recomputes the loop).",
                   "--ocean-fetch <m>", {1000.0, 1.0e7});
        oceanFloat("ocean_choppiness", &OS::choppiness,
                   "Horizontal displacement factor (Tessendorf's lambda) 0..2: sharper crests, more foam (recomputes the loop).",
                   "--ocean-choppiness <0..2>", {0.0, 2.0});
        oceanFloat("ocean_swell", &OS::swell,
                   "Swell 0..1: narrows the directional spread of the waves (Horvath 2015; recomputes the loop).",
                   "--ocean-swell <0..1>", {0.0, 1.0});
        oceanFloat("ocean_foam", &OS::foam,
                   "Foam amount 0..1: 0 none, 0.5 the whitecap coverage of the wind speed (Monahan 1980: 0.02% at 3 m/s, "
                   "1% at 10, 10% at 20), 1 four times that.",
                   "--ocean-foam <0..1>", {0.0, 1.0});
        oceanFloat("ocean_period", &OS::period,
                   "Loop period in seconds of the precomputed (looping) mode: the surface repeats exactly after it "
                   "(recomputes the loop; ignored in live mode).",
                   "--ocean-period <s>", {5.0, 120.0});
        {
            SettingSpec c;
            c.key = "ocean_color";
            c.scope = E;
            c.type = SettingType::Color3;
            c.help = "Water body colour, linear RGB 0..1: the light scattered back out of the water (times the sky's "
                     "diffuse light and the sun). The ocean's average albedo (0.066 + colour) feeds A6.2.";
            c.cliFlag = "--ocean-color <r,g,b>";
            c.hasRange = true;
            c.minimum = 0.0;
            c.maximum = 1.0;
            c.get = [](void* t) {
                const glm::vec3& v = static_cast<ES*>(t)->ground.ocean.color;
                return json{agent::floatJson(v.r), agent::floatJson(v.g), agent::floatJson(v.b)};
            };
            c.set = [](void* t, const json& v, std::string&) {
                static_cast<ES*>(t)->ground.ocean.color = glm::vec3(v[0].get<float>(), v[1].get<float>(), v[2].get<float>());
                return true;
            };
            addSetting(std::move(c));
        }

    }
    addSetting(boolSetting<VS>("scene_bounding_box", V, &VS::showSceneBoundingBox, "Yellow AABB around the scene."));
    addSetting(boolSetting<VS>("selection_box", V, &VS::showSelectedBoundingBox, "Orange OBB around the selected node."));
    addSetting(boolSetting<VS>("node_gizmo", V, &VS::showNodeCoordinateGizmo, "Axis gizmo at the selected node (UI overlay, not in captures)."));
    addSetting(boolSetting<VS>("force_double_sided", V, &VS::forceDoubleSided, "Disable back-face culling.", "--force-double-sided"));
    addSetting(boolSetting<VS>("invert_winding", V, &VS::invertWinding, "Flip front/back face winding.", "--invert-winding"));
    addSetting(boolSetting<VS>("wireframe_overlay", V, &VS::wireframeOverlay, "Mesh edges over the shaded image.", "--wireframe-overlay"));
    {
        SettingSpec spec;
        spec.key = "wireframe_color";
        spec.scope = V;
        spec.type = SettingType::Color4;
        spec.help = "Wireframe line colour and opacity (0..1).";
        spec.cliFlag = "--wireframe-color <r,g,b[,a]>";
        spec.get = [](void* t) {
            const glm::vec4& c = static_cast<VS*>(t)->wireframeColor;
            return json{agent::floatJson(c.r), agent::floatJson(c.g), agent::floatJson(c.b), agent::floatJson(c.a)};
        };
        spec.set = [](void* t, const json& v, std::string&) {
            static_cast<VS*>(t)->wireframeColor = glm::vec4(v[0].get<float>(), v[1].get<float>(), v[2].get<float>(), v[3].get<float>());
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec;
        spec.key = "wireframe_blend";
        spec.scope = V;
        spec.type = SettingType::Enum;
        spec.help = "Wireframe line blend mode.";
        spec.cliFlag = "--wireframe-blend <normal|additive|invert|xor>";
        spec.enumValues = {"normal", "additive", "invert", "xor"};
        spec.get = [values = spec.enumValues](void* t) {
            return json(values[static_cast<size_t>(static_cast<VS*>(t)->wireframeBlend)]);
        };
        spec.set = [values = spec.enumValues](void* t, const json& v, std::string&) {
            static_cast<VS*>(t)->wireframeBlend = static_cast<ux3d::imgui::WireframeBlendMode>(
                std::find(values.begin(), values.end(), v.get<std::string>()) - values.begin());
            return true;
        };
        addSetting(std::move(spec));
    }

    // --- Viewport: MSFS extension view helpers (the effects moved to the scene, see below) ---
    addSetting(boolSetting<VS>("show_sky_portals", V, &VS::showSkyPortals,
                               "Draw every sky portal's gizmo (UI overlay, not in captures); a selected portal always shows.",
                               "--show-sky-portals"));
    // Lens effects are renderer Quality settings; compatibility forwards are registered centrally.
    auto gridCompat = boolSetting<VS>("ground_grid", V, &VS::showGrid,
                                      "Compatibility key: viewport grid overlay, same as grid.", "--ground-grid <on|off>");
    gridCompat.compat = true;
    gridCompat.forwardsTo = V;
    addSetting(std::move(gridCompat));
}

} // namespace ux3d::imgui::application
