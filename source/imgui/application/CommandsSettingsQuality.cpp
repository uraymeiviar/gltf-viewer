// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Agent settings registry: the renderer-wide quality keys (anti-aliasing, effect quality levels, IBL, render
// scale, ground grid, A3.2 sun shadows; RenderQualitySettings).

#include "CommandsInternal.h"

namespace ux3d::imgui::application {

void Application::registerAgentQualitySettings()
{
    using VS = ux3d::imgui::ViewportSettingsState;
    using ES = ux3d::imgui::SceneEnvironmentState;
    constexpr SettingScope R = SettingScope::Renderer;
    // Defaults come from default-constructed state, so they never drift from the structs.
    auto addSetting = [this](SettingSpec spec) {
        if (spec.scope == SettingScope::Viewport) {
            VS defaults;
            spec.defaultValue = spec.get(&defaults);
        } else if (spec.scope == SettingScope::Environment) {
            ES defaults;
            spec.defaultValue = spec.get(&defaults);
        }
        if (!spec.onPersist) spec.onPersist = [] { ux3d::imgui::RenderQuality::save(); };
        agentRegistry.addSetting(std::move(spec));
    };

    // --- Renderer-wide quality: anti-aliasing (settings scopes: formerly per viewport) ---
    using RQ = ux3d::imgui::RenderQuality;
    {
        SettingSpec spec;
        spec.key = "aa";
        spec.scope = R;
        spec.type = SettingType::Enum;
        spec.help = "Legacy AA preset of the global quality: sets MSAA and post AA together (msaa4 = 4x + none, taa = "
                    "off + TAA); custom when the pair has no preset name.";
        spec.cliFlag = "--aa <none|msaa2|msaa4|msaa8|fxaa|taa>";
        spec.enumValues = {"none", "msaa2", "msaa4", "msaa8", "fxaa", "taa"};
        spec.defaultValue = "taa";
        spec.get = [values = spec.enumValues](void*) {
            const int index = RQ::settings().legacyAAIndex();
            return json(index >= 0 && index < 6 ? values[static_cast<size_t>(index)] : "custom");
        };
        spec.set = [values = spec.enumValues](void*, const json& v, std::string&) {
            const int index = static_cast<int>(std::find(values.begin(), values.end(), v.get<std::string>()) - values.begin());
            RQ::settings().msaaIdx = index >= 1 && index <= 3 ? index : 0;
            RQ::settings().postAAIdx = index >= 4 ? index - 3 : 0;
            RQ::pinAA();
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec;
        spec.key = "msaa";
        spec.scope = R;
        spec.type = SettingType::Enum;
        spec.help = "Geometry multisampling of every viewport (0 = off).";
        spec.cliFlag = "--msaa <0|2|4|8>";
        spec.enumValues = {"0", "2", "4", "8"};
        spec.enumAliases = {{"off", "0"}, {"2x", "2"}, {"4x", "4"}, {"8x", "8"}};
        spec.defaultValue = "0";
        spec.get = [values = spec.enumValues](void*) {
            return json(values[static_cast<size_t>(std::clamp(RQ::settings().msaaIdx, 0, 3))]);
        };
        spec.set = [values = spec.enumValues](void*, const json& v, std::string&) {
            RQ::settings().msaaIdx = static_cast<int>(std::find(values.begin(), values.end(), v.get<std::string>()) - values.begin());
            RQ::pinAA();
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec;
        spec.key = "post_aa";
        spec.scope = R;
        spec.type = SettingType::Enum;
        spec.help = "Post-process anti-aliasing of every viewport.";
        spec.cliFlag = "--post-aa <none|fxaa|taa>";
        spec.enumValues = {"none", "fxaa", "taa"};
        spec.defaultValue = "taa";
        spec.get = [values = spec.enumValues](void*) {
            return json(values[static_cast<size_t>(std::clamp(RQ::settings().postAAIdx, 0, 2))]);
        };
        spec.set = [values = spec.enumValues](void*, const json& v, std::string&) {
            RQ::settings().postAAIdx = static_cast<int>(std::find(values.begin(), values.end(), v.get<std::string>()) - values.begin());
            RQ::pinAA();
            return true;
        };
        addSetting(std::move(spec));
    }
    using QS = ux3d::imgui::RenderQualitySettings;
    const auto qualityBool = [&](const char* key, bool QS::*member, const char* cli, std::function<void()> pin = {}) {
        SettingSpec spec;
        spec.key = key; spec.scope = R; spec.type = SettingType::Bool;
        spec.help = "Quality effect enablement, shared by all viewports; the corresponding level retains its sample budget.";
        spec.cliFlag = cli; spec.defaultValue = QS{}.*member;
        spec.get = [member](void*) { return json(RQ::settings().*member); };
        spec.set = [member, pin](void*, const json& v, std::string&) {
            RQ::settings().*member = v.get<bool>(); if (pin) pin(); return true;
        };
        spec.onPersist = [] { RQ::save(); };
        addSetting(std::move(spec));
    };
    qualityBool("shadows", &QS::shadows, "--shadows <on|off>", [] { RQ::pinShadows(); });
    qualityBool("ssr", &QS::ssrEnabled, "--ssr <on|off>", [] { RQ::pinSsr(); });
    qualityBool("ssao", &QS::ssaoEnabled, "--ssao <on|off>");
    qualityBool("bloom", &QS::bloomEnabled, "--bloom <on|off>", [] { RQ::pinBloom(); });
    qualityBool("ibl", &QS::iblEnabled, "", [] { RQ::pinIbl(); });
    qualityBool("punctual_lights", &QS::lightsEnabled, "");
    qualityBool("ocean_play", &QS::oceanPlay, "--ocean-play <on|off>");
    {
        SettingSpec spec;
        spec.key = "ocean_mode"; spec.scope = R; spec.type = SettingType::Enum;
        spec.help = "Quality ocean simulation mode: precomputed loop or live FFT.";
        spec.cliFlag = "--ocean-mode <loop|live>"; spec.enumValues = {"loop", "live"};
        spec.enumAliases = {{"looping", "loop"}, {"precomputed", "loop"}}; spec.defaultValue = "loop";
        spec.get = [](void*) { return json(RQ::settings().oceanLive ? "live" : "loop"); };
        spec.set = [](void*, const json& v, std::string&) { RQ::settings().oceanLive = v == "live"; return true; };
        spec.onPersist = [] { RQ::save(); }; addSetting(std::move(spec));
    }
    {
        SettingSpec spec;
        spec.key = "sky_occlusion"; spec.scope = R; spec.type = SettingType::Enum;
        spec.help = "Quality sky occlusion: auto with procedural sky, on with either source, or off.";
        spec.cliFlag = "--sky-occlusion <auto|on|off>"; spec.enumValues = {"auto", "off", "on"}; spec.defaultValue = "auto";
        spec.get = [](void*) { int v = RQ::settings().skyOcclusion; return json(v < 0 ? "auto" : v ? "on" : "off"); };
        spec.set = [](void*, const json& v, std::string&) { RQ::settings().skyOcclusion = v == "auto" ? -1 : v == "on" ? 1 : 0; return true; };
        spec.onPersist = [] { RQ::save(); }; addSetting(std::move(spec));
    }
    const auto mapSize = [&](const char* key, bool QS::*enabled, uint32_t QS::*size, std::vector<std::string> values) {
        SettingSpec spec;
        spec.key = key; spec.scope = R; spec.type = SettingType::Enum;
        spec.help = "Quality map level: off or a size in texels. Off retains the previous allocation budget.";
        spec.enumValues = values; spec.enumAliases = {{"0", "off"}};
        spec.defaultValue = std::to_string(QS{}.*size);
        spec.get = [enabled,size](void*) { const auto& q = RQ::settings(); return json(q.*enabled ? std::to_string(q.*size) : "off"); };
        spec.set = [enabled,size](void*, const json& v, std::string&) {
            auto& q = RQ::settings(); q.*enabled = v != "off";
            if (v != "off") q.*size = static_cast<uint32_t>(std::stoul(v.get<std::string>()));
            if (size == &QS::iblCubeSize) RQ::pinIbl(); else RQ::pinShadows(); return true;
        };
        spec.onPersist = [] { RQ::save(); }; addSetting(std::move(spec));
    };
    mapSize("shadow_map", &QS::shadows, &QS::shadowResolution, {"off","1024","2048","4096"});
    mapSize("ibl_cube", &QS::iblEnabled, &QS::iblCubeSize, {"off","256","512","1024"});
    {
        SettingSpec spec;
        spec.key = "texture_max_size"; spec.scope = R; spec.type = SettingType::Int;
        spec.help = "Quality texture cap: 0 unlimited, or a power of two 64..32768. Re-decodes retained sources.";
        spec.hasRange = true; spec.minimum = 0; spec.maximum = 32768; spec.defaultValue = 0;
        spec.get = [](void*) { return json(RQ::settings().textureMaxSize); };
        spec.set = [](void*, const json& v, std::string& error) {
            const auto n = v.get<uint32_t>();
            if (n && (n < 64 || (n & (n - 1)))) { error = "texture cap must be 0 or a power of two 64..32768"; return false; }
            RQ::settings().textureMaxSize = n; return true;
        };
        spec.onPersist = [] { RQ::save(); }; addSetting(std::move(spec));
    }
    const auto level = [&](const char* key, bool QS::*enabled, int QS::*budget, std::function<void()> pin) {
        SettingSpec spec;
        spec.key = key; spec.scope = R; spec.type = SettingType::Enum;
        spec.help = "Quality level: off / low / mid / high. Off retains the last sample budget for compatibility.";
        spec.enumValues = {"off", "low", "mid", "high"}; spec.enumAliases = {{"medium", "mid"}};
        auto get = [enabled, budget](const QS& q) { static const char* names[] = {"low", "mid", "high"}; return json(q.*enabled ? names[std::clamp(q.*budget,0,2)] : "off"); };
        spec.defaultValue = get(QS{}); spec.get = [get](void*) { return get(RQ::settings()); };
        spec.set = [enabled, budget, pin](void*, const json& v, std::string&) {
            auto& q = RQ::settings(); q.*enabled = v != "off";
            if (v != "off") q.*budget = v == "low" ? 0 : v == "mid" ? 1 : 2;
            if (pin) pin(); return true;
        };
        spec.onPersist = [] { RQ::save(); }; addSetting(std::move(spec));
    };
    level("reflections", &QS::ssrEnabled, &QS::ssrQuality, [] { RQ::pinSsr(); });
    level("ssao_level", &QS::ssaoEnabled, &QS::ssaoQuality, {});
    // Renderer-wide quality as an enum over an int field of RenderQualitySettings.
    const auto qualityEnum = [&](std::string key, int ux3d::imgui::RenderQualitySettings::*member, std::vector<std::string> values,
                                 std::string help, std::string cli, std::map<std::string, std::string> aliases,
                                 std::function<void()> onSet) {
        SettingSpec spec;
        spec.key = std::move(key);
        spec.scope = R;
        spec.type = SettingType::Enum;
        spec.help = std::move(help);
        spec.cliFlag = std::move(cli);
        spec.enumValues = values;
        spec.enumAliases = std::move(aliases);
        const ux3d::imgui::RenderQualitySettings defaults;
        spec.defaultValue = values[static_cast<size_t>(std::clamp(defaults.*member, 0, static_cast<int>(values.size()) - 1))];
        spec.get = [member, values](void*) {
            return json(values[static_cast<size_t>(std::clamp(RQ::settings().*member, 0, static_cast<int>(values.size()) - 1))]);
        };
        spec.set = [member, values, onSet](void*, const json& v, std::string&) {
            RQ::settings().*member = static_cast<int>(std::find(values.begin(), values.end(), v.get<std::string>()) - values.begin());
            if (onSet) onSet();
            return true;
        };
        addSetting(std::move(spec));
    };
    qualityEnum("lens_flare", &QS::lensFlare, {"off", "ghosts", "streak", "both"},
                "Quality lens effect: off, ghosts + halo, anamorphic streak, ghosts + streak; intensity is also Quality.",
                "--lens-flare <off|on|ghosts|streak|both>", {{"on", "both"}, {"none", "off"}, {"starburst", "ghosts"}, {"anamorphic", "streak"}}, nullptr);
    {
        SettingSpec spec;
        spec.key = "lens_flare_intensity"; spec.scope = R; spec.type = SettingType::Float;
        spec.help = "Quality lens effect intensity (0..10), shared by every viewport.";
        spec.cliFlag = "--lens-flare-intensity <x>"; spec.hasRange = true; spec.minimum = 0; spec.maximum = 10; spec.defaultValue = 1.0;
        spec.get = [](void*) { return json(RQ::settings().lensFlareIntensity); };
        spec.set = [](void*, const json& v, std::string&) { RQ::settings().lensFlareIntensity = v.get<float>(); return true; };
        addSetting(std::move(spec));
    }
    qualityEnum("ssr_quality", &ux3d::imgui::RenderQualitySettings::ssrQuality, {"low", "medium", "high"},
                "SSR sample budget 256/512/1024 (every viewport).", "--ssr-quality <low|medium|high>", {}, [] { RQ::pinSsr(); });
    qualityEnum("ssao_quality", &ux3d::imgui::RenderQualitySettings::ssaoQuality, {"low", "medium", "high"},
                "SSAO samples per pixel: low 2 slices x 4 steps, medium 3 x 6, high 4 x 8 (per side; every viewport).",
                "--ssao-quality <low|medium|high>", {}, nullptr);
    {
        SettingSpec resolution;
        resolution.key = "ssao_resolution";
        resolution.scope = R;
        resolution.type = SettingType::Enum;
        resolution.help = "SSAO resolution of every viewport: full, or half with a depth-aware upsample.";
        resolution.cliFlag = "--ssao-resolution <full|half>";
        resolution.enumValues = {"full", "half"};
        resolution.defaultValue = "full";
        resolution.get = [](void*) { return json(RQ::settings().ssaoHalfResolution ? "half" : "full"); };
        resolution.set = [](void*, const json& v, std::string&) {
            RQ::settings().ssaoHalfResolution = v.get<std::string>() == "half";
            return true;
        };
        addSetting(std::move(resolution));
    }
    qualityEnum("sky_occlusion_quality", &ux3d::imgui::RenderQualitySettings::skyOcclusionQuality, {"low", "medium", "high"},
                "Sky occlusion bake: low 64 directions x up to 64 cells per axis, medium (default) 128 x 96, high 256 x 128 "
                "(renderer-wide).",
                "--sky-occlusion-quality <low|medium|high>", {}, nullptr);
    qualityEnum("ocean_quality", &ux3d::imgui::RenderQualitySettings::oceanQuality, {"low", "medium", "high"},
                "Ocean simulation quality of every viewport: low (2 cascades of 128^2, 32 frames, 12 MiB), medium (3 x "
                "256^2, 48 frames, 101 MiB), high (4 x 256^2, 64 frames, 176 MiB); live mode: 2 x 128^2, 3 x 256^2, "
                "4 x 512^2; recomputes the loop.",
                "--ocean-quality <low|medium|high>", {{"med", "medium"}, {"mid", "medium"}}, nullptr);
}

void Application::registerAgentRendererSettings()
{
    using VS = ux3d::imgui::ViewportSettingsState;
    using ES = ux3d::imgui::SceneEnvironmentState;
    constexpr SettingScope R = SettingScope::Renderer;
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

    // --- Renderer-wide ---
    {
        SettingSpec spec;
        spec.key = "sky_time_slicing";
        spec.scope = R;
        spec.type = SettingType::Bool;
        spec.help = "Procedural sky: spread IBL re-filtering over frames (reduced size while the sky changes, refined when "
                    "it stops). Off filters every change at the configured quality before the frame (reference path).";
        spec.cliFlag = "--sky-time-slicing <on|off>";
        spec.defaultValue = true;
        spec.get = [this](void*) { return json(s_skyTimeSlicing); };
        spec.set = [this](void*, const json& v, std::string&) {
            s_skyTimeSlicing = v.get<bool>();
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec;
        spec.key = "sky_budget_ms";
        spec.scope = R;
        spec.type = SettingType::Float;
        spec.help = "Procedural sky: GPU milliseconds per frame the time-sliced IBL work aims at (stated limit 4 ms).";
        spec.cliFlag = "--sky-budget-ms <ms>";
        spec.hasRange = true;
        spec.minimum = 0.1;
        spec.maximum = 100.0;
        spec.defaultValue = 2.5;
        spec.get = [this](void*) { return agent::floatJson(s_skyBudgetMs); };
        spec.set = [this](void*, const json& v, std::string&) {
            s_skyBudgetMs = static_cast<float>(v.get<double>());
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec;
        spec.key = "msfs_emissive";
        spec.scope = R;
        spec.type = SettingType::Bool;
        spec.help = "MSFS emissive mapping for ASOBO assets (off: clamped core emissive factor).";
        spec.cliFlag = "--msfs-emissive <on|off>";
        spec.defaultValue = true;
        spec.get = [this](void*) { return json(!s_disableMsfsEmissive); };
        spec.set = [this](void*, const json& v, std::string&) {
            s_disableMsfsEmissive = !v.get<bool>();
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec;
        spec.key = "grid_mode";
        spec.scope = R;
        spec.type = SettingType::Enum;
        spec.help = "Ground grid technique (renderer-wide).";
        spec.cliFlag = "--grid-mode <ray|plane>";
        spec.enumValues = {"ray", "plane"};
        spec.enumAliases = {{"screenray", "ray"}, {"planemesh", "plane"}};
        spec.persisted = true;
        spec.defaultValue = "ray";
        spec.get = [](void*) {
            return json(ux3d::imgui::RenderQuality::settings().gridMode == ux3d::imgui::GridMode::PlaneMesh ? "plane" : "ray");
        };
        spec.set = [](void*, const json& v, std::string&) {
            ux3d::imgui::RenderQuality::settings().gridMode =
                v.get<std::string>() == "plane" ? ux3d::imgui::GridMode::PlaneMesh : ux3d::imgui::GridMode::ScreenRay;
            return true;
        };
        addSetting(std::move(spec));
    }

    // --- Renderer-wide: A3.2 sun shadows (RenderQualitySettings; saved preferences like grid_mode) ---
    {
        using RQS = ux3d::imgui::RenderQualitySettings;
        const RQS defaults;
        auto shadowSpec = [](std::string key, SettingType type, std::string help, std::string cli) {
            SettingSpec spec;
            spec.key = std::move(key);
            spec.scope = SettingScope::Renderer;
            spec.type = type;
            spec.help = std::move(help);
            spec.cliFlag = std::move(cli);
            spec.persisted = true;
            return spec;
        };
        {
            // Old saved enable key, retained for existing clients (settings.ini shadows=).
            SettingSpec spec = shadowSpec("shadows_allowed", SettingType::Bool,
                                          "Compatibility key: Quality shadow-map enablement, same as shadows.", "");
            spec.compat = true;
            spec.forwardsTo = R;
            spec.defaultValue = defaults.shadows;
            spec.get = [](void*) { return json(ux3d::imgui::RenderQuality::settings().shadows); };
            spec.set = [](void*, const json& v, std::string&) {
                ux3d::imgui::RenderQuality::settings().shadows = v.get<bool>();
                ux3d::imgui::RenderQuality::pinShadows();
                return true;
            };
            addSetting(std::move(spec));
        }
        {
            SettingSpec spec = shadowSpec("shadow_resolution", SettingType::Enum,
                                          "Shadow map size per cascade (texels per side).", "--shadow-resolution <1024|2048|4096>");
            spec.enumValues = {"1024", "2048", "4096"};
            spec.defaultValue = std::to_string(defaults.shadowResolution);
            spec.get = [](void*) { return json(std::to_string(ux3d::imgui::RenderQuality::settings().shadowResolution)); };
            spec.set = [](void*, const json& v, std::string&) {
                ux3d::imgui::RenderQuality::settings().shadowResolution =
                    static_cast<uint32_t>(std::stoul(v.get<std::string>()));
                ux3d::imgui::RenderQuality::pinShadows();
                return true;
            };
            addSetting(std::move(spec));
        }
        {
            SettingSpec spec = shadowSpec("shadow_cascades", SettingType::Int, "Number of shadow cascades.",
                                          "--shadow-cascades <2..4>");
            spec.hasRange = true;
            spec.minimum = 2.0;
            spec.maximum = 4.0;
            spec.defaultValue = defaults.shadowCascades;
            spec.get = [](void*) { return json(ux3d::imgui::RenderQuality::settings().shadowCascades); };
            spec.set = [](void*, const json& v, std::string&) {
                ux3d::imgui::RenderQuality::settings().shadowCascades = v.get<int>();
                ux3d::imgui::RenderQuality::pinShadows();
                return true;
            };
            addSetting(std::move(spec));
        }
        {
            SettingSpec spec = shadowSpec("shadow_filter", SettingType::Enum,
                                          "Shadow filter: hard (1 bilinear compare), pcf (3x3), soft (16 rotated Poisson).",
                                          "--shadow-filter <hard|pcf|soft>");
            spec.enumValues = {"hard", "pcf", "soft"};
            spec.defaultValue = ux3d::imgui::RenderQuality::shadowFilterName(defaults.shadowFilter);
            spec.get = [](void*) {
                return json(ux3d::imgui::RenderQuality::shadowFilterName(ux3d::imgui::RenderQuality::settings().shadowFilter));
            };
            spec.set = [](void*, const json& v, std::string&) {
                const std::string name = v.get<std::string>();
                ux3d::imgui::RenderQuality::settings().shadowFilter = name == "hard" ? ux3d::imgui::ShadowFilter::Hard
                    : name == "soft" ? ux3d::imgui::ShadowFilter::Soft : ux3d::imgui::ShadowFilter::Pcf;
                ux3d::imgui::RenderQuality::pinShadows();
                return true;
            };
            addSetting(std::move(spec));
        }
        const auto floatShadow = [&](std::string key, float RQS::*member, std::string help, std::string cli, double lo, double hi) {
            SettingSpec spec = shadowSpec(std::move(key), SettingType::Float, std::move(help), std::move(cli));
            spec.hasRange = true;
            spec.minimum = lo;
            spec.maximum = hi;
            spec.defaultValue = agent::floatJson(defaults.*member);
            spec.get = [member](void*) { return agent::floatJson(ux3d::imgui::RenderQuality::settings().*member); };
            spec.set = [member](void*, const json& v, std::string&) {
                ux3d::imgui::RenderQuality::settings().*member = v.get<float>();
                ux3d::imgui::RenderQuality::pinShadows();
                return true;
            };
            addSetting(std::move(spec));
        };
        floatShadow("shadow_distance", &RQS::shadowDistance, "View depth (metres) where the sun shadows end.",
                    "--shadow-distance <m>", 1.0, 100000.0);
        floatShadow("shadow_split_lambda", &RQS::shadowSplitLambda,
                    "Cascade splits: 0 uniform .. 1 logarithmic (practical split scheme).", "--shadow-split-lambda <0..1>",
                    0.0, 1.0);
        floatShadow("shadow_normal_bias", &RQS::shadowNormalBias, "Receiver normal offset in shadow-map texels.",
                    "--shadow-normal-bias <x>", 0.0, 100.0);
        floatShadow("shadow_slope_bias", &RQS::shadowSlopeBias, "Caster slope-scaled depth bias (glPolygonOffset factor).",
                    "--shadow-slope-bias <x>", 0.0, 100.0);
        floatShadow("shadow_constant_bias", &RQS::shadowConstantBias, "Caster constant depth bias (glPolygonOffset units).",
                    "--shadow-constant-bias <x>", 0.0, 100.0);
    }
}

} // namespace ux3d::imgui::application
