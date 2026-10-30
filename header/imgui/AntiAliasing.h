// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <cstddef>
#include <vector>
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
    // A6.7 lens effect from the bloom mips: 0 off, 1 ghosts + halo, 2 anamorphic streak, 3 both.
    int lensFlare = 0;
    float lensFlareIntensity = 1.0f;
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
    // A6.4 (NightSky.h): the scene's linear values arrive multiplied by preExposure (1 = none); the displayed exposure
    // is 2^EV / preExposure. With physicalExposure the metered luminance is converted to EV100 (reflected meter,
    // K = 12.5), clamped to minEv100..maxEv100, and mapped to Glider's EV with the key curve plus the compensation.
    float preExposure = 1.0f;
    bool physicalExposure = false;
    float minEv100 = -12.0f;
    float maxEv100 = 18.0f;
    // A6.4b (NightSky.h): Auto with physicalExposure also meters the highlights (the darker of the average and the
    // highlight exposure) unless highlightMetering is off. Adaptation: autoExposureSpeedUp while the scene gets
    // brighter (the EV goes down), autoExposureSpeedDown while it gets darker.
    bool highlightMetering = true;
    // A6.4b: single-sample framebuffer holding the scene depth of this frame (0 = none, e.g. with MSAA): the highlight
    // meter then counts only surfaces (depth < 1), not the background sky; without it every cell counts.
    GLuint sceneDepthFbo = 0;
    // A6.6: largest weight of the per-pixel scotopic (rod vision) shift; 0 = off (the image is unchanged). Only with
    // calibrated units (the procedural sky's physical exposure).
    float nightVision = 0.0f;
    // C3: read the meter back asynchronously (pixel buffer + fence; the result is used once the GPU has finished it,
    // typically 1-2 frames later) instead of waiting for the GPU with glReadPixels. Interactive rendering only:
    // capture runs keep the synchronous meter, and snaps (snapExposure, a reset, the first meter) always read
    // synchronously so the snapped exposure is the current frame's.
    bool asyncMeterReadback = false;
    // C3: this render's input differs from the previous render's (its render key changed: camera, scene, environment,
    // size, settings); the asynchronous meter then reads the readback of the render where it settles.
    bool meterInputChanged = true;
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
    // A6.4: the scale of the next TAA input (the scene's pre-exposure). The history was written at the scale of its own
    // frame; a change rescales it by current / previous (exactly 1, so byte-identical, while the scale is unchanged).
    void setInputScale(float scale) { m_inputScale = scale > 0.0f ? scale : 1.0f; }
    float getLastHistoryScale() const { return m_lastHistoryScale; }
    uint64_t getHistoryRescales() const { return m_historyRescales; } // TAA passes whose history scale was not 1

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
    // A6.4: the last metered scene average luminance (environment units, pre-exposure removed; 0 before metering).
    float getMeteredLuminance() const { return m_meteredLuminance; }
    // A6.4b: the physically based Auto exposure's two meters (Glider EV, compensation included) and the highlight
    // luminance (brightest channel at kHighlightPercentile, environment units); valid after a physical metering.
    struct MeterReport {
        bool valid = false;
        float averageEv = 0.0f;
        float highlightEv = 0.0f;
        float highlightLuminance = 0.0f;
        float targetEv = 0.0f;
        bool highlightLimited = false; // the target is the highlight meter's
        float surfaceFraction = 1.0f;  // fraction of the meter cells counted by the highlight meter (surfaces)
    };
    const MeterReport& getMeterReport() const { return m_meterReport; }
    bool hasAdaptedExposure() const { return m_exposureValid; }
    // Restart auto-exposure (model load, camera jump); independent of the TAA history.
    void resetExposure();
    // C3: true while the meter still wants renders: the adapted EV is away from the target (as before), or, with the
    // asynchronous meter, the readback of the render where the adaptation settled has not been read yet (at most
    // kMeterDrainRenders more renders), so a still image is metered too. One drain per change of the meter's input
    // (HdrPostProcessParams::meterInputChanged): renders that only follow the exposure or settle TAA do not re-arm it,
    // so the TAA jitter's meter noise cannot keep a still viewport rendering.
    bool isExposureAdapting() const { return m_exposureAdapting || meterDraining(); }
    // C3: whether the last metered render read the meter back asynchronously (get_state reports it).
    bool lastMeterAsync() const { return m_lastMeterAsync; }
    // C3: drops pending asynchronous meter readbacks (viewport closed); the pixel buffers stay for reuse.
    void dropMeterReadbacks();

    static SelfCheckResult runTargetAllocationSelfCheck(bool forceFailure = false);

