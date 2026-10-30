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
    const auto overrideSetting = [&](std::string key, ux3d::imgui::EffectOverride VS::*member, std::string help, bool ssr) {
        SettingSpec spec;
        spec.key = std::move(key);
        spec.scope = V;
        spec.type = SettingType::Enum;
        spec.help = std::move(help);
        spec.enumValues = {"scene", "off", "on"};
        spec.enumAliases = {{"default", "scene"}, {"auto", "scene"}, {"false", "off"}, {"true", "on"}};
        spec.get = [member](void* t) { return json(ux3d::imgui::effectOverrideName(static_cast<VS*>(t)->*member)); };
        spec.set = [member, ssr, values = spec.enumValues](void* t, const json& v, std::string&) {
            static_cast<VS*>(t)->*member = static_cast<ux3d::imgui::EffectOverride>(
                std::find(values.begin(), values.end(), v.get<std::string>()) - values.begin());
            if (ssr) RQ::pinSsr();
            return true;
        };
        addSetting(std::move(spec));
    };
    overrideSetting("viewport_shadows", &VS::shadowsOverride,
                    "This viewport's sun shadows: scene (follow the scene's shadows, default), off or on.", false);
    overrideSetting("viewport_ssr", &VS::ssrOverride,
                    "This viewport's windshield SSR: scene (follow the scene's ssr, default), off or on.", true);
    overrideSetting("viewport_ssao", &VS::ssaoOverride,
                    "This viewport's SSAO: scene (follow the scene's ssao, default), off or on.", false);

    // --- Viewport: shading and overlays ---
    {
        SettingSpec spec;
        spec.key = "render_mode";
        spec.aliases = {"debug_view"};
        spec.scope = V;
        spec.type = SettingType::Enum;
        spec.help = "Shaded, wireframe or a debug render mode (View menu names; spaces, _ and - are ignored).";
        spec.cliFlag = "--debug-view <name> / --render-mode <shaded|wireframe>";
        spec.enumValues = renderModeNames();
        spec.enumAliases = {{"default", "shaded"}, {"none", "shaded"}};
        spec.get = [this](void* t) { return json(renderModeName(static_cast<VS*>(t)->renderMode)); };
        spec.set = [this](void* t, const json& v, std::string& error) {
            const auto mode = getRenderModeFromName(v.get<std::string>());
            if (!mode.has_value()) {
                error = "unknown render mode " + v.get<std::string>();
                return false;
            }
            static_cast<VS*>(t)->renderMode = mode.value();
            static_cast<VS*>(t)->renderModeIdx = getIndexFromRenderMode(mode.value());
            return true;
        };
        addSetting(std::move(spec));
    }
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
        spec.scope = V;
        spec.type = SettingType::Enum;
        spec.help = "What the viewport draws on the ground: the editor grid (default), an infinite ground plane "
                    "that receives IBL and the sun's shadow, or an animated FFT ocean (A6.8; needs OpenGL 4.3, "
                    "otherwise refused with a warning and the ground is kept).";
        spec.cliFlag = "--ground <grid|plane|ocean>";
        spec.enumValues = {"grid", "plane", "ocean"};
        spec.get = [values = spec.enumValues](void* t) {
            return json(values[static_cast<size_t>(static_cast<VS*>(t)->ground.mode)]);
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
            static_cast<VS*>(t)->ground.mode = mode;
            return true;
        };
        addSetting(std::move(spec));

        SettingSpec heightMode;
        heightMode.key = "ground_height_mode";
        heightMode.scope = V;
        heightMode.type = SettingType::Enum;
        heightMode.help = "Plane height: the scene's lowest point (model_base; static pose, animation does not move it), "
                          "Y = 0 (zero) or ground_height (custom).";
        heightMode.cliFlag = "--ground-height <model-base|zero|number>";
        heightMode.enumValues = {"model_base", "zero", "custom"};
        heightMode.enumAliases = {{"model-base", "model_base"}, {"modelbase", "model_base"}, {"base", "model_base"}};
        heightMode.get = [values = heightMode.enumValues](void* t) {
            return json(values[static_cast<size_t>(static_cast<VS*>(t)->ground.heightMode)]);
        };
        heightMode.set = [values = heightMode.enumValues](void* t, const json& v, std::string&) {
            static_cast<VS*>(t)->ground.heightMode = static_cast<ux3d::imgui::GroundHeightMode>(
                std::find(values.begin(), values.end(), v.get<std::string>()) - values.begin());
            return true;
        };
        addSetting(std::move(heightMode));

        const auto groundFloat = [&](const char* key, float GS::*member, const char* help, const char* cli,
                                     std::optional<std::pair<double, double>> range, bool editsSurface) {
            SettingSpec f;
            f.key = key;
            f.scope = V;
            f.type = SettingType::Float;
            f.help = help;
            f.cliFlag = cli;
            if (range) {
                f.hasRange = true;
                f.minimum = range->first;
                f.maximum = range->second;
            }
            f.get = [member](void* t) { return agent::floatJson(static_cast<VS*>(t)->ground.*member); };
            f.set = [member, editsSurface](void* t, const json& v, std::string&) {
                auto& ground = static_cast<VS*>(t)->ground;
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
                    "Horizon fog visibility distance in scene units (distance of 2% contrast); 0 = automatic (0.9 x far plane).",
                    "--ground-haze <units>", std::pair{0.0, 1.0e9}, false);

        SettingSpec preset;
        preset.key = "ground_preset";
        preset.scope = V;
        preset.type = SettingType::Enum;
        preset.help = "Plane surface preset: sets colour, roughness and detail (concrete, grass, sand, grey); "
                      "custom keeps the current values.";
        preset.cliFlag = "--ground-preset <concrete|grass|sand|grey>";
        preset.enumValues = {"concrete", "grass", "sand", "grey", "custom"};
        preset.enumAliases = {{"tarmac", "concrete"}, {"gray", "grey"}};
        preset.get = [values = preset.enumValues](void* t) {
            return json(values[static_cast<size_t>(static_cast<VS*>(t)->ground.preset)]);
        };
        preset.set = [values = preset.enumValues](void* t, const json& v, std::string&) {
            static_cast<VS*>(t)->ground.applyPreset(static_cast<ux3d::imgui::GroundPreset>(
                std::find(values.begin(), values.end(), v.get<std::string>()) - values.begin()));
            return true;
        };
        addSetting(std::move(preset));

        SettingSpec color;
        color.key = "ground_color";
        color.scope = V;
        color.type = SettingType::Color3;
        color.help = "Plane albedo (linear base colour, also the average albedo A6.2 reads); marks the preset custom.";
        color.cliFlag = "--ground-color <r,g,b>";
        color.get = [](void* t) {
            const glm::vec3& c = static_cast<VS*>(t)->ground.albedo;
            return json{agent::floatJson(c.r), agent::floatJson(c.g), agent::floatJson(c.b)};
        };
        color.set = [](void* t, const json& v, std::string&) {
            auto& ground = static_cast<VS*>(t)->ground;
            ground.albedo = glm::vec3(v[0].get<float>(), v[1].get<float>(), v[2].get<float>());
            ground.preset = ux3d::imgui::GroundPreset::Custom;
            return true;
        };
        addSetting(std::move(color));

        SettingSpec onGround = boolSetting<VS>("ground_grid", V, &VS::showGrid, "", "--ground-grid <on|off>");
        onGround.help = "Draw the grid on top of the ground plane (needs the viewport grid switch on).";
        onGround.get = [](void* t) { return json(static_cast<VS*>(t)->ground.showGrid); };
        onGround.set = [](void* t, const json& v, std::string&) {
            static_cast<VS*>(t)->ground.showGrid = v.get<bool>();
            return true;
        };
        addSetting(std::move(onGround));

        // A6.8: the ocean (all its settings are in OceanSettings, per viewport, not persisted).
        using OS = ux3d::imgui::OceanSettings;
        const auto oceanFloat = [&](const char* key, float OS::*member, const char* help, const char* cli,
                                    std::pair<double, double> range) {
            SettingSpec f;
            f.key = key;
            f.scope = V;
            f.type = SettingType::Float;
            f.help = help;
            f.cliFlag = cli;
            f.hasRange = true;
            f.minimum = range.first;
            f.maximum = range.second;
            f.get = [member](void* t) { return agent::floatJson(static_cast<VS*>(t)->ground.ocean.*member); };
            f.set = [member](void* t, const json& v, std::string&) {
                static_cast<VS*>(t)->ground.ocean.*member = static_cast<float>(v.get<double>());
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
            c.scope = V;
            c.type = SettingType::Color3;
            c.help = "Water body colour, linear RGB 0..1: the light scattered back out of the water (times the sky's "
                     "diffuse light and the sun). The ocean's average albedo (0.066 + colour) feeds A6.2.";
            c.cliFlag = "--ocean-color <r,g,b>";
            c.hasRange = true;
            c.minimum = 0.0;
            c.maximum = 1.0;
            c.get = [](void* t) {
                const glm::vec3& v = static_cast<VS*>(t)->ground.ocean.color;
                return json{agent::floatJson(v.r), agent::floatJson(v.g), agent::floatJson(v.b)};
            };
            c.set = [](void* t, const json& v, std::string&) {
                static_cast<VS*>(t)->ground.ocean.color = glm::vec3(v[0].get<float>(), v[1].get<float>(), v[2].get<float>());
                return true;
            };
            addSetting(std::move(c));
        }
        {
            SettingSpec mode;
            mode.key = "ocean_mode";
            mode.scope = V;
            mode.type = SettingType::Enum;
            mode.help = "Ocean mode: loop (default; a precomputed set that repeats every ocean_period, per-frame cost is "
                        "texture sampling) or live (the FFT runs every frame; 512^2 at high quality). A mode change builds "
                        "the new set at once.";
            mode.cliFlag = "--ocean-mode <loop|live>";
            mode.enumValues = {"loop", "live"};
            mode.enumAliases = {{"looping", "loop"}, {"precomputed", "loop"}};
            mode.get = [](void* t) { return json(static_cast<VS*>(t)->ground.ocean.live ? "live" : "loop"); };
            mode.set = [](void* t, const json& v, std::string&) {
                static_cast<VS*>(t)->ground.ocean.live = v.get<std::string>() == "live";
                return true;
            };
            addSetting(std::move(mode));
        }
        {
            SettingSpec play = boolSetting<VS>("ocean_play", V, &VS::showGrid, "", "--ocean-play <on|off>");
            play.help = "Whether the ocean animates (on, default) or holds its current time (off; a pinned ocean_time "
                        "holds regardless).";
            play.get = [](void* t) { return json(static_cast<VS*>(t)->ground.ocean.play); };
            play.set = [](void* t, const json& v, std::string&) {
                static_cast<VS*>(t)->ground.ocean.play = v.get<bool>();
                return true;
            };
            addSetting(std::move(play));
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
    // --- Viewport: lens effect (A6.7; docs/ASOBO_EXTENSIONS_PHASE_6.md) ---
    {
        SettingSpec spec;
        spec.key = "lens_flare";
        spec.scope = V;
        spec.type = SettingType::Enum;
        spec.help = "Lens effect from the bloom mips: ghosts + halo, an anamorphic streak, or both (on = both). Off by "
                    "default; part of the bloom chain, so bloom off removes it.";
        spec.cliFlag = "--lens-flare <off|on|ghosts|streak|both>";
        spec.enumValues = {"off", "ghosts", "streak", "both"};
        spec.enumAliases = {{"on", "both"}, {"none", "off"}, {"starburst", "ghosts"}, {"anamorphic", "streak"}};
        spec.get = [values = spec.enumValues](void* t) {
            return json(values[static_cast<size_t>(std::clamp(static_cast<VS*>(t)->lensFlare, 0, 3))]);
        };
        spec.set = [values = spec.enumValues](void* t, const json& v, std::string&) {
            static_cast<VS*>(t)->lensFlare =
                static_cast<int>(std::find(values.begin(), values.end(), v.get<std::string>()) - values.begin());
            return true;
        };
        addSetting(std::move(spec));
    }
    addSetting(floatSetting<VS>("lens_flare_intensity", V, &VS::lensFlareIntensity,
                                "Strength of the lens effect (1 = default).", "--lens-flare-intensity <x>",
                                std::pair{0.0, 10.0}));
}

} // namespace ux3d::imgui::application
