// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// AntiAliasing::runTargetAllocationSelfCheck: the render-target allocation / release self-check.

#include "imgui/AntiAliasing.h"
#include "imgui/NightSky.h"
#include "imgui/Profiling.h"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <sstream>
#include <vector>

namespace ux3d::imgui {

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
