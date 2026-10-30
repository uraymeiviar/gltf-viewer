// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Per-viewport anti-aliasing: programs and quad, target ownership and release, TAA jitter and history, the FXAA
// and TAA buffers and the AA render pass. Bloom, lens flare, exposure metering and the tone-mapping composite
// are in AntiAliasingBloom.cpp; the target allocation self-check in AntiAliasingSelfCheck.cpp; the GLSL in
// AntiAliasingShaders.h.

#include "imgui/AntiAliasing.h"
#include "imgui/NightSky.h"
#include "imgui/Profiling.h"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <sstream>
#include <vector>
#include "AntiAliasingShaders.h"

namespace ux3d::imgui {

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
        m_taaHistoryScaleLoc = glGetUniformLocation(m_taaProgram, "u_HistoryScale");
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
        m_tmNightShiftLoc = glGetUniformLocation(m_tonemapProgram, "u_NightShift");
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
    if (m_lensProgram) {
        glDeleteProgram(m_lensProgram);
        m_lensProgram = 0;
    }
    m_lensProgramTried = false;
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
        // The history was written at its frame's pre-exposure: bring it to this frame's (exact 1 when unchanged).
        const float historyScale = historyWasValid && m_historyInputScale > 0.0f ? m_inputScale / m_historyInputScale : 1.0f;
        m_lastHistoryScale = historyScale;
        if (historyScale != 1.0f) ++m_historyRescales;
        glUniform1f(m_taaHistoryScaleLoc, historyScale);
        m_historyInputScale = m_inputScale;

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

} // namespace ux3d::imgui
