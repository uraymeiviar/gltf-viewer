// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/RenderQuality.h"
#include "imgui/MemoryInfo.h"
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
bool s_autoSuppressed = false;
bool s_pinIbl = false, s_pinScale = false, s_pinAA = false, s_pinSsr = false, s_pinBloom = false;
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
    if (index == 0) {
        p.textureMaxSize = 512; p.skybox = p.ibl = false;
        p.render.gridMode = GridMode::PlaneMesh;
        p.render.iblCubeSize = 256; p.render.iblIrradianceSize = 32;
        p.render.iblLutSize = 128; p.render.iblSpecularSamples = 128; p.render.iblHalfFloat = true;
    } else if (index == 1) {
        p.textureMaxSize = 1024; p.msaaIdx = 1;
        p.render.iblCubeSize = 512; p.render.iblIrradianceSize = 64;
        p.render.iblLutSize = 256; p.render.iblSpecularSamples = 512; p.render.iblHalfFloat = true;
    } else {
        p.textureMaxSize = index == 2 ? 4096 : 0;
        p.msaaIdx = index == 3 ? 2 : 1;
        p.postAAIdx = 2; p.ssr = p.bloom = true; p.ssrQuality = index == 3 ? 2 : 1;
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
        if (field == "textureMaxSize") {
            if (parseUint(value, u) && isValidTextureMax(u)) profile.textureMaxSize = u; else invalid = true;
        } else if (field == "msaa") {
            if (parseInt(value, i) && i >= 0 && i <= 3) profile.msaaIdx = i; else invalid = true;
        } else if (field == "postAA") {
            if (parseInt(value, i) && i >= 0 && i <= 2) profile.postAAIdx = i; else invalid = true;
        } else if (field == "ssrQuality") {
            if (parseInt(value, i) && i >= 0 && i <= 2) profile.ssrQuality = i; else invalid = true;
        } else if (field == "skybox") {
            if (parseBool(value, b)) profile.skybox = b; else invalid = true;
        } else if (field == "ibl") {
            if (parseBool(value, b)) profile.ibl = b; else invalid = true;
        } else if (field == "lights") {
            if (parseBool(value, b)) profile.lights = b; else invalid = true;
        } else if (field == "ssr") {
            if (parseBool(value, b)) profile.ssr = b; else invalid = true;
        } else if (field == "bloom") {
            if (parseBool(value, b)) profile.bloom = b; else invalid = true;
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
        }
    }
    if (invalid) {
        logSettingsMessage("Ignoring invalid quality preset values in settings.ini.", true);
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
    RenderQualitySettings s;
    bool parsedAny = false;
    for (const SettingsEntry& entry : entries) {
        uint32_t u = 0;
        int i = 0;
        bool b = false;
        float f = 1.0f;
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
bool RenderQuality::allowIbl() { return (!s_profilesActive || s_pinIbl || targetProfile().ibl) && (!autoActive() || s_pinIbl || s_autoLevel >= 1); }
bool RenderQuality::allowSkybox(bool requested) { return requested && (!s_profilesActive || targetProfile().skybox); }
bool RenderQuality::allowLights(bool requested) { return requested && (!s_profilesActive || targetProfile().lights); }
bool RenderQuality::legacyCaptureUi() { return s_autoSuppressed && !s_showRuntimeUi; }
bool RenderQuality::qualityReached() { return !autoActive() || s_autoLevel == 6; }
uint32_t RenderQuality::textureMaxSize() { return s_profilesActive ? targetProfile().textureMaxSize : 0; }
bool RenderQuality::unlimitedTextures() { return s_profilesActive && targetProfile().textureMaxSize == 0; }
bool RenderQuality::takeTextureRefresh() { return std::exchange(s_textureRefresh, false); }
bool RenderQuality::textureRefreshPending() { return s_textureRefresh; }
int RenderQuality::selectedProfile() { return s_selectedProfile; }
double RenderQuality::gpuFrameMs() { return s_gpuMs; }
const char* RenderQuality::profileName(int selection)
{
    static const char* names[] = {"Auto", "Low", "Med", "High", "Ultra"};
    return selection >= 0 && selection <= 4 ? names[selection] : "Auto";
}
void RenderQuality::selectProfile(int selection)
{
    if (selection < 0 || selection > 4) return;
    s_selectedProfile = selection; s_profilesActive = true;
    ++s_profileVersion;
    const auto& p = targetProfile();
    settings() = p.render; settings().autoQuality = selection == 0;
    s_autoSuppressed = false; s_showRuntimeUi = true; s_textureRefresh = true;
    s_pinIbl = s_pinScale = s_pinAA = s_pinSsr = s_pinBloom = false;
    s_autoLevel = 0; s_autoCeiling = 6; s_autoSamples = 0; s_autoMs = 0;
    s_autoWindow = std::chrono::steady_clock::now();
    save();
}

void RenderQuality::drawProfileSelector(SceneRevisions& revisions)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(4,0));
    ImGui::SetNextItemWidth(105);
    int choice = s_selectedProfile;
    const char* names[] = {"Auto", "Low", "Mid", "High", "Ultra"};
    if (ImGui::Combo("##QualityPreset", &choice, names, 5)) {
        selectProfile(choice); revisions.mark(SceneChange::ViewCamera);
    }
    UiProbe::item("quality.preset", choice);
    ImGui::PopStyleVar();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Quality: %s\nShared by all viewports. Texture changes re-decode from retained source data.\nAuto targets your Ultra profile within available frame/memory budgets.", profileName(choice));
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
        changed |= sizeCombo("Texture maximum",p.textureMaxSize,textureSizes,"Unlimited");
        UiProbe::item("quality.texture.max",i+1);
        const char* aa[] = {"Off","2x","4x","8x"};
        const char* post[] = {"None","FXAA","TAA"};
        changed |= ImGui::Combo("MSAA",&p.msaaIdx,aa,4);
        changed |= ImGui::Combo("Post AA",&p.postAAIdx,post,3);
        changed |= ImGui::Checkbox("HDR skybox",&p.skybox);
        changed |= ImGui::Checkbox("Image-based lighting (IBL)",&p.ibl);
        changed |= ImGui::Checkbox("Punctual lights",&p.lights);
        changed |= sizeCombo("IBL cube",p.render.iblCubeSize,kCubeSizes,"");
        changed |= sizeCombo("Irradiance",p.render.iblIrradianceSize,kIrradianceSizes,"Full cube");
        changed |= sizeCombo("IBL LUT",p.render.iblLutSize,kLutSizes,"");
        changed |= sizeCombo("IBL samples",p.render.iblSpecularSamples,kSampleCounts,"");
        changed |= ImGui::Checkbox("IBL half float",&p.render.iblHalfFloat);
        changed |= ImGui::Checkbox("Screen-space reflections",&p.ssr);
        const char* ssr[] = {"Low","Med","High"};
        changed |= ImGui::Combo("SSR quality",&p.ssrQuality,ssr,3);
        changed |= ImGui::Checkbox("Bloom",&p.bloom);
        int grid = static_cast<int>(p.render.gridMode);
        const char* grids[] = {"Screen ray","Mesh"};
        if (ImGui::Combo("Ground grid",&grid,grids,2)) { p.render.gridMode = static_cast<GridMode>(grid); changed = true; }
        changed |= ImGui::SliderFloat("Render scale",&p.render.renderScale,.5f,1.f,"%.2f");
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
    static bool initialized = false, previous = requested;
    if (initialized && requested != previous) s_pinBloom = true;
    initialized = true; previous = requested;
    return requested && (!s_profilesActive || s_pinBloom || targetProfile().bloom) && (!autoActive() || s_pinBloom || s_autoLevel >= 4);
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
    return result;
}

