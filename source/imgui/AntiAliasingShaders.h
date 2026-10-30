// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Private to AntiAliasing*.cpp: the full-screen quad, the GLSL sources of the post-processing passes (FXAA, TAA,
// tone mapping, bloom down / upsample, A6.7 lens flare, exposure meter) and their compile / link helpers. Each
// including file gets its own internal copy.

#pragma once

#include "imgui/AntiAliasing.h"
#include "imgui/NightSky.h"
#include "imgui/Profiling.h"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <sstream>
#include <vector>

namespace ux3d::imgui {

struct QuadVertex {
    float x, y;
    float u, v;
};

static const QuadVertex s_quadVertices[6] = {
    // Triangle 1
    { -1.0f, -1.0f, 0.0f, 0.0f },
    {  1.0f, -1.0f, 1.0f, 0.0f },
    {  1.0f,  1.0f, 1.0f, 1.0f },

    // Triangle 2
    { -1.0f, -1.0f, 0.0f, 0.0f },
    {  1.0f,  1.0f, 1.0f, 1.0f },
    { -1.0f,  1.0f, 0.0f, 1.0f }
};

static const char* s_quadVS = R"(#version 410 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 v_TexCoord;

void main() {
    v_TexCoord = aTexCoord;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

// FXAA 3.11 Quality Fragment Shader
static const char* s_fxaaFS = R"(#version 410 core
in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_Texture;
uniform vec2 u_TexelSize;

#define FXAA_REDUCE_MIN   (1.0 / 128.0)
#define FXAA_REDUCE_MUL   (1.0 / 8.0)
#define FXAA_SPAN_MAX     8.0

void main() {
    vec3 rgbNW = texture(u_Texture, v_TexCoord + vec2(-1.0, -1.0) * u_TexelSize).rgb;
    vec3 rgbNE = texture(u_Texture, v_TexCoord + vec2( 1.0, -1.0) * u_TexelSize).rgb;
    vec3 rgbSW = texture(u_Texture, v_TexCoord + vec2(-1.0,  1.0) * u_TexelSize).rgb;
    vec3 rgbSE = texture(u_Texture, v_TexCoord + vec2( 1.0,  1.0) * u_TexelSize).rgb;
    vec3 rgbM  = texture(u_Texture, v_TexCoord).rgb;

    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

    dir = min(vec2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
          max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
          dir * rcpDirMin)) * u_TexelSize;

    vec3 rgbA = 0.5 * (
        texture(u_Texture, v_TexCoord + dir * (1.0 / 3.0 - 0.5)).rgb +
        texture(u_Texture, v_TexCoord + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(u_Texture, v_TexCoord + dir * -0.5).rgb +
        texture(u_Texture, v_TexCoord + dir * 0.5).rgb);

    float lumaB = dot(rgbB, luma);
    if ((lumaB < lumaMin) || (lumaB > lumaMax)) {
        FragColor = vec4(rgbA, 1.0);
    } else {
        FragColor = vec4(rgbB, 1.0);
    }
}
)";

// TAA Fragment Shader with reversible Karis luminance weighting and 3x3 neighborhood clamp in HDR
static const char* s_taaFS = R"(#version 410 core
in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_CurrentTexture;
uniform sampler2D u_HistoryTexture;
uniform vec2 u_TexelSize;
uniform float u_BlendFactor;
uniform int u_RawMode;
uniform float u_HistoryScale; // current / history input scale (pre-exposure change); 1 otherwise

float hdrLuma(vec3 c) {
    return max(0.0, dot(c, vec3(0.2126, 0.7152, 0.0722)));
}

vec3 karisWeight(vec3 c) {
    return c / (1.0 + hdrLuma(c));
}

vec3 karisUnweight(vec3 c) {
    return c / max(1.0 - hdrLuma(c), 1e-5);
}

void main() {
    vec4 current4 = texture(u_CurrentTexture, v_TexCoord);
    if (u_BlendFactor >= 0.9999) {
        FragColor = current4;
        return;
    }

    vec3 current = max(vec3(0.0), current4.rgb);
    vec3 currentW = (u_RawMode != 0) ? current : karisWeight(current);

    // 3x3 color neighborhood bounding box in Karis-weighted HDR space
    vec3 minColor = currentW;
    vec3 maxColor = currentW;
    float minAlpha = current4.a;
    float maxAlpha = current4.a;

    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec4 neighbor4 = texture(u_CurrentTexture, v_TexCoord + vec2(float(x), float(y)) * u_TexelSize);
            vec3 neighbor = max(vec3(0.0), neighbor4.rgb);
            vec3 neighborW = (u_RawMode != 0) ? neighbor : karisWeight(neighbor);
            minColor = min(minColor, neighborW);
            maxColor = max(maxColor, neighborW);
            minAlpha = min(minAlpha, neighbor4.a);
            maxAlpha = max(maxAlpha, neighbor4.a);
        }
    }

    vec4 history4 = texture(u_HistoryTexture, v_TexCoord);
    vec3 history = max(vec3(0.0), history4.rgb) * u_HistoryScale;
    vec3 historyW = (u_RawMode != 0) ? history : karisWeight(history);

    // Clamp history into the current neighborhood bounding box
    historyW = clamp(historyW, minColor, maxColor);
    float historyA = clamp(history4.a, minAlpha, maxAlpha);

    // Exponential moving average blend in Karis-weighted space, then invert
    vec3 blendedW = mix(historyW, currentW, u_BlendFactor);
    vec3 result = (u_RawMode != 0) ? blendedW : karisUnweight(blendedW);
    float resultA = mix(historyA, current4.a, u_BlendFactor);
    FragColor = vec4(result, resultA);
}
)";

