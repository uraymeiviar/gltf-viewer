// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/GladGL.h"
#include "imgui/RenderQuality.h"
#include "imgui/GltfExtensionSettings.h"
#include "imgui/MemoryInfo.h"
#include "imgui/SettingsScopes.h"
#include "imgui/ViewportSettingsPanel.h"
#include "core/LoadProgress.h"
#include "core/Logger.h"
#include <chrono>
#include <unordered_map>
#include <array>
#include <utility>
#include "imgui/SettingsFile.h"
#include "imgui/SettingsStorage.h"
#include <sstream>
#include "imgui/UiProbe.h"
#include "renderer/gltf/GltfRenderLayer.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace ux3d::imgui {

namespace {

bool s_persistenceEnabled = true;
RenderQualitySettings s_savedQuality, s_observedQuality;
int s_savedSelection = 0, s_observedSelection = 0;
bool s_sessionReady = false;
bool s_autoSuppressed = false;
bool s_pinIbl = false, s_pinScale = false, s_pinAA = false, s_pinSsr = false, s_pinBloom = false, s_pinShadow = false;
int s_autoLevel = 0, s_autoCeiling = 6;
unsigned s_autoSamples = 0;
double s_autoMs = 0;
auto s_autoWindow = std::chrono::steady_clock::now();
auto s_retryAfter = s_autoWindow;
std::string s_autoStatus = "Auto quality: starting";
RenderQualitySettings s_appliedIbl;
bool s_haveAppliedIbl = false;
GLuint s_gpuQueries[4][2]{};
bool s_gpuPending[4]{};
bool s_gpuRendered[4]{};
int s_gpuWriting = -1, s_gpuNext = 0;
double s_gpuMs = 0;
bool s_profilesActive = false, s_textureRefresh = false, s_showRuntimeUi = false;
int s_selectedProfile = 0;
uint64_t s_profileVersion = 1;

QualityProfile defaultProfile(int index)
{
    QualityProfile p;
    p.render.autoQuality = false;
    p.render.ssrEnabled = p.render.bloomEnabled = false;
    // AA and SSR quality as the profiles had them before they moved into render (Low: none / low).
    p.render.msaaIdx = 0;
    p.render.postAAIdx = 0;
    p.render.ssrQuality = 0;
    // A3.2 sun shadows: Low 1024 x 2 hard, Mid 1024 x 3 PCF, High 2048 x 3 PCF (the default), Ultra 2048 x 4 soft.
    p.render.shadowResolution = index <= 1 ? 1024 : 2048;
    p.render.shadowCascades = index == 0 ? 2 : (index == 3 ? 4 : 3);
    p.render.shadowFilter = index == 0 ? ShadowFilter::Hard : (index == 3 ? ShadowFilter::Soft : ShadowFilter::Pcf);
    // A3.5 SSAO (maintainer's choice, 2026-10-31): Low off, Mid low, High mid, Ultra high. Low keeps the retained
    // budget (mid) for a later switch on. Capture and test launches do not apply a profile and keep SSAO off.
    p.render.ssaoEnabled = index >= 1;
    if (index >= 1) p.render.ssaoQuality = index - 1;
    if (index == 0) {
        p.render.textureMaxSize = 512; p.skybox = p.render.iblEnabled = false;
        p.render.gridMode = GridMode::PlaneMesh;
        p.render.iblCubeSize = 256; p.render.iblIrradianceSize = 32;
        p.render.iblLutSize = 128; p.render.iblSpecularSamples = 128; p.render.iblHalfFloat = true;
    } else if (index == 1) {
        p.render.textureMaxSize = 1024; p.render.msaaIdx = 1;
        p.render.iblCubeSize = 512; p.render.iblIrradianceSize = 64;
        p.render.iblLutSize = 256; p.render.iblSpecularSamples = 512; p.render.iblHalfFloat = true;
    } else {
        p.render.textureMaxSize = index == 2 ? 4096 : 0;
        p.render.msaaIdx = index == 3 ? 2 : 1;
        p.render.postAAIdx = 2; p.render.ssrEnabled = p.render.bloomEnabled = true; p.render.ssrQuality = index == 3 ? 2 : 1;
    }
    return p;
}
std::array<QualityProfile,4> s_profiles = {defaultProfile(0), defaultProfile(1), defaultProfile(2), defaultProfile(3)};
const QualityProfile& targetProfile() { return s_profiles[s_selectedProfile == 0 ? 3 : s_selectedProfile-1]; }

const uint32_t kCubeSizes[] = { 256, 512, 1024 };
const uint32_t kIrradianceSizes[] = { 0, 32, 64, 128, 256 };
const uint32_t kLutSizes[] = { 128, 256, 512, 1024 };
const uint32_t kSampleCounts[] = { 128, 256, 512, 1024 };
const float kRenderScales[] = { 0.5f, 0.67f, 0.75f, 0.85f, 1.0f };
const uint32_t kShadowResolutions[] = { 1024, 2048, 4096 };
constexpr float kShadowDistanceMin = 1.0f;
constexpr float kShadowDistanceMax = 100000.0f;

bool validShadowDistance(float value) { return std::isfinite(value) && value >= kShadowDistanceMin && value <= kShadowDistanceMax; }
bool validShadowLambda(float value) { return std::isfinite(value) && value >= 0.0f && value <= 1.0f; }
bool validShadowBias(float value) { return std::isfinite(value) && value >= 0.0f && value <= 100.0f; }

template <typename T, size_t N>
bool isOneOf(T value, const T (&values)[N])
{
    return std::find(std::begin(values), std::end(values), value) != std::end(values);
}

// Applies a parsed value only when it is one the UI offers.
template <typename T, size_t N>
bool setChecked(T& field, T value, const T (&allowed)[N])
{
    if (!isOneOf(value, allowed)) {
        return false;
    }
    field = value;
    return true;
}

bool parseUint(const std::string& text, uint32_t& value)
{
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(text.c_str(), &end, 10);
    if (end == text.c_str() || (end != nullptr && *end != '\0') || parsed > 0xFFFFFFFFul) {
        return false;
    }
    value = static_cast<uint32_t>(parsed);
    return true;
}

bool parseInt(const std::string& text, int& value)
{
    char* end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || (end != nullptr && *end != '\0')) {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

bool parseFloat(const std::string& text, float& value)
{
    char* end = nullptr;
    const float parsed = std::strtof(text.c_str(), &end);
    if (end == text.c_str() || (end != nullptr && *end != '\0') || !std::isfinite(parsed)) {
        return false;
    }
    value = parsed;
    return true;
}

bool parseBool(const std::string& text, bool& value)
{
    if (text == "1" || text == "true" || text == "True") { value = true; return true; }
    if (text == "0" || text == "false" || text == "False") { value = false; return true; }
    return false;
}

std::string intText(long long value)
{
    return std::to_string(value);
}

std::string boolText(bool value)
{
    return value ? "1" : "0";
}

std::string floatText(float value)
{
    std::ostringstream out;
    out.precision(9);
    out << value;
    return out.str();
}

bool isValidTextureMax(uint32_t value)
{
    const auto power = [](uint32_t n) { return n && !(n & (n - 1)); };
    return value == 0 || (power(value) && value >= 64 && value <= 32768);
}

// New layer fields are read in both sections; old gate and budget keys remain accepted below.
bool readLayerField(RenderQualitySettings& q, const std::string& key, const std::string& value, bool* applied = nullptr)
{
    bool b = false; int i = 0; float f = 0; uint32_t u = 0;
    if (key == "ssaoHalfResolution") { if (parseBool(value,b)) { q.ssaoHalfResolution = b; if (applied) *applied = true; } else logSettingsMessage("Invalid SSAO resolution",true); return true; }
    if (key == "ssrEnabled") { if (parseBool(value,b)) { q.ssrEnabled = b; if (applied) *applied = true; } else logSettingsMessage("Invalid Quality ssrEnabled", true); return true; }
    if (key == "ssaoEnabled") { if (parseBool(value,b)) { q.ssaoEnabled = b; if (applied) *applied = true; } else logSettingsMessage("Invalid Quality ssaoEnabled", true); return true; }
    if (key == "iblEnabled") { if (parseBool(value,b)) { q.iblEnabled = b; if (applied) *applied = true; } else logSettingsMessage("Invalid Quality iblEnabled", true); return true; }
    if (key == "bloomEnabled") { if (parseBool(value,b)) { q.bloomEnabled = b; if (applied) *applied = true; } else logSettingsMessage("Invalid Quality bloomEnabled", true); return true; }
    if (key == "lightsEnabled") { if (parseBool(value,b)) { q.lightsEnabled = b; if (applied) *applied = true; } else logSettingsMessage("Invalid Quality lightsEnabled", true); return true; }
    if (key == "oceanLive") { if (parseBool(value,b)) { q.oceanLive = b; if (applied) *applied = true; } else logSettingsMessage("Invalid Quality oceanLive", true); return true; }
    if (key == "oceanPlay") { if (parseBool(value,b)) { q.oceanPlay = b; if (applied) *applied = true; } else logSettingsMessage("Invalid Quality oceanPlay", true); return true; }
    if (key == "msaaIdx") { if (parseInt(value,i) && i >= 0 && i <= 3) { q.msaaIdx = i; if (applied) *applied = true; } else logSettingsMessage("Invalid Quality msaaIdx", true); return true; }
    if (key == "postAAIdx") { if (parseInt(value,i) && i >= 0 && i <= 2) { q.postAAIdx = i; if (applied) *applied = true; } else logSettingsMessage("Invalid Quality postAAIdx", true); return true; }
    if (key == "ssrQuality") { if (parseInt(value,i) && i >= 0 && i <= 2) { q.ssrQuality = i; if (applied) *applied = true; } else logSettingsMessage("Invalid Quality ssrQuality", true); return true; }
    if (key == "ssaoQuality") { if (parseInt(value,i) && i >= 0 && i <= 2) { q.ssaoQuality = i; if (applied) *applied = true; } else logSettingsMessage("Invalid Quality ssaoQuality", true); return true; }
    if (key == "skyOcclusionQuality") { if (parseInt(value,i) && i >= 0 && i <= 2) { q.skyOcclusionQuality = i; if (applied) *applied = true; } else logSettingsMessage("Invalid Quality skyOcclusionQuality", true); return true; }
    if (key == "oceanQuality") { if (parseInt(value,i) && i >= 0 && i <= 2) { q.oceanQuality = i; if (applied) *applied = true; } else logSettingsMessage("Invalid Quality oceanQuality", true); return true; }
    if (key == "skyOcclusion") { if (parseInt(value,i) && i >= -1 && i <= 1) { q.skyOcclusion = i; if (applied) *applied = true; } else logSettingsMessage("Invalid Quality skyOcclusion", true); return true; }
    if (key == "lensFlare") { if (parseInt(value,i) && i >= 0 && i <= 3) { q.lensFlare = i; if (applied) *applied = true; } else logSettingsMessage("Invalid Quality lensFlare", true); return true; }
    if (key == "lensFlareIntensity") { if (parseFloat(value,f) && f >= 0 && f <= 10) { q.lensFlareIntensity = f; if (applied) *applied = true; } else logSettingsMessage("Invalid Quality lensFlareIntensity", true); return true; }
    if (key == "shadowSplitLambda") { if (parseFloat(value,f) && f >= 0 && f <= 1) { q.shadowSplitLambda = f; if (applied) *applied = true; } else logSettingsMessage("Invalid Quality shadowSplitLambda", true); return true; }
    if (key == "shadowNormalBias") { if (parseFloat(value,f) && f >= 0 && f <= 100) { q.shadowNormalBias = f; if (applied) *applied = true; } else logSettingsMessage("Invalid Quality shadowNormalBias", true); return true; }
    if (key == "shadowSlopeBias") { if (parseFloat(value,f) && f >= 0 && f <= 100) { q.shadowSlopeBias = f; if (applied) *applied = true; } else logSettingsMessage("Invalid Quality shadowSlopeBias", true); return true; }
    if (key == "shadowConstantBias") { if (parseFloat(value,f) && f >= 0 && f <= 100) { q.shadowConstantBias = f; if (applied) *applied = true; } else logSettingsMessage("Invalid Quality shadowConstantBias", true); return true; }
    if (key == "textureMaxSize") { if (parseUint(value,u) && isValidTextureMax(u)) { q.textureMaxSize = u; if (applied) *applied = true; } else logSettingsMessage("Invalid texture cap",true); return true; }
    return false;
}

bool s_loadedQuality = false;

bool readQualityProfiles(SettingsFile& settings)
{
    const std::vector<SettingsEntry> entries = settings.section("QualityProfiles");
    if (entries.empty()) {
        return false;
    }
    auto parsed = s_profiles;
    int selected = s_selectedProfile;
    bool invalid = false;
    for (const SettingsEntry& entry : entries) {
        const std::string& key = entry.first;
        const std::string& value = entry.second;
        if (key == "selection") {
            int parsedSelection = 0;
            if (parseInt(value, parsedSelection) && parsedSelection >= 0 && parsedSelection <= 4) {
                selected = parsedSelection;
            } else {
                invalid = true;
            }
            continue;
        }
        if (key.rfind("profile", 0) != 0) {
            continue;
        }
        const size_t dot = key.find('.');
        if (dot == std::string::npos || dot <= 7) {
            invalid = true;
            continue;
        }
        int index = 0;
        if (!parseInt(key.substr(7, dot - 7), index) || index < 0 || index > 3) {
            invalid = true;
            continue;
        }
        auto& profile = parsed[static_cast<size_t>(index)];
        const std::string field = key.substr(dot + 1);
        uint32_t u = 0;
        int i = 0;
        bool b = false;
        if (readLayerField(profile.render, field, value)) continue;
        if (field == "textureMaxSize") {
            if (parseUint(value, u) && isValidTextureMax(u)) profile.render.textureMaxSize = u; else invalid = true;
        } else if (field == "msaa") {
            if (parseInt(value, i) && i >= 0 && i <= 3) profile.render.msaaIdx = i; else invalid = true;
        } else if (field == "postAA") {
            if (parseInt(value, i) && i >= 0 && i <= 2) profile.render.postAAIdx = i; else invalid = true;
        } else if (field == "ssrQuality") {
            if (parseInt(value, i) && i >= 0 && i <= 2) profile.render.ssrQuality = i; else invalid = true;
        } else if (field == "ssaoQuality") {
            if (parseInt(value, i) && i >= 0 && i <= 2) profile.render.ssaoQuality = i; else invalid = true;
        } else if (field == "ssaoResolution") {
            if (value == "full" || value == "half") profile.render.ssaoHalfResolution = value == "half"; else invalid = true;
        } else if (field == "skyOcclusionQuality") {
            if (parseInt(value, i) && i >= 0 && i <= 2) profile.render.skyOcclusionQuality = i; else invalid = true;
        } else if (field == "oceanQuality") {
            if (parseInt(value, i) && i >= 0 && i <= 2) profile.render.oceanQuality = i; else invalid = true;
        } else if (field == "skybox") {
            if (parseBool(value, b)) profile.skybox = b; else invalid = true;
        } else if (field == "ibl") {
            if (parseBool(value, b)) profile.render.iblEnabled = b; else invalid = true;
        } else if (field == "lights") {
            if (parseBool(value, b)) profile.render.lightsEnabled = b; else invalid = true;
        } else if (field == "ssr") {
            if (parseBool(value, b)) profile.render.ssrEnabled = b; else invalid = true;
        } else if (field == "bloom") {
            if (parseBool(value, b)) profile.render.bloomEnabled = b; else invalid = true;
        } else if (field == "cube") {
            if (parseUint(value, u) && setChecked(profile.render.iblCubeSize, u, kCubeSizes)) {} else invalid = true;
        } else if (field == "irradiance") {
            if (parseUint(value, u) && setChecked(profile.render.iblIrradianceSize, u, kIrradianceSizes)) {} else invalid = true;
        } else if (field == "lut") {
            if (parseUint(value, u) && setChecked(profile.render.iblLutSize, u, kLutSizes)) {} else invalid = true;
        } else if (field == "samples") {
            if (parseUint(value, u) && setChecked(profile.render.iblSpecularSamples, u, kSampleCounts)) {} else invalid = true;
        } else if (field == "halfFloat") {
            if (parseBool(value, b)) profile.render.iblHalfFloat = b; else invalid = true;
        } else if (field == "grid") {
            if (parseInt(value, i) && (i == 0 || i == 1)) profile.render.gridMode = static_cast<GridMode>(i); else invalid = true;
        } else if (field == "scale") {
            float f = 1.0f;
            if (parseFloat(value, f) && setChecked(profile.render.renderScale, f, kRenderScales)) {} else invalid = true;
        } else if (field == "shadows") {
            if (parseBool(value, b)) profile.render.shadows = b; else invalid = true;
        } else if (field == "shadowResolution") {
            if (parseUint(value, u) && setChecked(profile.render.shadowResolution, u, kShadowResolutions)) {} else invalid = true;
        } else if (field == "shadowCascades") {
            if (parseInt(value, i) && i >= 2 && i <= 4) profile.render.shadowCascades = i; else invalid = true;
        } else if (field == "shadowDistance") {
            float f = 0.0f;
            if (parseFloat(value, f) && validShadowDistance(f)) profile.render.shadowDistance = f; else invalid = true;
        } else if (field == "shadowFilter") {
            if (parseInt(value, i) && i >= 0 && i <= 2) profile.render.shadowFilter = static_cast<ShadowFilter>(i); else invalid = true;
        }
    }
    if (invalid) {
        logSettingsMessage("Ignoring invalid quality preset values in settings.ini.", true);
    }
    // New explicit fields win over old allow-gates regardless of entry order.
    for (const auto& [key,value] : entries) {
        if (key.size() > 9 && key.rfind("profile",0) == 0 && key[8] == '.' && key[7] >= '0' && key[7] <= '3')
            readLayerField(parsed[key[7]-'0'].render, key.substr(9), value);
    }
    s_profiles = parsed;
    s_selectedProfile = selected;
    return true;
}

bool readRenderQuality(SettingsFile& store)
{
    const std::vector<SettingsEntry> entries = store.section("RenderQuality");
    if (entries.empty()) {
        return false;
    }
    RenderQualitySettings s = RenderQuality::settings();
    bool parsedAny = false;
    for (const SettingsEntry& entry : entries) {
        uint32_t u = 0;
        int i = 0;
        bool b = false;
        float f = 1.0f;
        if (readLayerField(s, entry.first, entry.second, &parsedAny)) continue;
        if (entry.first == "autoQuality") {
            if (parseBool(entry.second, b)) { s.autoQuality = b; parsedAny = true; }
        } else if (entry.first == "preset") {
            if (parseInt(entry.second, i) && i >= 0 && i <= 3) {
                s.preset = static_cast<QualityPreset>(i); parsedAny = true;
            }
        } else if (entry.first == "iblCubeSize") {
            if (parseUint(entry.second, u)) { if (setChecked(s.iblCubeSize, u, kCubeSizes)) parsedAny = true; }
        } else if (entry.first == "iblIrradianceSize") {
            if (parseUint(entry.second, u)) { if (setChecked(s.iblIrradianceSize, u, kIrradianceSizes)) parsedAny = true; }
        } else if (entry.first == "iblLutSize") {
            if (parseUint(entry.second, u)) { if (setChecked(s.iblLutSize, u, kLutSizes)) parsedAny = true; }
        } else if (entry.first == "iblHalfFloat") {
            if (parseBool(entry.second, b)) { s.iblHalfFloat = b; parsedAny = true; }
        } else if (entry.first == "iblSpecularSamples") {
            if (parseUint(entry.second, u)) { if (setChecked(s.iblSpecularSamples, u, kSampleCounts)) parsedAny = true; }
        } else if (entry.first == "gridMode") {
            if (parseInt(entry.second, i) && (i == 0 || i == 1)) {
                s.gridMode = static_cast<GridMode>(i); parsedAny = true;
            }
        } else if (entry.first == "renderScale") {
            if (parseFloat(entry.second, f)) { if (setChecked(s.renderScale, f, kRenderScales)) parsedAny = true; }
        } else if (entry.first == "shadows") {
            if (parseBool(entry.second, b)) { s.shadows = b; parsedAny = true; }
        } else if (entry.first == "shadowResolution") {
            if (parseUint(entry.second, u)) { if (setChecked(s.shadowResolution, u, kShadowResolutions)) parsedAny = true; }
        } else if (entry.first == "shadowCascades") {
            if (parseInt(entry.second, i) && i >= 2 && i <= 4) { s.shadowCascades = i; parsedAny = true; }
        } else if (entry.first == "shadowDistance") {
            if (parseFloat(entry.second, f) && validShadowDistance(f)) { s.shadowDistance = f; parsedAny = true; }
        } else if (entry.first == "shadowFilter") {
            if (parseInt(entry.second, i) && i >= 0 && i <= 2) { s.shadowFilter = static_cast<ShadowFilter>(i); parsedAny = true; }
        } else if (entry.first == "shadowSplitLambda") {
            if (parseFloat(entry.second, f) && validShadowLambda(f)) { s.shadowSplitLambda = f; parsedAny = true; }
        } else if (entry.first == "shadowNormalBias") {
            if (parseFloat(entry.second, f) && validShadowBias(f)) { s.shadowNormalBias = f; parsedAny = true; }
        } else if (entry.first == "shadowSlopeBias") {
            if (parseFloat(entry.second, f) && validShadowBias(f)) { s.shadowSlopeBias = f; parsedAny = true; }
        } else if (entry.first == "shadowConstantBias") {
            if (parseFloat(entry.second, f) && validShadowBias(f)) { s.shadowConstantBias = f; parsedAny = true; }
        }
    }
    if (parsedAny) {
        RenderQuality::settings() = s;
    }
    return parsedAny;
}

uint64_t cubeBytes(uint32_t size, bool mips, uint32_t texelBytes)
{
    uint64_t total = 0;
    for (uint32_t s = size; s > 0; s >>= 1) {
        total += 6ull * s * s * texelBytes;
        if (!mips) {
            break;
        }
    }
    return total;
}

// Cube face / LUT / sample count combo. Returns true when the value changed.
template <size_t N>
bool sizeCombo(const char* label, uint32_t& field, const uint32_t (&values)[N], const char* zeroLabel)
{
    const std::string current = field == 0 ? std::string(zeroLabel) : std::to_string(field);
    bool changed = false;
    if (ImGui::BeginCombo(label, current.c_str())) {
        for (uint32_t v : values) {
            const std::string name = v == 0 ? std::string(zeroLabel) : std::to_string(v);
            if (ImGui::Selectable(name.c_str(), field == v) && field != v) {
                field = v;
                changed = true;
            }
            if (std::strcmp(label,"Texture maximum") == 0) UiProbe::item("quality.texture.option",v);
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool levelCombo(const char* label, bool& enabled, int& budget)
{
    int value = enabled ? std::clamp(budget, 0, 2) + 1 : 0;
    if (!ImGui::Combo(label, &value, "Off\0Low\0Mid\0High\0")) return false;
    enabled = value != 0;
    if (enabled) budget = value - 1;
    return true;
}

template <size_t N> bool enabledSizeCombo(const char* label, bool& enabled, uint32_t& size, const uint32_t (&values)[N])
{
    bool changed = false;
    const std::string preview = enabled ? std::to_string(size) : "Off";
    if (ImGui::BeginCombo(label, preview.c_str())) {
        if (ImGui::Selectable("Off", !enabled)) { enabled = false; changed = true; }
        if (std::strcmp(label,"IBL cube") == 0) UiProbe::item("quality.ibl.option",0);
        for (uint32_t value : values) {
            if (ImGui::Selectable(std::to_string(value).c_str(), enabled && size == value)) { enabled = true; size = value; changed = true; }
            if (std::strcmp(label,"IBL cube") == 0) UiProbe::item("quality.ibl.option",value);
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool drawEffectLevels(RenderQualitySettings& q, bool showLights = true)
{
    bool changed = levelCombo("Reflections (SSR)", q.ssrEnabled, q.ssrQuality);
    UiProbe::item("quality.ssr", 0);
    changed |= levelCombo("SSAO", q.ssaoEnabled, q.ssaoQuality);
    UiProbe::item("quality.ssao", 0);
    if (q.ssaoEnabled) {
        int half = q.ssaoHalfResolution ? 1 : 0;
        if (ImGui::Combo("SSAO resolution", &half, "Full\0Half\0")) { q.ssaoHalfResolution = half == 1; changed = true; }
        UiProbe::item("quality.ssao_resolution", 0);
    }
    int sky = q.skyOcclusion < 0 ? 0 : q.skyOcclusion ? 1 : 2;
    if (ImGui::Combo("Sky occlusion", &sky, "Auto\0On\0Off\0")) { q.skyOcclusion = sky == 0 ? -1 : sky == 1 ? 1 : 0; changed = true; }
    UiProbe::item("quality.sky_occlusion.mode", 0);
    changed |= ImGui::Combo("Sky occlusion quality", &q.skyOcclusionQuality, "Low\0Mid\0High\0");
    UiProbe::item("quality.sky_occlusion", 0);
    changed |= ImGui::Combo("Ocean quality", &q.oceanQuality, "Low\0Mid\0High\0");
    UiProbe::item("quality.ocean", 0);
    int mode = q.oceanLive ? 1 : 0;
    if (ImGui::Combo("Ocean mode", &mode, "Precomputed\0Live FFT\0")) { q.oceanLive = mode == 1; changed = true; }
    UiProbe::item("quality.ocean.mode", 0);
    changed |= ImGui::Checkbox("Ocean animation", &q.oceanPlay);
    UiProbe::item("quality.ocean.play", 0);
    changed |= ImGui::Checkbox("Bloom", &q.bloomEnabled);
    UiProbe::item("quality.bloom", 0);
    int lens = q.lensFlare == 2 ? 3 : q.lensFlare == 3 ? 2 : q.lensFlare;
    if (ImGui::Combo("Lens effect", &lens, "Off\0Ghosts + halo\0Ghosts + streak\0Anamorphic streak\0")) {
        q.lensFlare = lens == 2 ? 3 : lens == 3 ? 2 : lens; changed = true;
    }
    UiProbe::item("quality.lens", 0);
    if (q.lensFlare) changed |= ImGui::SliderFloat("Lens intensity (Quality)", &q.lensFlareIntensity, 0, 10, "%.2f");
    if (showLights) {
        changed |= ImGui::Checkbox("Punctual lights", &q.lightsEnabled);
        UiProbe::item("quality.lights", 0);
    }
    return changed;
}

} // namespace

RenderQualitySettings RenderQualitySettings::fromPreset(QualityPreset preset)
{
    RenderQualitySettings s;
    s.preset = preset;
    switch (preset) {
    case QualityPreset::Balanced:
        // Only changes measured as invisible, including with a very bright sun; fewer
        // specular samples or a 256 LUT moved rough-metal highlights there.
        s.iblIrradianceSize = 64;
        s.iblLutSize = 512;
        s.iblHalfFloat = true;
        s.gridMode = GridMode::PlaneMesh;
        break;
    case QualityPreset::Performance:
        s.iblCubeSize = 512;
        s.iblIrradianceSize = 32;
        s.iblLutSize = 256;
        s.iblHalfFloat = true;
        s.iblSpecularSamples = 256;
        s.gridMode = GridMode::PlaneMesh;
        s.renderScale = 0.75f;
        break;
    case QualityPreset::High:
    case QualityPreset::Custom:
        break;
    }
    return s;
}

bool RenderQualitySettings::sameIbl(const RenderQualitySettings& other) const
{
    return iblCubeSize == other.iblCubeSize
        && iblIrradianceSize == other.iblIrradianceSize
        && iblLutSize == other.iblLutSize
        && iblHalfFloat == other.iblHalfFloat
        && iblSpecularSamples == other.iblSpecularSamples;
}

uint64_t RenderQualitySettings::estimatedShadowBytes() const
{
    if (!shadows) {
        return 0;
    }
    return static_cast<uint64_t>(shadowResolution) * shadowResolution * static_cast<uint64_t>(std::clamp(shadowCascades, 2, 4)) * 4u;
}

void RenderQualitySettings::copyShadowTuning(const RenderQualitySettings& from)
{
    shadowSplitLambda = from.shadowSplitLambda;
    shadowNormalBias = from.shadowNormalBias;
    shadowSlopeBias = from.shadowSlopeBias;
    shadowConstantBias = from.shadowConstantBias;
}

void RenderQualitySettings::copyShadows(const RenderQualitySettings& from)
{
    shadows = from.shadows;
    shadowResolution = from.shadowResolution;
    shadowCascades = from.shadowCascades;
    shadowDistance = from.shadowDistance;
    shadowFilter = from.shadowFilter;
    copyShadowTuning(from);
}

void RenderQualitySettings::copyEffectQuality(const RenderQualitySettings& from)
{
    ssrEnabled = from.ssrEnabled;
    ssaoEnabled = from.ssaoEnabled;
    iblEnabled = from.iblEnabled;
    bloomEnabled = from.bloomEnabled;
    lightsEnabled = from.lightsEnabled;
    skyOcclusion = from.skyOcclusion;
    oceanLive = from.oceanLive;
    oceanPlay = from.oceanPlay;
    lensFlare = from.lensFlare;
    lensFlareIntensity = from.lensFlareIntensity;
    textureMaxSize = from.textureMaxSize;
    msaaIdx = from.msaaIdx;
    postAAIdx = from.postAAIdx;
    ssrQuality = from.ssrQuality;
    ssaoQuality = from.ssaoQuality;
    ssaoHalfResolution = from.ssaoHalfResolution;
    skyOcclusionQuality = from.skyOcclusionQuality;
    oceanQuality = from.oceanQuality;
}

int RenderQualitySettings::legacyAAIndex() const
{
    if (postAAIdx == 0) return std::clamp(msaaIdx, 0, 3);
    if (msaaIdx == 0) return postAAIdx == 1 ? 4 : 5;
    return -1;
}

uint64_t RenderQualitySettings::estimatedIblBytes() const
{
    const uint32_t texelBytes = iblHalfFloat ? 8u : 16u;
    const uint32_t irradiance = iblIrradianceSize == 0 ? iblCubeSize : std::min(iblIrradianceSize, iblCubeSize);
    return 2ull * cubeBytes(iblCubeSize, true, texelBytes) // specular + sheen
        + cubeBytes(irradiance, false, texelBytes)
        + 3ull * iblLutSize * iblLutSize * texelBytes;
}

RenderQualitySettings& RenderQuality::settings()
{
    static RenderQualitySettings s_settings;
    return s_settings;
}

bool RenderQuality::autoActive() { return settings().autoQuality && !s_autoSuppressed; }
bool RenderQuality::allowIbl() { return settings().iblEnabled && (!autoActive() || s_pinIbl || s_autoLevel >= 1); }
bool RenderQuality::allowSkybox(bool requested) { return requested; }
bool RenderQuality::allowLights(bool requested) { return requested && settings().lightsEnabled; }
bool RenderQuality::legacyCaptureUi() { return s_autoSuppressed && !s_showRuntimeUi; }
bool RenderQuality::qualityReached() { return !autoActive() || s_autoLevel == 6; }
uint32_t RenderQuality::textureMaxSize() { return settings().textureMaxSize; }
bool RenderQuality::unlimitedTextures() { return s_profilesActive && settings().textureMaxSize == 0; }
bool RenderQuality::takeTextureRefresh() {
    static uint32_t previous = textureMaxSize();
    if (previous != textureMaxSize()) { previous = textureMaxSize(); s_textureRefresh = true; }
    return std::exchange(s_textureRefresh, false);
}
bool RenderQuality::textureRefreshPending() { return s_textureRefresh; }
int RenderQuality::selectedProfile() { return s_selectedProfile; }
double RenderQuality::gpuFrameMs() { return s_gpuMs; }
const char* RenderQuality::profileName(int selection)
{
    static const char* names[] = {"Auto", "Low", "Mid", "High", "Ultra"};
    return selection >= 0 && selection <= 4 ? names[selection] : "Auto";
}
void RenderQuality::selectProfile(int selection)
{
    if (selection < 0 || selection > 4) return;
    s_selectedProfile = selection; s_profilesActive = true;
    ++s_profileVersion;
    const auto& p = targetProfile();
    settings() = p.render; settings().autoQuality = selection == 0;
    // Choosing a preset intentionally replaces current overrides, including any session-only ones.
    s_savedQuality = settings();
    s_savedSelection = selection;
    s_autoSuppressed = false; s_showRuntimeUi = true; s_textureRefresh = true;
    s_pinIbl = s_pinScale = s_pinAA = s_pinSsr = s_pinBloom = s_pinShadow = false;
    s_autoLevel = 0; s_autoCeiling = 6; s_autoSamples = 0; s_autoMs = 0;
    s_autoWindow = std::chrono::steady_clock::now();
    save();
}

void RenderQuality::drawProfileSelector(SceneRevisions& revisions, const char* probeTag)
{
    // Narrow horizontal padding; the vertical padding stays the caller's so the combo lines up with the row.
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(4,ImGui::GetStyle().FramePadding.y));
    ImGui::SetNextItemWidth(105);
    int choice = s_selectedProfile;
    const char* names[] = {"Auto", "Low", "Mid", "High", "Ultra"};
    if (ImGui::Combo("##QualityPreset", &choice, names, 5)) {
        selectProfile(choice); revisions.mark(SceneChange::ViewCamera);
    }
    UiProbe::item(probeTag, choice);
    ImGui::PopStyleVar();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Quality: %s\nShared by all viewports. Texture changes re-decode from retained source data.\nAuto targets your Ultra profile within available frame/memory budgets.\nRight-click the status-bar selector to open Quality settings.", profileName(choice));
}

void RenderQuality::drawProfilesEditor(SceneRevisions& revisions)
{
    ImGui::TextWrapped("Edit each preset independently. Auto targets Ultra and adapts to the machine. Changes to an active preset apply immediately.");
    if (!ImGui::BeginTabBar("QualityProfiles")) return;
    for (int i = 0; i < 4; ++i) {
        const bool open = ImGui::BeginTabItem(profileName(i+1));
        UiProbe::item("quality.tab",i+1);
        if (!open) continue;
        ImGui::PushID(i);
        auto& p = s_profiles[i];
        bool changed = false;
        if (ImGui::Button("Reset to default")) { p = defaultProfile(i); changed = true; }
        UiProbe::item("quality.reset",i+1);
        ImGui::SameLine();
        if (ImGui::Button("Use this preset")) { selectProfile(i+1); revisions.mark(SceneChange::ViewCamera); }
        UiProbe::item("quality.use",i+1);
        ImGui::Separator();
        ImGui::PushItemWidth(150);
        const uint32_t textureSizes[] = {0,128,256,512,1024,2048,4096,8192,16384};
        changed |= sizeCombo("Texture maximum",p.render.textureMaxSize,textureSizes,"Unlimited");
        UiProbe::item("quality.texture.max",i+1);
        const char* aa[] = {"Off","2x","4x","8x"};
        const char* post[] = {"None","FXAA","TAA"};
        changed |= ImGui::Combo("MSAA",&p.render.msaaIdx,aa,4);
        changed |= ImGui::Combo("Post AA",&p.render.postAAIdx,post,3);
        changed |= enabledSizeCombo("IBL cube", p.render.iblEnabled, p.render.iblCubeSize, kCubeSizes);
        if (p.render.iblEnabled) {
            changed |= sizeCombo("Irradiance", p.render.iblIrradianceSize, kIrradianceSizes, "Full cube");
            changed |= sizeCombo("IBL LUT", p.render.iblLutSize, kLutSizes, "");
            changed |= sizeCombo("IBL samples", p.render.iblSpecularSamples, kSampleCounts, "");
            changed |= ImGui::Checkbox("IBL half float", &p.render.iblHalfFloat);
        }
        changed |= drawEffectLevels(p.render);
        int grid = static_cast<int>(p.render.gridMode);
        const char* grids[] = {"Screen ray","Mesh"};
        if (ImGui::Combo("Ground grid",&grid,grids,2)) { p.render.gridMode = static_cast<GridMode>(grid); changed = true; }
        int scale = static_cast<int>(std::find(std::begin(kRenderScales),std::end(kRenderScales),p.render.renderScale)-std::begin(kRenderScales));
        if (ImGui::Combo("Render scale", &scale, "50%\0 67%\0 75%\0 85%\0 100%\0")) { p.render.renderScale = kRenderScales[scale]; changed = true; }
        // A3.2 sun shadows (used only while the sun is on).
        changed |= enabledSizeCombo("Shadow map", p.render.shadows, p.render.shadowResolution, kShadowResolutions);
        if (p.render.shadows) {
        changed |= ImGui::SliderInt("Shadow cascades",&p.render.shadowCascades,2,4);
        changed |= ImGui::DragFloat("Shadow distance",&p.render.shadowDistance,1.0f,kShadowDistanceMin,kShadowDistanceMax,"%.0f m",ImGuiSliderFlags_AlwaysClamp);
        {
            int filter = static_cast<int>(p.render.shadowFilter);
            const char* filters[] = {"Hard","PCF","Soft"};
            if (ImGui::Combo("Shadow filter",&filter,filters,3)) { p.render.shadowFilter = static_cast<ShadowFilter>(filter); changed = true; }
        }
        changed |= ImGui::SliderFloat("Split lambda", &p.render.shadowSplitLambda, 0, 1);
        changed |= ImGui::DragFloat("Normal bias", &p.render.shadowNormalBias, 0.05f, 0, 100, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        changed |= ImGui::DragFloat("Slope bias", &p.render.shadowSlopeBias, 0.05f, 0, 100, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        changed |= ImGui::DragFloat("Constant bias", &p.render.shadowConstantBias, 0.05f, 0, 100, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        }
        ImGui::PopItemWidth();
        if (changed) {
            save();
            if (s_profilesActive && (s_selectedProfile == i+1 || (s_selectedProfile == 0 && i == 3))) {
                selectProfile(s_selectedProfile); revisions.mark(SceneChange::ViewCamera);
            }
        }
        ImGui::PopID(); ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
}
bool RenderQuality::allowBloom(bool requested)
{
    return requested && settings().bloomEnabled && (!autoActive() || s_pinBloom || s_autoLevel >= 4);
}
void RenderQuality::pinShadows() { s_pinShadow = true; }
void RenderQuality::pinAA() { s_pinAA = true; }
void RenderQuality::pinSsr() { s_pinSsr = true; }
void RenderQuality::pinBloom() { s_pinBloom = true; }
bool RenderQuality::allowSsr()
{
    return settings().ssrEnabled && (!autoActive() || s_pinSsr || s_autoLevel >= 5);
}
bool RenderQuality::validShadowResolution(uint32_t value) { return isOneOf(value, kShadowResolutions); }
const char* RenderQuality::shadowFilterName(ShadowFilter filter)
{
    switch (filter) {
    case ShadowFilter::Hard: return "hard";
    case ShadowFilter::Soft: return "soft";
    case ShadowFilter::Pcf: break;
    }
    return "pcf";
}
const std::string& RenderQuality::autoStatus() { return s_autoStatus; }
int RenderQuality::autoLevel() { return s_autoLevel; }
bool RenderQuality::wantsSample()
{
    return autoActive() && (s_autoSamples < 12 ||
        std::chrono::steady_clock::now()-s_autoWindow >= std::chrono::milliseconds(350));
}
void RenderQuality::beginGpuSample()
{
    if (legacyCaptureUi()) return;
    for (int i = 0; i < 4; ++i) {
        if (!s_gpuPending[i]) continue;
        GLint ready = 0;
        glGetQueryObjectiv(s_gpuQueries[i][1], GL_QUERY_RESULT_AVAILABLE, &ready);
        if (ready) {
            GLuint64 start = 0, end = 0;
            glGetQueryObjectui64v(s_gpuQueries[i][0], GL_QUERY_RESULT, &start);
            glGetQueryObjectui64v(s_gpuQueries[i][1], GL_QUERY_RESULT, &end);
            if (s_gpuRendered[i]) s_gpuMs = double(end-start)/1e6;
            s_gpuPending[i] = false;
        }
    }
    if (s_gpuPending[s_gpuNext]) return;
    if (!s_gpuQueries[s_gpuNext][0]) glGenQueries(2, s_gpuQueries[s_gpuNext]);
    s_gpuWriting = s_gpuNext;
    glQueryCounter(s_gpuQueries[s_gpuWriting][0], GL_TIMESTAMP);
}
void RenderQuality::endGpuSample(bool rendered)
{
    if (s_gpuWriting < 0) return;
    glQueryCounter(s_gpuQueries[s_gpuWriting][1], GL_TIMESTAMP);
    s_gpuPending[s_gpuWriting] = true;
    s_gpuRendered[s_gpuWriting] = rendered;
    s_gpuNext = (s_gpuWriting+1)%4; s_gpuWriting = -1;
}
void RenderQuality::releaseGpuSamples()
{
    for (auto& queries : s_gpuQueries) if (queries[0]) { glDeleteQueries(2, queries); queries[0] = queries[1] = 0; }
}

RenderQualitySettings RenderQuality::effective()
{
    auto result = settings();
    if (!autoActive()) return result;
    if (!s_pinIbl && s_autoLevel < 3) {
        result.iblCubeSize = std::min(result.iblCubeSize, s_autoLevel < 2 ? 256u : 512u);
        result.iblIrradianceSize = 32;
        result.iblLutSize = std::min(result.iblLutSize, 256u);
        result.iblHalfFloat = true;
        result.iblSpecularSamples = std::min(result.iblSpecularSamples, 256u);
    }
    if (!s_pinScale && s_autoLevel < 6) result.renderScale = std::min(result.renderScale, .75f);
    // A3.2: Auto starts the shadows cheaper (1024, two cascades, at most PCF) and steps up to the configured
    // values; it never switches them off and never goes above the configured choice.
    if (!s_pinShadow && s_autoLevel < 4) {
        result.shadowResolution = std::min(result.shadowResolution, s_autoLevel < 2 ? 1024u : 2048u);
        if (s_autoLevel < 2) result.shadowCascades = std::min(result.shadowCascades, 2);
        if (result.shadowFilter == ShadowFilter::Soft) result.shadowFilter = ShadowFilter::Pcf;
    }
    // AA and SSR quality (formerly adapted per viewport): cheap AA until step 4, low SSR until step 6. A user edit
    // pins the group for the session; the controller never writes the configured settings.
    if (!s_pinAA && s_autoLevel < 4) { result.msaaIdx = 0; result.postAAIdx = 1; }
    if (!s_pinSsr && s_autoLevel < 6) result.ssrQuality = 0;
    result.ssrEnabled = allowSsr();
    result.iblEnabled = allowIbl();
    result.bloomEnabled = allowBloom(true);
    return result;
}

ViewportSettingsState RenderQuality::effectiveViewport(const ViewportSettingsState& configured)
{
    auto result = configured;
    const RenderQualitySettings quality = effective();
    result.msaaIdx = std::clamp(quality.msaaIdx, 0, 3);
    result.postAAIdx = configured.postEffects ? std::clamp(quality.postAAIdx, 0, 2) : 0;
    result.ssrQuality = std::clamp(quality.ssrQuality, 0, 2);
    result.screenSpaceReflections = configured.postEffects && configured.screenSpaceReflections && allowSsr();
    return result;
}

void RenderQuality::resolveViewport(ViewportSettingsState& viewport, const SceneEffectSettings& effects)
{
    const RenderQualitySettings& quality = settings();
    viewport.antiAliasingIdx = quality.legacyAAIndex();
    viewport.msaaIdx = std::clamp(quality.msaaIdx, 0, 3);
    viewport.postAAIdx = viewport.postEffects ? std::clamp(quality.postAAIdx, 0, 2) : 0;
    viewport.ssrQuality = std::clamp(quality.ssrQuality, 0, 2);
    viewport.ssao.quality = static_cast<SsaoQuality>(std::clamp(quality.ssaoQuality, 0, 2));
    viewport.ssao.halfResolution = quality.ssaoHalfResolution;
    viewport.ground.ocean.quality = static_cast<OceanQuality>(std::clamp(quality.oceanQuality, 0, 2));

    viewport.sunShadows = quality.shadows;
    viewport.screenSpaceReflections = viewport.postEffects && quality.ssrEnabled;
    viewport.ssao.enabled = viewport.postEffects && quality.ssaoEnabled;
    viewport.ssao.radius = effects.ssaoRadius;
    viewport.ssao.intensity = effects.ssaoIntensity;
    viewport.interiorSkyReflection = GltfExtensionSettings::instance().tuning("interior_sky_reflection");
    viewport.windshieldIcing = effects.windshieldIcing;
    viewport.skyPortalStrength = effects.skyPortalStrength;
    viewport.decalDepthToleranceMm = GltfExtensionSettings::instance().tuning("decal_tolerance_mm");
    viewport.lensFlare = viewport.postEffects ? quality.lensFlare : 0;
    viewport.lensFlareIntensity = quality.lensFlareIntensity;
}

void RenderQuality::configureAuto(int argc, char** argv)
{
    bool explicitAuto = false, deterministic = false, explicitProfile = false, legacyQuality = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        explicitAuto |= arg == "--auto-quality";
        explicitProfile |= arg == "--quality-preset";
        legacyQuality |= arg == "--quality";
        s_showRuntimeUi |= arg == "--show-telemetry" || arg == "--quality-preset";
        deterministic |= arg == "--isolated" || arg == "--no-persist" || arg == "--dump-frame" ||
            arg == "--capture-viewport" || arg == "--capture-first-frame" || arg == "--fixed-frame-time" || arg == "--ui-script";
        s_pinIbl |= arg.starts_with("--ibl-");
        s_pinScale |= arg == "--render-scale";
        s_pinAA |= arg == "--aa" || arg == "--msaa" || arg == "--post-aa";
        s_pinSsr |= arg == "--ssr" || arg == "--ssr-quality";
        s_pinBloom |= arg == "--bloom" || arg == "--bloom-intensity" || arg == "--bloom-radius";
        s_pinShadow |= arg == "--shadows" || arg.starts_with("--shadow-");
    }
    s_autoSuppressed = deterministic && !explicitAuto;
    s_profilesActive = explicitProfile || (!deterministic && !legacyQuality);
    if (s_profilesActive && !explicitProfile && !s_loadedQuality) {
        const auto before = settings();
        settings() = targetProfile().render;
        settings().autoQuality = explicitAuto ? before.autoQuality : s_selectedProfile == 0;
        if (s_pinIbl) {
            settings().iblCubeSize = before.iblCubeSize; settings().iblIrradianceSize = before.iblIrradianceSize;
            settings().iblLutSize = before.iblLutSize; settings().iblSpecularSamples = before.iblSpecularSamples;
            settings().iblHalfFloat = before.iblHalfFloat;
            settings().iblEnabled = before.iblEnabled;
        }
        if (s_pinScale) settings().renderScale = before.renderScale;
        // A3.2: command-line shadow values win over the profile; each profile also carries its tuning values.
        if (s_pinShadow) settings().copyShadows(before);

    }
    if (explicitProfile) s_autoSuppressed = deterministic && s_selectedProfile != 0;
    s_autoWindow = std::chrono::steady_clock::now();
    s_autoStatus = autoActive() ? "Auto quality: starting - IBL off" : "Auto quality: off";
}

void RenderQuality::updateAuto(double frameMs, ux3d::renderer::GltfRenderLayer* layer, SceneRevisions& revisions)
{
    const auto now = std::chrono::steady_clock::now();
    if (autoActive()) {
        // Force real rendered samples even for a stationary model. Never use idle
        // event-wait intervals or capture readback time to decide quality.
        if (frameMs > 0) { s_autoMs += std::max(frameMs, s_gpuMs); ++s_autoSamples; }
        if (s_autoSamples >= 12 && now-s_autoWindow >= std::chrono::milliseconds(400)) {
            const auto memory = queryMemoryInfo();
            const double mean = s_autoMs/s_autoSamples;
            const bool lowRam = memory.availableRam && memory.availableRam < 512ull*1024*1024;
            const uint64_t currentIblBytes = effective().estimatedIblBytes();
            ++s_autoLevel;
            const uint64_t nextIblBytes = effective().estimatedIblBytes();
            --s_autoLevel;
            const uint64_t extraIblBytes = nextIblBytes > currentIblBytes ? nextIblBytes-currentIblBytes : 0;
            const uint64_t requiredHeadroom = std::max<uint64_t>(256ull*1024*1024, extraIblBytes+128ull*1024*1024);
            const bool lowVram = memory.gpuKnown && (memory.availableVram < requiredHeadroom ||
                (memory.gpuBudget && memory.gpuBudget <= 1024ull*1024*1024));
            const int before = s_autoLevel;
            const std::string oldStatus = s_autoStatus;
            if (lowRam || lowVram) {
                // The small IBL is already allocated at startup. Keep that useful
                // lighting contribution when it fits, even if the target is limited;
                // otherwise an asset without punctual lights stays entirely dark.
                s_autoLevel = !lowRam && memory.availableVram >= 128ull*1024*1024 ? 1 : 0;
                s_autoStatus = lowRam ? "Auto quality: limited by RAM" : "Auto quality: limited by VRAM";
            } else if (mean > 50.0 && s_autoLevel > 0) {
                --s_autoLevel;
                s_autoCeiling = s_autoLevel;
                s_retryAfter = now + std::chrono::seconds(5);
                s_autoStatus = "Auto quality: limited by frame time";
            } else if (mean < 28.0 && s_autoLevel < 6 && (s_autoLevel < s_autoCeiling || now > s_retryAfter)) {
                ++s_autoLevel;
                s_autoStatus = "Auto quality: stepping up - IBL " + std::to_string(effective().iblCubeSize);
                if (s_autoLevel == 6) s_autoStatus = "Auto quality: configured quality reached";
            } else if (s_autoLevel < 6) {
                s_autoStatus = "Auto quality: limited by frame time";
            }
            if (s_autoLevel != before || oldStatus != s_autoStatus) core::logInfo("%s (step %d/6, %.1f ms)", s_autoStatus.c_str(), s_autoLevel, mean);
            if (s_autoLevel != before) revisions.mark(SceneChange::ViewCamera);
            s_autoSamples = 0; s_autoMs = 0; s_autoWindow = now;
        }
    } else {
        s_autoStatus = "Auto quality: off";
    }
    const auto desired = effective();
    if (layer && (!s_haveAppliedIbl || !desired.sameIbl(s_appliedIbl))) {
        applyIblParameters(*layer);
        // A6.2: the procedural sky reads the layer's IBL parameters every frame and rebuilds itself.
        if (layer->getActiveEnvironmentMap() != nullptr && !layer->isExternalEnvironment()) {
            const auto path = layer->getEnvironmentMapPath();
            const core::loading::Timer timer(core::loading::Ibl);
            layer->setEnvironmentMap(path);
            layer->filterEnvironmentMap();
        }
        s_appliedIbl = desired; s_haveAppliedIbl = true;
        revisions.mark(SceneChange::ViewCamera);
    }
}

void RenderQuality::applyIblParameters(ux3d::renderer::GltfRenderLayer& layer)
{
    const RenderQualitySettings s = effective();
    layer.iblParameters.textureSize = s.iblCubeSize;
    layer.iblParameters.lambertianSize = s.iblIrradianceSize;
    layer.iblParameters.lutResolution = s.iblLutSize;
    layer.iblParameters.halfFloat = s.iblHalfFloat;
    layer.iblSampling.ggxSampleCount = s.iblSpecularSamples;
    s_appliedIbl = s; s_haveAppliedIbl = true;
}

void RenderQuality::setPersistenceEnabled(bool enabled)
{
    s_persistenceEnabled = enabled;
    SettingsFile::instance().setPersistenceEnabled(enabled);
}

void RenderQuality::load()
{
    if (!s_persistenceEnabled) {
        return;
    }
    SettingsFile& store = SettingsFile::instance();
    if (!store.load()) {
        return;
    }
    const bool hasProfiles = readQualityProfiles(store);
    if (hasProfiles || !store.section("RenderQuality").empty()) {
        settings() = targetProfile().render;
        settings().autoQuality = s_selectedProfile == 0;
    }
    s_loadedQuality = readRenderQuality(store);
    s_savedQuality = hasProfiles || s_loadedQuality ? settings() : targetProfile().render;
    s_savedQuality.autoQuality = s_selectedProfile == 0;
    s_savedSelection = s_selectedProfile;
}

void RenderQuality::beginSession()
{
    s_sessionReady = true;
    observeSessionEdit();
}
void RenderQuality::observeSessionEdit()
{
    if (!s_sessionReady) return;
    s_observedQuality = settings(); s_observedSelection = s_selectedProfile;
}
void RenderQuality::pinIbl() { s_pinIbl = true; }
void RenderQuality::commitSetting(const std::string& key)
{
    const auto& current = settings();
    if (key == "aa") { s_savedQuality.msaaIdx = current.msaaIdx; s_savedQuality.postAAIdx = current.postAAIdx; }
    if (key == "msaa") { s_savedQuality.msaaIdx = current.msaaIdx; }
    if (key == "post_aa") { s_savedQuality.postAAIdx = current.postAAIdx; }
    if (key == "ssr") { s_savedQuality.ssrEnabled = current.ssrEnabled; }
    if (key == "ssr_quality") { s_savedQuality.ssrQuality = current.ssrQuality; }
    if (key == "reflections") { s_savedQuality.ssrEnabled = current.ssrEnabled; s_savedQuality.ssrQuality = current.ssrQuality; }
    if (key == "ssao") { s_savedQuality.ssaoEnabled = current.ssaoEnabled; }
    if (key == "ssao_level") { s_savedQuality.ssaoEnabled = current.ssaoEnabled; s_savedQuality.ssaoQuality = current.ssaoQuality; }
    if (key == "ssao_quality") { s_savedQuality.ssaoQuality = current.ssaoQuality; }
    if (key == "ssao_resolution") { s_savedQuality.ssaoHalfResolution = current.ssaoHalfResolution; }
    if (key == "sky_occlusion") { s_savedQuality.skyOcclusion = current.skyOcclusion; }
    if (key == "sky_occlusion_quality") { s_savedQuality.skyOcclusionQuality = current.skyOcclusionQuality; }
    if (key == "ocean_quality") { s_savedQuality.oceanQuality = current.oceanQuality; }
    if (key == "ocean_mode") { s_savedQuality.oceanLive = current.oceanLive; }
    if (key == "ocean_play") { s_savedQuality.oceanPlay = current.oceanPlay; }
    if (key == "bloom") { s_savedQuality.bloomEnabled = current.bloomEnabled; }
    if (key == "ibl") { s_savedQuality.iblEnabled = current.iblEnabled; }
    if (key == "ibl_cube") { s_savedQuality.iblEnabled = current.iblEnabled; s_savedQuality.iblCubeSize = current.iblCubeSize; }
    if (key == "punctual_lights") { s_savedQuality.lightsEnabled = current.lightsEnabled; }
    if (key == "lens_flare") { s_savedQuality.lensFlare = current.lensFlare; }
    if (key == "lens_flare_intensity") { s_savedQuality.lensFlareIntensity = current.lensFlareIntensity; }
    if (key == "texture_max_size") { s_savedQuality.textureMaxSize = current.textureMaxSize; }
    if (key == "shadows") { s_savedQuality.shadows = current.shadows; }
    if (key == "shadows_allowed") { s_savedQuality.shadows = current.shadows; }
    if (key == "shadow_map") { s_savedQuality.shadows = current.shadows; s_savedQuality.shadowResolution = current.shadowResolution; }
    if (key == "grid_mode") { s_savedQuality.gridMode = current.gridMode; }
    if (key == "shadow_resolution") { s_savedQuality.shadowResolution = current.shadowResolution; }
    if (key == "shadow_cascades") { s_savedQuality.shadowCascades = current.shadowCascades; }
    if (key == "shadow_distance") { s_savedQuality.shadowDistance = current.shadowDistance; }
    if (key == "shadow_filter") { s_savedQuality.shadowFilter = current.shadowFilter; }
    if (key == "shadow_split_lambda") { s_savedQuality.shadowSplitLambda = current.shadowSplitLambda; }
    if (key == "shadow_normal_bias") { s_savedQuality.shadowNormalBias = current.shadowNormalBias; }
    if (key == "shadow_slope_bias") { s_savedQuality.shadowSlopeBias = current.shadowSlopeBias; }
    if (key == "shadow_constant_bias") { s_savedQuality.shadowConstantBias = current.shadowConstantBias; }
    observeSessionEdit();
}

void RenderQuality::save()
{
    if (!s_persistenceEnabled) {
        return;
    }
    if (s_sessionReady) {
        const auto& current = settings();
        if (current.autoQuality != s_observedQuality.autoQuality) s_savedQuality.autoQuality = current.autoQuality;
        if (current.preset != s_observedQuality.preset) s_savedQuality.preset = current.preset;
        if (current.iblCubeSize != s_observedQuality.iblCubeSize) s_savedQuality.iblCubeSize = current.iblCubeSize;
        if (current.iblIrradianceSize != s_observedQuality.iblIrradianceSize) s_savedQuality.iblIrradianceSize = current.iblIrradianceSize;
        if (current.iblLutSize != s_observedQuality.iblLutSize) s_savedQuality.iblLutSize = current.iblLutSize;
        if (current.iblHalfFloat != s_observedQuality.iblHalfFloat) s_savedQuality.iblHalfFloat = current.iblHalfFloat;
        if (current.iblSpecularSamples != s_observedQuality.iblSpecularSamples) s_savedQuality.iblSpecularSamples = current.iblSpecularSamples;
        if (current.gridMode != s_observedQuality.gridMode) s_savedQuality.gridMode = current.gridMode;
        if (current.renderScale != s_observedQuality.renderScale) s_savedQuality.renderScale = current.renderScale;
        if (current.shadows != s_observedQuality.shadows) s_savedQuality.shadows = current.shadows;
        if (current.shadowResolution != s_observedQuality.shadowResolution) s_savedQuality.shadowResolution = current.shadowResolution;
        if (current.shadowCascades != s_observedQuality.shadowCascades) s_savedQuality.shadowCascades = current.shadowCascades;
        if (current.shadowDistance != s_observedQuality.shadowDistance) s_savedQuality.shadowDistance = current.shadowDistance;
        if (current.shadowFilter != s_observedQuality.shadowFilter) s_savedQuality.shadowFilter = current.shadowFilter;
        if (current.shadowSplitLambda != s_observedQuality.shadowSplitLambda) s_savedQuality.shadowSplitLambda = current.shadowSplitLambda;
        if (current.shadowNormalBias != s_observedQuality.shadowNormalBias) s_savedQuality.shadowNormalBias = current.shadowNormalBias;
        if (current.shadowSlopeBias != s_observedQuality.shadowSlopeBias) s_savedQuality.shadowSlopeBias = current.shadowSlopeBias;
        if (current.shadowConstantBias != s_observedQuality.shadowConstantBias) s_savedQuality.shadowConstantBias = current.shadowConstantBias;
        if (current.msaaIdx != s_observedQuality.msaaIdx) s_savedQuality.msaaIdx = current.msaaIdx;
        if (current.postAAIdx != s_observedQuality.postAAIdx) s_savedQuality.postAAIdx = current.postAAIdx;
        if (current.ssrQuality != s_observedQuality.ssrQuality) s_savedQuality.ssrQuality = current.ssrQuality;
        if (current.ssaoQuality != s_observedQuality.ssaoQuality) s_savedQuality.ssaoQuality = current.ssaoQuality;
        if (current.ssaoHalfResolution != s_observedQuality.ssaoHalfResolution) s_savedQuality.ssaoHalfResolution = current.ssaoHalfResolution;
        if (current.skyOcclusionQuality != s_observedQuality.skyOcclusionQuality) s_savedQuality.skyOcclusionQuality = current.skyOcclusionQuality;
        if (current.oceanQuality != s_observedQuality.oceanQuality) s_savedQuality.oceanQuality = current.oceanQuality;
        if (current.ssrEnabled != s_observedQuality.ssrEnabled) s_savedQuality.ssrEnabled = current.ssrEnabled;
        if (current.ssaoEnabled != s_observedQuality.ssaoEnabled) s_savedQuality.ssaoEnabled = current.ssaoEnabled;
        if (current.iblEnabled != s_observedQuality.iblEnabled) s_savedQuality.iblEnabled = current.iblEnabled;
        if (current.bloomEnabled != s_observedQuality.bloomEnabled) s_savedQuality.bloomEnabled = current.bloomEnabled;
        if (current.lightsEnabled != s_observedQuality.lightsEnabled) s_savedQuality.lightsEnabled = current.lightsEnabled;
        if (current.skyOcclusion != s_observedQuality.skyOcclusion) s_savedQuality.skyOcclusion = current.skyOcclusion;
        if (current.oceanLive != s_observedQuality.oceanLive) s_savedQuality.oceanLive = current.oceanLive;
        if (current.oceanPlay != s_observedQuality.oceanPlay) s_savedQuality.oceanPlay = current.oceanPlay;
        if (current.lensFlare != s_observedQuality.lensFlare) s_savedQuality.lensFlare = current.lensFlare;
        if (current.lensFlareIntensity != s_observedQuality.lensFlareIntensity) s_savedQuality.lensFlareIntensity = current.lensFlareIntensity;
        if (current.textureMaxSize != s_observedQuality.textureMaxSize) s_savedQuality.textureMaxSize = current.textureMaxSize;
        if (s_selectedProfile != s_observedSelection) s_savedSelection = s_selectedProfile;
        observeSessionEdit();
    }
    SettingsFile& store = SettingsFile::instance();
    store.load();

    std::vector<SettingsEntry> profileEntries;
    if (s_savedSelection != 0) {
        profileEntries.emplace_back("selection", intText(s_savedSelection));
    }
    for (size_t i = 0; i < s_profiles.size(); ++i) {
        const QualityProfile& profile = s_profiles[i];
        const QualityProfile defaults = defaultProfile(static_cast<int>(i));
        const std::string prefix = "profile" + std::to_string(i) + ".";
        const auto add = [&](const char* key, const std::string& value) {
            profileEntries.emplace_back(prefix + key, value);
        };
        if (profile.render.ssrEnabled != defaults.render.ssrEnabled) add("ssrEnabled", boolText(profile.render.ssrEnabled));
        if (profile.render.ssaoEnabled != defaults.render.ssaoEnabled) add("ssaoEnabled", boolText(profile.render.ssaoEnabled));
        if (profile.render.iblEnabled != defaults.render.iblEnabled) add("iblEnabled", boolText(profile.render.iblEnabled));
        if (profile.render.bloomEnabled != defaults.render.bloomEnabled) add("bloomEnabled", boolText(profile.render.bloomEnabled));
        if (profile.render.lightsEnabled != defaults.render.lightsEnabled) add("lightsEnabled", boolText(profile.render.lightsEnabled));
        if (profile.render.oceanLive != defaults.render.oceanLive) add("oceanLive", boolText(profile.render.oceanLive));
        if (profile.render.oceanPlay != defaults.render.oceanPlay) add("oceanPlay", boolText(profile.render.oceanPlay));
        if (profile.render.skyOcclusion != defaults.render.skyOcclusion) add("skyOcclusion", intText(profile.render.skyOcclusion));
        if (profile.render.lensFlare != defaults.render.lensFlare) add("lensFlare", intText(profile.render.lensFlare));
        if (profile.render.textureMaxSize != defaults.render.textureMaxSize) add("textureMaxSize", intText(profile.render.textureMaxSize));
        if (profile.render.lensFlareIntensity != defaults.render.lensFlareIntensity) add("lensFlareIntensity", floatText(profile.render.lensFlareIntensity));
        if (profile.render.shadowSplitLambda != defaults.render.shadowSplitLambda) add("shadowSplitLambda", floatText(profile.render.shadowSplitLambda));
        if (profile.render.shadowNormalBias != defaults.render.shadowNormalBias) add("shadowNormalBias", floatText(profile.render.shadowNormalBias));
        if (profile.render.shadowSlopeBias != defaults.render.shadowSlopeBias) add("shadowSlopeBias", floatText(profile.render.shadowSlopeBias));
        if (profile.render.shadowConstantBias != defaults.render.shadowConstantBias) add("shadowConstantBias", floatText(profile.render.shadowConstantBias));
        if (profile.render.msaaIdx != defaults.render.msaaIdx) add("msaa", intText(profile.render.msaaIdx));
        if (profile.render.postAAIdx != defaults.render.postAAIdx) add("postAA", intText(profile.render.postAAIdx));
        if (profile.render.ssrQuality != defaults.render.ssrQuality) add("ssrQuality", intText(profile.render.ssrQuality));
        if (profile.render.ssaoQuality != defaults.render.ssaoQuality) add("ssaoQuality", intText(profile.render.ssaoQuality));
        if (profile.render.ssaoHalfResolution != defaults.render.ssaoHalfResolution) add("ssaoResolution", profile.render.ssaoHalfResolution ? "half" : "full");
        if (profile.render.skyOcclusionQuality != defaults.render.skyOcclusionQuality) add("skyOcclusionQuality", intText(profile.render.skyOcclusionQuality));
        if (profile.render.oceanQuality != defaults.render.oceanQuality) add("oceanQuality", intText(profile.render.oceanQuality));
        if (profile.skybox != defaults.skybox) add("skybox", boolText(profile.skybox));
        if (profile.render.iblCubeSize != defaults.render.iblCubeSize) add("cube", intText(profile.render.iblCubeSize));
        if (profile.render.iblIrradianceSize != defaults.render.iblIrradianceSize) add("irradiance", intText(profile.render.iblIrradianceSize));
        if (profile.render.iblLutSize != defaults.render.iblLutSize) add("lut", intText(profile.render.iblLutSize));
        if (profile.render.iblSpecularSamples != defaults.render.iblSpecularSamples) add("samples", intText(profile.render.iblSpecularSamples));
        if (profile.render.iblHalfFloat != defaults.render.iblHalfFloat) add("halfFloat", boolText(profile.render.iblHalfFloat));
        if (profile.render.gridMode != defaults.render.gridMode) add("grid", intText(static_cast<int>(profile.render.gridMode)));
        if (profile.render.renderScale != defaults.render.renderScale) add("scale", floatText(profile.render.renderScale));
        if (profile.render.shadows != defaults.render.shadows) add("shadows", boolText(profile.render.shadows));
        if (profile.render.shadowResolution != defaults.render.shadowResolution) add("shadowResolution", intText(profile.render.shadowResolution));
        if (profile.render.shadowCascades != defaults.render.shadowCascades) add("shadowCascades", intText(profile.render.shadowCascades));
        if (profile.render.shadowDistance != defaults.render.shadowDistance) add("shadowDistance", floatText(profile.render.shadowDistance));
        if (profile.render.shadowFilter != defaults.render.shadowFilter) add("shadowFilter", intText(static_cast<int>(profile.render.shadowFilter)));
    }
    store.replaceSection("QualityProfiles", std::move(profileEntries));

    const RenderQualitySettings current = s_sessionReady ? s_savedQuality : settings();
    RenderQualitySettings defaults = s_profiles[s_savedSelection == 0 ? 3 : s_savedSelection - 1].render;
    defaults.autoQuality = s_savedSelection == 0;
    std::vector<SettingsEntry> qualityEntries;
    if (current.autoQuality != defaults.autoQuality) qualityEntries.emplace_back("autoQuality", boolText(current.autoQuality));
    if (current.preset != defaults.preset) qualityEntries.emplace_back("preset", intText(static_cast<int>(current.preset)));
    if (current.iblCubeSize != defaults.iblCubeSize) qualityEntries.emplace_back("iblCubeSize", intText(current.iblCubeSize));
    if (current.iblIrradianceSize != defaults.iblIrradianceSize) qualityEntries.emplace_back("iblIrradianceSize", intText(current.iblIrradianceSize));
    if (current.iblLutSize != defaults.iblLutSize) qualityEntries.emplace_back("iblLutSize", intText(current.iblLutSize));
    if (current.iblHalfFloat != defaults.iblHalfFloat) qualityEntries.emplace_back("iblHalfFloat", boolText(current.iblHalfFloat));
    if (current.iblSpecularSamples != defaults.iblSpecularSamples) qualityEntries.emplace_back("iblSpecularSamples", intText(current.iblSpecularSamples));
    if (current.gridMode != defaults.gridMode) qualityEntries.emplace_back("gridMode", intText(static_cast<int>(current.gridMode)));
    if (current.renderScale != defaults.renderScale) qualityEntries.emplace_back("renderScale", floatText(current.renderScale));
    if (current.shadows != defaults.shadows) qualityEntries.emplace_back("shadows", boolText(current.shadows));
    if (current.shadowResolution != defaults.shadowResolution) qualityEntries.emplace_back("shadowResolution", intText(current.shadowResolution));
    if (current.shadowCascades != defaults.shadowCascades) qualityEntries.emplace_back("shadowCascades", intText(current.shadowCascades));
    if (current.shadowDistance != defaults.shadowDistance) qualityEntries.emplace_back("shadowDistance", floatText(current.shadowDistance));
    if (current.shadowFilter != defaults.shadowFilter) qualityEntries.emplace_back("shadowFilter", intText(static_cast<int>(current.shadowFilter)));
    if (current.shadowSplitLambda != defaults.shadowSplitLambda) qualityEntries.emplace_back("shadowSplitLambda", floatText(current.shadowSplitLambda));
    if (current.shadowNormalBias != defaults.shadowNormalBias) qualityEntries.emplace_back("shadowNormalBias", floatText(current.shadowNormalBias));
    if (current.shadowSlopeBias != defaults.shadowSlopeBias) qualityEntries.emplace_back("shadowSlopeBias", floatText(current.shadowSlopeBias));
    if (current.shadowConstantBias != defaults.shadowConstantBias) qualityEntries.emplace_back("shadowConstantBias", floatText(current.shadowConstantBias));
    if (current.ssrEnabled != defaults.ssrEnabled) qualityEntries.emplace_back("ssrEnabled", boolText(current.ssrEnabled));
    if (current.ssaoEnabled != defaults.ssaoEnabled) qualityEntries.emplace_back("ssaoEnabled", boolText(current.ssaoEnabled));
    if (current.iblEnabled != defaults.iblEnabled) qualityEntries.emplace_back("iblEnabled", boolText(current.iblEnabled));
    if (current.bloomEnabled != defaults.bloomEnabled) qualityEntries.emplace_back("bloomEnabled", boolText(current.bloomEnabled));
    if (current.lightsEnabled != defaults.lightsEnabled) qualityEntries.emplace_back("lightsEnabled", boolText(current.lightsEnabled));
    if (current.oceanLive != defaults.oceanLive) qualityEntries.emplace_back("oceanLive", boolText(current.oceanLive));
    if (current.oceanPlay != defaults.oceanPlay) qualityEntries.emplace_back("oceanPlay", boolText(current.oceanPlay));
    if (current.msaaIdx != defaults.msaaIdx) qualityEntries.emplace_back("msaaIdx", intText(current.msaaIdx));
    if (current.postAAIdx != defaults.postAAIdx) qualityEntries.emplace_back("postAAIdx", intText(current.postAAIdx));
    if (current.ssrQuality != defaults.ssrQuality) qualityEntries.emplace_back("ssrQuality", intText(current.ssrQuality));
    if (current.ssaoQuality != defaults.ssaoQuality) qualityEntries.emplace_back("ssaoQuality", intText(current.ssaoQuality));
    if (current.skyOcclusionQuality != defaults.skyOcclusionQuality) qualityEntries.emplace_back("skyOcclusionQuality", intText(current.skyOcclusionQuality));
    if (current.oceanQuality != defaults.oceanQuality) qualityEntries.emplace_back("oceanQuality", intText(current.oceanQuality));
    if (current.skyOcclusion != defaults.skyOcclusion) qualityEntries.emplace_back("skyOcclusion", intText(current.skyOcclusion));
    if (current.lensFlare != defaults.lensFlare) qualityEntries.emplace_back("lensFlare", intText(current.lensFlare));
    if (current.textureMaxSize != defaults.textureMaxSize) qualityEntries.emplace_back("textureMaxSize", intText(current.textureMaxSize));
    if (current.lensFlareIntensity != defaults.lensFlareIntensity) qualityEntries.emplace_back("lensFlareIntensity", floatText(current.lensFlareIntensity));
    if (current.ssaoHalfResolution != defaults.ssaoHalfResolution) qualityEntries.emplace_back("ssaoHalfResolution", boolText(current.ssaoHalfResolution));
    store.replaceSection("RenderQuality", std::move(qualityEntries));

    store.save();
}

bool RenderQuality::parseArgument(int argc, char** argv, int& i, std::string& error)
{
    const std::string arg = argv[i];
    if (arg == "--show-telemetry") { s_showRuntimeUi = true; return true; }
    // Settings scopes: --shadows is the scene's switch (CommandLine.cpp, through the registry); configureAuto still
    // reads it as a shadow pin.
    const bool shadowFlag = arg == "--shadow-resolution" || arg == "--shadow-cascades"
        || arg == "--shadow-distance" || arg == "--shadow-filter" || arg == "--shadow-split-lambda"
        || arg == "--shadow-normal-bias" || arg == "--shadow-slope-bias" || arg == "--shadow-constant-bias";
    const bool takesValue = arg == "--quality-preset" || arg == "--auto-quality" || arg == "--quality" || arg == "--ibl-size" || arg == "--ibl-irradiance-size"
        || arg == "--ibl-lut-size" || arg == "--ibl-precision" || arg == "--ibl-samples"
        || arg == "--grid-mode" || arg == "--render-scale" || shadowFlag;
    if (!takesValue) {
        return false;
    }
    if (i + 1 >= argc) {
        error = arg + " needs a value";
        return true;
    }
    const std::string value = argv[++i];
    RenderQualitySettings& s = settings();
    const auto u = static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
    bool ok = true;
    if (arg == "--quality-preset") {
        const char* names[] = {"auto","low","med","high","ultra"};
        const auto it = std::find(std::begin(names),std::end(names),value == "mid" ? std::string("med") : value);
        ok = it != std::end(names);
        if (ok) {
            s_selectedProfile = static_cast<int>(it-std::begin(names)); s_profilesActive = true;
            s = targetProfile().render; s.autoQuality = s_selectedProfile == 0; s_showRuntimeUi = true;
        }
    } else if (arg == "--auto-quality") {
        ok = value == "on" || value == "off";
        s.autoQuality = value == "on";
    } else if (arg == "--quality") {
        const RenderQualitySettings before = s;
        if (value == "high") s = RenderQualitySettings::fromPreset(QualityPreset::High);
        else if (value == "balanced") s = RenderQualitySettings::fromPreset(QualityPreset::Balanced);
        else if (value == "performance") s = RenderQualitySettings::fromPreset(QualityPreset::Performance);
        else ok = false;
        s.autoQuality = before.autoQuality;
        s.copyShadows(before); // the legacy presets predate the sun shadows and leave them alone
        s.copyEffectQuality(before); // and the AA / effect quality that moved here from the viewports
    } else if (shadowFlag) {
        // A3.2 sun shadows; the same values the registry settings accept.
        float f = 0.0f;
        if (arg == "--shadow-resolution") {
            if (value == "off" || value == "0") s.shadows = false;
            else { ok = setChecked(s.shadowResolution, u, kShadowResolutions); if (ok) s.shadows = true; }
        } else if (arg == "--shadow-cascades") {
            int cascades = 0;
            ok = parseInt(value, cascades) && cascades >= 2 && cascades <= 4;
            if (ok) s.shadowCascades = cascades;
        } else if (arg == "--shadow-distance") {
            ok = parseFloat(value, f) && validShadowDistance(f);
            if (ok) s.shadowDistance = f;
        } else if (arg == "--shadow-filter") {
            ok = value == "hard" || value == "pcf" || value == "soft";
            if (ok) s.shadowFilter = value == "hard" ? ShadowFilter::Hard : value == "soft" ? ShadowFilter::Soft : ShadowFilter::Pcf;
        } else if (arg == "--shadow-split-lambda") {
            ok = parseFloat(value, f) && validShadowLambda(f);
            if (ok) s.shadowSplitLambda = f;
        } else {
            ok = parseFloat(value, f) && validShadowBias(f);
            if (ok && arg == "--shadow-normal-bias") s.shadowNormalBias = f;
            if (ok && arg == "--shadow-slope-bias") s.shadowSlopeBias = f;
            if (ok && arg == "--shadow-constant-bias") s.shadowConstantBias = f;
        }
    } else {
        s.preset = QualityPreset::Custom;
        if (arg == "--ibl-size") {
            if (value == "off" || value == "0") s.iblEnabled = false;
            else { ok = setChecked(s.iblCubeSize, u, kCubeSizes); if (ok) s.iblEnabled = true; }
        } else if (arg == "--ibl-irradiance-size") ok = setChecked(s.iblIrradianceSize, u, kIrradianceSizes);
        else if (arg == "--ibl-lut-size") ok = setChecked(s.iblLutSize, u, kLutSizes);
        else if (arg == "--ibl-samples") ok = setChecked(s.iblSpecularSamples, u, kSampleCounts);
        else if (arg == "--ibl-precision") {
            ok = value == "32" || value == "16";
            s.iblHalfFloat = value == "16";
        } else if (arg == "--grid-mode") {
            ok = value == "ray" || value == "plane";
            s.gridMode = value == "plane" ? GridMode::PlaneMesh : GridMode::ScreenRay;
        } else if (arg == "--render-scale") {
            ok = setChecked(s.renderScale, std::strtof(value.c_str(), nullptr), kRenderScales);
        }
    }
    if (!ok) {
        error = "Invalid " + arg + " '" + value + "'";
    }
    return true;
}

const char* RenderQuality::presetName(QualityPreset preset)
{
    switch (preset) {
    case QualityPreset::High: return "High (original)";
    case QualityPreset::Balanced: return "Balanced";
    case QualityPreset::Performance: return "Performance";
    case QualityPreset::Custom: return "Custom";
    }
    return "Custom";
}

void RenderQuality::drawSettingsTab(ux3d::renderer::GltfRenderLayer* layer, SceneRevisions& revisions)
{
    if (autoActive() && !qualityReached()) ImGui::TextWrapped("%s", autoStatus().c_str());
    ImGui::TextWrapped("Quality controls effect levels for every viewport. Scene controls appearance; Shaded omits post effects.");
    ImGui::Spacing();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Active preset");
    ImGui::SameLine();
    drawProfileSelector(revisions, "quality.settings.preset");

    // Configured (the preset or your edits) next to in effect (Auto quality starts cheaper and steps up).
    const bool summaryOpen = ImGui::CollapsingHeader("Configured and in effect", ImGuiTreeNodeFlags_DefaultOpen);
    UiProbe::item("quality.summary", 0);
    if (summaryOpen) {
        const RenderQualitySettings configured = settings();
        const RenderQualitySettings now = effective();
        const char* msaa[] = { "Off", "2x", "4x", "8x" };
        const char* post[] = { "None", "FXAA", "TAA" };
        const char* levels[] = { "Low", "Medium", "High" };
        struct Row { const char* name; std::string configured; std::string effective; };
        const auto scale = [](float value) { return std::to_string(static_cast<int>(std::lround(value * 100.0f))) + "%"; };
        const auto shadow = [](const RenderQualitySettings& q) {
            return q.shadows ? std::to_string(q.shadowResolution) + " x " + std::to_string(q.shadowCascades) : std::string("Off");
        };
        const std::vector<Row> rows = {
            { "MSAA", msaa[std::clamp(configured.msaaIdx, 0, 3)], msaa[std::clamp(now.msaaIdx, 0, 3)] },
            { "Post AA", post[std::clamp(configured.postAAIdx, 0, 2)], post[std::clamp(now.postAAIdx, 0, 2)] },
            { "Render scale", scale(configured.renderScale), scale(now.renderScale) },
            { "IBL cube", configured.iblEnabled ? std::to_string(configured.iblCubeSize) : "Off", now.iblEnabled ? std::to_string(now.iblCubeSize) : "Off" },
            { "Reflections", configured.ssrEnabled ? levels[std::clamp(configured.ssrQuality,0,2)] : "Off", now.ssrEnabled ? levels[std::clamp(now.ssrQuality,0,2)] : "Off" },
            { "SSAO", configured.ssaoEnabled ? levels[std::clamp(configured.ssaoQuality,0,2)] : "Off", now.ssaoEnabled ? levels[std::clamp(now.ssaoQuality,0,2)] : "Off" },
            { "Sun shadow map", shadow(configured), shadow(now) },
        };
        if (ImGui::BeginTable("QualityEffectiveTable", 3, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Setting");
            ImGui::TableSetupColumn("Configured");
            ImGui::TableSetupColumn("In effect now");
            ImGui::TableHeadersRow();
            for (const Row& row : rows) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(row.name);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(row.configured.c_str());
                ImGui::TableNextColumn();
                if (row.configured != row.effective) {
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", row.effective.c_str());
                } else {
                    ImGui::TextUnformatted(row.effective.c_str());
                }
            }
            ImGui::EndTable();
        }
        ImGui::TextDisabled(autoActive() ? "Highlighted values are limited by Auto quality for now." : "Auto quality is off: configured = in effect.");
    }
    ImGui::Spacing();

    if (ImGui::BeginTabBar("QualitySettingsTabs")) {
        const bool presets = ImGui::BeginTabItem("Presets");
        UiProbe::item("settings.quality.presets", 0);
        if (presets) {
            drawProfilesEditor(revisions);
            ImGui::EndTabItem();
        }
        const bool current = ImGui::BeginTabItem("Current Settings");
        UiProbe::item("settings.quality.current", 0);
        if (current) {
            drawSettings(layer, revisions);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void RenderQuality::drawSettings(ux3d::renderer::GltfRenderLayer* layer, SceneRevisions& revisions)
{
    RenderQualitySettings& s = settings();

    RenderQualitySettings before = s;

    if (ImGui::Checkbox("Punctual lights##currentQuality", &s.lightsEnabled)) revisions.mark(SceneChange::RenderSettings);
    UiProbe::item("quality.current.lights", 0);
    // The IBL filter (formerly edited in the Scene Inspector, which now only summarises it).
    ImGui::SeparatorText("Image-Based Lighting");
    drawIblSettings(layer, revisions);
    // drawIblSettings applies, re-filters and saves its own edits; the checks below must not repeat them.
    before.iblEnabled = s.iblEnabled;
    before.iblCubeSize = s.iblCubeSize;
    before.iblIrradianceSize = s.iblIrradianceSize;
    before.iblLutSize = s.iblLutSize;
    before.iblHalfFloat = s.iblHalfFloat;
    before.iblSpecularSamples = s.iblSpecularSamples;
    before.preset = s.preset;

    // Each control switches the preset to Custom when it changes a value.
    bool custom = false;

    // Settings scopes: anti-aliasing is renderer-wide (it was per viewport). Compare modes in two instances.
    ImGui::SeparatorText("Anti-Aliasing");
    {
        const char* msaaModes[] = { "Off", "2x", "4x", "8x" };
        int msaa = std::clamp(s.msaaIdx, 0, 3);
        if (ImGui::Combo("MSAA##qualityMsaa", &msaa, msaaModes, 4)) {
            s.msaaIdx = msaa;
            s_pinAA = true;
            revisions.mark(SceneChange::ViewportSettings);
        }
        UiProbe::item("quality.msaa", 0);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Crisp geometry edges; costs memory and fill rate.");
        const char* postModes[] = { "None", "FXAA", "TAA" };
        int post = std::clamp(s.postAAIdx, 0, 2);
        if (ImGui::Combo("Post AA##qualityPostAA", &post, postModes, 3)) {
            s.postAAIdx = post;
            s_pinAA = true;
            revisions.mark(SceneChange::ViewportSettings);
        }
        UiProbe::item("quality.post_aa", 0);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("FXAA: cheap, slightly soft.\nTAA: stable, best on shader aliasing and decals; can ghost.");
        const RenderQualitySettings aaEffective = effective();
        if (aaEffective.msaaIdx != s.msaaIdx || aaEffective.postAAIdx != s.postAAIdx) {
            ImGui::TextDisabled("Auto quality: %s + %s for now", msaaModes[std::clamp(aaEffective.msaaIdx, 0, 3)],
                                postModes[std::clamp(aaEffective.postAAIdx, 0, 2)]);
        }
    }

    ImGui::SeparatorText("Viewport");
    int gridMode = static_cast<int>(s.gridMode);
    const char* gridModes[] = { "Screen ray (original)", "Plane mesh (faster)" };
    if (ImGui::Combo("Ground Grid", &gridMode, gridModes, 2)) {
        s.gridMode = static_cast<GridMode>(gridMode);
        custom = true;
    }
    int scaleIdx = static_cast<int>(std::find(std::begin(kRenderScales), std::end(kRenderScales), s.renderScale) - std::begin(kRenderScales));
    const char* scales[] = { "50%", "67%", "75%", "85%", "100%" };
    if (ImGui::Combo("Render Scale", &scaleIdx, scales, IM_ARRAYSIZE(scales))) {
        s.renderScale = kRenderScales[scaleIdx];
        custom = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Renders the 3D view at a fraction of its window size and stretches it.\nFor weak GPUs; lowers sharpness.");
    }

    ImGui::SeparatorText("Effect levels");
    if (drawEffectLevels(s, false)) {
        if (s.ssrEnabled != before.ssrEnabled || s.ssrQuality != before.ssrQuality) s_pinSsr = true;
        if (s.bloomEnabled != before.bloomEnabled) s_pinBloom = true;
        revisions.mark(SceneChange::RenderSettings);
    }
    const uint32_t textureSizes[] = {0,128,256,512,1024,2048,4096,8192,16384};
    if (sizeCombo("Texture maximum", s.textureMaxSize, textureSizes, "Unlimited")) {
        s_textureRefresh = true; revisions.mark(SceneChange::RenderSettings);
    }
    // A3.2: cascaded shadow maps of the sun. Renderer-wide; each viewport keeps its own map, fitted to its view.
    // The scene switches the shadows on (Scene Inspector > Shadows, Reflections & AO); this gate is the cost side
    // (part of the quality presets).
    ImGui::SeparatorText("Sun Shadows");
    bool shadowChanged = enabledSizeCombo("Shadow map", s.shadows, s.shadowResolution, kShadowResolutions);
    UiProbe::item("quality.shadows", 0);
    if (s.shadows) {
    shadowChanged |= ImGui::SliderInt("Cascades", &s.shadowCascades, 2, 4);
    UiProbe::item("quality.shadow.details", 0);
    shadowChanged |= ImGui::DragFloat("Max Distance", &s.shadowDistance, 1.0f, kShadowDistanceMin, kShadowDistanceMax, "%.0f m",
                                      ImGuiSliderFlags_AlwaysClamp);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("View depth where the shadows end (they are also cut to the scene's extent).");
    {
        int filter = static_cast<int>(s.shadowFilter);
        const char* filters[] = { "Hard", "PCF", "Soft" };
        if (ImGui::Combo("Filter", &filter, filters, 3)) {
            s.shadowFilter = static_cast<ShadowFilter>(filter);
            shadowChanged = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Hard: 1 bilinear compare. PCF: 3x3 compares. Soft: 16 rotated Poisson compares.");
    }
    shadowChanged |= ImGui::SliderFloat("Split Lambda", &s.shadowSplitLambda, 0.0f, 1.0f, "%.2f");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cascade split scheme: 0 uniform, 1 logarithmic.");
    shadowChanged |= ImGui::DragFloat("Normal Bias", &s.shadowNormalBias, 0.05f, 0.0f, 10.0f, "%.2f texels", ImGuiSliderFlags_AlwaysClamp);
    shadowChanged |= ImGui::DragFloat("Slope Bias", &s.shadowSlopeBias, 0.05f, 0.0f, 10.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
    shadowChanged |= ImGui::DragFloat("Constant Bias", &s.shadowConstantBias, 0.1f, 0.0f, 100.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    ImGui::TextDisabled("Shadow map memory: %.0f MiB per viewport", static_cast<double>(s.estimatedShadowBytes()) / (1024.0 * 1024.0));
    const RenderQualitySettings shadowEffective = effective();
    if (s.shadows && (shadowEffective.shadowResolution != s.shadowResolution || shadowEffective.shadowCascades != s.shadowCascades
                      || shadowEffective.shadowFilter != s.shadowFilter)) {
        ImGui::TextDisabled("Auto quality: %u x %d cascades for now", shadowEffective.shadowResolution, shadowEffective.shadowCascades);
    }
    }
    if (shadowChanged) {
        s_pinShadow = true;
        revisions.mark(SceneChange::RenderSettings);
    }

    if (custom) {
        s.preset = QualityPreset::Custom;
    }
    if (s == before) {
        return;
    }
    if (s.renderScale != before.renderScale) s_pinScale = true;
    if (!s.sameIbl(before)) s_pinIbl = true;
    // The preset also sets the IBL fields, which are edited in the Scene Inspector.
    if (!s.sameIbl(before) && layer != nullptr) {
        applyIblParameters(*layer);
        if (layer->getActiveEnvironmentMap() != nullptr && !layer->isExternalEnvironment()) {
            const auto path = layer->getEnvironmentMapPath();
            layer->setEnvironmentMap(path);
            layer->filterEnvironmentMap();
        }
        revisions.mark(SceneChange::RenderSettings);
    }
    if (s.gridMode != before.gridMode || s.renderScale != before.renderScale) {
        revisions.mark(SceneChange::ViewportSettings);
    }
    save();
}

void RenderQuality::drawIblSettings(ux3d::renderer::GltfRenderLayer* layer, SceneRevisions& revisions)
{
    RenderQualitySettings& s = settings();
    const RenderQualitySettings before = s;

    // In a narrow window, size the combos so the right-hand labels are not clipped by the edge;
    // otherwise use the same default width as the other Quality rows, so the columns line up.
    const float labelWidth = ImGui::CalcTextSize("Specular Samples").x;
    const float fitWidth = std::max(80.0f, ImGui::GetContentRegionAvail().x - labelWidth - ImGui::GetStyle().ItemInnerSpacing.x);
    const float itemWidth = std::min(fitWidth, ImGui::CalcItemWidth());
    ImGui::PushItemWidth(itemWidth);

    bool changed = false;
    changed |= enabledSizeCombo("IBL cube", s.iblEnabled, s.iblCubeSize, kCubeSizes);
    UiProbe::item("quality.ibl_cube", 0);
    if (s.iblEnabled) {
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Face size of the reflection cube. The skybox shows its sharpest level,\nso smaller sizes blur the background.");
    }
    // The id carries the live value so a scenario can assert both visibility and content.
    UiProbe::item("scene.ibl.cube", static_cast<long long>(s.iblCubeSize));
    UiProbe::item("quality.ibl.details", 0);
    changed |= sizeCombo("Irradiance Cube", s.iblIrradianceSize, kIrradianceSizes, "Same as specular");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Diffuse lighting is very smooth; 32-64 is usually indistinguishable\nand filters much faster.");
    }
    changed |= sizeCombo("BRDF LUT", s.iblLutSize, kLutSizes, "");
    changed |= sizeCombo("Specular Samples", s.iblSpecularSamples, kSampleCounts, "");
    int precision = s.iblHalfFloat ? 1 : 0;
    const char* precisions[] = { "32-bit float", "16-bit half float" };
    if (ImGui::Combo("Precision", &precision, precisions, 2)) {
        s.iblHalfFloat = precision == 1;
        changed = true;
    }
    ImGui::TextDisabled("Estimated IBL memory: %.1f MB", static_cast<double>(s.estimatedIblBytes()) / (1024.0 * 1024.0));
    }
    ImGui::PopItemWidth();

    if (changed) {
        s.preset = QualityPreset::Custom;
        s_pinIbl = true;
    }
    if (s == before) {
        return;
    }
    if (layer != nullptr) {
        applyIblParameters(*layer);
        if (layer->getActiveEnvironmentMap() != nullptr && !layer->isExternalEnvironment()) {
            const auto path = layer->getEnvironmentMapPath();
            layer->setEnvironmentMap(path);
            layer->filterEnvironmentMap();
        }
    }
    revisions.mark(SceneChange::RenderSettings);
    save();
}

} // namespace ux3d::imgui