private:
    static constexpr int kBloomMips = 5;
    static constexpr int kBloomCanonicalHeight = 225;
    static constexpr int kMeterSize = 64;

    void ensureFxaaBuffers(int width, int height);
    void ensureTaaBuffers(int width, int height);
    void ensureBloomBuffers(int width, int height);
    void releaseBloomTargets();
    GLuint renderBloom(GLuint inputHdrTexture, int width, int height, const HdrPostProcessParams& params, float exposure);
    // A6.7: adds the lens effect into bloom level 0 (before the upsample chain) from the downsampled levels 1 and 3.
    void renderLensFlare(int width, int height, const HdrPostProcessParams& params, float exposure);
    void ensureMeterBuffers();
    void releaseMeterTargets();
    float updateAutoExposure(GLuint inputHdrTexture, int width, int height, const HdrPostProcessParams& params);
    // The meter's result (the target EV and its limits); the CPU half of the meter, shared by both readback paths.
    struct MeterTarget {
        float targetEv = 0.0f;
        float minEv = 0.0f;
        float maxEv = 0.0f;
    };
    MeterTarget evaluateMeter(const std::vector<float>& pixels, std::vector<float>& peaks, float surfaceFraction,
                              const HdrPostProcessParams& params);
    float adaptExposure(const MeterTarget& target, bool snap, const HdrPostProcessParams& params);
    static float meterPeaks(const std::vector<float>& cells, const std::vector<float>& depth, std::vector<float>& peaks);
    void issueMeterReadback(GLuint inputHdrTexture, const HdrPostProcessParams& params, int width, int height);
    bool ensureMeterDepthCopy();
    void releaseMeterDepthCopy();
    bool consumeMeterReadbacks();
    void releaseMeterReadbacks();

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
    GLint m_taaHistoryScaleLoc = -1;
    bool m_rawMode = false;
    float m_inputScale = 1.0f;        // scale of the TAA input being rendered (pre-exposure)
    float m_historyInputScale = 1.0f; // scale the history was written at
    float m_lastHistoryScale = 1.0f;  // current / history scale of the last TAA pass
    uint64_t m_historyRescales = 0;

    // Post-processing Tone Mapping resources
    GLuint m_tonemapProgram = 0;
    GLint m_tmHdrTexLoc = -1;
    GLint m_tmBloomTexLoc = -1;
    GLint m_tmExposureLoc = -1;
    GLint m_tmNightShiftLoc = -1;
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

    // A6.7 lens effect (compiled on first use, so it costs nothing while off)
    GLuint m_lensProgram = 0;
    bool m_lensProgramTried = false;
    GLint m_lensSourceLoc = -1;
    GLint m_lensBlurLoc = -1;
    GLint m_lensModeLoc = -1;
    GLint m_lensScaleLoc = -1;
    GLint m_lensExposureLoc = -1;
    GLint m_lensAspectLoc = -1;
    GLint m_lensTexelLoc = -1;

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
    GLuint m_meterDepthFbo = 0; // A6.4b: kMeterSize^2 nearest-sampled scene depth (sky exclusion)
    GLuint m_meterDepthRbo = 0;
    float m_adaptedEv = 0.0f;
    float m_meteredLuminance = 0.0f;
    MeterReport m_meterReport;
    bool m_exposureValid = false;
    bool m_exposureAdapting = false;
    bool m_snapExposureNext = false;
    // C3: asynchronous meter readbacks, a ring of pixel buffers (RG32F meter, optional D24S8 meter depth as float) with
    // a fence each. A slot keeps the parameters of the render it metered (pre-exposure, limits, meter mode).
    static constexpr int kMeterReadbackSlots = 3;
    struct MeterReadback {
        GLuint colourBuffer = 0;
        GLuint depthBuffer = 0;
        GLsync fence = nullptr;
        bool pending = false;
        bool hasDepth = false;
        uint64_t serial = 0;
        HdrPostProcessParams params;
    };
    MeterReadback m_meterReadbacks[kMeterReadbackSlots];
    uint64_t m_meterSerial = 0;
    MeterTarget m_lastMeterTarget;
    bool m_lastMeterTargetValid = false;
    // The serial of the readback issued by the render where the asynchronous adaptation settled (0 = none), the
    // newest serial read back, the renders since that settle render, and whether the next settle drains.
    static constexpr int kMeterDrainRenders = 8;
    bool m_meterDrainArmed = true;
    bool m_lastMeterAsync = false;
    // The asynchronous highlight meter's depth: a 64x64 depth texture (blit target) and its values in R32F.
    GLuint m_meterDepthCopyTexture = 0;
    GLuint m_meterDepthCopyFbo = 0;
    GLuint m_meterDepthValueTexture = 0;
    GLuint m_meterDepthValueFbo = 0;
    GLuint m_meterDepthProgram = 0;
    GLint m_meterDepthTexLoc = -1;
    bool m_meterDepthCopyTried = false;
    uint64_t m_meterSettleSerial = 0;
    uint64_t m_meterConsumedSerial = 0;
    int m_meterDrainRenders = 0;
    bool meterDraining() const
    {
        return m_meterSettleSerial != 0 && m_meterConsumedSerial < m_meterSettleSerial && m_meterDrainRenders < kMeterDrainRenders;
    }

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