// Post-processing Tone Mapping Fragment Shader (matches resources/shaders/tonemapping.glsl)
static const char* s_tonemapFS = R"(#version 410 core
in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_HdrTexture;
uniform sampler2D u_BloomTexture;
uniform float u_Exposure;
uniform int u_ToneMapMode;
uniform int u_RawMode;
uniform int u_HasClearColor;
uniform vec3 u_ClearColor;
uniform int u_BloomEnabled;
uniform float u_BloomIntensity;
// A6.6 night vision (NightSky.h): x the largest scotopic weight (0 = off: the image is unchanged), y the factor from the
// input values to cd/m^2 (12,800 / pre-exposure).
uniform vec2 u_NightShift;

const float GAMMA = 2.2;
const float INV_GAMMA = 1.0 / GAMMA;

const mat3 ACESInputMat = mat3(
    0.59719, 0.07600, 0.02840,
    0.35458, 0.90834, 0.13383,
    0.04823, 0.01566, 0.83777
);

const mat3 ACESOutputMat = mat3(
    1.60475, -0.10208, -0.00327,
    -0.53108,  1.10813, -0.07276,
    -0.07367, -0.00605,  1.07602
);

vec3 linearTosRGB(vec3 color) {
    return pow(max(color, vec3(0.0)), vec3(INV_GAMMA));
}

