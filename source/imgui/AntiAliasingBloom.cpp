// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// HDR post processing of AntiAliasing: physically based bloom, the A6.7 lens flare, auto-exposure metering and the
// tone-mapping composite (renderPostProcess).

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

void AntiAliasing::renderLensFlare(int width, int height, const HdrPostProcessParams& params, float exposure)
{
    if (!m_lensProgramTried) {
        m_lensProgramTried = true;
        ux3d::imgui::profiling::countShaderCompile();
        ux3d::imgui::profiling::countShaderCompile();
        ux3d::imgui::profiling::countProgramLink();
        m_lensProgram = linkProgram(s_quadVS, s_lensFlareFS);
        if (m_lensProgram) {
            m_lensSourceLoc = glGetUniformLocation(m_lensProgram, "u_Source");
            m_lensBlurLoc = glGetUniformLocation(m_lensProgram, "u_Blur");
            m_lensModeLoc = glGetUniformLocation(m_lensProgram, "u_Mode");
            m_lensScaleLoc = glGetUniformLocation(m_lensProgram, "u_Scale");
            m_lensExposureLoc = glGetUniformLocation(m_lensProgram, "u_Exposure");
            m_lensAspectLoc = glGetUniformLocation(m_lensProgram, "u_Aspect");
            m_lensTexelLoc = glGetUniformLocation(m_lensProgram, "u_SourceTexel");
        }
    }
    if (m_lensProgram == 0 || m_bloomTexture[1] == 0 || m_bloomTexture[3] == 0) {
        return;
    }
    // The composite adds bloom x clamp(intensity, 0, 1) / 5 (s_tonemapFS) before the exposure: write the flare so it
    // arrives as lensFlareIntensity x the flare in exposed units.
    const float bloomMix = std::clamp(params.bloomIntensity, 0.0f, 1.0f);
    const float scale = std::max(params.lensFlareIntensity, 0.0f) * 5.0f /
                        (std::max(bloomMix, 1.0e-4f) * std::max(exposure, 1.0e-8f));
    glViewport(0, 0, m_bloomMipW[0], m_bloomMipH[0]);
    glBindFramebuffer(GL_FRAMEBUFFER, m_bloomFbo[0]);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE);
    glUseProgram(m_lensProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_bloomTexture[1]);
    glUniform1i(m_lensSourceLoc, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_bloomTexture[3]);
    glUniform1i(m_lensBlurLoc, 1);
    glUniform1i(m_lensModeLoc, std::clamp(params.lensFlare, 0, 3));
    glUniform1f(m_lensScaleLoc, scale);
    glUniform1f(m_lensExposureLoc, exposure);
    glUniform1f(m_lensAspectLoc, static_cast<float>(width) / static_cast<float>(std::max(1, height)));
    glUniform2f(m_lensTexelLoc, 1.0f / static_cast<float>(std::max(1, m_bloomMipW[1])),
                1.0f / static_cast<float>(std::max(1, m_bloomMipH[1])));
    glBindVertexArray(m_quadVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    ux3d::imgui::profiling::countAppDrawCall();
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_BLEND);
}

