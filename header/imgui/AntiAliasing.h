// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <cstddef>
#include "imgui/SelfCheckResult.h"

namespace ux3d::imgui {

enum class AntiAliasingMode {
    None = 0,
    MSAA_2x = 1,
    MSAA_4x = 2, // Legacy mode values are stable for automation
    MSAA_8x = 3,
    FXAA = 4,
    TAA = 5
};

struct HdrPostProcessParams {
    GLuint exposureHdrTexture = 0; // optional normally shaded input for debug-overlay metering
    bool ssrDebugOverlay = false; // alpha > 1 marks raw windshield debug pixels
    bool rawPassThrough = false; // true for debug render modes (--debug-view)
    float exposureEv = 0.0f;     // manual exposure EV
    int toneMappingMode = 1;     // 0 = PBR Neutral, 1 = ACES Hill, 2 = ACES Narkowicz
    bool hasClearColor = false;  // true when skybox is hidden (!showBackground)
    glm::vec3 clearColor{ 0.12f, 0.12f, 0.14f };
    // Phase 2: Bloom
    bool bloomEnabled = true;
    float bloomIntensity = 0.04f;
    float bloomRadius = 1.0f;
    // Phase 3: Auto-Exposure
    bool autoExposure = false;
    float exposureCompensationEv = 0.0f;
    float autoExposureMinEv = -6.0f;
    float autoExposureMaxEv = 6.0f;
    float autoExposureSpeedUp = 3.0f;
    float autoExposureSpeedDown = 1.5f;
    float deltaTime = 1.0f / 60.0f;
    bool snapExposure = false;
    bool startFromManualEv = false;
};

class AntiAliasing {
public:
    AntiAliasing();
    ~AntiAliasing();

    AntiAliasing(const AntiAliasing&) = delete;
    AntiAliasing& operator=(const AntiAliasing&) = delete;

    void init();
    void cleanup();

    // Returns required hardware multisample count for Framebuffer
    static int getSampleCount(AntiAliasingMode mode);

    // Get subpixel projection jitter for TAA
    glm::vec2 getJitter(AntiAliasingMode mode, int width, int height);

    // Execute post-processing AA passes (FXAA or TAA).
    // For MSAA or None, directly returns inputColorTexture.
    GLuint render(AntiAliasingMode mode,
                  GLuint inputColorTexture,
                  int width, int height,
                  const glm::mat4& currentViewProj,
                  bool sceneDirty);

    // Execute HDR post-processing (Bloom + Tone Mapping) into targetLdrFbo.
    void renderPostProcess(GLuint inputHdrTexture,
                           GLuint targetLdrFbo,
                           int width, int height,
                           const HdrPostProcessParams& params);

    void setRawMode(bool rawMode) { m_rawMode = rawMode; }

    void resetHistory();

    // Target management (P3.1)
    void releaseTargets();
    void releaseFxaaTargets();
    void releaseTaaTargets();

    // Diagnostic & verification inspection (P3.1)
    size_t getOwnedTargetCount() const;
    size_t getOwnedFramebufferCount() const;
    uint64_t getEstimatedLiveBytes() const;
    uint64_t getTargetAllocationCount() const { return m_targetAllocationCount; }
    uint64_t getTargetDeletionCount() const { return m_targetDeletionCount; }
    void resetTargetCounters();

