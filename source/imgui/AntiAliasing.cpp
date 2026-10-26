// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/AntiAliasing.h"
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
    vec3 history = max(vec3(0.0), history4.rgb);
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

vec3 applyToneMap(vec3 color) {
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
        }
    }
    vec3 avgRgb = sumRgb * 0.25;
    float luma = dot(avgRgb, vec3(0.2126, 0.7152, 0.0722));
    FragColor = vec4(luma, 0.0, 0.0, 1.0);
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

AntiAliasing::AntiAliasing() = default;

AntiAliasing::~AntiAliasing()
{
    cleanup();
}

void AntiAliasing::init()
{
    cleanup();

    // Create Quad VAO and VBO
    glGenVertexArrays(1, &m_quadVao);
    glGenBuffers(1, &m_quadVbo);
    ux3d::imgui::profiling::countVaoCreation();
    ux3d::imgui::profiling::countBufferCreation();
    ux3d::imgui::profiling::countBufferAllocation(sizeof(s_quadVertices));
    ux3d::imgui::profiling::countBufferUpload(sizeof(s_quadVertices));
    ux3d::imgui::profiling::countShaderCompile();
    ux3d::imgui::profiling::countShaderCompile();
    ux3d::imgui::profiling::countProgramLink();
    ux3d::imgui::profiling::countShaderCompile();
    ux3d::imgui::profiling::countShaderCompile();
    ux3d::imgui::profiling::countProgramLink();

    glBindVertexArray(m_quadVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_quadVertices), s_quadVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(QuadVertex), reinterpret_cast<void*>(offsetof(QuadVertex, x)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(QuadVertex), reinterpret_cast<void*>(offsetof(QuadVertex, u)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Compile FXAA program
    m_fxaaProgram = linkProgram(s_quadVS, s_fxaaFS);
    if (m_fxaaProgram) {
        m_fxaaTexLoc = glGetUniformLocation(m_fxaaProgram, "u_Texture");
        m_fxaaTexelSizeLoc = glGetUniformLocation(m_fxaaProgram, "u_TexelSize");
    }

    // Compile TAA program
    m_taaProgram = linkProgram(s_quadVS, s_taaFS);
    if (m_taaProgram) {
        m_taaCurrTexLoc = glGetUniformLocation(m_taaProgram, "u_CurrentTexture");
        m_taaHistTexLoc = glGetUniformLocation(m_taaProgram, "u_HistoryTexture");
        m_taaTexelSizeLoc = glGetUniformLocation(m_taaProgram, "u_TexelSize");
        m_taaBlendFactorLoc = glGetUniformLocation(m_taaProgram, "u_BlendFactor");
        m_taaRawModeLoc = glGetUniformLocation(m_taaProgram, "u_RawMode");
    }

    // Compile Tone Mapping program
    m_tonemapProgram = linkProgram(s_quadVS, s_tonemapFS);
    if (m_tonemapProgram) {
        m_tmHdrTexLoc = glGetUniformLocation(m_tonemapProgram, "u_HdrTexture");
        m_tmBloomTexLoc = glGetUniformLocation(m_tonemapProgram, "u_BloomTexture");
        m_tmExposureLoc = glGetUniformLocation(m_tonemapProgram, "u_Exposure");
        m_tmToneMapModeLoc = glGetUniformLocation(m_tonemapProgram, "u_ToneMapMode");
        m_tmRawModeLoc = glGetUniformLocation(m_tonemapProgram, "u_RawMode");
        m_tmHasClearColorLoc = glGetUniformLocation(m_tonemapProgram, "u_HasClearColor");
        m_tmClearColorLoc = glGetUniformLocation(m_tonemapProgram, "u_ClearColor");
        m_tmBloomEnabledLoc = glGetUniformLocation(m_tonemapProgram, "u_BloomEnabled");
        m_tmBloomIntensityLoc = glGetUniformLocation(m_tonemapProgram, "u_BloomIntensity");
    }

    // Compile Bloom Downsample & Upsample programs
    m_bloomDownsampleProgram = linkProgram(s_quadVS, s_bloomDownsampleFS);
    if (m_bloomDownsampleProgram) {
        m_bdSrcTexLoc = glGetUniformLocation(m_bloomDownsampleProgram, "u_SrcTexture");
        m_bdSrcTexelSizeLoc = glGetUniformLocation(m_bloomDownsampleProgram, "u_SrcTexelSize");
        m_bdFirstPassLoc = glGetUniformLocation(m_bloomDownsampleProgram, "u_FirstPass");
        m_bdHasClearColorLoc = glGetUniformLocation(m_bloomDownsampleProgram, "u_HasClearColor");
        m_bdClearColorLoc = glGetUniformLocation(m_bloomDownsampleProgram, "u_ClearColor");
    }

    m_bloomUpsampleProgram = linkProgram(s_quadVS, s_bloomUpsampleFS);
    if (m_bloomUpsampleProgram) {
        m_buSrcTexLoc = glGetUniformLocation(m_bloomUpsampleProgram, "u_SrcTexture");
        m_buTexelSizeLoc = glGetUniformLocation(m_bloomUpsampleProgram, "u_TexelSize");
        m_buFilterRadiusLoc = glGetUniformLocation(m_bloomUpsampleProgram, "u_FilterRadius");
    }

    // Compile Auto-Exposure metering program
    m_meterProgram = linkProgram(s_quadVS, s_meterFS);
    if (m_meterProgram) {
        m_meterHdrTexLoc = glGetUniformLocation(m_meterProgram, "u_HdrTexture");
        m_meterTexelSizeLoc = glGetUniformLocation(m_meterProgram, "u_TexelSize");
        m_meterHasClearColorLoc = glGetUniformLocation(m_meterProgram, "u_HasClearColor");
        m_meterClearColorLoc = glGetUniformLocation(m_meterProgram, "u_ClearColor");
    }
}

void AntiAliasing::cleanup()
{
    releaseTargets();
    releaseBloomTargets();
    releaseMeterTargets();

    if (m_quadVbo) {
        glDeleteBuffers(1, &m_quadVbo);
        m_quadVbo = 0;
    }
    if (m_quadVao) {
        glDeleteVertexArrays(1, &m_quadVao);
        m_quadVao = 0;
    }
    if (m_fxaaProgram) {
        glDeleteProgram(m_fxaaProgram);
        m_fxaaProgram = 0;
    }
    if (m_taaProgram) {
        glDeleteProgram(m_taaProgram);
        m_taaProgram = 0;
    }
    if (m_tonemapProgram) {
        glDeleteProgram(m_tonemapProgram);
        m_tonemapProgram = 0;
    }
    if (m_bloomDownsampleProgram) {
        glDeleteProgram(m_bloomDownsampleProgram);
        m_bloomDownsampleProgram = 0;
    }
    if (m_bloomUpsampleProgram) {
        glDeleteProgram(m_bloomUpsampleProgram);
        m_bloomUpsampleProgram = 0;
    }
    if (m_meterProgram) {
        glDeleteProgram(m_meterProgram);
        m_meterProgram = 0;
    }
}

void AntiAliasing::releaseFxaaTargets()
{
    if (m_fxaaTexture != 0) {
        glDeleteTextures(1, &m_fxaaTexture);
        m_fxaaTexture = 0;
        ++m_targetDeletionCount;
    }
    if (m_fxaaFbo != 0) {
        glDeleteFramebuffers(1, &m_fxaaFbo);
        m_fxaaFbo = 0;
    }
}

void AntiAliasing::releaseTaaTargets()
{
    for (int i = 0; i < 2; ++i) {
        if (m_taaTexture[i] != 0) {
            glDeleteTextures(1, &m_taaTexture[i]);
            m_taaTexture[i] = 0;
            ++m_targetDeletionCount;
        }
        if (m_taaFbo[i] != 0) {
            glDeleteFramebuffers(1, &m_taaFbo[i]);
            m_taaFbo[i] = 0;
        }
    }
    m_historyValid = false;
    m_frameIndex = 0;
    m_taaCurrentIndex = 0;
    m_lastTaaHistoryValid = false;
    m_lastTaaBlendFactor = 0.0f;
    m_lastTaaTargetGeneration = 0;
}

void AntiAliasing::releaseTargets()
{
    releaseFxaaTargets();
    releaseTaaTargets();
    m_width = 0;
    m_height = 0;
}

size_t AntiAliasing::getOwnedTargetCount() const
{
    size_t count = 0;
    if (m_fxaaTexture != 0) ++count;
    if (m_taaTexture[0] != 0) ++count;
    if (m_taaTexture[1] != 0) ++count;
    return count;
}

size_t AntiAliasing::getOwnedFramebufferCount() const
{
    size_t count = 0;
    if (m_fxaaFbo != 0) ++count;
    if (m_taaFbo[0] != 0) ++count;
    if (m_taaFbo[1] != 0) ++count;
    return count;
}

uint64_t AntiAliasing::getEstimatedLiveBytes() const
{
    uint64_t bytes = 0;
    const uint64_t pixels = static_cast<uint64_t>(m_width) * static_cast<uint64_t>(m_height);
    if (m_fxaaTexture != 0) bytes += pixels * 4ull;
    if (m_taaTexture[0] != 0) bytes += pixels * 8ull;
    if (m_taaTexture[1] != 0) bytes += pixels * 8ull;
    return bytes;
}

void AntiAliasing::resetTargetCounters()
{
    m_targetAllocationCount = 0;
    m_targetDeletionCount = 0;
}

int AntiAliasing::getSampleCount(AntiAliasingMode mode)
{
    switch (mode) {
        case AntiAliasingMode::MSAA_2x: return 2;
        case AntiAliasingMode::MSAA_4x: return 4;
        case AntiAliasingMode::MSAA_8x: return 8;
        default: return 1;
    }
}

static float halton(int index, int base)
{
    float result = 0.0f;
    float f = 1.0f / static_cast<float>(base);
    int i = index;
    while (i > 0) {
        result += f * static_cast<float>(i % base);
        i /= base;
        f /= static_cast<float>(base);
    }
    return result;
}

glm::vec2 AntiAliasing::getJitter(AntiAliasingMode mode, int width, int height)
{
    if (mode != AntiAliasingMode::TAA || width <= 0 || height <= 0) {
        return glm::vec2(0.0f);
    }

    // 8-phase Halton(2, 3) subpixel pattern
    int sampleIdx = static_cast<int>((m_frameIndex % 8) + 1);
    float jx = (halton(sampleIdx, 2) - 0.5f) / static_cast<float>(width);
    float jy = (halton(sampleIdx, 3) - 0.5f) / static_cast<float>(height);
    return glm::vec2(jx, jy);
}

void AntiAliasing::resetHistory()
{
    // TAA history only. Animation frames, timeline scrubs and parameter edits reset it,
    // and restarting the exposure there re-adapted the whole image on every small change.
    m_historyValid = false;
    m_frameIndex = 0;
}

void AntiAliasing::resetExposure()
{
    // The next metered frame sets the exposure directly. "Start from manual EV" only
    // smooths the switch from Manual to Auto, never a load or a camera jump.
    m_exposureValid = false;
    m_exposureAdapting = false;
    m_snapExposureNext = true;
}

void AntiAliasing::ensureFxaaBuffers(int width, int height)
{
    if (width < 1 || height < 1) return;

    if (!m_quadVao || !m_fxaaProgram) {
        init();
    }

    if (m_fxaaTexture != 0 && m_fxaaFbo != 0 && m_width == width && m_height == height) {
        return;
    }

    releaseFxaaTargets();

    m_width = width;
    m_height = height;
    ++m_targetGeneration;

    glGenTextures(1, &m_fxaaTexture);
    glBindTexture(GL_TEXTURE_2D, m_fxaaTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &m_fxaaFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fxaaFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fxaaTexture, 0);

    GLenum fxaaStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (fxaaStatus != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "FXAA Framebuffer incomplete! Status: 0x%X\n", fxaaStatus);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    ++m_targetAllocationCount;
    ux3d::imgui::profiling::countTextureCreation(1);
    ux3d::imgui::profiling::countFramebufferCreation(1);
    ux3d::imgui::profiling::countTextureAllocation(static_cast<uint64_t>(m_width) * static_cast<uint64_t>(m_height) * 4ull);
}

void AntiAliasing::ensureTaaBuffers(int width, int height)
{
    if (width < 1 || height < 1) return;

    if (!m_quadVao || !m_taaProgram) {
        init();
    }

    if (m_taaTexture[0] != 0 && m_taaTexture[1] != 0 &&
        m_taaFbo[0] != 0 && m_taaFbo[1] != 0 &&
        m_width == width && m_height == height) {
        return;
    }

    releaseTaaTargets();

    m_width = width;
    m_height = height;
    ++m_targetGeneration;

    for (int i = 0; i < 2; ++i) {
        glGenTextures(1, &m_taaTexture[i]);
        glBindTexture(GL_TEXTURE_2D, m_taaTexture[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenFramebuffers(1, &m_taaFbo[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, m_taaFbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_taaTexture[i], 0);

        GLenum taaStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (taaStatus != GL_FRAMEBUFFER_COMPLETE) {
            std::fprintf(stderr, "TAA Framebuffer [%d] incomplete! Status: 0x%X\n", i, taaStatus);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_targetAllocationCount += 2;
    ux3d::imgui::profiling::countTextureCreation(2);
    ux3d::imgui::profiling::countFramebufferCreation(2);
    ux3d::imgui::profiling::countTextureAllocation(static_cast<uint64_t>(m_width) * static_cast<uint64_t>(m_height) * 8ull * 2ull);
}

GLuint AntiAliasing::render(AntiAliasingMode mode,
                            GLuint inputColorTexture,
                            int width, int height,
                            const glm::mat4& currentViewProj,
                            bool sceneDirty)
{
    if (mode == AntiAliasingMode::None ||
        mode == AntiAliasingMode::MSAA_2x ||
        mode == AntiAliasingMode::MSAA_4x ||
        mode == AntiAliasingMode::MSAA_8x ||
        inputColorTexture == 0) {
        releaseTargets();
        return inputColorTexture;
    }

    if (width <= 0 || height <= 0) {
        return inputColorTexture;
    }

    if (mode == AntiAliasingMode::FXAA) {
        releaseTaaTargets();
        ensureFxaaBuffers(width, height);

        if (!m_fxaaProgram || !m_fxaaFbo || !m_quadVao || !m_fxaaTexture) {
            return inputColorTexture;
        }

        ux3d::imgui::profiling::countAntiAliasingPass();

        // Explicitly configure all render state for 2D post-processing pass
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_STENCIL_TEST);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glViewport(0, 0, width, height);

        glBindFramebuffer(GL_FRAMEBUFFER, m_fxaaFbo);
        glUseProgram(m_fxaaProgram);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, inputColorTexture);
        glUniform1i(m_fxaaTexLoc, 0);
        glUniform2f(m_fxaaTexelSizeLoc, 1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height));

        glBindVertexArray(m_quadVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        ux3d::imgui::profiling::countAppDrawCall();

        // Always restore texture unit 0 before returning to ImGui
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);

        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDepthMask(GL_TRUE);

        return m_fxaaTexture;
    }

    if (mode == AntiAliasingMode::TAA) {
        releaseFxaaTargets();
        ensureTaaBuffers(width, height);

        if (!m_taaProgram || !m_taaFbo[0] || !m_taaFbo[1] || !m_quadVao || !m_taaTexture[0] || !m_taaTexture[1]) {
            return inputColorTexture;
        }

        ux3d::imgui::profiling::countAntiAliasingPass();

        // Explicitly configure all render state for 2D post-processing pass
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_STENCIL_TEST);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glViewport(0, 0, width, height);

        int cur = m_taaCurrentIndex;
        int prev = 1 - cur;

        bool cameraMoved = false;
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                if (std::abs(currentViewProj[c][r] - m_lastViewProj[c][r]) > 1e-4f) {
                    cameraMoved = true;
                    break;
                }
            }
            if (cameraMoved) break;
        }
        m_lastViewProj = currentViewProj;

        bool historyWasValid = m_historyValid;
        float blendFactor = 0.10f; // Default stationary accumulation
        if (!historyWasValid) {
            blendFactor = 1.0f; // Discard invalid history completely on first frame
        } else if (cameraMoved || sceneDirty) {
            blendFactor = 0.50f; // Responsive motion blend
        }

        m_lastTaaHistoryValid = historyWasValid;
        m_lastTaaBlendFactor = blendFactor;
        m_lastTaaTargetGeneration = m_targetGeneration;

        glBindFramebuffer(GL_FRAMEBUFFER, m_taaFbo[cur]);
        glUseProgram(m_taaProgram);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, inputColorTexture);
        glUniform1i(m_taaCurrTexLoc, 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_historyValid ? m_taaTexture[prev] : inputColorTexture);
        glUniform1i(m_taaHistTexLoc, 1);

        glUniform2f(m_taaTexelSizeLoc, 1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height));
        glUniform1f(m_taaBlendFactorLoc, blendFactor);
        glUniform1i(m_taaRawModeLoc, m_rawMode ? 1 : 0);

        glBindVertexArray(m_quadVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        ux3d::imgui::profiling::countAppDrawCall();

        // Always restore active texture unit to GL_TEXTURE0 before returning to ImGui
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);

        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDepthMask(GL_TRUE);

        m_historyValid = true;
        m_frameIndex++;
        m_taaCurrentIndex = prev; // Ping-pong for next frame

        return m_taaTexture[cur];
    }

    return inputColorTexture;
}