vec3 RRTAndODTFit(vec3 v) {
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

vec3 toneMapACES_Hill(vec3 color) {
    color = ACESInputMat * color;
    color = RRTAndODTFit(color);
    color = ACESOutputMat * color;
    return clamp(color, 0.0, 1.0);
}

vec3 toneMapACES_Narkowicz(vec3 color) {
    const float A = 2.51;
    const float B = 0.03;
    const float C = 2.43;
    const float D = 0.59;
    const float E = 0.14;
    return clamp((color * (A * color + B)) / (color * (C * color + D) + E), 0.0, 1.0);
}

vec3 toneMap_KhronosPbrNeutral(vec3 color) {
    const float startCompression = 0.8 - 0.04;
    const float desaturation = 0.15;

    float x = min(color.r, min(color.g, color.b));
    float offset = x < 0.08 ? x - 6.25 * x * x : 0.04;
    color -= offset;

    float peak = max(color.r, max(color.g, color.b));
    if (peak < startCompression) return color;

    const float d = 1.0 - startCompression;
    float newPeak = 1.0 - d * d / (peak + d - startCompression);
    color *= newPeak / peak;

    float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
    return mix(color, newPeak * vec3(1.0, 1.0, 1.0), g);
}

// A6.6: rod (scotopic) vision. Per pixel, below the photopic range (3 cd/m^2 down to 0.01 cd/m^2, log scale) the colour
// moves towards its luminance with a blue tint (the Purkinje shift: Jensen et al. 2000, "A Physically-Based Night Sky
// Model"); bright pixels (emissive screens, the moon) keep their colour. It is the "white balance" of a moonlit scene:
// the moonlight itself is not tinted.
vec3 nightVision(vec3 color) {
    const vec3 kScotopicTint = vec3(0.85, 1.0, 1.42); // luminance 1
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float cd = max(luminance * u_NightShift.y, 1.0e-12);
    float weight = clamp((log(3.0) - log(cd)) / (log(3.0) - log(0.01)), 0.0, 1.0);
    weight = weight * weight * (3.0 - 2.0 * weight) * u_NightShift.x;
    return mix(color, luminance * kScotopicTint, weight);
}

vec3 applyToneMap(vec3 color) {
    if (u_NightShift.x > 0.0) color = nightVision(color);
    color *= u_Exposure;
    if (u_ToneMapMode == 0) {
        return linearTosRGB(clamp(toneMap_KhronosPbrNeutral(color), 0.0, 1.0));
    } else if (u_ToneMapMode == 2) {
        return linearTosRGB(toneMapACES_Narkowicz(color));
    } else {
        color = toneMapACES_Hill(color);
        return linearTosRGB(color);
    }
}

void main() {
    vec4 hdr = texture(u_HdrTexture, v_TexCoord);
    if (u_RawMode == 1 || (u_RawMode == 2 && hdr.a > 1.5)) {
        FragColor = vec4(clamp(hdr.rgb, 0.0, 1.0), 1.0);
        return;
    }

    vec3 bloom = vec3(0.0);
    float bFactor = 0.0;
    if (u_BloomEnabled != 0) {
        // The tent upsample adds every pyramid level on top of the next (GL_ONE, GL_ONE), so
        // the chain holds the sum of kBloomMips (5) blurred copies. Average them, or the
        // intensity mix brightens and veils the whole frame instead of conserving energy.
        bloom = max(vec3(0.0), texture(u_BloomTexture, v_TexCoord).rgb) / 5.0;
        bFactor = clamp(u_BloomIntensity, 0.0, 1.0);
    }

    vec3 ldr;
    if (u_HasClearColor != 0) {
        float cov = clamp(hdr.a, 0.0, 1.0);
        vec3 unlitBg = u_ClearColor * (1.0 - cov);
        vec3 litHdr = max(vec3(0.0), hdr.rgb - unlitBg);
        vec3 bloomedLit = mix(litHdr, bloom, bFactor);
        ldr = unlitBg + ((cov > 0.0 || dot(bloomedLit, vec3(1.0)) > 1e-5) ? applyToneMap(bloomedLit) : vec3(0.0));
    } else {
        vec3 bloomedHdr = mix(max(vec3(0.0), hdr.rgb), bloom, bFactor);
        ldr = applyToneMap(bloomedHdr);
    }
    FragColor = vec4(clamp(ldr, 0.0, 1.0), 1.0);
}
)";

// Jimenez 2014 13-tap bilinear downsample (with Karis firefly suppression on first pass)
static const char* s_bloomDownsampleFS = R"(
#version 410 core
in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_SrcTexture;
uniform vec2 u_SrcTexelSize;
uniform int u_FirstPass;
uniform int u_HasClearColor;
uniform vec3 u_ClearColor;

