// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Agent settings registry: the scene environment keys (one set per glTF scene: HDR, exposure, sun, shadows,
// time of day, sky, night, ambient, sky occlusion, emissive).

#include "CommandsInternal.h"

namespace ux3d::imgui::application {

void Application::registerAgentEnvironmentSettings()
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

    // --- Environment (one per glTF scene, shared by every viewport) ---
    addSetting(floatSetting<ES>("exposure", E, &ES::exposureEV, "Manual exposure in EV (2^EV).", "--exposure <ev>"));
    {
        // A6.4: "default" (index -1) = ACES Hill with the HDR source (as before), PBR Neutral with the procedural sky.
        SettingSpec spec;
        spec.key = "tone_mapping";
        spec.scope = E;
        spec.type = SettingType::Enum;
        spec.help = "Tone mapping operator (Khronos PBR Neutral, ACES Hill, ACES Narkowicz); default = ACES Hill with the "
                    "HDR source, PBR Neutral with the procedural sky.";
        spec.cliFlag = "--tone-mapping <default|neutral|hill|narkowicz>";
        static const std::vector<std::string> names = {"default", "neutral", "hill", "narkowicz"};
        spec.enumValues = names;
        spec.enumAliases = {{"pbr_neutral", "neutral"}, {"aces_hill", "hill"}, {"aces_narkowicz", "narkowicz"}};
        spec.get = [](void* t) {
            const int index = std::clamp(static_cast<ES*>(t)->toneMappingIdx, -1, 2);
            return json(names[static_cast<size_t>(index + 1)]);
        };
        spec.set = [](void* t, const json& v, std::string&) {
            const auto it = std::find(names.begin(), names.end(), v.get<std::string>());
            static_cast<ES*>(t)->toneMappingIdx = static_cast<int>(it - names.begin()) - 1;
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        // A6.4 exposure mode (NightSky.h). auto_exposure below is the same state seen as a switch.
        SettingSpec spec;
        spec.key = "exposure_mode";
        spec.scope = E;
        spec.type = SettingType::Enum;
        spec.help = "Exposure: manual (the exposure EV), auto (eye adaptation: metered; with the procedural sky in EV100 "
                    "with the key curve and exposure_min/max_ev100), curve (procedural sky: from the sun/moon/sky "
                    "illuminance of the time of day; Manual with the HDR) or default (Manual with the HDR source, auto "
                    "with the procedural sky).";
        spec.cliFlag = "--exposure-mode <default|manual|auto|curve>";
        static const std::vector<std::string> modes = {"default", "manual", "auto", "curve"};
        spec.enumValues = modes;
        spec.enumAliases = {{"eye_adaptation", "auto"}, {"time_of_day", "curve"}};
        spec.get = [](void* t) {
            const int index = std::clamp(static_cast<ES*>(t)->exposureMode, -1, 2);
            return json(modes[static_cast<size_t>(index + 1)]);
        };
        spec.set = [](void* t, const json& v, std::string&) {
            const auto it = std::find(modes.begin(), modes.end(), v.get<std::string>());
            static_cast<ES*>(t)->exposureMode = static_cast<int>(it - modes.begin()) - 1;
            return true;
        };
        addSetting(std::move(spec));
    }
    addSetting(floatSetting<ES>("exposure_min_ev100", E, &ES::exposureMinEV100,
                                "Procedural sky, Auto/Curve exposure: darkest scene exposed for, in EV100 (default -12, a "
                                "starlit night; Glider EV = 13.38 - EV100 + key).",
                                "--exposure-min-ev100 <ev>", std::pair{-20.0, 25.0}));
    addSetting(floatSetting<ES>("exposure_max_ev100", E, &ES::exposureMaxEV100,
                                "Procedural sky, Auto/Curve exposure: brightest scene exposed for, in EV100 (default 18).",
                                "--exposure-max-ev100 <ev>", std::pair{-20.0, 25.0}));
    addSetting(indexEnumSetting<ES>("exposure_metering", E, &ES::exposureMetering, {"average", "highlight"},
                                    "Procedural sky, Auto exposure: average (trimmed centre-weighted log average with the "
                                    "key curve) or highlight (default: the darker of the average and an exposure that keeps "
                                    "the brightest lit surfaces (0.3% of the image) below the tone mapper's shoulder; lit "
                                    "screens stay readable; the sky, moon, stars and single lights are ignored).",
                                    "--exposure-metering <average|highlight>"));
    addSetting(floatSetting<ES>("hdr_rotation", E, &ES::hdrRotation, "Environment rotation in degrees.", "--hdr-rotation <deg>"));
    addSetting(floatSetting<ES>("hdr_intensity", E, &ES::hdrIntensity, "Environment (IBL) intensity."));
    addSetting(floatSetting<ES>("hdr_blur", E, &ES::hdrBlur, "Background blur.", {}, std::pair{0.0, 1.0}));
    {
        SettingSpec spec = indexEnumSetting<ES>("time_of_day", E, &ES::timeOfDay, {"day", "night", "auto"},
                                                "ASOBO_material_emissive day/night multiplier: day, night, or auto "
                                                "(night while the sun's apparent elevation is below night_threshold). "
                                                "Not the clock: see time_of_day_mode.",
                                                "--time-of-day <day|night|auto>");
        spec.enumAliases = {{"automatic", "auto"}};
        addSetting(std::move(spec));
    }
    {
        using ux3d::imgui::TimeOfDayState;
        using ux3d::imgui::TimePreset;
        auto clockBool = [&](const char* key, bool TimeOfDayState::*member, const char* help, const char* cli) {
            SettingSpec spec;
            spec.key = key;
            spec.scope = E;
            spec.type = SettingType::Bool;
            spec.help = help;
            spec.cliFlag = cli;
            spec.get = [member](void* t) { return json(static_cast<ES*>(t)->clock.*member); };
            spec.set = [member, sunMode = std::string(key) == "time_of_day_mode"](void* t, const json& v, std::string&) {
                auto& env = *static_cast<ES*>(t);
                env.clock.*member = v.get<bool>();
                if (sunMode) {
                    // On = the Inspector's Sun Source "Time of Day" (selectSunSource: the HDR sun is enabled, so a later
                    // "off" restores the retained HDR mode, Manual or From HDR, exactly as after the UI selection).
                    if (v.get<bool>()) env.sunEnabled = true;
                    env.sunSource = v.get<bool>() ? ux3d::imgui::SunSource::Clock : !env.sunEnabled
                        ? ux3d::imgui::SunSource::Off : env.sunMode == static_cast<int>(ux3d::imgui::SunMode::FromHdr)
                        ? ux3d::imgui::SunSource::FromHdr : ux3d::imgui::SunSource::Manual;
                    env.skySunSource = static_cast<int>(v.get<bool>() ? ux3d::imgui::SunSource::Clock : ux3d::imgui::SunSource::Manual);
                }
                return true;
            };
            addSetting(std::move(spec));
        };
        auto clockFloat = [&](const char* key, float TimeOfDayState::*member, const char* help, const char* cli,
                              double minimum, double maximum, bool wrapDegrees = false) {
            SettingSpec spec;
            spec.key = key;
            spec.scope = E;
            spec.type = SettingType::Float;
            spec.help = help;
            spec.cliFlag = cli;
            spec.hasRange = true;
            spec.minimum = minimum;
            spec.maximum = maximum;
            spec.get = [member](void* t) { return agent::floatJson(static_cast<ES*>(t)->clock.*member); };
            spec.set = [member, wrapDegrees](void* t, const json& v, std::string&) {
                float value = static_cast<float>(v.get<double>());
                if (wrapDegrees) value = static_cast<float>(ux3d::imgui::solar::normalizeDegrees(value));
                static_cast<ES*>(t)->clock.*member = value;
                return true;
            };
            addSetting(std::move(spec));
        };
        clockBool("time_of_day_mode", &TimeOfDayState::enabled,
                  "Legacy source selection for both environments: on = Clock; off restores the retained HDR enable/mode "
                  "and sky Manual. Later source selections win. Not emissive day/night: see time_of_day.",
                  "--time-of-day-mode <on|off>");
        {
            SettingSpec spec;
            spec.key = "date";
            spec.scope = E;
            spec.type = SettingType::String;
            spec.help = "Local calendar date at the location, YYYY-MM-DD (1900..2100).";
            spec.cliFlag = "--date YYYY-MM-DD";
            spec.get = [](void* t) {
                const auto& c = static_cast<ES*>(t)->clock;
                return json(ux3d::imgui::formatDate(c.year, c.month, c.day));
            };
            spec.set = [](void* t, const json& v, std::string& error) {
                int y = 0, m = 0, d = 0;
                if (!ux3d::imgui::parseDate(v.get<std::string>(), y, m, d, error)) return false;
                auto& c = static_cast<ES*>(t)->clock;
                c.year = y;
                c.month = m;
                c.day = d;
                return true;
            };
            addSetting(std::move(spec));
        }
        {
            SettingSpec spec;
            spec.key = "time";
            spec.scope = E;
            spec.type = SettingType::String;
            spec.help = "Local clock time, HH:MM[:SS] (or decimal hours). Selects Custom: the time_preset is dropped.";
            spec.cliFlag = "--time HH:MM[:SS]";
            spec.get = [](void* t) {
                return json(ux3d::imgui::formatTime(ux3d::imgui::effectiveLocalHours(static_cast<ES*>(t)->clock)));
            };
            spec.set = [](void* t, const json& v, std::string& error) {
                double hours = 0.0;
                if (!ux3d::imgui::parseTime(v.get<std::string>(), hours, error)) return false;
                auto& c = static_cast<ES*>(t)->clock;
                c.hours = hours;
                c.preset = static_cast<int>(TimePreset::Custom);
                return true;
            };
            addSetting(std::move(spec));
        }
        clockFloat("utc_offset", &TimeOfDayState::utcOffsetHours,
                   "Hours the local clock is ahead of UTC (UTC = time - utc_offset), e.g. -7 for Pacific daylight time.",
                   "--utc-offset <hours>", -12.0, 14.0);
        clockFloat("latitude", &TimeOfDayState::latitude, "Latitude in degrees, positive north.", "--latitude <deg>", -90.0, 90.0);
        clockFloat("longitude", &TimeOfDayState::longitude, "Longitude in degrees, positive east.", "--longitude <deg>", -180.0, 180.0);
        clockFloat("heading", &TimeOfDayState::heading,
                   "True compass direction the model's nose points to, degrees (0 = north, 90 = east); MSFS aircraft "
                   "have their nose on glTF +Z and their left side on +X.",
                   "--heading <deg>", -3600.0, 3600.0, true);
        {
            SettingSpec spec;
            spec.key = "time_preset";
            spec.scope = E;
            spec.type = SettingType::Enum;
            spec.help = "dawn / morning / noon / afternoon / dusk / night for the current date and place (dawn and dusk: "
                        "the sun 3 degrees up; noon: solar transit; night: solar midnight; morning and afternoon: halfway). "
                        "A preset follows date, latitude, longitude and utc_offset; custom keeps the current time.";
            spec.cliFlag = "--time-preset <dawn|morning|noon|afternoon|dusk|night>";
            spec.enumValues.assign(ux3d::imgui::kTimePresetNames.begin(), ux3d::imgui::kTimePresetNames.end());
            spec.get = [](void* t) {
                const int preset = std::clamp(static_cast<ES*>(t)->clock.preset, 0, static_cast<int>(TimePreset::Count) - 1);
                return json(ux3d::imgui::kTimePresetNames[static_cast<size_t>(preset)]);
            };
            spec.set = [](void* t, const json& v, std::string&) {
                auto& c = static_cast<ES*>(t)->clock;
                const std::string name = v.get<std::string>();
                int index = 0;
                for (size_t i = 0; i < ux3d::imgui::kTimePresetNames.size(); ++i) {
                    if (name == ux3d::imgui::kTimePresetNames[i]) index = static_cast<int>(i);
                }
                if (index == static_cast<int>(TimePreset::Custom)) ux3d::imgui::materializePreset(c);
                else c.preset = index;
                return true;
            };
            addSetting(std::move(spec));
        }
        clockBool("sun_refraction", &TimeOfDayState::refraction,
                  "Atmospheric refraction raises the lit sun's elevation (about 0.5 degrees at the horizon).",
                  "--sun-refraction <on|off>");
        clockFloat("night_threshold", &TimeOfDayState::nightThreshold,
                   "time_of_day auto: night while the sun's elevation is below this many degrees (-6 = civil twilight).",
                   "--night-threshold <deg>", -90.0, 90.0);
        {
            SettingSpec spec;
            spec.key = "time_playing";
            spec.scope = E;
            spec.type = SettingType::Bool;
            spec.help = "Advance the clock by time_scale x real time (session state, not saved).";
            spec.cliFlag = "--time-play <on|off>";
            spec.get = [](void* t) { return json(static_cast<ES*>(t)->clock.playing); };
            spec.set = [](void* t, const json& v, std::string&) {
                auto& c = static_cast<ES*>(t)->clock;
                c.playing = v.get<bool>();
                if (c.playing) ux3d::imgui::materializePreset(c);
                return true;
            };
            addSetting(std::move(spec));
        }
        clockFloat("time_scale", &TimeOfDayState::timeScale,
                   "Simulated seconds per real second while time_playing (600 = 10 minutes per second).",
                   "--time-scale <x>", 0.0, 1.0e6);
    }
    {
        auto spec = floatSetting<ES>("msfs_emissive_scale", E, &ES::msfsEmissiveScale,
            "Effective ASOBO emissive scale: sky 1/12800 (cd/m^2 to environment units), HDR 1/1000. "
            "Setting a value overrides both sources. Core glTF, including KHR emissive_strength, is unchanged.",
            "--msfs-emissive-scale <x>");
        spec.get = [](void* t) { return json(ux3d::imgui::effectiveMsfsEmissiveScale(*static_cast<ES*>(t))); };
        spec.set = [](void* t, const json& v, std::string&) {
            auto& environment = *static_cast<ES*>(t);
            environment.msfsEmissiveScale = static_cast<float>(v.get<double>());
            environment.msfsEmissiveScaleOverride = true;
            return true;
        };
        addSetting(std::move(spec));
    }
    addSetting(boolSetting<ES>("bloom", E, &ES::bloomEnabled, "Physically based bloom.", "--bloom <on|off>"));
    addSetting(floatSetting<ES>("bloom_intensity", E, &ES::bloomIntensity, "Bloom intensity.", "--bloom-intensity <x>"));
    addSetting(floatSetting<ES>("bloom_radius", E, &ES::bloomRadius, "Bloom radius.", "--bloom-radius <x>"));
    {
        // The exposure mode as a switch: on = auto, off = manual (also with the procedural sky, so scripts that pass
        // --auto-exposure off keep the manual EV); get is true while Auto is in effect.
        SettingSpec spec;
        spec.key = "auto_exposure";
        spec.scope = E;
        spec.type = SettingType::Bool;
        spec.help = "Auto exposure (metered): on = exposure_mode auto, off = exposure_mode manual.";
        spec.cliFlag = "--auto-exposure <on|off>";
        spec.get = [](void* t) {
            return json(ux3d::imgui::effectiveExposureMode(*static_cast<ES*>(t)) == ux3d::imgui::ExposureMode::Auto);
        };
        spec.set = [](void* t, const json& v, std::string&) {
            static_cast<ES*>(t)->exposureMode =
                static_cast<int>(v.get<bool>() ? ux3d::imgui::ExposureMode::Auto : ux3d::imgui::ExposureMode::Manual);
            return true;
        };
        addSetting(std::move(spec));
    }
    addSetting(floatSetting<ES>("exposure_compensation", E, &ES::exposureCompensationEV,
                                              "Auto-exposure compensation in EV.", "--exposure-compensation <ev>"));
    addSetting(boolSetting<ES>("background", E, &ES::showBackground, "Skybox background (off: clear colour).", "--clear-color disables it"));
    {
        SettingSpec spec;
        spec.key = "clear_color";
        spec.scope = E;
        spec.type = SettingType::Color3;
        spec.help = "Background colour (linear) used while background is off.";
        spec.cliFlag = "--clear-color <r,g,b>";
        spec.get = [](void* t) {
            const glm::vec4& c = static_cast<ES*>(t)->clearColor;
            return json{agent::floatJson(c.r), agent::floatJson(c.g), agent::floatJson(c.b)};
        };
        spec.set = [](void* t, const json& v, std::string&) {
            static_cast<ES*>(t)->clearColor = glm::vec4(v[0].get<float>(), v[1].get<float>(), v[2].get<float>(), 1.0f);
            return true;
        };
        addSetting(std::move(spec));
    }
    addSetting(boolSetting<ES>("ibl", E, &ES::iblEnabled, "Image-based lighting."));
    addSetting(boolSetting<ES>("punctual_lights", E, &ES::punctualLightsEnabled, "KHR_lights_punctual lights."));
    // A3.1 sun (SceneEnvironmentState: conventions there and in docs/ASOBO_EXTENSIONS_PHASE_3.md).
    {
        auto spec = boolSetting<ES>("sun", E, &ES::sunEnabled,
                               "Sun (directional light). While on, the HDR's own sun is removed from IBL and background.",
                               "--sun <on|off>");
        spec.set = [](void* t, const json& v, std::string&) {
            auto& env = *static_cast<ES*>(t);
            env.sunEnabled = v.get<bool>();
            env.sunSource = !env.sunEnabled ? ux3d::imgui::SunSource::Off
                : env.sunMode == static_cast<int>(ux3d::imgui::SunMode::FromHdr) ? ux3d::imgui::SunSource::FromHdr
                                                                                 : ux3d::imgui::SunSource::Manual;
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec;
        spec.key = "sun_source";
        spec.scope = E;
        spec.type = SettingType::Enum;
        spec.enumValues = {"off", "hdr", "manual", "clock"};
        spec.cliFlag = "--sun-source <off|hdr|manual|clock>";
        spec.help = "Sun Source: HDR supports off/hdr/manual/clock; sky supports manual/clock. Sky keeps its own "
                    "choice; invalid sky choices map to manual. HDR is locked to the panorama, never moved by the clock. "
                    "Applied in order: legacy sun/sun_mode target HDR, time_of_day_mode selects both environments, "
                    "sun_source selects the current environment (last applicable selection wins).";
        spec.get = [](void* t) { return json(ux3d::imgui::sunSourceName(ux3d::imgui::effectiveSunSource(*static_cast<ES*>(t)))); };
        spec.set = [](void* t, const json& v, std::string&) {
            const std::string value = v.get<std::string>();
            ux3d::imgui::selectSunSource(*static_cast<ES*>(t), value == "off" ? ux3d::imgui::SunSource::Off
                : value == "hdr" ? ux3d::imgui::SunSource::FromHdr : value == "clock" ? ux3d::imgui::SunSource::Clock
                                                                                  : ux3d::imgui::SunSource::Manual);
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec = indexEnumSetting<ES>("sun_mode", E, &ES::sunMode, {"manual", "hdr"},
                                                "Sun from the manual angles/colour/intensity, or found in the HDR (turned with "
                                                "hdr_rotation, scaled by hdr_intensity).",
                                                "--sun-mode <manual|hdr>");
        spec.enumAliases = {{"from_hdr", "hdr"}, {"fromhdr", "hdr"}};
        spec.set = [](void* t, const json& v, std::string&) {
            auto& env = *static_cast<ES*>(t);
            env.sunMode = v.get<std::string>() == "hdr" ? 1 : 0;
            if (env.sunEnabled || env.sunSource == ux3d::imgui::SunSource::Clock) {
                env.sunSource = env.sunMode != 0 ? ux3d::imgui::SunSource::FromHdr : ux3d::imgui::SunSource::Manual;
                env.sunEnabled = true;
            }
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec;
        spec.key = "sun_dir";
        spec.aliases = {"sun_direction"};
        spec.scope = E;
        spec.type = SettingType::Vec2;
        spec.help = "Manual sun azimuth and elevation in degrees: azimuth around +Y from +Z towards +X, elevation above "
                    "the horizon (glTF Y-up).";
        spec.cliFlag = "--sun-dir <az,el>";
        spec.get = [](void* t) {
            const auto* env = static_cast<ES*>(t);
            return json{agent::floatJson(env->sunAzimuth), agent::floatJson(env->sunElevation)};
        };
        spec.set = [](void* t, const json& v, std::string& error) {
            const float elevation = v[1].get<float>();
            if (elevation < -90.0f || elevation > 90.0f) {
                error = "sun_dir elevation must be in [-90, 90]";
                return false;
            }
            static_cast<ES*>(t)->sunAzimuth = v[0].get<float>();
            static_cast<ES*>(t)->sunElevation = elevation;
            return true;
        };
        addSetting(std::move(spec));
    }
    addSetting(floatSetting<ES>("sun_azimuth", E, &ES::sunAzimuth, "Manual sun azimuth in degrees (from +Z towards +X).",
                                "--sun-dir <az,el>"));
    addSetting(floatSetting<ES>("sun_elevation", E, &ES::sunElevation, "Manual sun elevation in degrees above the horizon.",
                                "--sun-dir <az,el>", std::pair{-90.0, 90.0}));
    {
        SettingSpec spec;
        spec.key = "sun_color";
        spec.scope = E;
        spec.type = SettingType::Color3;
        spec.help = "Manual sun colour, linear RGB (luminance 1 keeps sun_intensity the illuminance).";
        spec.cliFlag = "--sun-color <r,g,b>";
        spec.hasRange = true;
        spec.minimum = 0.0;
        spec.maximum = 1000.0;
        spec.get = [](void* t) {
            const glm::vec3& c = static_cast<ES*>(t)->sunColor;
            return json{agent::floatJson(c.r), agent::floatJson(c.g), agent::floatJson(c.b)};
        };
        spec.set = [](void* t, const json& v, std::string&) {
            static_cast<ES*>(t)->sunColor = glm::vec3(v[0].get<float>(), v[1].get<float>(), v[2].get<float>());
            return true;
        };
        addSetting(std::move(spec));
    }
    addSetting(floatSetting<ES>("sun_intensity", E, &ES::sunIntensity,
                                "Manual sun illuminance (environment radiance units x sr; a white Lambert surface facing "
                                "the sun reflects colour x intensity / pi).",
                                "--sun-intensity <x>", std::pair{0.0, 1.0e6}));
    addSetting(floatSetting<ES>("sun_angular_radius", E, &ES::sunAngularRadius,
                                "Sun angular radius in degrees (widens the specular highlight; 0.27 = the real sun).",
                                "--sun-angular-radius <deg>", std::pair{0.0, 10.0}));
    // A3.8 viewer ambient light (SceneEnvironment.h kAmbientUnitLuminance): not part of the glTF.
    {
        SettingSpec spec;
        spec.key = "ambient";
        spec.scope = E;
        spec.type = SettingType::Enum;
        spec.enumValues = {"auto", "on", "off"};
        spec.enumAliases = {{"true", "on"}, {"false", "off"}, {"1", "on"}, {"0", "off"}};
        spec.help = "Viewer ambient: auto (default) = enabled with sky, disabled with HDR; on/off force it. "
                    "Legacy on selects Manual when no intensity mode was specified. Never part of the glTF.";
        spec.cliFlag = "--ambient <auto|on|off>";
        spec.get = [](void* t) { const int a = static_cast<ES*>(t)->ambient; return json(a < 0 ? "auto" : a ? "on" : "off"); };
        spec.set = [](void* t, const json& v, std::string&) {
            auto& env = *static_cast<ES*>(t);
            const std::string value = v.get<std::string>();
            env.ambient = value == "auto" ? -1 : value == "on" ? 1 : 0;
            if (value == "on" && env.ambientMode < 0) env.ambientMode = 0;
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        auto spec = indexEnumSetting<ES>("ambient_mode", E, &ES::ambientMode, {"manual", "auto"},
            "Ambient intensity: Manual uses cd/m^2; Auto uses white L = fill * outdoor horizontal illuminance / pi, "
            "weighted by hidden sky where a visibility volume is active (uniform without a volume).",
            "--ambient-mode <auto|manual>");
        spec.get = [](void* t) { return json(ux3d::imgui::effectiveAmbientAuto(*static_cast<ES*>(t)) ? "auto" : "manual"); };
        addSetting(std::move(spec));
    }
    addSetting(floatSetting<ES>("ambient_fill", E, &ES::ambientFill, "Auto ambient fill fraction k (0..1).",
                                "--ambient-fill <k>", std::pair{0.0, 1.0}));
    {
        SettingSpec spec;
        spec.key = "ambient_color";
        spec.scope = E;
        spec.type = SettingType::Color3;
        spec.help = "Ambient light colour, linear RGB (multiplies ambient_intensity; white = luminance 1).";
        spec.cliFlag = "--ambient-color <r,g,b>";
        spec.hasRange = true;
        spec.minimum = 0.0;
        spec.maximum = 1000.0;
        spec.get = [](void* t) {
            const glm::vec3& c = static_cast<ES*>(t)->ambientColor;
            return json{agent::floatJson(c.r), agent::floatJson(c.g), agent::floatJson(c.b)};
        };
        spec.set = [](void* t, const json& v, std::string&) {
            static_cast<ES*>(t)->ambientColor = glm::vec3(v[0].get<float>(), v[1].get<float>(), v[2].get<float>());
            return true;
        };
        addSetting(std::move(spec));
    }
    addSetting(floatSetting<ES>("ambient_intensity", E, &ES::ambientLuminance,
                                "Ambient luminance in cd/m^2 (1 environment unit = 12,800 cd/m^2 with both sources; the "
                                "HDR has no absolute scale, so the same unit is used for it).",
                                "--ambient-intensity <cd/m^2>", std::pair{0.0, static_cast<double>(ux3d::imgui::kAmbientMaxLuminance)}));
    // A3.9 sky occlusion inside the model (SkyOcclusion.h): scene-level, like the environment it occludes.
    {
        SettingSpec spec;
        spec.key = "sky_occlusion";
        spec.scope = E;
        spec.type = SettingType::Enum;
        spec.help = "Sky occlusion from the model's own geometry (a baked sky-visibility volume) for the IBL terms: auto "
                    "(default) = on with the procedural sky, off with the HDR. The sun, punctual lights, emissive, the "
                    "Manual ambient light and the background are unchanged. Auto ambient fills the hidden-sky fraction.";
        spec.cliFlag = "--sky-occlusion <auto|on|off>";
        spec.enumValues = {"auto", "on", "off"};
        spec.get = [](void* t) {
            const int value = static_cast<ES*>(t)->skyOcclusion;
            return json(value < 0 ? "auto" : value != 0 ? "on" : "off");
        };
        spec.set = [](void* t, const json& v, std::string&) {
            const std::string value = v.get<std::string>();
            static_cast<ES*>(t)->skyOcclusion = value == "auto" ? -1 : value == "on" ? 1 : 0;
            return true;
        };
        addSetting(std::move(spec));
    }
    addSetting(boolSetting<ES>("sky_occlusion_occluders", E, &ES::skyOcclusionOccluders,
                               "Environment occluders (ASOBO_material_environment_occluder) block the sky in the sky "
                               "occlusion bake (default on; they stay hidden in the image either way).",
                               "--sky-occlusion-occluders <on|off>"));
    // A6.7 sun disk (SunDisk.h): background only, never in the IBL input.
    {
        SettingSpec spec;
        spec.key = "sun_disk";
        spec.scope = E;
        spec.type = SettingType::Bool;
        spec.help = "Sun disk in the background (both environment sources) while the sun is on: mean radiance "
                    "E / (pi sin^2 R), limb-darkened, never in the IBL input.";
        spec.cliFlag = "--sun-disk <on|off>";
        spec.get = [](void* t) { return json(static_cast<ES*>(t)->sunDisk.enabled); };
        spec.set = [](void* t, const json& v, std::string&) {
            static_cast<ES*>(t)->sunDisk.enabled = v.get<bool>();
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec;
        spec.key = "sun_disk_scale";
        spec.scope = E;
        spec.type = SettingType::Float;
        spec.help = "Artistic size multiplier of the sun disk's angular radius (1 = sun_angular_radius); the disk keeps "
                    "the sun's illuminance, so a larger disk is dimmer. The A3.1 highlight is not scaled.";
        spec.cliFlag = "--sun-disk-scale <x>";
        spec.hasRange = true;
        spec.minimum = 0.0;
        spec.maximum = ux3d::imgui::sundisk::kMaxScale;
        spec.get = [](void* t) { return agent::floatJson(static_cast<ES*>(t)->sunDisk.scale); };
        spec.set = [](void* t, const json& v, std::string&) {
            static_cast<ES*>(t)->sunDisk.scale = static_cast<float>(v.get<double>());
            return true;
        };
        addSetting(std::move(spec));
    }
    {
        SettingSpec spec;
        spec.key = "sun_disk_limb_darkening";
        spec.scope = E;
        spec.type = SettingType::Bool;
        spec.help = "Limb darkening of the sun disk (Hestroffer & Magnan power law; off = uniform disk, same mean).";
        spec.cliFlag = "--sun-disk-limb-darkening <on|off>";
        spec.get = [](void* t) { return json(static_cast<ES*>(t)->sunDisk.limbDarkening); };
        spec.set = [](void* t, const json& v, std::string&) {
            static_cast<ES*>(t)->sunDisk.limbDarkening = v.get<bool>();
            return true;
        };
        addSetting(std::move(spec));
    }
    // A6.2 procedural sky (SceneEnvironmentState; model and units in SkyAtmosphere.h).
    {
        SettingSpec spec = indexEnumSetting<ES>("environment", E, &ES::environmentSource, {"hdr", "sky"},
                                                "Environment source: the HDR panorama, or the procedural sky (Hillaire 2020 "
                                                "atmosphere: background, IBL filtered from it, sun colour and illuminance from "
                                                "its transmittance; the sun is always on).",
                                                "--environment <hdr|sky>");
        spec.aliases = {"environment_source"};
        spec.enumAliases = {{"procedural", "sky"}, {"panorama", "hdr"}};
        addSetting(std::move(spec));
    }
    addSetting(floatSetting<ES>("sky_mie", E, &ES::skyMieDensity,
                                "Procedural sky: Mie (aerosol) density multiplier; 1 = clear sky, larger = hazier.",
                                "--sky-mie <x>", std::pair{0.0, 20.0}));
    addSetting(boolSetting<ES>("sky_ozone", E, &ES::skyOzone, "Procedural sky: ozone absorption.", "--sky-ozone <on|off>"));
    addSetting(floatSetting<ES>("sky_ground_albedo", E, &ES::skyGroundAlbedo,
                                "Procedural sky: grey ground albedo below the horizon; -1 = automatic (the plane or ocean "
                                "albedo of the lowest-id open viewport that draws one, else 0.3).",
                                "--sky-ground-albedo <x|auto>", std::pair{-1.0, 1.0}));
    // A6.6 night sky (NightSky.h): moon and stars need the A6.1 clock; the glow is part of the sky (and its IBL).
    {
        auto nightBool = [&](const char* key, bool ux3d::imgui::night::NightSettings::*member, const char* help, const char* cli) {
            SettingSpec spec;
            spec.key = key;
            spec.scope = E;
            spec.type = SettingType::Bool;
            spec.help = help;
            spec.cliFlag = cli;
            spec.get = [member](void* t) { return json(static_cast<ES*>(t)->nightSky.*member); };
            spec.set = [member](void* t, const json& v, std::string&) {
                static_cast<ES*>(t)->nightSky.*member = v.get<bool>();
                return true;
            };
            addSetting(std::move(spec));
        };
        auto nightFloat = [&](const char* key, float ux3d::imgui::night::NightSettings::*member, const char* help, const char* cli,
                              double lo, double hi) {
            SettingSpec spec;
            spec.key = key;
            spec.scope = E;
            spec.type = SettingType::Float;
            spec.help = help;
            spec.cliFlag = cli;
            spec.hasRange = true;
            spec.minimum = lo;
            spec.maximum = hi;
            spec.get = [member](void* t) { return agent::floatJson(static_cast<ES*>(t)->nightSky.*member); };
            spec.set = [member](void* t, const json& v, std::string&) {
                static_cast<ES*>(t)->nightSky.*member = static_cast<float>(v.get<double>());
                return true;
            };
            addSetting(std::move(spec));
        };
        nightBool("moon", &ux3d::imgui::night::NightSettings::moonEnabled,
                  "Procedural sky with the clock: the moon disk (A6.1 position and phase) and the moonlight (the directional "
                  "light with the A3.2 shadows while it is brighter than the sun).",
                  "--moon <on|off>");
        nightFloat("moonlight_scale", &ux3d::imgui::night::NightSettings::moonlightScale,
                   "Multiplier of the moonlight (full moon about 0.25 lux); the disk is not scaled.", "--moonlight-scale <x>",
                   0.0, 1000.0);
        nightBool("stars", &ux3d::imgui::night::NightSettings::starsEnabled,
                  "Procedural sky with the clock: procedural stars turning with the sidereal time, fading in below civil "
                  "twilight (background only).",
                  "--stars <on|off>");
        nightFloat("star_density", &ux3d::imgui::night::NightSettings::starDensity,
                   "Multiplier of the number of stars (1 = about 8,900 down to magnitude 6.5).", "--star-density <x>", 0.0, 10.0);
        nightFloat("night_vision", &ux3d::imgui::night::NightSettings::nightVision,
                   "Procedural sky with Auto/Curve exposure: largest weight of the per-pixel rod-vision shift (dim pixels "
                   "desaturate towards blue-grey below 3 cd/m^2; Jensen et al. 2000); 0 = off.", "--night-vision <0..1>", 0.0, 1.0);
        nightFloat("sky_airglow", &ux3d::imgui::night::NightSettings::airglow,
                   "Night sky glow level of the procedural sky (1 = 2e-4 cd/m^2 at the zenith, a dark moonless sky).",
                   "--sky-airglow <x>", 0.0, 1000.0);
    }
}

} // namespace ux3d::imgui::application
