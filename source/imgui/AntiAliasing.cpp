// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/AntiAliasing.h"
#include "imgui/Profiling.h"
#include <cstdio>
#include <cmath>
#include <sstream>

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
            vec3 neighbor = texture(u_CurrentTexture, v_TexCoord + vec2(float(x), float(y)) * u_TexelSize).rgb;
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
    }
}

void AntiAliasing::cleanup()
{
    releaseTargets();

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
    const uint64_t perTexture = static_cast<uint64_t>(m_width) * static_cast<uint64_t>(m_height) * 4ull;
    if (m_fxaaTexture != 0) bytes += perTexture;
    if (m_taaTexture[0] != 0) bytes += perTexture;
    if (m_taaTexture[1] != 0) bytes += perTexture;
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
    m_historyValid = false;
    m_frameIndex = 0;
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
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
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
    ux3d::imgui::profiling::countTextureAllocation(static_cast<uint64_t>(m_width) * static_cast<uint64_t>(m_height) * 4ull * 2ull);
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
    check(aa.getEstimatedLiveBytes() == 800ULL * 600ULL * 4ULL * 2ULL, "TAA live bytes matches 800x600x4x2 (3,840,000 bytes)");
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
    check(aa.getEstimatedLiveBytes() == 1280ULL * 720ULL * 4ULL * 2ULL, "TAA live bytes updated to 1280x720x4x2 (7,372,800 bytes)");
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
    check(vp2.getEstimatedLiveBytes() == 1024ULL * 768ULL * 4ULL * 2ULL, "Multi-viewport: vp2 has 6,291,456 live bytes");
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