    GLuint getFxaaTexture() const { return m_fxaaTexture; }
    GLuint getFxaaFbo() const { return m_fxaaFbo; }
    GLuint getTaaTexture(int index) const { return (index >= 0 && index < 2) ? m_taaTexture[index] : 0; }
    GLuint getTaaFbo(int index) const { return (index >= 0 && index < 2) ? m_taaFbo[index] : 0; }
    bool isHistoryValid() const { return m_historyValid; }
    // A held TAA frame that was not rendered (P5.3) still advances the jitter phase, so
    // the next render (and its picking pass) uses the jitter of an every-frame schedule.
    void skipFrame() { ++m_frameIndex; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

    uint64_t getTargetGeneration() const { return m_targetGeneration; }
    bool getLastTaaHistoryValid() const { return m_lastTaaHistoryValid; }
    float getLastTaaBlendFactor() const { return m_lastTaaBlendFactor; }
    uint64_t getLastTaaTargetGeneration() const { return m_lastTaaTargetGeneration; }
    float getAdaptedExposureEv() const { return m_adaptedEv; }
    // Restart auto-exposure (model load, camera jump); independent of the TAA history.
    void resetExposure();
    bool isExposureAdapting() const { return m_exposureAdapting; }

    static SelfCheckResult runTargetAllocationSelfCheck(bool forceFailure = false);

private:
    static constexpr int kBloomMips = 5;
    static constexpr int kBloomCanonicalHeight = 225;
    static constexpr int kMeterSize = 64;

    void ensureFxaaBuffers(int width, int height);
    void ensureTaaBuffers(int width, int height);
    void ensureBloomBuffers(int width, int height);
    void releaseBloomTargets();
    GLuint renderBloom(GLuint inputHdrTexture, int width, int height, const HdrPostProcessParams& params);
    void ensureMeterBuffers();
    void releaseMeterTargets();
    float updateAutoExposure(GLuint inputHdrTexture, int width, int height, const HdrPostProcessParams& params);

    // Quad VAO & VBO
    GLuint m_quadVao = 0;
    GLuint m_quadVbo = 0;

    // FXAA resources
    GLuint m_fxaaProgram = 0;
    GLint m_fxaaTexLoc = -1;
    GLint m_fxaaTexelSizeLoc = -1;
    GLuint m_fxaaFbo = 0;
    GLuint m_fxaaTexture = 0;

    // TAA Ping-Pong resources
    GLuint m_taaProgram = 0;
    GLint m_taaCurrTexLoc = -1;
    GLint m_taaHistTexLoc = -1;
    GLint m_taaTexelSizeLoc = -1;
    GLint m_taaBlendFactorLoc = -1;
    GLint m_taaRawModeLoc = -1;
    bool m_rawMode = false;

    // Post-processing Tone Mapping resources
    GLuint m_tonemapProgram = 0;
    GLint m_tmHdrTexLoc = -1;
    GLint m_tmBloomTexLoc = -1;
    GLint m_tmExposureLoc = -1;
    GLint m_tmToneMapModeLoc = -1;
    GLint m_tmRawModeLoc = -1;
    GLint m_tmHasClearColorLoc = -1;
    GLint m_tmClearColorLoc = -1;
    GLint m_tmBloomEnabledLoc = -1;
    GLint m_tmBloomIntensityLoc = -1;

    // Jimenez 2014 Bloom resources
    GLuint m_bloomDownsampleProgram = 0;
    GLint m_bdSrcTexLoc = -1;
    GLint m_bdSrcTexelSizeLoc = -1;
    GLint m_bdFirstPassLoc = -1;
    GLint m_bdHasClearColorLoc = -1;
    GLint m_bdClearColorLoc = -1;

    GLuint m_bloomUpsampleProgram = 0;
    GLint m_buSrcTexLoc = -1;
    GLint m_buTexelSizeLoc = -1;
    GLint m_buFilterRadiusLoc = -1;

    GLuint m_bloomFbo[kBloomMips] = {};
    GLuint m_bloomTexture[kBloomMips] = {};
    int m_bloomMipW[kBloomMips] = {};
    int m_bloomMipH[kBloomMips] = {};

    // Auto-Exposure metering resources
    GLuint m_meterProgram = 0;
    GLint m_meterHdrTexLoc = -1;
    GLint m_meterTexelSizeLoc = -1;
    GLint m_meterHasClearColorLoc = -1;
    GLint m_meterClearColorLoc = -1;
    GLuint m_meterFbo = 0;
    GLuint m_meterTexture = 0;
    float m_adaptedEv = 0.0f;
    bool m_exposureValid = false;
    bool m_exposureAdapting = false;
    bool m_snapExposureNext = false;

    GLuint m_taaFbo[2] = { 0, 0 };
    GLuint m_taaTexture[2] = { 0, 0 };
    int m_taaCurrentIndex = 0;
    bool m_historyValid = false;

    int m_width = 0;
    int m_height = 0;

    uint32_t m_frameIndex = 0;
    glm::mat4 m_lastViewProj = glm::mat4(1.0f);

    uint64_t m_targetAllocationCount = 0;
    uint64_t m_targetDeletionCount = 0;

    uint64_t m_targetGeneration = 0;
    bool m_lastTaaHistoryValid = false;
    float m_lastTaaBlendFactor = 0.0f;
    uint64_t m_lastTaaTargetGeneration = 0;
};

} // namespace ux3d::imgui
