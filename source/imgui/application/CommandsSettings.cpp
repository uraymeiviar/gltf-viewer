// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Settings registry of the agent control channel: registerAgentSettings registers every setting in its
// documented order through the area functions (quality and renderer-wide: CommandsSettingsQuality.cpp,
// viewport: CommandsSettingsViewport.cpp, scene environment: CommandsSettingsEnvironment.cpp); the scene
// effects and the compatibility keys are here.

#include "CommandsInternal.h"

namespace ux3d::imgui::application {

// ---------------------------------------------------------------------------------------------
// Settings registry
// ---------------------------------------------------------------------------------------------

void Application::registerAgentSettings()
{
    // Registration order is the describe / --agent-guide order.
    registerAgentQualitySettings();
    registerAgentViewportSettings();
    registerAgentSceneEffectSettings();
    registerAgentEnvironmentSettings();
    registerAgentRendererSettings();
    registerAgentCompatibilitySettings();
}

void Application::registerAgentSceneEffectSettings()
{
    using VS = ux3d::imgui::ViewportSettingsState;
    using ES = ux3d::imgui::SceneEnvironmentState;
    constexpr SettingScope E = SettingScope::Environment;
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

    // --- Scene effects (settings scopes: SceneEnvironmentState::effects; formerly per viewport, shadows renderer-wide) ---
    using FX = ux3d::imgui::SceneEffectSettings;
    const auto effectFloat = [&](std::string key, float FX::*member, std::string help, std::string cli, double lo, double hi) {
        SettingSpec spec;
        spec.key = std::move(key);
        spec.scope = E;
        spec.type = SettingType::Float;
        spec.help = std::move(help);
        spec.cliFlag = std::move(cli);
        spec.hasRange = true;
        spec.minimum = lo;
        spec.maximum = hi;
        spec.get = [member](void* t) { return agent::floatJson(static_cast<ES*>(t)->effects.*member); };
        spec.set = [member](void* t, const json& v, std::string&) {
            static_cast<ES*>(t)->effects.*member = static_cast<float>(v.get<double>());
            return true;
        };
        addSetting(std::move(spec));
    };
    effectFloat("ssao_radius", &FX::ssaoRadius,
                "SSAO radius in world units (metres in MSFS assets); occluders farther away do not count.",
                "--ssao-radius <x>", 0.01, 100.0);
    effectFloat("ssao_intensity", &FX::ssaoIntensity,
                "Exponent on the SSAO visibility (1 = the GTAO value, 0 = no occlusion).", "--ssao-intensity <x>", 0.0, 4.0);
    effectFloat("windshield_icing", &FX::windshieldIcing,
                "Windshield icing and frosted decal coverage (the aircraft's icing state).", "--windshield-icing <0..1>",
                0.0, 1.0);
    // A3.7: ASOBO_sky_portal (its on/off switch is the generic ASOBO_sky_portal viewport key).
    effectFloat("sky_portal_strength", &FX::skyPortalStrength,
                "Hidden compatibility override of the fixed default sky-portal strength 0.35; no UI control.",
                "--sky-portal-strength <0..4>", 0.0, 4.0);

    for (const std::string key : {"interior_sky_reflection", "decal_tolerance_mm"}) {
        SettingSpec spec;
        spec.key = key; spec.scope = R; spec.type = SettingType::Float;
        spec.help = "ASOBO app tuning; session-only unless set_persist(true).";
        spec.cliFlag = key == "decal_tolerance_mm" ? "--decal-tolerance <mm>" : "--interior-sky-reflection <0..1>";
        spec.hasRange = true; spec.minimum = 0; spec.maximum = key == "decal_tolerance_mm" ? 10 : 1;
        spec.defaultValue = key == "decal_tolerance_mm" ? 1.2 : 0.5;
        spec.get = [key](void*) { return json(ux3d::imgui::GltfExtensionSettings::instance().tuning(key)); };
        spec.set = [key](void*, const json& v, std::string&) {
            ux3d::imgui::GltfExtensionSettings::instance().setTuning(key, v.get<float>(), false); return true;
        };
        spec.onPersist = [key] { auto& e = ux3d::imgui::GltfExtensionSettings::instance(); e.setTuning(key, e.tuning(key), true); };
        addSetting(std::move(spec));
    }

    // glTF extension switches are an app setting (docs/SETTINGS_SCOPES.md): one global set for every viewport and
    // model, saved in settings.ini [GltfExtensions]. The canonical keys are in the renderer scope, keyed by the
    // extension name ("asobo" is the ASOBO group's master switch); the former viewport keys are compatibility keys.
    // A set changes this run only; under set_persist(true) onPersist commits the key.
    {
        using GES = ux3d::imgui::GltfExtensionSettings;
        const auto extensionSwitch = [&](std::string key, std::string group, std::string help, std::string cli) {
            SettingSpec spec;
            spec.key = std::move(key);
            spec.scope = R;
            spec.type = SettingType::Bool;
            spec.help = std::move(help);
            spec.cliFlag = std::move(cli);
            spec.defaultValue = true;
            const std::string name = group;
            spec.get = [name](void*) { return json(GES::instance().enabled(name)); };
            spec.set = [name](void*, const json& v, std::string&) {
                GES::instance().setEnabled(name, v.get<bool>(), false);
                return true;
            };
            spec.onPersist = [name] { GES::instance().commit(name); };
            addSetting(std::move(spec));
        };
        for (const ux3d::imgui::GltfExtensionGroup& group : GES::groups()) {
            extensionSwitch(group.masterKey, group.vendor,
                            "Master switch of the " + group.title + " glTF extensions (app setting, every viewport; "
                            "saved with set_persist(true)).", group.vendor == "ASOBO" ? "--asobo <on|off>" : "");
            for (const ux3d::imgui::GltfExtensionEntry& entry : group.extensions) {
                extensionSwitch(entry.name, entry.name,
                                "Apply " + entry.name + " (app setting, every viewport; saved with set_persist(true)).",
                                group.vendor == "ASOBO" ? "--asobo-disable " + entry.name : "");
            }
        }
        SettingSpec spec;
        spec.key = "show_invisible_materials";
        spec.scope = R;
        spec.type = SettingType::Bool;
        spec.help = "Also draw ASOBO_material_invisible and environment-occluder materials (app setting, every viewport).";
        spec.cliFlag = "--show-invisible-materials";
        spec.defaultValue = false;
        spec.get = [](void*) {
            auto& ext = GES::instance();
            return json(!ext.applies("ASOBO_material_invisible") && !ext.applies("ASOBO_material_environment_occluder") &&
                        ext.enabled("ASOBO"));
        };
        spec.set = [](void*, const json& v, std::string&) {
            GES::instance().setEnabled("ASOBO_material_invisible", !v.get<bool>(), false);
            GES::instance().setEnabled("ASOBO_material_environment_occluder", !v.get<bool>(), false);
            return true;
        };
        spec.onPersist = [] {
            GES::instance().commit("ASOBO_material_invisible");
            GES::instance().commit("ASOBO_material_environment_occluder");
        };
        addSetting(std::move(spec));
    }
}

void Application::registerAgentCompatibilitySettings()
{
    constexpr SettingScope V = SettingScope::Viewport;
    constexpr SettingScope E = SettingScope::Environment;
    constexpr SettingScope R = SettingScope::Renderer;

    // --- Compatibility keys (settings scopes): every key keeps working in its former scope ---
    // A forwarder has the canonical setting's type, range and default and reads/writes the canonical target (the
    // active scene's environment or the global quality); set_* marks the canonical scope's revision.
    const auto forward = [this](const std::string& key, SettingScope legacyScope, SettingScope canonicalScope) {
        const SettingSpec* found = agentRegistry.findSetting(canonicalScope, key);
        if (found == nullptr) return;
        SettingSpec spec = *found;
        spec.scope = legacyScope;
        spec.forwardsTo = canonicalScope;
        spec.compat = true;
        spec.help = std::string("Compatibility key: the ") + agent::CommandRegistry::scopeName(canonicalScope) + " setting " +
                    key + " (shared by every viewport). " + found->help;
        spec.cliFlag.clear();
        spec.aliases.clear();
        const auto targetFor = [this, canonicalScope]() -> void* {
            return canonicalScope == SettingScope::Environment
                       ? static_cast<void*>(&getSceneEnvironment(sceneEnvironments, gltfModel, activeScene))
                       : nullptr;
        };
        spec.get = [get = found->get, targetFor](void*) { return get(targetFor()); };
        spec.set = [set = found->set, targetFor](void*, const json& v, std::string& error) { return set(targetFor(), v, error); };
        agentRegistry.addSetting(std::move(spec));
    };
    for (const char* key : {"aa", "msaa", "post_aa", "ssr_quality", "ssao_quality", "ssao_resolution", "ocean_quality"}) {
        forward(key, V, R);
    }
    for (const char* key : {"windshield_icing", "sky_portal_strength",
                            "ssao_radius", "ssao_intensity"}) {
        forward(key, V, E);
    }
    // The glTF extension switches left the viewport: their viewport keys set the global (app) switch.
    forward("asobo", V, R);
    forward("show_invisible_materials", V, R);
    for (const ux3d::imgui::GltfExtensionGroup& group : ux3d::imgui::GltfExtensionSettings::groups()) {
        for (const ux3d::imgui::GltfExtensionEntry& entry : group.extensions) forward(entry.name, V, R);
    }
    for (const char* key : {"shadows", "ssr", "ssao", "bloom", "ibl", "punctual_lights", "sky_occlusion",
                            "interior_sky_reflection", "decal_tolerance_mm"}) forward(key, E, R);
    for (const char* key : {"ssr", "ssao", "lens_flare", "lens_flare_intensity", "ocean_mode", "ocean_play",
                            "interior_sky_reflection", "decal_tolerance_mm"}) forward(key, V, R);
    for (const char* key : {"ground", "ground_height_mode", "ground_height", "ground_roughness", "ground_detail",
                            "ground_detail_scale", "ground_joint_spacing", "ground_haze", "ground_preset", "ground_color",
                            "ocean_wind_speed", "ocean_wind_dir", "ocean_time", "ocean_fetch", "ocean_choppiness",
                            "ocean_swell", "ocean_foam", "ocean_period", "ocean_color"}) forward(key, V, E);
    forward("sky_occlusion_quality", E, R);
}

} // namespace ux3d::imgui::application