GLuint AntiAliasing::renderBloom(GLuint inputHdrTexture, int width, int height, const HdrPostProcessParams& params, float exposure)
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

    // A6.7: the lens effect reads the pure downsampled levels 1 and 3 and adds into level 0; the upsample below adds
    // into level 0 as well, so the order does not matter. Off (the default) skips it entirely.
    if (params.lensFlare > 0 && params.lensFlareIntensity > 0.0f) {
        renderLensFlare(width, height, params, exposure);
        glBindVertexArray(m_quadVao);
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
    if (m_meterDepthRbo != 0) {
        glDeleteRenderbuffers(1, &m_meterDepthRbo);
        m_meterDepthRbo = 0;
    }
    if (m_meterDepthFbo != 0) {
        glDeleteFramebuffers(1, &m_meterDepthFbo);
        m_meterDepthFbo = 0;
    }
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, kMeterSize, kMeterSize, 0, GL_RG, GL_FLOAT, nullptr);
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
    // A6.4b: the brightest channel per cell, for the physically based highlight meter, and which cells show a surface
    // (scene depth below 1, nearest-sampled at the cell): the background sky (its glow around the sun, the moon, the
    // stars) may compress in the tone mapper's shoulder as before; lit surfaces (screens, panels, an exterior seen
    // through a window) are what the highlight meter keeps readable.
    std::vector<float> peaks;
    float surfaceFraction = 1.0f;
    if (params.physicalExposure && params.highlightMetering) {
        std::vector<float> cells(kMeterSize * kMeterSize, 0.0f);
        glReadPixels(0, 0, kMeterSize, kMeterSize, GL_GREEN, GL_FLOAT, cells.data());
        std::vector<float> depth;
        if (params.sceneDepthFbo != 0) {
            if (m_meterDepthFbo == 0) {
                glGenRenderbuffers(1, &m_meterDepthRbo);
                glBindRenderbuffer(GL_RENDERBUFFER, m_meterDepthRbo);
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, kMeterSize, kMeterSize); // the scene's format
                glBindRenderbuffer(GL_RENDERBUFFER, 0);
                glGenFramebuffers(1, &m_meterDepthFbo);
                glBindFramebuffer(GL_FRAMEBUFFER, m_meterDepthFbo);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_meterDepthRbo);
                glDrawBuffer(GL_NONE);
                glReadBuffer(GL_NONE);
            }
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_meterDepthFbo);
            if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, params.sceneDepthFbo);
                glBlitFramebuffer(0, 0, width, height, 0, 0, kMeterSize, kMeterSize, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, m_meterDepthFbo);
                depth.assign(kMeterSize * kMeterSize, 1.0f);
                glReadPixels(0, 0, kMeterSize, kMeterSize, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
            }
            glBindFramebuffer(GL_FRAMEBUFFER, m_meterFbo);
        }
        peaks.reserve(cells.size());
        size_t surfaces = 0;
        for (size_t i = 0; i < cells.size(); ++i) {
            // Sky cells count as black: they still make up their share of the image area.
            const bool surface = depth.empty() || depth[i] < 1.0f;
            peaks.push_back(surface ? cells[i] : 0.0f);
            surfaces += surface ? 1 : 0;
        }
        surfaceFraction = static_cast<float>(surfaces) / static_cast<float>(cells.size());
    }

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
    float minEv = std::min(params.autoExposureMinEv, params.autoExposureMaxEv);
    float maxEv = std::max(params.autoExposureMinEv, params.autoExposureMaxEv);
    float targetEv = 0.0f;
    if (params.physicalExposure) {
        namespace night = ux3d::imgui::night;
        // A6.4: EV100 of the metered average (pre-exposure removed), limited, then Glider's EV with the key curve.
        const float luminance = std::max(avgLuma, 1e-4f) / params.preExposure;
        m_meteredLuminance = luminance;
        targetEv = night::gliderEvFromEv100(night::ev100FromLuminance(luminance), params.minEv100, params.maxEv100,
                                            params.exposureCompensationEv);
        minEv = night::gliderEvFromEv100(params.maxEv100, params.minEv100, params.maxEv100, params.exposureCompensationEv);
        maxEv = night::gliderEvFromEv100(params.minEv100, params.minEv100, params.maxEv100, params.exposureCompensationEv);
        m_meterReport = MeterReport{};
        m_meterReport.valid = true;
        m_meterReport.averageEv = targetEv;
        m_meterReport.highlightEv = targetEv;
        if (!peaks.empty()) {
            // A6.4b highlight meter (NightSky.h): the brightest channel below which kHighlightPercentile of the cells lie,
            // exposed to kHighlightTarget (plus the compensation); the darker of the two meters.
            const size_t rank = static_cast<size_t>(night::kHighlightPercentile * static_cast<float>(peaks.size() - 1) + 0.5f);
            std::nth_element(peaks.begin(), peaks.begin() + static_cast<std::ptrdiff_t>(rank), peaks.end());
            const float highlight = std::max(peaks[rank], 1e-4f) / params.preExposure;
            const float highlightEv = std::log2(night::kHighlightTarget / highlight) + params.exposureCompensationEv;
            m_meterReport.highlightLuminance = highlight;
            m_meterReport.surfaceFraction = surfaceFraction;
            m_meterReport.highlightEv = highlightEv;
            if (highlightEv < targetEv) {
                targetEv = highlightEv;
                m_meterReport.highlightLimited = true;
            }
        }
        targetEv = std::clamp(targetEv, minEv, maxEv);
        m_meterReport.targetEv = targetEv;
    } else {
        m_meteredLuminance = std::max(avgLuma, 1e-4f);
        targetEv = std::clamp(params.exposureCompensationEv - std::log2(std::max(avgLuma, 1e-4f) / 0.18f), minEv, maxEv);
    }

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
        // A6.4b: with the physically based exposure the speeds are the scene brightness going up (the EV goes down:
        // light adaptation, the faster default) and going down (dark adaptation).
        const float speed = params.physicalExposure
                                ? ((targetEv < m_adaptedEv) ? params.autoExposureSpeedUp : params.autoExposureSpeedDown)
                                : ((targetEv > m_adaptedEv) ? params.autoExposureSpeedUp : params.autoExposureSpeedDown);
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
    // A6.4: the input is pre-exposed; the displayed exposure removes it (exactly 2^EV while preExposure is 1).
    const GLuint bloomTex = renderBloom(inputHdrTexture, width, height, params, std::pow(2.0f, effectiveEv) / params.preExposure);

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

    const float exposureLinear = std::pow(2.0f, effectiveEv) / params.preExposure;
    glUniform1f(m_tmExposureLoc, exposureLinear);
    glUniform1i(m_tmToneMapModeLoc, params.toneMappingMode);
    glUniform1i(m_tmRawModeLoc, params.rawPassThrough ? 1 : (params.ssrDebugOverlay ? 2 : 0));
    glUniform1i(m_tmHasClearColorLoc, params.hasClearColor ? 1 : 0);
    glUniform3f(m_tmClearColorLoc, params.clearColor.r, params.clearColor.g, params.clearColor.b);
    glUniform1i(m_tmBloomEnabledLoc, (bloomTex != 0) ? 1 : 0);
    glUniform1f(m_tmBloomIntensityLoc, params.bloomIntensity);
    glUniform2f(m_tmNightShiftLoc, params.nightVision, params.nightVision > 0.0f ? 12800.0f / params.preExposure : 0.0f);

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

} // namespace ux3d::imgui