float calcLuma(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

vec3 sampleSrc(vec2 uv) {
    vec4 s = texture(u_SrcTexture, uv);
    vec3 c = max(vec3(0.0), s.rgb);
    if (u_FirstPass != 0 && u_HasClearColor != 0) {
        float cov = clamp(s.a, 0.0, 1.0);
        c = max(vec3(0.0), c - u_ClearColor * (1.0 - cov));
    }
    return c;
}

void main() {
    vec2 d = u_SrcTexelSize;
    vec3 a  = sampleSrc(v_TexCoord + vec2(-2.0,  2.0) * d);
    vec3 b  = sampleSrc(v_TexCoord + vec2( 0.0,  2.0) * d);
    vec3 c  = sampleSrc(v_TexCoord + vec2( 2.0,  2.0) * d);
    vec3 d0 = sampleSrc(v_TexCoord + vec2(-2.0,  0.0) * d);
    vec3 e  = sampleSrc(v_TexCoord);
    vec3 f  = sampleSrc(v_TexCoord + vec2( 2.0,  0.0) * d);
    vec3 g  = sampleSrc(v_TexCoord + vec2(-2.0, -2.0) * d);
    vec3 h  = sampleSrc(v_TexCoord + vec2( 0.0, -2.0) * d);
    vec3 i  = sampleSrc(v_TexCoord + vec2( 2.0, -2.0) * d);
    vec3 j  = sampleSrc(v_TexCoord + vec2(-1.0,  1.0) * d);
    vec3 k  = sampleSrc(v_TexCoord + vec2( 1.0,  1.0) * d);
    vec3 l  = sampleSrc(v_TexCoord + vec2(-1.0, -1.0) * d);
    vec3 m  = sampleSrc(v_TexCoord + vec2( 1.0, -1.0) * d);

    vec3 g0 = (j + k + l + m) * 0.25;
    vec3 g1 = (a + b + d0 + e) * 0.25;
    vec3 g2 = (b + c + e + f) * 0.25;
    vec3 g3 = (d0 + e + g + h) * 0.25;
    vec3 g4 = (e + f + h + i) * 0.25;

    if (u_FirstPass != 0) {
        float l1 = calcLuma(g1);
        float l2 = calcLuma(g2);
        float l3 = calcLuma(g3);
        float l4 = calcLuma(g4);
        float hi1 = max(l1, l2), lo1 = min(l1, l2);
        float hi2 = max(l3, l4), lo2 = min(l3, l4);
        float lSecond = max(min(hi1, hi2), max(lo1, lo2));

        float w0 = 0.500 / (1.0 + max(0.0, calcLuma(g0) - lSecond));
        float w1 = 0.125 / (1.0 + max(0.0, l1 - lSecond));
        float w2 = 0.125 / (1.0 + max(0.0, l2 - lSecond));
        float w3 = 0.125 / (1.0 + max(0.0, l3 - lSecond));
        float w4 = 0.125 / (1.0 + max(0.0, l4 - lSecond));
        float wSum = max(w0 + w1 + w2 + w3 + w4, 1e-6);
        FragColor = vec4((w0 * g0 + w1 * g1 + w2 * g2 + w3 * g3 + w4 * g4) / wSum, 1.0);
    } else {
        FragColor = vec4(0.5 * g0 + 0.125 * (g1 + g2 + g3 + g4), 1.0);
    }
}
)";

// Jimenez 2014 3x3 tent upsample filter
static const char* s_bloomUpsampleFS = R"(
#version 410 core
in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_SrcTexture;
uniform vec2 u_TexelSize;
uniform float u_FilterRadius;

