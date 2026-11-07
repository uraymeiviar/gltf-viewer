// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// The Luminance debug view (RenderMode::Luminance): a false-colour heatmap of the scene's luminance in cd/m^2, like
// MSFS's luminance debug view, so an artist can read the absolute brightness of every surface and screen. The scene is
// shaded exactly like Shaded + post effects (lighting, IBL, sun, emissive, sky, SSR, SSAO); the HDR post pass then maps
// Y = 0.2126 R + 0.7152 G + 0.0722 B of each (resolved) pixel, before exposure, bloom and lens effects, to the Turbo
// colormap instead of tone mapping it (AntiAliasingLuminance.cpp). Units: 12,800 cd/m^2 per environment unit
// (docs/ASOBO_EXTENSIONS_PHASE_1.md A1 emissive units), the scene's pre-exposure divided out. "Emissive only" draws the
// materials' emission alone (pbr.frag DEBUG_LUMINANCE) over a black background. Scales: Absolute, log10 cd/m^2 from
// luminance_min to luminance_max; Exposure-relative, EV around middle grey (0.18) at the displayed exposure. Below the
// scale (and zero) is black, above it magenta; bands (default) quantise to a decade or a stop. All settings are
// session-only viewport view settings (ViewportSettingsState::luminance*).

#pragma once

#include "imgui/AntiAliasing.h"
#include <imgui.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <limits>
#include <string>

namespace ux3d::imgui {

struct ViewportSettingsState;

namespace luminance {

constexpr float kCdPerEnvironmentUnit = 12800.0f; // A1 emissive units
constexpr float kMiddleGrey = 0.18f;

// The scale's ends in its own unit: log10 cd/m^2 (Absolute) or EV (Relative). Invalid ends fall back to the defaults.
struct Scale {
    float low = -2.0f;
    float high = 5.0f;
    static Scale of(bool relative, float minCd, float maxCd, float evMin, float evMax)
    {
        Scale s;
        if (relative) {
            s.low = evMin < evMax ? evMin : -6.0f;
            s.high = evMin < evMax ? evMax : 6.0f;
        } else {
            const bool valid = minCd > 0.0f && maxCd > minCd && std::isfinite(minCd) && std::isfinite(maxCd);
            s.low = valid ? std::log10(minCd) : -2.0f;
            s.high = valid ? std::log10(maxCd) : 5.0f;
        }
        return s;
    }
};

// The Turbo colormap (Anton Mikhailov, Google 2019, Apache-2.0), the published polynomial approximation (Ruofei Du,
// Google, Apache-2.0): the same polynomial as the GPU pass. Display (sRGB-encoded) values.
inline glm::vec3 turbo(float x)
{
    x = std::clamp(x, 0.0f, 1.0f);
    const float x2 = x * x, x3 = x2 * x, x4 = x2 * x2, x5 = x4 * x;
    const glm::vec3 c(0.13572138f + 4.61539260f * x - 42.66032258f * x2 + 132.13108234f * x3 - 152.94239396f * x4 + 59.28637943f * x5,
                      0.09140261f + 2.19418839f * x + 4.84296658f * x2 - 14.18503333f * x3 + 4.27729857f * x4 + 2.82956604f * x5,
                      0.10667330f + 12.64194608f * x - 60.58204836f * x2 + 110.36276771f * x3 - 89.90310912f * x4 + 27.34824973f * x5);
    return glm::clamp(c, glm::vec3(0.0f), glm::vec3(1.0f));
}

// The colour of a value in the scale's unit (log10 cd/m^2 or EV), as the GPU pass draws it.
inline glm::vec3 colour(const Scale& scale, bool bands, float value)
{
    const float span = scale.high - scale.low;
    float t = (value - scale.low) / span;
    if (!(t >= 0.0f)) return glm::vec3(0.0f);
    if (t > 1.0f) return glm::vec3(1.0f, 0.0f, 1.0f);
    if (bands) {
        const float count = std::max(std::ceil(span - 1.0e-4f), 1.0f);
        const float k = std::min(std::floor(value - scale.low), count - 1.0f);
        t = std::min((k + 0.5f) / span, 1.0f);
    }
    return turbo(t);
}

// A value with three significant digits ("0.0123", "1.23", "123", "123000").
std::string threeDigits(double value);

// Which sections of a viewport's legend are open (session only, per viewport).
struct LegendPanels {
    bool settings = false;
};

// The legend in the viewport's lower left corner: the colour bar with its ticks, the below / above swatches, the
// hovered value (sample valid and hovered) and a collapsible Settings section (settings() draws its widgets).
void drawLegend(const ImVec2& imagePos, const ImVec2& imageSize, const ViewportSettingsState& state,
                const AntiAliasing::LuminanceSample& sample, bool hovered, LegendPanels& panels,
                const std::function<void()>& settings);

} // namespace luminance
} // namespace ux3d::imgui