ViewportSettingsState RenderQuality::effectiveViewport(ViewportSettingsState& configured)
{
    static std::unordered_map<const ViewportSettingsState*, ViewportSettingsState> previous;
    static std::unordered_map<const ViewportSettingsState*, uint64_t> versions;
    if (s_profilesActive && versions[&configured] != s_profileVersion) {
        const auto& p = targetProfile();
        if (!s_pinAA) { configured.msaaIdx = p.msaaIdx; configured.postAAIdx = p.postAAIdx; }
        if (!s_pinSsr) { configured.screenSpaceReflections = p.ssr; configured.ssrQuality = p.ssrQuality; }
        previous[&configured] = configured;
        versions[&configured] = s_profileVersion;
    }
    if (auto it = previous.find(&configured); it != previous.end()) {
        s_pinAA |= configured.effectiveMsaaIdx() != it->second.effectiveMsaaIdx() ||
            configured.effectivePostAAIdx() != it->second.effectivePostAAIdx();
        s_pinSsr |= configured.screenSpaceReflections != it->second.screenSpaceReflections || configured.ssrQuality != it->second.ssrQuality;
        it->second = configured;
    } else previous.emplace(&configured, configured);
    auto result = configured;
    if (s_profilesActive) {
        const auto& p = targetProfile();
        if (!s_pinAA) { result.msaaIdx = p.msaaIdx; result.postAAIdx = p.postAAIdx; }
        if (!s_pinSsr) { result.screenSpaceReflections = p.ssr; result.ssrQuality = p.ssrQuality; }
    }
    if (!autoActive()) return result;
    // A user edit pins that group for this session. The controller never writes the
    // configured state, so it cannot mistake its own changes for manual edits.
    if (!s_pinAA && s_autoLevel < 4) { result.msaaIdx = 0; result.postAAIdx = 1; }
    if (!s_pinSsr) {
        result.screenSpaceReflections = result.screenSpaceReflections && s_autoLevel >= 5;
        if (s_autoLevel < 6) result.ssrQuality = 0;
    }
    return result;
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
    }
    s_autoSuppressed = deterministic && !explicitAuto;
    s_profilesActive = explicitProfile || (!deterministic && !legacyQuality);
    if (s_profilesActive && !explicitProfile) {
        const auto before = settings();
        settings() = targetProfile().render;
        settings().autoQuality = explicitAuto ? before.autoQuality : s_selectedProfile == 0;
        if (s_pinIbl) {
            settings().iblCubeSize = before.iblCubeSize; settings().iblIrradianceSize = before.iblIrradianceSize;
            settings().iblLutSize = before.iblLutSize; settings().iblSpecularSamples = before.iblSpecularSamples;
            settings().iblHalfFloat = before.iblHalfFloat;
        }
        if (s_pinScale) settings().renderScale = before.renderScale;
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
        if (!layer->getEnvironmentMapPath().toString().empty()) {
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
    readQualityProfiles(store);
    readRenderQuality(store);
}

void RenderQuality::save()
{
    if (!s_persistenceEnabled) {
        return;
    }
    SettingsFile& store = SettingsFile::instance();
    store.load();

    std::vector<SettingsEntry> profileEntries;
    if (s_selectedProfile != 0) {
        profileEntries.emplace_back("selection", intText(s_selectedProfile));
    }
    for (size_t i = 0; i < s_profiles.size(); ++i) {
        const QualityProfile& profile = s_profiles[i];
        const QualityProfile defaults = defaultProfile(static_cast<int>(i));
        const std::string prefix = "profile" + std::to_string(i) + ".";
        const auto add = [&](const char* key, const std::string& value) {
            profileEntries.emplace_back(prefix + key, value);
        };
        if (profile.textureMaxSize != defaults.textureMaxSize) add("textureMaxSize", intText(profile.textureMaxSize));
        if (profile.msaaIdx != defaults.msaaIdx) add("msaa", intText(profile.msaaIdx));
        if (profile.postAAIdx != defaults.postAAIdx) add("postAA", intText(profile.postAAIdx));
        if (profile.ssrQuality != defaults.ssrQuality) add("ssrQuality", intText(profile.ssrQuality));
        if (profile.skybox != defaults.skybox) add("skybox", boolText(profile.skybox));
        if (profile.ibl != defaults.ibl) add("ibl", boolText(profile.ibl));
        if (profile.lights != defaults.lights) add("lights", boolText(profile.lights));
        if (profile.ssr != defaults.ssr) add("ssr", boolText(profile.ssr));
        if (profile.bloom != defaults.bloom) add("bloom", boolText(profile.bloom));
        if (profile.render.iblCubeSize != defaults.render.iblCubeSize) add("cube", intText(profile.render.iblCubeSize));
        if (profile.render.iblIrradianceSize != defaults.render.iblIrradianceSize) add("irradiance", intText(profile.render.iblIrradianceSize));
        if (profile.render.iblLutSize != defaults.render.iblLutSize) add("lut", intText(profile.render.iblLutSize));
        if (profile.render.iblSpecularSamples != defaults.render.iblSpecularSamples) add("samples", intText(profile.render.iblSpecularSamples));
        if (profile.render.iblHalfFloat != defaults.render.iblHalfFloat) add("halfFloat", boolText(profile.render.iblHalfFloat));
        if (profile.render.gridMode != defaults.render.gridMode) add("grid", intText(static_cast<int>(profile.render.gridMode)));
        if (profile.render.renderScale != defaults.render.renderScale) add("scale", floatText(profile.render.renderScale));
    }
    store.replaceSection("QualityProfiles", std::move(profileEntries));

    const RenderQualitySettings current = settings();
    const RenderQualitySettings defaults;
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
    store.replaceSection("RenderQuality", std::move(qualityEntries));

    store.save();
}

bool RenderQuality::parseArgument(int argc, char** argv, int& i, std::string& error)
{
    const std::string arg = argv[i];
    if (arg == "--show-telemetry") { s_showRuntimeUi = true; return true; }
    const bool takesValue = arg == "--quality-preset" || arg == "--auto-quality" || arg == "--quality" || arg == "--ibl-size" || arg == "--ibl-irradiance-size"
        || arg == "--ibl-lut-size" || arg == "--ibl-precision" || arg == "--ibl-samples"
        || arg == "--grid-mode" || arg == "--render-scale";
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
        const bool autoQuality = s.autoQuality;
        if (value == "high") s = RenderQualitySettings::fromPreset(QualityPreset::High);
        else if (value == "balanced") s = RenderQualitySettings::fromPreset(QualityPreset::Balanced);
        else if (value == "performance") s = RenderQualitySettings::fromPreset(QualityPreset::Performance);
        else ok = false;
        s.autoQuality = autoQuality;
    } else {
        s.preset = QualityPreset::Custom;
        if (arg == "--ibl-size") ok = setChecked(s.iblCubeSize, u, kCubeSizes);
        else if (arg == "--ibl-irradiance-size") ok = setChecked(s.iblIrradianceSize, u, kIrradianceSizes);
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

void RenderQuality::drawSettings(ux3d::renderer::GltfRenderLayer* layer, SceneRevisions& revisions)
{
    RenderQualitySettings& s = settings();
    if (autoActive() && !qualityReached()) ImGui::TextWrapped("%s", autoStatus().c_str());

    ImGui::TextDisabled("Quality presets apply to all viewports.");
    drawProfileSelector(revisions);
    const RenderQualitySettings before = s;
    ImGui::TextWrapped("Edit saved Low / Med / High / Ultra defaults in File > Settings > Quality Presets.");

    // Each control switches the preset to Custom when it changes a value.
    bool custom = false;

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
        if (!layer->getEnvironmentMapPath().toString().empty()) {
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

    // The Inspector dock is often narrow; size the combos so the right-hand labels are not
    // clipped by the panel edge.
    const float labelWidth = ImGui::CalcTextSize("Specular / Skybox Cube").x;
    const float itemWidth = std::max(80.0f, ImGui::GetContentRegionAvail().x - labelWidth - ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::PushItemWidth(itemWidth);

    bool changed = false;
    changed |= sizeCombo("Specular / Skybox Cube", s.iblCubeSize, kCubeSizes, "");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Face size of the reflection cube. The skybox shows its sharpest level,\nso smaller sizes blur the background.");
    }
    // The id carries the live value so a scenario can assert both visibility and content.
    UiProbe::item("scene.ibl.cube", static_cast<long long>(s.iblCubeSize));
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
        if (!layer->getEnvironmentMapPath().toString().empty()) {
            const auto path = layer->getEnvironmentMapPath();
            layer->setEnvironmentMap(path);
            layer->filterEnvironmentMap();
        }
    }
    revisions.mark(SceneChange::RenderSettings);
    save();
}

} // namespace ux3d::imgui