void main() {
    vec2 d = u_TexelSize * u_FilterRadius;
    vec3 a  = texture(u_SrcTexture, v_TexCoord + vec2(-d.x,  d.y)).rgb;
    vec3 b  = texture(u_SrcTexture, v_TexCoord + vec2( 0.0,  d.y)).rgb;
    vec3 c  = texture(u_SrcTexture, v_TexCoord + vec2( d.x,  d.y)).rgb;
    vec3 d0 = texture(u_SrcTexture, v_TexCoord + vec2(-d.x,  0.0)).rgb;
    vec3 e  = texture(u_SrcTexture, v_TexCoord).rgb;
    vec3 f  = texture(u_SrcTexture, v_TexCoord + vec2( d.x,  0.0)).rgb;
    vec3 g  = texture(u_SrcTexture, v_TexCoord + vec2(-d.x, -d.y)).rgb;
    vec3 h  = texture(u_SrcTexture, v_TexCoord + vec2( 0.0, -d.y)).rgb;
    vec3 i  = texture(u_SrcTexture, v_TexCoord + vec2( d.x, -d.y)).rgb;

    vec3 tent = (a + c + g + i) + 2.0 * (b + d0 + f + h) + 4.0 * e;
    FragColor = vec4(tent * (1.0 / 16.0), 1.0);
}
)";

// A6.7 lens effect, screen space from the bloom pyramid (after the downsample chain, before the upsample): ghosts of
// the bright light mirrored through the image centre (with a slight chromatic split), a halo ring, and a horizontal
// anamorphic streak. Only light well above white (kLensThreshold in exposed units) takes part, so the sun disk and
// its mirror highlights flare and an ordinary sky does not. Written additively into bloom level 0 in bloom units
// (u_Scale converts exposed units back), so the composite's bloom mix carries it and bloom off removes it.
static const char* s_lensFlareFS = R"(
#version 410 core
in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_Source; // bloom level 1 (half of the canonical 450 rows)
uniform sampler2D u_Blur;   // bloom level 3
uniform int u_Mode;         // 1 ghosts + halo, 2 streak, 3 both
uniform float u_Scale;      // intensity x 5 / (bloom mix x exposure)
uniform float u_Exposure;
uniform float u_Aspect;     // width / height
uniform vec2 u_SourceTexel;

const float kLensThreshold = 4.0;

vec3 bright(vec3 c)
{
    vec3 e = max(c, vec3(0.0)) * u_Exposure;
    float l = dot(e, vec3(0.2126, 0.7152, 0.0722));
    return e * (max(l - kLensThreshold, 0.0) / max(l, 1.0e-6));
}

float window(vec2 uv)
{
    vec2 edge = smoothstep(vec2(0.0), vec2(0.08), uv) * smoothstep(vec2(0.0), vec2(0.08), vec2(1.0) - uv);
    return edge.x * edge.y;
}

vec3 ghosts(vec2 uv)
{
    // Ghost i of a light at L lies at c + (L - c) * k[i]: this pixel shows the light found at c + (uv - c) / k[i].
    const float k[5] = float[5](-0.32, -0.62, -1.15, 0.42, -2.2);
    const float w[5] = float[5](1.0, 0.7, 0.45, 0.35, 0.25);
    const vec3 tint[5] = vec3[5](vec3(1.0, 0.75, 0.35), vec3(0.35, 0.75, 1.0), vec3(0.7, 1.0, 0.5),
                                 vec3(1.0, 0.45, 0.75), vec3(0.55, 0.65, 1.0));
    vec2 d = uv - 0.5;
    vec3 sum = vec3(0.0);
    for (int i = 0; i < 5; ++i) {
        vec2 sr = 0.5 + d / (k[i] * 1.03);
        vec2 sg = 0.5 + d / k[i];
        vec2 sb = 0.5 + d / (k[i] * 0.97);
        vec3 c = vec3(bright(texture(u_Blur, sr).rgb).r, bright(texture(u_Blur, sg).rgb).g, bright(texture(u_Blur, sb).rgb).b);
        sum += c * tint[i] * (w[i] * window(sg));
    }
    // Halo: a ring of radius 0.45 (image heights) around a light near the centre.
    vec2 toCentre = (vec2(0.5) - uv) * vec2(u_Aspect, 1.0);
    vec2 haloOffset = (length(toCentre) > 1.0e-5 ? normalize(toCentre) : vec2(0.0, 1.0)) * 0.45 / vec2(u_Aspect, 1.0);
    vec2 hs = uv + haloOffset;
    float haloWeight = pow(max(1.0 - length((hs - 0.5) * vec2(u_Aspect, 1.0)) / 0.7, 0.0), 5.0);
    sum += bright(texture(u_Blur, hs).rgb) * vec3(0.9, 0.85, 1.0) * (1.5 * haloWeight * window(hs));
    return sum * 2.0e-3;
}

