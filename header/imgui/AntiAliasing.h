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
    MSAA_4x = 2, // Recommended default
    MSAA_8x = 3,
    FXAA = 4,
    TAA = 5
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
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

    uint64_t getTargetGeneration() const { return m_targetGeneration; }
    bool getLastTaaHistoryValid() const { return m_lastTaaHistoryValid; }
    float getLastTaaBlendFactor() const { return m_lastTaaBlendFactor; }
    uint64_t getLastTaaTargetGeneration() const { return m_lastTaaTargetGeneration; }

    static SelfCheckResult runTargetAllocationSelfCheck(bool forceFailure = false);

private:
    void ensureFxaaBuffers(int width, int height);
    void ensureTaaBuffers(int width, int height);

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