void AntiAliasing::releaseBloomTargets()
{
    for (int i = 0; i < kBloomMips; ++i) {
        if (m_bloomTexture[i] != 0) {
            glDeleteTextures(1, &m_bloomTexture[i]);
            m_bloomTexture[i] = 0;
        }
        if (m_bloomFbo[i] != 0) {
            glDeleteFramebuffers(1, &m_bloomFbo[i]);
            m_bloomFbo[i] = 0;
        }
        m_bloomMipW[i] = 0;
        m_bloomMipH[i] = 0;
    }
}

void AntiAliasing::ensureBloomBuffers(int width, int height)
{
    if (width < 1 || height < 1) return;

    const double aspect = static_cast<double>(width) / static_cast<double>(std::max(1, height));
    int targetW[kBloomMips] = {};
    int targetH[kBloomMips] = {};
    bool match = true;
    for (int i = 0; i < kBloomMips; ++i) {
        targetH[i] = std::max(1, kBloomCanonicalHeight >> i);
        targetW[i] = std::max(1, static_cast<int>(std::lround(static_cast<double>(targetH[i]) * aspect)));
        if (m_bloomTexture[i] == 0 || m_bloomFbo[i] == 0 ||
            m_bloomMipW[i] != targetW[i] || m_bloomMipH[i] != targetH[i]) {
            match = false;
        }
    }
    if (match) {
        return;
    }

    releaseBloomTargets();

    for (int i = 0; i < kBloomMips; ++i) {
        m_bloomMipW[i] = targetW[i];
        m_bloomMipH[i] = targetH[i];

        glGenTextures(1, &m_bloomTexture[i]);
        glBindTexture(GL_TEXTURE_2D, m_bloomTexture[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_bloomMipW[i], m_bloomMipH[i], 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenFramebuffers(1, &m_bloomFbo[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_bloomTexture[i], 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

GLuint AntiAliasing::renderBloom(GLuint inputHdrTexture, int width, int height, const HdrPostProcessParams& params)
{
    static_assert(kBloomMips == 5, "s_tonemapFS averages the five accumulated bloom levels");
    if (!params.bloomEnabled || params.rawPassThrough || params.bloomIntensity <= 0.0f ||
        !m_bloomDownsampleProgram || !m_bloomUpsampleProgram) {
        return 0;
    }

    ensureBloomBuffers(width, height);
    if (m_bloomTexture[0] == 0 || m_bloomFbo[0] == 0) {
        return 0;
    }

    const float invAspect = static_cast<float>(height) / static_cast<float>(std::max(1, width));

    // 1. 13-tap Downsample chain (mip 0 .. kBloomMips - 1)
    glDisable(GL_BLEND);
    glUseProgram(m_bloomDownsampleProgram);
    glUniform1i(m_bdSrcTexLoc, 0);
    glUniform1i(m_bdHasClearColorLoc, params.hasClearColor ? 1 : 0);
    glUniform3f(m_bdClearColorLoc, params.clearColor.r, params.clearColor.g, params.clearColor.b);

    glBindVertexArray(m_quadVao);
    for (int i = 0; i < kBloomMips; ++i) {
        glViewport(0, 0, m_bloomMipW[i], m_bloomMipH[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFbo[i]);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, (i == 0) ? inputHdrTexture : m_bloomTexture[i - 1]);

        const float srcVirtualH = static_cast<float>((i == 0) ? (kBloomCanonicalHeight * 2) : m_bloomMipH[i - 1]);
        const float stepY = 1.0f / std::max(1.0f, srcVirtualH);
        const float stepX = stepY * invAspect;
        glUniform2f(m_bdSrcTexelSizeLoc, stepX, stepY);
        glUniform1i(m_bdFirstPassLoc, (i == 0) ? 1 : 0);

        glDrawArrays(GL_TRIANGLES, 0, 6);
        ux3d::imgui::profiling::countAppDrawCall();
    }

    // 2. 3x3 Tent Upsample chain with additive accumulation (kBloomMips - 1 down to 1 -> into i - 1)
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE);
    glUseProgram(m_bloomUpsampleProgram);
    glUniform1i(m_buSrcTexLoc, 0);
    glUniform1f(m_buFilterRadiusLoc, std::max(0.1f, params.bloomRadius));

    for (int i = kBloomMips - 1; i >= 1; --i) {
        glViewport(0, 0, m_bloomMipW[i - 1], m_bloomMipH[i - 1]);
        glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFbo[i - 1]);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_bloomTexture[i]);

        const float stepY = 1.0f / static_cast<float>(std::max(1, m_bloomMipH[i]));
        const float stepX = stepY * invAspect;
        glUniform2f(m_buTexelSizeLoc, stepX, stepY);

        glDrawArrays(GL_TRIANGLES, 0, 6);
        ux3d::imgui::profiling::countAppDrawCall();
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
    return m_bloomTexture[0];
}

void AntiAliasing::releaseMeterTargets()
{
    if (m_meterTexture != 0) {
        glDeleteTextures(1, &m_meterTexture);
        m_meterTexture = 0;
    }
    if (m_meterFbo != 0) {
        glDeleteFramebuffers(1, &m_meterFbo);
        m_meterFbo = 0;
    }
}

void AntiAliasing::ensureMeterBuffers()
{
    if (m_meterTexture != 0 && m_meterFbo != 0) {
        return;
    }
    releaseMeterTargets();

    glGenTextures(1, &m_meterTexture);
    glBindTexture(GL_TEXTURE_2D, m_meterTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, kMeterSize, kMeterSize, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &m_meterFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_meterFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_meterTexture, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

float AntiAliasing::updateAutoExposure(GLuint inputHdrTexture, int width, int height, const HdrPostProcessParams& params)
{
    if (!params.autoExposure || params.rawPassThrough || !m_meterProgram) {
        m_exposureValid = false;
        m_exposureAdapting = false;
        m_adaptedEv = params.exposureEv;
        return params.exposureEv;
    }

    ensureMeterBuffers();
    if (m_meterFbo == 0 || m_meterTexture == 0) {
        return params.exposureEv;
    }

    glDisable(GL_BLEND);
    glViewport(0, 0, kMeterSize, kMeterSize);
    glBindFramebuffer(GL_FRAMEBUFFER, m_meterFbo);
    glUseProgram(m_meterProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputHdrTexture);
    glUniform1i(m_meterHdrTexLoc, 0);
    glUniform2f(m_meterTexelSizeLoc, 1.0f / static_cast<float>(kMeterSize), 1.0f / static_cast<float>(kMeterSize));
    glUniform1i(m_meterHasClearColorLoc, params.hasClearColor ? 1 : 0);
    glUniform3f(m_meterClearColorLoc, params.clearColor.r, params.clearColor.g, params.clearColor.b);

    glBindVertexArray(m_quadVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    ux3d::imgui::profiling::countAppDrawCall();

    std::vector<float> pixels(kMeterSize * kMeterSize, 0.0f);
    glReadPixels(0, 0, kMeterSize, kMeterSize, GL_RED, GL_FLOAT, pixels.data());

    struct Sample {
        float logLuma;
        float weight;
    };
    std::vector<Sample> samples;
    samples.reserve(kMeterSize * kMeterSize);
    float totalWeight = 0.0f;
    for (int y = 0; y < kMeterSize; ++y) {
        const float dy = (static_cast<float>(y) + 0.5f) / static_cast<float>(kMeterSize) - 0.5f;
        for (int x = 0; x < kMeterSize; ++x) {
            const float dx = (static_cast<float>(x) + 0.5f) / static_cast<float>(kMeterSize) - 0.5f;
            const float w = std::exp(-4.0f * (dx * dx + dy * dy));
            const float luma = std::max(pixels[static_cast<size_t>(y * kMeterSize + x)], 1e-4f);
            samples.push_back({ std::log2(luma), w });
            totalWeight += w;
        }
    }

    std::stable_sort(samples.begin(), samples.end(), [](const Sample& a, const Sample& b) {
        return a.logLuma < b.logLuma;
    });

    // Outlier rejection: ignore darkest 50% and brightest 2% of centre-weighted samples
    const float loCut = 0.50f * totalWeight;
    const float hiCut = 0.98f * totalWeight;
    float accumW = 0.0f;
    float sumWeightedLogLuma = 0.0f;
    float sumAcceptedW = 0.0f;
    for (const Sample& s : samples) {
        const float nextW = accumW + s.weight;
        const float segLo = std::max(accumW, loCut);
        const float segHi = std::min(nextW, hiCut);
        if (segHi > segLo) {
            const float w = segHi - segLo;
            sumWeightedLogLuma += w * s.logLuma;
            sumAcceptedW += w;
        }
        accumW = nextW;
    }

    const float meanLogLuma = (sumAcceptedW > 1e-6f) ? (sumWeightedLogLuma / sumAcceptedW) : std::log2(0.18f);
    const float avgLuma = std::exp2(meanLogLuma);
    const float minEv = std::min(params.autoExposureMinEv, params.autoExposureMaxEv);
    const float maxEv = std::max(params.autoExposureMinEv, params.autoExposureMaxEv);
    const float targetEv = std::clamp(params.exposureCompensationEv - std::log2(std::max(avgLuma, 1e-4f) / 0.18f), minEv, maxEv);

    const bool snap = params.snapExposure || m_snapExposureNext || (!m_exposureValid && !params.startFromManualEv);
    m_snapExposureNext = false;
    if (snap) {
        m_adaptedEv = targetEv;
        m_exposureValid = true;
        m_exposureAdapting = false;
    } else {
        if (!m_exposureValid) {
            m_adaptedEv = std::clamp(params.exposureEv, minEv, maxEv);
            m_exposureValid = true;
        }
        const float speed = (targetEv > m_adaptedEv) ? params.autoExposureSpeedUp : params.autoExposureSpeedDown;
        const float dt = std::max(0.0f, params.deltaTime);
        const float factor = 1.0f - std::exp(-std::max(0.0f, speed * dt));
        m_adaptedEv = std::clamp(m_adaptedEv + (targetEv - m_adaptedEv) * factor, minEv, maxEv);
        m_exposureAdapting = std::abs(targetEv - m_adaptedEv) > 1e-3f;
    }

    return m_adaptedEv;
}

void AntiAliasing::renderPostProcess(GLuint inputHdrTexture,
                                     GLuint targetLdrFbo,
                                     int width, int height,
                                     const HdrPostProcessParams& params)
{
    if (inputHdrTexture == 0 || targetLdrFbo == 0 || width <= 0 || height <= 0) {
        return;
    }

    if (!m_quadVao || !m_tonemapProgram) {
        init();
    }
    if (!m_tonemapProgram || !m_quadVao) {
        return;
    }

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    // Phase 3: Exposure metering (HDR after TAA resolve)
    const float effectiveEv = updateAutoExposure(params.exposureHdrTexture != 0 ? params.exposureHdrTexture : inputHdrTexture,
                                                  width, height, params);

    // Phase 2: Physically based bloom (Jimenez 2014)
    const GLuint bloomTex = renderBloom(inputHdrTexture, width, height, params);

    // Phase 1: Tone mapping composite pass into LDR FBO
    glViewport(0, 0, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, targetLdrFbo);
    glUseProgram(m_tonemapProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputHdrTexture);
    glUniform1i(m_tmHdrTexLoc, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, (bloomTex != 0) ? bloomTex : inputHdrTexture);
    glUniform1i(m_tmBloomTexLoc, 1);

    const float exposureLinear = std::pow(2.0f, effectiveEv);
    glUniform1f(m_tmExposureLoc, exposureLinear);
    glUniform1i(m_tmToneMapModeLoc, params.toneMappingMode);
    glUniform1i(m_tmRawModeLoc, params.rawPassThrough ? 1 : (params.ssrDebugOverlay ? 2 : 0));
    glUniform1i(m_tmHasClearColorLoc, params.hasClearColor ? 1 : 0);
    glUniform3f(m_tmClearColorLoc, params.clearColor.r, params.clearColor.g, params.clearColor.b);
    glUniform1i(m_tmBloomEnabledLoc, (bloomTex != 0) ? 1 : 0);
    glUniform1f(m_tmBloomIntensityLoc, params.bloomIntensity);

    glBindVertexArray(m_quadVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    ux3d::imgui::profiling::countAppDrawCall();

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDepthMask(GL_TRUE);
}

SelfCheckResult AntiAliasing::runTargetAllocationSelfCheck(bool forceFailure)
{
    SelfCheckResult result;
    result.success = true;
    std::ostringstream report;
    report << "P3.1 anti-aliasing target allocation self-check\n";

    auto check = [&](bool condition, const char* message) {
        if (!condition) {
            result.success = false;
            report << "  FAIL: " << message << "\n";
        } else {
            report << "  PASS: " << message << "\n";
        }
    };

    if (forceFailure) {
        check(false, "forced AA target allocation self-check failure");
        result.report = report.str();
        return result;
    }

    // Create a mock input color texture for testing render calls
    GLuint testInputTex = 0;
    glGenTextures(1, &testInputTex);
    glBindTexture(GL_TEXTURE_2D, testInputTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 800, 600, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    const glm::mat4 identityMat(1.0f);

    AntiAliasing aa;
    aa.init();

    // 1. Initial State: 0 targets, 0 FBOs, 0 live bytes
    check(aa.getOwnedTargetCount() == 0, "Initial target count is 0");
    check(aa.getOwnedFramebufferCount() == 0, "Initial FBO count is 0");
    check(aa.getEstimatedLiveBytes() == 0, "Initial live bytes is 0");
    check(aa.getFxaaTexture() == 0, "Initial FXAA texture is 0");
    check(aa.getTaaTexture(0) == 0 && aa.getTaaTexture(1) == 0, "Initial TAA textures are 0");

    // 2. Startup in None & MSAA: no post-process targets allocated
    GLuint outTex = aa.render(AntiAliasingMode::None, testInputTex, 800, 600, identityMat, false);
    check(outTex == testInputTex, "None mode returns inputColorTexture");
    check(aa.getOwnedTargetCount() == 0, "None mode owns 0 targets");
    check(aa.getEstimatedLiveBytes() == 0, "None mode has 0 live bytes");

    outTex = aa.render(AntiAliasingMode::MSAA_4x, testInputTex, 800, 600, identityMat, false);
    check(outTex == testInputTex, "MSAA_4x mode returns inputColorTexture");
    check(aa.getOwnedTargetCount() == 0, "MSAA_4x owns 0 targets");
    check(aa.getEstimatedLiveBytes() == 0, "MSAA_4x has 0 live bytes");

    // Steady state in None / MSAA: 10 frames with 0 allocations, 0 deletions
    uint64_t allocsBefore = aa.getTargetAllocationCount();
    uint64_t delsBefore = aa.getTargetDeletionCount();
    for (int f = 0; f < 10; ++f) {
        aa.render(AntiAliasingMode::MSAA_4x, testInputTex, 800, 600, identityMat, false);
    }
    check(aa.getTargetAllocationCount() == allocsBefore, "MSAA steady state produces 0 allocations");
    check(aa.getTargetDeletionCount() == delsBefore, "MSAA steady state produces 0 deletions");

    // 3. FXAA startup and target allocation
    aa.resetTargetCounters();
    GLuint fxaaTex = aa.render(AntiAliasingMode::FXAA, testInputTex, 800, 600, identityMat, false);
    check(fxaaTex != 0 && fxaaTex != testInputTex, "FXAA allocates valid output texture");
    check(aa.getOwnedTargetCount() == 1, "FXAA owns exactly 1 target");
    check(aa.getOwnedFramebufferCount() == 1, "FXAA owns exactly 1 FBO");
    check(aa.getFxaaTexture() == fxaaTex, "FXAA target query matches returned texture");
    check(aa.getTaaTexture(0) == 0 && aa.getTaaTexture(1) == 0, "FXAA allocates 0 TAA targets");
    check(aa.getEstimatedLiveBytes() == 800ULL * 600ULL * 4ULL, "FXAA live bytes matches 800x600x4 (1,920,000 bytes)");
    check(aa.getTargetAllocationCount() == 1, "FXAA startup allocates exactly 1 texture");
    check(aa.getTargetDeletionCount() == 0, "FXAA startup deletes 0 textures");
    check(glGetError() == GL_NO_ERROR, "GL error is GL_NO_ERROR after FXAA startup");

    // FXAA steady-state: 10 frames with no allocations or deletions
    allocsBefore = aa.getTargetAllocationCount();
    delsBefore = aa.getTargetDeletionCount();
    for (int f = 0; f < 10; ++f) {
        GLuint steadyTex = aa.render(AntiAliasingMode::FXAA, testInputTex, 800, 600, identityMat, false);
        check(steadyTex == fxaaTex, "FXAA steady state retains same output texture ID");
    }
    check(aa.getTargetAllocationCount() == allocsBefore, "FXAA steady state produces 0 allocations");
    check(aa.getTargetDeletionCount() == delsBefore, "FXAA steady state produces 0 deletions");

    // 4. Transition: FXAA -> TAA
    aa.resetTargetCounters();
    GLuint taaTex1 = aa.render(AntiAliasingMode::TAA, testInputTex, 800, 600, identityMat, false);
    check(taaTex1 != 0 && taaTex1 != testInputTex, "TAA allocates valid output texture");
    check(aa.getOwnedTargetCount() == 2, "TAA owns exactly 2 targets (ping-pong)");
    check(aa.getOwnedFramebufferCount() == 2, "TAA owns exactly 2 FBOs");
    check(aa.getFxaaTexture() == 0, "FXAA target released upon entering TAA");
    check(aa.getTaaTexture(0) != 0 && aa.getTaaTexture(1) != 0, "Both TAA ping-pong targets are allocated");
    check(aa.getEstimatedLiveBytes() == 800ULL * 600ULL * 8ULL * 2ULL, "TAA live bytes matches 800x600x8x2 (7,680,000 bytes)");
    check(aa.getTargetAllocationCount() == 2, "Entering TAA allocates 2 textures");
    check(aa.getTargetDeletionCount() == 1, "Entering TAA deletes 1 inactive FXAA texture");
    check(aa.isHistoryValid() == true, "History becomes valid after first TAA frame");

    // TAA steady-state & ping-pong: 10 frames
    allocsBefore = aa.getTargetAllocationCount();
    delsBefore = aa.getTargetDeletionCount();
    for (int f = 0; f < 10; ++f) {
        GLuint pingPongTex = aa.render(AntiAliasingMode::TAA, testInputTex, 800, 600, identityMat, false);
        check(pingPongTex == aa.getTaaTexture(0) || pingPongTex == aa.getTaaTexture(1), "TAA renders into owned ping-pong targets");
    }
    check(aa.getTargetAllocationCount() == allocsBefore, "TAA steady state produces 0 allocations");
    check(aa.getTargetDeletionCount() == delsBefore, "TAA steady state produces 0 deletions");
    check(aa.getOwnedTargetCount() == 2, "TAA steady state retains exactly 2 targets");

    // 5. Transition: TAA -> None / MSAA
    aa.resetTargetCounters();
    outTex = aa.render(AntiAliasingMode::None, testInputTex, 800, 600, identityMat, false);
    check(outTex == testInputTex, "Transition to None returns inputColorTexture");
    check(aa.getOwnedTargetCount() == 0, "Transition to None releases all targets (0 owned)");
    check(aa.getOwnedFramebufferCount() == 0, "Transition to None releases all FBOs (0 owned)");
    check(aa.getEstimatedLiveBytes() == 0, "Transition to None drops live bytes to 0");
    check(aa.getTargetAllocationCount() == 0, "Transition to None allocates 0 textures");
    check(aa.getTargetDeletionCount() == 2, "Transition to None deletes 2 TAA textures");
    check(aa.isHistoryValid() == false, "Transition to None invalidates TAA history");

    // 6. Transition: None -> TAA
    aa.resetTargetCounters();
    GLuint taaTexReentry = aa.render(AntiAliasingMode::TAA, testInputTex, 800, 600, identityMat, false);
    check(aa.getOwnedTargetCount() == 2, "Re-entering TAA allocates 2 targets");
    check(aa.getFxaaTexture() == 0, "Re-entering TAA does not allocate FXAA target");
    check(aa.getTargetAllocationCount() == 2, "Re-entering TAA allocates 2 textures");
    check(aa.getTargetDeletionCount() == 0, "Re-entering TAA deletes 0 textures");

    // 7. Resizing in TAA: 800x600 -> 1280x720
    aa.resetTargetCounters();
    uint64_t genBeforeResize = aa.getTargetGeneration();
    GLuint taaResized = aa.render(AntiAliasingMode::TAA, testInputTex, 1280, 720, identityMat, false);
    check(aa.getWidth() == 1280 && aa.getHeight() == 720, "TAA dimensions updated to 1280x720");
    check(aa.getOwnedTargetCount() == 2, "TAA retains 2 targets after resize");
    check(aa.getEstimatedLiveBytes() == 1280ULL * 720ULL * 8ULL * 2ULL, "TAA live bytes updated to 1280x720x8x2 (14,745,600 bytes)");
    check(aa.getTargetAllocationCount() == 2, "Resize allocates 2 new textures");
    check(aa.getTargetDeletionCount() == 2, "Resize deletes 2 old textures");
    check(aa.getTargetGeneration() > genBeforeResize, "Target generation advanced on resize");
    check(aa.getLastTaaTargetGeneration() == aa.getTargetGeneration(), "First post-resize TAA pass matches new target generation");
    check(aa.getLastTaaHistoryValid() == false, "First post-resize TAA pass observed history as invalid");
    check(std::abs(aa.getLastTaaBlendFactor() - 1.0f) < 1e-5f, "First post-resize TAA pass used effective blend factor 1.0f");
    // Steady state at new size
    allocsBefore = aa.getTargetAllocationCount();
    delsBefore = aa.getTargetDeletionCount();
    for (int f = 0; f < 5; ++f) {
        aa.render(AntiAliasingMode::TAA, testInputTex, 1280, 720, identityMat, false);
    }
    check(aa.getTargetAllocationCount() == allocsBefore, "TAA steady state post-resize produces 0 allocations");
    check(aa.getTargetDeletionCount() == delsBefore, "TAA steady state post-resize produces 0 deletions");
    check(aa.getLastTaaHistoryValid() == true, "Steady state TAA pass observed history as valid");
    check(std::abs(aa.getLastTaaBlendFactor() - 0.10f) < 1e-5f, "Steady state TAA pass used stationary blend factor 0.10f");

    // 8. Resizing in FXAA: switch to FXAA at 1280x720, then resize to 1920x1080
    aa.render(AntiAliasingMode::FXAA, testInputTex, 1280, 720, identityMat, false);
    check(aa.getOwnedTargetCount() == 1, "Switch to FXAA at 1280x720 owns 1 target");
    aa.resetTargetCounters();
    GLuint fxaaResized = aa.render(AntiAliasingMode::FXAA, testInputTex, 1920, 1080, identityMat, false);
    check(aa.getWidth() == 1920 && aa.getHeight() == 1080, "FXAA dimensions updated to 1920x1080");
    check(aa.getOwnedTargetCount() == 1, "FXAA retains 1 target after resize");
    check(aa.getEstimatedLiveBytes() == 1920ULL * 1080ULL * 4ULL, "FXAA live bytes updated to 1920x1080x4 (8,294,400 bytes)");
    check(aa.getTargetAllocationCount() == 1, "FXAA resize allocates 1 new texture");
    check(aa.getTargetDeletionCount() == 1, "FXAA resize deletes 1 old texture");

    // 9. Rapid mode switching loop (50 cycles): verify 0 leaks
    aa.releaseTargets();
    aa.resetTargetCounters();
    const AntiAliasingMode cycleModes[] = {
        AntiAliasingMode::None,
        AntiAliasingMode::MSAA_2x,
        AntiAliasingMode::MSAA_4x,
        AntiAliasingMode::MSAA_8x,
        AntiAliasingMode::FXAA,
        AntiAliasingMode::TAA,
        AntiAliasingMode::FXAA,
        AntiAliasingMode::None
    };
    for (int cycle = 0; cycle < 50; ++cycle) {
        for (AntiAliasingMode m : cycleModes) {
            aa.render(m, testInputTex, 800, 600, identityMat, false);
        }
    }
    check(aa.getOwnedTargetCount() == 0, "After 50 cycles ending in None, 0 targets owned");
    check(aa.getOwnedFramebufferCount() == 0, "After 50 cycles ending in None, 0 FBOs owned");
    check(aa.getEstimatedLiveBytes() == 0, "After 50 cycles ending in None, 0 live bytes");
    check(aa.getTargetAllocationCount() == aa.getTargetDeletionCount(), "Total allocations equal total deletions after cycling (no leak)");
    check(glGetError() == GL_NO_ERROR, "GL error is GL_NO_ERROR after 50 mode switch cycles");

    // 10. Zero / minimized dimensions
    outTex = aa.render(AntiAliasingMode::FXAA, testInputTex, 0, 0, identityMat, false);
    check(outTex == testInputTex, "FXAA with 0x0 dimension returns inputColorTexture safely");
    check(aa.getOwnedTargetCount() == 0, "FXAA with 0x0 dimension does not allocate 0x0 target");
    outTex = aa.render(AntiAliasingMode::TAA, testInputTex, -10, 0, identityMat, false);
    check(outTex == testInputTex, "TAA with negative dimension returns inputColorTexture safely");
    check(aa.getOwnedTargetCount() == 0, "TAA with negative dimension does not allocate targets");
    check(glGetError() == GL_NO_ERROR, "GL error is GL_NO_ERROR after zero dimension checks");

    // Restoration after zero dimension
    outTex = aa.render(AntiAliasingMode::TAA, testInputTex, 800, 600, identityMat, false);
    check(aa.getOwnedTargetCount() == 2, "TAA restores and allocates 2 targets after zero dimension");

    // 11. Multi-viewport independent allocation
    AntiAliasing vp1;
    AntiAliasing vp2;
    AntiAliasing vp3;
    vp1.init();
    vp2.init();
    vp3.init();

    vp1.render(AntiAliasingMode::FXAA, testInputTex, 800, 600, identityMat, false);
    vp2.render(AntiAliasingMode::TAA, testInputTex, 1024, 768, identityMat, false);
    vp3.render(AntiAliasingMode::MSAA_4x, testInputTex, 1280, 720, identityMat, false);

    check(vp1.getOwnedTargetCount() == 1, "Multi-viewport: vp1 (FXAA) owns 1 target");
    check(vp2.getOwnedTargetCount() == 2, "Multi-viewport: vp2 (TAA) owns 2 targets");
    check(vp3.getOwnedTargetCount() == 0, "Multi-viewport: vp3 (MSAA_4x) owns 0 targets");

    check(vp1.getEstimatedLiveBytes() == 800ULL * 600ULL * 4ULL, "Multi-viewport: vp1 has 1,920,000 live bytes");
    check(vp2.getEstimatedLiveBytes() == 1024ULL * 768ULL * 8ULL * 2ULL, "Multi-viewport: vp2 has 12,582,912 live bytes");
    check(vp3.getEstimatedLiveBytes() == 0, "Multi-viewport: vp3 has 0 live bytes");

    // Viewport closure simulation
    vp2.releaseTargets();
    check(vp2.getOwnedTargetCount() == 0, "Multi-viewport: vp2 closure releases all targets");
    check(vp1.getOwnedTargetCount() == 1, "Multi-viewport: vp1 unaffected by vp2 closure");

    vp2.render(AntiAliasingMode::TAA, testInputTex, 1024, 768, identityMat, false);
    check(vp2.getOwnedTargetCount() == 2, "Multi-viewport: vp2 reopening reallocates TAA targets");

    vp1.cleanup();
    vp2.cleanup();
    vp3.cleanup();
    aa.cleanup();

    if (testInputTex != 0) {
        glDeleteTextures(1, &testInputTex);
    }

    check(glGetError() == GL_NO_ERROR, "Final GL error check: GL_NO_ERROR");

    result.report = report.str();
    return result;
}

} // namespace ux3d::imgui
