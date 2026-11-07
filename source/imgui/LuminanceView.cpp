// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// The Luminance debug view's legend (LuminanceView.h); the GPU pass is in AntiAliasingLuminance.cpp.

#include "imgui/LuminanceView.h"
#include "imgui/UiProbe.h"
#include "imgui/ViewportSettingsPanel.h"

#include <cstdio>
#include <vector>

namespace ux3d::imgui::luminance {

namespace {

ImU32 toImGui(const glm::vec3& c)
{
    return IM_COL32(static_cast<int>(std::lround(std::clamp(c.r, 0.0f, 1.0f) * 255.0f)),
                    static_cast<int>(std::lround(std::clamp(c.g, 0.0f, 1.0f) * 255.0f)),
                    static_cast<int>(std::lround(std::clamp(c.b, 0.0f, 1.0f) * 255.0f)), 255);
}

// A decade's tick label: 0.01 0.1 1 10 100 1k 10k 100k 1M ...
std::string decadeLabel(int decade)
{
    char text[32];
    if (decade >= 9) {
        std::snprintf(text, sizeof(text), "1e%d", decade);
    } else if (decade >= 6) {
        std::snprintf(text, sizeof(text), "%.0fM", std::pow(10.0, decade - 6));
    } else if (decade >= 3) {
        std::snprintf(text, sizeof(text), "%.0fk", std::pow(10.0, decade - 3));
    } else if (decade >= 0) {
        std::snprintf(text, sizeof(text), "%.0f", std::pow(10.0, decade));
    } else if (decade >= -6) {
        std::snprintf(text, sizeof(text), "%.*f", -decade, std::pow(10.0, decade));
    } else {
        std::snprintf(text, sizeof(text), "1e%d", decade);
    }
    return text;
}

} // namespace

std::string threeDigits(double value)
{
    if (!std::isfinite(value)) return value > 0.0 ? "inf" : "-inf";
    if (value == 0.0) return "0";
    const double magnitude = std::abs(value);
    const int exponent = static_cast<int>(std::floor(std::log10(magnitude)));
    char text[64];
    if (exponent >= 2) {
        const double unit = std::pow(10.0, exponent - 2);
        std::snprintf(text, sizeof(text), "%.0f", std::round(value / unit) * unit);
    } else {
        std::snprintf(text, sizeof(text), "%.*f", 2 - exponent, value);
    }
    return text;
}

void drawLegend(const ImVec2& imagePos, const ImVec2& imageSize, const ViewportSettingsState& state,
                const AntiAliasing::LuminanceSample& sample, bool hovered, LegendPanels& panels,
                const std::function<void()>& settings)
{
    const bool relative = state.luminanceScale == 1;
    const Scale scale = Scale::of(relative, state.luminanceMin, state.luminanceMax, state.luminanceEvMin, state.luminanceEvMax);
    const ImU32 main = IM_COL32(255, 255, 255, 255);
    const ImU32 dim = IM_COL32(199, 199, 199, 255);

    // Like the Texture Distortion / Density legend: in the image's lower left corner, as big as what is open; a child
    // window, so the viewport does not count as hovered over it.
    const float margin = 8.0f;
    ImGui::SetCursorScreenPos(imagePos);
    ImGui::SetNextWindowPos(ImVec2(imagePos.x + margin, imagePos.y + imageSize.y - margin), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(std::max(imageSize.x - 2.0f * margin, 64.0f), std::max(imageSize.y - 2.0f * margin, 64.0f)));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(20, 20, 20, 200));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
    const bool visible = ImGui::BeginChild("##LuminanceLegend", ImVec2(0.0f, 0.0f),
                                           ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize,
                                           ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    if (visible) {
        ImGui::TextUnformatted(relative ? "Luminance (EV from middle grey at the displayed exposure)"
                                        : (state.luminanceEmissiveOnly ? "Emitted luminance (cd/m\xC2\xB2)" : "Luminance (cd/m\xC2\xB2)"));

        // The bar: a black swatch (below the scale and zero), the scale, a magenta swatch (above it).
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
        const float swatch = 14.0f;
        const float gap = 4.0f;
        const float barWidth = 288.0f;
        const float barHeight = 14.0f;
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(swatch + gap + barWidth + gap + swatch, 2.0f + barHeight + 2.0f + lineHeight));
        const float top = origin.y + 2.0f;
        const float bottom = top + barHeight;
        const float barLeft = origin.x + swatch + gap;
        drawList->AddRectFilled(ImVec2(origin.x, top), ImVec2(origin.x + swatch, bottom), IM_COL32(0, 0, 0, 255));
        drawList->AddRect(ImVec2(origin.x, top), ImVec2(origin.x + swatch, bottom), IM_COL32(110, 110, 110, 255));
        const float aboveLeft = barLeft + barWidth + gap;
        drawList->AddRectFilled(ImVec2(aboveLeft, top), ImVec2(aboveLeft + swatch, bottom), IM_COL32(255, 0, 255, 255));
        const float span = scale.high - scale.low;
        const int segments = state.luminanceBands ? std::max(static_cast<int>(std::ceil(span - 1.0e-4f)), 1) : 96;
        for (int i = 0; i < segments; ++i) {
            float x0, x1, value;
            if (state.luminanceBands) {
                x0 = barLeft + barWidth * std::min(static_cast<float>(i) / span, 1.0f);
                x1 = barLeft + barWidth * std::min(static_cast<float>(i + 1) / span, 1.0f);
                value = scale.low + static_cast<float>(i) + 0.5f;
            } else {
                x0 = barLeft + barWidth * static_cast<float>(i) / static_cast<float>(segments);
                x1 = barLeft + barWidth * static_cast<float>(i + 1) / static_cast<float>(segments);
                value = scale.low + span * (static_cast<float>(i) + 0.5f) / static_cast<float>(segments);
            }
            drawList->AddRectFilled(ImVec2(x0, top), ImVec2(x1, bottom), toImGui(colour(scale, state.luminanceBands, std::min(value, scale.high))));
        }
        // Ticks: every decade (Absolute) or stop (Relative; every second one past 13 ticks), labelled under the bar.
        const int first = static_cast<int>(std::ceil(scale.low - 1.0e-4f));
        const int last = static_cast<int>(std::floor(scale.high + 1.0e-4f));
        const int step = (last - first) > 12 ? 2 : 1;
        // The scale's top label (with the unit) is always drawn; a tick label that would overlap it, or the one before,
        // is left out.
        const auto tickX = [&](int tick) { return barLeft + barWidth * (static_cast<float>(tick) - scale.low) / span; };
        const auto tickLabel = [&](int tick) {
            return relative ? (tick > 0 ? "+" + std::to_string(tick) : std::to_string(tick)) : decadeLabel(tick);
        };
        const std::string topLabel = tickLabel(last) + (relative ? " EV" : " cd/m\xC2\xB2");
        const ImVec2 topSize = ImGui::CalcTextSize(topLabel.c_str());
        const float topLeft = std::min(tickX(last) - ImGui::CalcTextSize(tickLabel(last).c_str()).x * 0.5f,
                                       aboveLeft + swatch - topSize.x);
        float lastLabelRight = -1.0e9f;
        for (int tick = first; tick <= last; tick += step) {
            const float x = tickX(tick);
            drawList->AddLine(ImVec2(x, bottom), ImVec2(x, bottom + 3.0f), main);
            if (tick == last) continue;
            const std::string label = tickLabel(tick);
            const ImVec2 size = ImGui::CalcTextSize(label.c_str());
            const float left = x - size.x * 0.5f;
            if (left < lastLabelRight + 3.0f || left + size.x + 3.0f > topLeft) continue;
            drawList->AddText(ImVec2(left, bottom + 3.0f), dim, label.c_str());
            lastLabelRight = left + size.x;
        }
        if ((last - first) % step != 0) {
            drawList->AddLine(ImVec2(tickX(last), bottom), ImVec2(tickX(last), bottom + 3.0f), main);
        }
        drawList->AddText(ImVec2(topLeft, bottom + 3.0f), dim, topLabel.c_str());

        // The hovered pixel (the readback is one frame or more old).
        if (hovered && sample.valid) {
            std::string text = "Cursor: " + threeDigits(sample.cd) + " cd/m\xC2\xB2";
            if (relative) {
                char ev[32];
                if (std::isfinite(sample.ev)) std::snprintf(ev, sizeof(ev), ", %+.2f EV", sample.ev);
                else std::snprintf(ev, sizeof(ev), ", no light");
                text += ev;
            }
            ImGui::TextUnformatted(text.c_str());
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, dim);
            ImGui::TextUnformatted("Cursor: hover the image to read a pixel");
            ImGui::PopStyleColor();
        }

        ImGui::SetNextItemOpen(panels.settings, ImGuiCond_Always);
        panels.settings = ImGui::TreeNodeEx("Settings###LuminanceSettings", ImGuiTreeNodeFlags_NoTreePushOnOpen);
        UiProbe::item("luminance_legend.settings", 0);
        if (panels.settings && settings) {
            settings();
        }
    }
    ImGui::EndChild();
}

} // namespace ux3d::imgui::luminance
