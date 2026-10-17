// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/AntiAliasing.h"
#include <cstdio>
#include <cmath>

namespace ux3d::imgui {

// Fullscreen triangle vertex shader (no VBO needed)
static const char* s_fullscreenVS = R"(#version 410 core
out vec2 v_TexCoord;
void main() {
    float x = -1.0 + float((gl_VertexID & 1) << 2);
    float y = -1.0 + float((gl_VertexID & 2) << 1);
    v_TexCoord = vec2(x * 0.5 + 0.5, y * 0.5 + 0.5);
    gl_Position = vec4(x, y, 0.0, 1.0);
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

// TAA Fragment Shader with 3x3 color neighborhood clamp to prevent ghosting
static const char* s_taaFS = R"(#version 410 core
in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_CurrentTexture;
uniform sampler2D u_HistoryTexture;
uniform vec2 u_TexelSize;
uniform float u_BlendFactor;

void main() {
    vec3 current = texture(u_CurrentTexture, v_TexCoord).rgb;
    vec3 history = texture(u_HistoryTexture, v_TexCoord).rgb;

    // 3x3 color neighborhood bounding box
    vec3 minColor = current;
    vec3 maxColor = current;

    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec3 neighbor = texture(u_CurrentTexture, v_TexCoord + vec2(x, y) * u_TexelSize).rgb;
            minColor = min(minColor, neighbor);
            maxColor = max(maxColor, neighbor);
        }
    }

    // Clamp history into the current neighborhood color bounding box
    history = clamp(history, minColor, maxColor);

    // Exponential moving average blend
    vec3 result = mix(history, current, u_BlendFactor);
    FragColor = vec4(result, 1.0);
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

    glGenVertexArrays(1, &m_quadVao);

    // Compile FXAA program
    m_fxaaProgram = linkProgram(s_fullscreenVS, s_fxaaFS);
    if (m_fxaaProgram) {
        m_fxaaTexLoc = glGetUniformLocation(m_fxaaProgram, "u_Texture");
        m_fxaaTexelSizeLoc = glGetUniformLocation(m_fxaaProgram, "u_TexelSize");
    }

    // Compile TAA program
    m_taaProgram = linkProgram(s_fullscreenVS, s_taaFS);
    if (m_taaProgram) {
        m_taaCurrTexLoc = glGetUniformLocation(m_taaProgram, "u_CurrentTexture");
        m_taaHistTexLoc = glGetUniformLocation(m_taaProgram, "u_HistoryTexture");
        m_taaTexelSizeLoc = glGetUniformLocation(m_taaProgram, "u_TexelSize");
        m_taaBlendFactorLoc = glGetUniformLocation(m_taaProgram, "u_BlendFactor");
    }
}

void AntiAliasing::cleanup()
{
    if (m_quadVao) {
        glDeleteVertexArrays(1, &m_quadVao);
        m_quadVao = 0;
    }
    if (m_fxaaProgram) {
        glDeleteProgram(m_fxaaProgram);
        m_fxaaProgram = 0;
    }
    if (m_fxaaFbo) {
        glDeleteFramebuffers(1, &m_fxaaFbo);
        m_fxaaFbo = 0;
    }
    if (m_fxaaTexture) {
        glDeleteTextures(1, &m_fxaaTexture);
        m_fxaaTexture = 0;
    }
    if (m_taaProgram) {
        glDeleteProgram(m_taaProgram);
        m_taaProgram = 0;
    }
    for (int i = 0; i < 2; ++i) {
        if (m_taaFbo[i]) {
            glDeleteFramebuffers(1, &m_taaFbo[i]);
            m_taaFbo[i] = 0;
        }
        if (m_taaTexture[i]) {
            glDeleteTextures(1, &m_taaTexture[i]);
            m_taaTexture[i] = 0;
        }
    }
    m_width = 0;
    m_height = 0;
    m_historyValid = false;
    m_frameIndex = 0;
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
    m_historyValid = false;
    m_frameIndex = 0;
}

void AntiAliasing::ensureBuffers(int width, int height)
{
    if (width < 1) width = 1;
    if (height < 1) height = 1;

    if (m_width == width && m_height == height && m_fxaaTexture != 0 && m_taaTexture[0] != 0) {
        return;
    }

    m_width = width;
    m_height = height;
    m_historyValid = false;

    // 1. Setup FXAA FBO and Texture
    if (m_fxaaTexture) glDeleteTextures(1, &m_fxaaTexture);
    if (m_fxaaFbo) glDeleteFramebuffers(1, &m_fxaaFbo);

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

    // 2. Setup TAA Ping-Pong FBOs and Textures
    for (int i = 0; i < 2; ++i) {
        if (m_taaTexture[i]) glDeleteTextures(1, &m_taaTexture[i]);
        if (m_taaFbo[i]) glDeleteFramebuffers(1, &m_taaFbo[i]);

        glGenTextures(1, &m_taaTexture[i]);
        glBindTexture(GL_TEXTURE_2D, m_taaTexture[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glGenFramebuffers(1, &m_taaFbo[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, m_taaFbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_taaTexture[i], 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
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
        mode == AntiAliasingMode::MSAA_8x) {
        return inputColorTexture;
    }

    ensureBuffers(width, height);

    // Disable depth testing and blending for fullscreen post-processing
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glViewport(0, 0, width, height);

    if (mode == AntiAliasingMode::FXAA) {
        if (!m_fxaaProgram || !m_fxaaFbo) {
            return inputColorTexture;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, m_fxaaFbo);
        glUseProgram(m_fxaaProgram);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, inputColorTexture);
        glUniform1i(m_fxaaTexLoc, 0);
        glUniform2f(m_fxaaTexelSizeLoc, 1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height));

        glBindVertexArray(m_quadVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);

        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return m_fxaaTexture;
    }

    if (mode == AntiAliasingMode::TAA) {
        if (!m_taaProgram || !m_taaFbo[0] || !m_taaFbo[1]) {
            return inputColorTexture;
        }

        int cur = m_taaCurrentIndex;
        int prev = 1 - cur;

        bool cameraMoved = (currentViewProj != m_lastViewProj);
        m_lastViewProj = currentViewProj;

        float blendFactor = 0.10f; // Default stationary accumulation
        if (!m_historyValid || sceneDirty) {
            blendFactor = 1.0f; // Discard invalid history completely
        } else if (cameraMoved) {
            blendFactor = 0.50f; // Responsive camera motion blend
        }

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

        glBindVertexArray(m_quadVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);

        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        m_historyValid = true;
        m_frameIndex++;
        m_taaCurrentIndex = prev; // Ping-pong for next frame

        return m_taaTexture[cur];
    }

    return inputColorTexture;
}

} // namespace ux3d::imgui