vec3 streak(vec2 uv)
{
    vec3 sum = vec3(0.0);
    float stepX = 2.0 * u_SourceTexel.x;
    for (int j = -24; j <= 24; ++j) {
        float w = exp(-abs(float(j)) / 7.0);
        sum += bright(texture(u_Source, uv + vec2(float(j) * stepX, 0.0)).rgb) * w;
    }
    return sum * vec3(0.5, 0.72, 1.0) * 1.6e-4;
}

void main()
{
    vec3 flare = vec3(0.0);
    if (u_Mode == 1 || u_Mode == 3) flare += ghosts(v_TexCoord);
    if (u_Mode == 2 || u_Mode == 3) flare += streak(v_TexCoord);
    FragColor = vec4(flare * u_Scale, 0.0);
}
)";

// 64x64 luminance metering downsample shader for Auto-Exposure
static const char* s_meterFS = R"(
#version 410 core
in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_HdrTexture;
uniform vec2 u_TexelSize;
uniform int u_HasClearColor;
uniform vec3 u_ClearColor;

void main() {
    vec3 sumRgb = vec3(0.0);
    float peak = 0.0;
    for (int y = -1; y <= 1; y += 2) {
        for (int x = -1; x <= 1; x += 2) {
            vec2 uv = clamp(v_TexCoord + vec2(float(x), float(y)) * 0.25 * u_TexelSize, vec2(0.001), vec2(0.999));
            vec4 s = texture(u_HdrTexture, uv);
            vec3 c = max(vec3(0.0), s.rgb);
            if (u_HasClearColor != 0) {
                float cov = clamp(s.a, 0.0, 1.0);
                c = max(vec3(0.0), c - u_ClearColor * (1.0 - cov));
            }
            sumRgb += c;
            peak = max(peak, max(c.r, max(c.g, c.b)));
        }
    }
    vec3 avgRgb = sumRgb * 0.25;
    float luma = dot(avgRgb, vec3(0.2126, 0.7152, 0.0722));
    // A6.4b: the brightest channel of the four taps for the highlight meter (a thin screen strip partly covering the
    // cell still reads its own value); the HDR's auto exposure reads only the red channel.
    FragColor = vec4(luma, peak, 0.0, 1.0);
}
)";

// C3: the asynchronous highlight meter reads the scene depth at the meter cells as a colour value (a depth readback
// would wait for the GPU). texelFetch of the D24S8 copy gives the same normalised depth as the synchronous float read.
static const char* s_meterDepthFS = R"(
#version 410 core
out vec4 FragColor;

uniform sampler2D u_Depth;

void main() {
    FragColor = vec4(texelFetch(u_Depth, ivec2(gl_FragCoord.xy), 0).r, 0.0, 0.0, 1.0);
}
)";

static GLuint compileShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        std::fprintf(stderr, "AntiAliasing shader compile error: %s\n", infoLog);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint linkProgram(const char* vsSrc, const char* fsSrc)
{
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
        std::fprintf(stderr, "AntiAliasing program link error: %s\n", infoLog);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

} // namespace ux3d::imgui
