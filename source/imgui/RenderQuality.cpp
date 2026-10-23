// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/RenderQuality.h"
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

std::filesystem::path configPath()
{
    const char* appData = std::getenv("APPDATA");
    if (appData == nullptr || std::strlen(appData) == 0) {
        return "render_quality.txt";
    }
    std::filesystem::path dir = std::filesystem::path(appData) / "Glider";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir / "render_quality.txt";
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

void RenderQuality::applyIblParameters(ux3d::renderer::GltfRenderLayer& layer)
{
    const RenderQualitySettings& s = settings();
    layer.iblParameters.textureSize = s.iblCubeSize;
    layer.iblParameters.lambertianSize = s.iblIrradianceSize;
    layer.iblParameters.lutResolution = s.iblLutSize;
    layer.iblParameters.halfFloat = s.iblHalfFloat;
    layer.iblSampling.ggxSampleCount = s.iblSpecularSamples;
}

void RenderQuality::setPersistenceEnabled(bool enabled)
{
    s_persistenceEnabled = enabled;
}

void RenderQuality::load()
{
    if (!s_persistenceEnabled) {
        return;
    }
    std::ifstream in(configPath());
    if (!in.is_open()) {
        return;
    }
    RenderQualitySettings s = settings();
    std::string key;
    double value = 0.0;
    while (in >> key >> value) {
        const auto u = static_cast<uint32_t>(value);
        if (key == "preset" && value >= 0 && value <= 3) s.preset = static_cast<QualityPreset>(u);
        else if (key == "iblCubeSize") setChecked(s.iblCubeSize, u, kCubeSizes);
        else if (key == "iblIrradianceSize") setChecked(s.iblIrradianceSize, u, kIrradianceSizes);
        else if (key == "iblLutSize") setChecked(s.iblLutSize, u, kLutSizes);
        else if (key == "iblHalfFloat") s.iblHalfFloat = value != 0.0;
        else if (key == "iblSpecularSamples") setChecked(s.iblSpecularSamples, u, kSampleCounts);
        else if (key == "gridMode" && (u == 0 || u == 1)) s.gridMode = static_cast<GridMode>(u);
        else if (key == "renderScale") setChecked(s.renderScale, static_cast<float>(value), kRenderScales);
    }
    settings() = s;
}

void RenderQuality::save()
{
    if (!s_persistenceEnabled) {
        return;
    }
    std::ofstream out(configPath());
    if (!out.is_open()) {
        return;
    }
    const RenderQualitySettings& s = settings();
    out << "preset " << static_cast<int>(s.preset) << "\n";
    out << "iblCubeSize " << s.iblCubeSize << "\n";
    out << "iblIrradianceSize " << s.iblIrradianceSize << "\n";
    out << "iblLutSize " << s.iblLutSize << "\n";
    out << "iblHalfFloat " << (s.iblHalfFloat ? 1 : 0) << "\n";
    out << "iblSpecularSamples " << s.iblSpecularSamples << "\n";
    out << "gridMode " << static_cast<int>(s.gridMode) << "\n";
    out << "renderScale " << s.renderScale << "\n";
}

bool RenderQuality::parseArgument(int argc, char** argv, int& i, std::string& error)
{
    const std::string arg = argv[i];
    const bool takesValue = arg == "--quality" || arg == "--ibl-size" || arg == "--ibl-irradiance-size"
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
    if (arg == "--quality") {
        if (value == "high") s = RenderQualitySettings::fromPreset(QualityPreset::High);
        else if (value == "balanced") s = RenderQualitySettings::fromPreset(QualityPreset::Balanced);
        else if (value == "performance") s = RenderQualitySettings::fromPreset(QualityPreset::Performance);
        else ok = false;
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
    const RenderQualitySettings before = s;

    ImGui::TextDisabled("Applies to all viewports. High keeps the original quality.");
    ImGui::TextDisabled("IBL filter sizes are in the Scene Inspector (select the scene).");
    if (ImGui::BeginCombo("Preset", presetName(s.preset))) {
        for (int p = 0; p <= static_cast<int>(QualityPreset::Performance); ++p) {
            const auto preset = static_cast<QualityPreset>(p);
            if (ImGui::Selectable(presetName(preset), s.preset == preset)) {
                s = RenderQualitySettings::fromPreset(preset);
            }
        }
        ImGui::EndCombo();
    }

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
