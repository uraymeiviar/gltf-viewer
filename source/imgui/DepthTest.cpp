// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/DepthTest.h"
#include <glad/glad.h>
#include "imgui/GladGL.h"
#include "imgui/Profiling.h"
#include "renderer/Renderer.h"
#include "renderer/CommandGenerator.h"
#include "renderer/CommandExecutor.h"
#include "renderer/Command.h"
#include "renderer/Mesh.h"
#include "renderer/gl/Buffer.h"
#include "renderer/gl/Program.h"
#include "renderer/gl/VertexLayout.h"

#include <array>
#include <sstream>
#include <vector>
#include <cmath>
#include <cstdio>

namespace ux3d::imgui {

namespace {

GLuint compileShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint status = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Shader compile error: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint linkProgram(GLuint vs, GLuint fs)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint status = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::fprintf(stderr, "Program link error: %s\n", log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

template <typename T>
ux3d::renderer::gl::Buffer makeGlBuffer(ux3d::renderer::gl::GL* gl, const std::vector<T>& data)
{
    return ux3d::renderer::gl::Buffer(gl, ux3d::core::MemoryRange<const uint8_t>(
        reinterpret_cast<const uint8_t*>(data.data()), data.size() * sizeof(T)));
}

} // namespace

SelfCheckResult runDepthSelfCheck(
    ux3d::renderer::gl::GL* gl,
    bool forceFailure,
    const std::string& testCase,
    ux3d::renderer::Renderer* glRenderer,
    ux3d::renderer::CommandGenerator* commandGenerator,
    std::function<bool(const std::string&)> reloadAndDraw
)
{
    SelfCheckResult result;
    result.success = true;
    std::ostringstream report;
    report << "P3.4.4 CommandExecutor Depth Test Enablement and Depth Function Cache self-check\n";
    report << "Test case filter: " << (testCase.empty() ? "all" : testCase) << "\n";

    size_t checks = 0;
    std::vector<std::string> failures;

    auto check = [&](bool condition, const std::string& desc) {
        ++checks;
        if (!condition) {
            result.success = false;
            failures.push_back(desc);
            report << "  FAIL: " << desc << "\n";
        } else {
            report << "  PASS: " << desc << "\n";
        }
    };

    if (forceFailure) {
        check(false, "forced depth self-check failure requested");
        report << "\nTotal assertions evaluated: " << checks << "\n";
        report << "Failed assertions: " << failures.size() << "\n";
        report << "Result: FAIL\n";
        result.success = false;
        result.report = report.str();
        return result;
    }

    if (gl == nullptr) {
        check(false, "GL wrapper pointer is null");
        report << "\nTotal assertions evaluated: " << checks << "\n";
        report << "Failed assertions: " << failures.size() << "\n";
        report << "Result: FAIL\n";
        result.success = false;
        result.report = report.str();
        return result;
    }

    const std::string filter = testCase.empty() ? "all" : testCase;
    const bool runTransitions = (filter == "all" || filter == "transitions" || filter == "calls");
    const bool runCorrectness = (filter == "all" || filter == "correctness" || filter == "rendering");
    const bool runBoundary = (filter == "all" || filter == "boundary" || filter == "isolation");
    const bool runIntegration = (filter == "all" || filter == "integration" || filter == "gltf");

    // -------------------------------------------------------------
    // Shaders & Shared Geometry Fixtures
    // -------------------------------------------------------------
    const char* vsSrc =
        "#version 330 core\n"
        "layout(location = 0) in vec3 a_position;\n"
        "void main() {\n"
        "    gl_Position = vec4(a_position, 1.0);\n"
        "}\n";

    const char* fsGreenSrc =
        "#version 330 core\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    fragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
        "}\n";

    const char* fsRedSrc =
        "#version 330 core\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    fragColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
        "}\n";

    const char* fsBlueSrc =
        "#version 330 core\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    fragColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
        "}\n";

    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fsGreen = compileShader(GL_FRAGMENT_SHADER, fsGreenSrc);
    GLuint fsRed = compileShader(GL_FRAGMENT_SHADER, fsRedSrc);
    GLuint fsBlue = compileShader(GL_FRAGMENT_SHADER, fsBlueSrc);
    GLuint progGreenHandle = linkProgram(vs, fsGreen);
    GLuint progRedHandle = linkProgram(vs, fsRed);
    GLuint progBlueHandle = linkProgram(vs, fsBlue);

    check(progGreenHandle != 0, "Green test shader program linked successfully");
    check(progRedHandle != 0, "Red test shader program linked successfully");
    check(progBlueHandle != 0, "Blue test shader program linked successfully");

    ux3d::renderer::gl::Program progGreen(gl, progGreenHandle);
    ux3d::renderer::gl::Program progRed(gl, progRedHandle);
    ux3d::renderer::gl::Program progBlue(gl, progBlueHandle);

    // Quad Near at Z = 0.0f
    std::vector<float> quadNearPos = {
        -0.8f, -0.8f, 0.0f,
         0.8f, -0.8f, 0.0f,
         0.8f,  0.8f, 0.0f,
        -0.8f,  0.8f, 0.0f
    };
    auto quadNearPosBuf = makeGlBuffer(gl, quadNearPos);
    std::vector<uint16_t> quadIndices = { 0, 1, 2, 0, 2, 3 };
    auto quadIdxBuf = makeGlBuffer(gl, quadIndices);

    ux3d::renderer::Mesh meshQuadNear(gl);
    ux3d::renderer::gl::Attribute attrNear("a_position");
    attrNear.buffer = &quadNearPosBuf;
    attrNear.dimension = 3;
    attrNear.type = ux3d::renderer::gl::ComponentType::Float;
    meshQuadNear.attributes.push_back(attrNear);
    meshQuadNear.indexBuffer = &quadIdxBuf;
    meshQuadNear.indexCount = 6;
    meshQuadNear.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
    meshQuadNear.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
    std::shared_ptr<ux3d::renderer::gl::VertexLayout> layoutQuadNear = progGreen.createVertexLayout(meshQuadNear.attributes);
    check(layoutQuadNear != nullptr && layoutQuadNear->handle != 0, "Near quad vertex layout created");

    // Quad Far at Z = 0.5f
    std::vector<float> quadFarPos = {
        -0.8f, -0.8f, 0.5f,
         0.8f, -0.8f, 0.5f,
         0.8f,  0.8f, 0.5f,
        -0.8f,  0.8f, 0.5f
    };
    auto quadFarPosBuf = makeGlBuffer(gl, quadFarPos);

    ux3d::renderer::Mesh meshQuadFar(gl);
    ux3d::renderer::gl::Attribute attrFar("a_position");
    attrFar.buffer = &quadFarPosBuf;
    attrFar.dimension = 3;
    attrFar.type = ux3d::renderer::gl::ComponentType::Float;
    meshQuadFar.attributes.push_back(attrFar);
    meshQuadFar.indexBuffer = &quadIdxBuf;
    meshQuadFar.indexCount = 6;
    meshQuadFar.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
    meshQuadFar.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
    std::shared_ptr<ux3d::renderer::gl::VertexLayout> layoutQuadFar = progRed.createVertexLayout(meshQuadFar.attributes);
    check(layoutQuadFar != nullptr && layoutQuadFar->handle != 0, "Far quad vertex layout created");

    // Quad Equal at Z = 0.0f
    std::vector<float> quadEqualPos = {
        -0.8f, -0.8f, 0.0f,
         0.8f, -0.8f, 0.0f,
         0.8f,  0.8f, 0.0f,
        -0.8f,  0.8f, 0.0f
    };
    auto quadEqualPosBuf = makeGlBuffer(gl, quadEqualPos);

    ux3d::renderer::Mesh meshQuadEqual(gl);
    ux3d::renderer::gl::Attribute attrEqual("a_position");
    attrEqual.buffer = &quadEqualPosBuf;
    attrEqual.dimension = 3;
    attrEqual.type = ux3d::renderer::gl::ComponentType::Float;
    meshQuadEqual.attributes.push_back(attrEqual);
    meshQuadEqual.indexBuffer = &quadIdxBuf;
    meshQuadEqual.indexCount = 6;
    meshQuadEqual.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
    meshQuadEqual.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
    std::shared_ptr<ux3d::renderer::gl::VertexLayout> layoutQuadEqual = progBlue.createVertexLayout(meshQuadEqual.attributes);
    check(layoutQuadEqual != nullptr && layoutQuadEqual->handle != 0, "Equal quad vertex layout created");

    // Offscreen 64x64 FBO with RGBA8 color and DEPTH24 renderbuffer
    GLuint fbo = 0;
    GLuint colorTex = 0;
    GLuint depthRbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenTextures(1, &colorTex);
    glBindTexture(GL_TEXTURE_2D, colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

    glGenRenderbuffers(1, &depthRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 64, 64);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRbo);
    glViewport(0, 0, 64, 64);

    const GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    check(fboStatus == GL_FRAMEBUFFER_COMPLETE, "Offscreen test FBO with depth buffer is complete");

    auto clearFbo = [&]() {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, 64, 64);
        glDepthMask(GL_TRUE);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClearDepth(1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    };

    auto readCenterPixel = [&]() -> std::array<uint8_t, 4> {
        std::array<uint8_t, 4> px = { 0, 0, 0, 0 };
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        return px;
    };

    // Helper command builders
    auto makeDrawNear = [&]() -> ux3d::renderer::Command {
        ux3d::renderer::Command cmd(gl, ux3d::renderer::CommandTag::Draw);
        cmd.program = &progGreen;
        cmd.vertexLayout = layoutQuadNear;
        cmd.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
        cmd.primitiveCount = 6;
        cmd.primitiveOffset = 0;
        cmd.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
        cmd.indexBuffer = &quadIdxBuf;
        cmd.polygonMode = ux3d::renderer::gl::PolygonMode::Fill;
        return cmd;
    };

    auto makeDrawFar = [&]() -> ux3d::renderer::Command {
        ux3d::renderer::Command cmd(gl, ux3d::renderer::CommandTag::Draw);
        cmd.program = &progRed;
        cmd.vertexLayout = layoutQuadFar;
        cmd.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
        cmd.primitiveCount = 6;
        cmd.primitiveOffset = 0;
        cmd.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
        cmd.indexBuffer = &quadIdxBuf;
        cmd.polygonMode = ux3d::renderer::gl::PolygonMode::Fill;
        return cmd;
    };

    auto makeDrawEqual = [&]() -> ux3d::renderer::Command {
        ux3d::renderer::Command cmd(gl, ux3d::renderer::CommandTag::Draw);
        cmd.program = &progBlue;
        cmd.vertexLayout = layoutQuadEqual;
        cmd.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
        cmd.primitiveCount = 6;
        cmd.primitiveOffset = 0;
        cmd.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
        cmd.indexBuffer = &quadIdxBuf;
        cmd.polygonMode = ux3d::renderer::gl::PolygonMode::Fill;
        return cmd;
    };

    // =========================================================================
    // SECTION 1: Actual Call Counts and State Transitions (P3.4.4)
    // =========================================================================
    if (runTransitions) {
        report << "\n--- Section 1: Call Counts and Mode Transitions ---\n";

        ux3d::renderer::CommandExecutor executor(gl);
        executor.init();

        check(!executor.getDepthTestEnabled().has_value(), "Executor depthTestEnabled is uninitialized on init");
        check(!executor.getDepthFunc().has_value(), "Executor depthFunc is uninitialized on init");

        ux3d::imgui::profiling::setCountingEnabled(true);
        const auto& counters = ux3d::imgui::profiling::frameCounters();

        const uint64_t de0 = counters.depthTestEnableCalls;
        const uint64_t df0 = counters.depthFuncCalls;

        auto cmdEnable = ux3d::renderer::Command::makeDepthTestCommand(gl, true);
        auto cmdDisable = ux3d::renderer::Command::makeDepthTestCommand(gl, false);

        // Command 1: Initial enabled request
        executor.execute(cmdEnable);
        check(counters.depthTestEnableCalls == de0 + 1, "Cmd 1 (Enabled): Initial depth test enable issued glEnable(GL_DEPTH_TEST) (enableCalls=1)");
        check(counters.depthFuncCalls == df0 + 1, "Cmd 1 (Enabled): Initial depth func issued glDepthFunc(GL_LEQUAL) (depthFuncCalls=1)");
        check(executor.getDepthTestEnabled() == true, "Cmd 1: Executor cached depthTestEnabled == true");
        check(executor.getDepthFunc() == GL_LEQUAL, "Cmd 1: Executor cached depthFunc == GL_LEQUAL");

        // Command 2: Enabled -> Enabled (redundant)
        executor.execute(cmdEnable);
        check(counters.depthTestEnableCalls == de0 + 1, "Cmd 2 (Enabled -> Enabled): Redundant depth test enable suppressed (enableCalls remains 1)");
        check(counters.depthFuncCalls == df0 + 1, "Cmd 2 (Enabled -> Enabled): Redundant depth func suppressed (depthFuncCalls remains 1)");
        check(executor.getDepthTestEnabled() == true, "Cmd 2: Executor retains depthTestEnabled == true");
        check(executor.getDepthFunc() == GL_LEQUAL, "Cmd 2: Executor retains depthFunc == GL_LEQUAL");

        // Command 3: Enabled -> Disabled
        executor.execute(cmdDisable);
        check(counters.depthTestEnableCalls == de0 + 2, "Cmd 3 (Enabled -> Disabled): Issued glDisable(GL_DEPTH_TEST) (enableCalls=2)");
        check(counters.depthFuncCalls == df0 + 1, "Cmd 3 (Enabled -> Disabled): Disabling depth test does not call glDepthFunc (depthFuncCalls remains 1)");
        check(executor.getDepthTestEnabled() == false, "Cmd 3: Executor cached depthTestEnabled == false");
        check(executor.getDepthFunc() == GL_LEQUAL, "Cmd 3: Disabling depth test retains previously established depth func in shadow state");

        // Command 4: Disabled -> Disabled (redundant)
        executor.execute(cmdDisable);
        check(counters.depthTestEnableCalls == de0 + 2, "Cmd 4 (Disabled -> Disabled): Redundant depth test disable suppressed (enableCalls remains 2)");
        check(counters.depthFuncCalls == df0 + 1, "Cmd 4 (Disabled -> Disabled): Depth func call suppressed (depthFuncCalls remains 1)");
        check(executor.getDepthTestEnabled() == false, "Cmd 4: Executor retains depthTestEnabled == false");

        // Command 5: Disabled -> Enabled (depth func was already known as GL_LEQUAL)
        executor.execute(cmdEnable);
        check(counters.depthTestEnableCalls == de0 + 3, "Cmd 5 (Disabled -> Enabled): Re-enabling depth test issued glEnable(GL_DEPTH_TEST) (enableCalls=3)");
        check(counters.depthFuncCalls == df0 + 1, "Cmd 5 (Disabled -> Enabled): Depth func already known as GL_LEQUAL, glDepthFunc suppressed (depthFuncCalls remains 1)");
        check(executor.getDepthTestEnabled() == true, "Cmd 5: Executor cached depthTestEnabled == true");
        check(executor.getDepthFunc() == GL_LEQUAL, "Cmd 5: Executor retains depthFunc == GL_LEQUAL");

        // Invalidation via resetState()
        executor.resetState();
        check(!executor.getDepthTestEnabled().has_value(), "ResetState: depthTestEnabled invalidated to nullopt");
        check(!executor.getDepthFunc().has_value(), "ResetState: depthFunc invalidated to nullopt");

        // Command 6: First command after reset with identical requested state (enabled)
        executor.execute(cmdEnable);
        check(counters.depthTestEnableCalls == de0 + 4, "Cmd 6 (Post-Reset Enabled): First draw re-issues glEnable(GL_DEPTH_TEST) (enableCalls=4)");
        check(counters.depthFuncCalls == df0 + 2, "Cmd 6 (Post-Reset Enabled): First draw re-issues glDepthFunc(GL_LEQUAL) (depthFuncCalls=2)");
        check(executor.getDepthTestEnabled() == true, "Cmd 6: Executor cached depthTestEnabled == true");
        check(executor.getDepthFunc() == GL_LEQUAL, "Cmd 6: Executor cached depthFunc == GL_LEQUAL");

        // Invalidation via resetState(), then disabled command first
        executor.resetState();
        check(!executor.getDepthTestEnabled().has_value(), "ResetState: depthTestEnabled invalidated");
        check(!executor.getDepthFunc().has_value(), "ResetState: depthFunc invalidated");

        // Command 7: Disabled command when depth func is unknown
        executor.execute(cmdDisable);
        check(counters.depthTestEnableCalls == de0 + 5, "Cmd 7 (Post-Reset Disabled): Issued glDisable(GL_DEPTH_TEST) (enableCalls=5)");
        check(counters.depthFuncCalls == df0 + 2, "Cmd 7 (Post-Reset Disabled): Did not call glDepthFunc (depthFuncCalls remains 2)");
        check(executor.getDepthTestEnabled() == false, "Cmd 7: Executor cached depthTestEnabled == false");
        check(!executor.getDepthFunc().has_value(), "Cmd 7: Initially disabled command does NOT mark depth func as known (remains nullopt)");

        // Command 8: Re-enabling after disabled when depth func was unknown
        executor.execute(cmdEnable);
        check(counters.depthTestEnableCalls == de0 + 6, "Cmd 8 (Unknown DepthFunc Re-Enabled): Issued glEnable(GL_DEPTH_TEST) (enableCalls=6)");
        check(counters.depthFuncCalls == df0 + 3, "Cmd 8 (Unknown DepthFunc Re-Enabled): Depth func was unknown, issued glDepthFunc(GL_LEQUAL) (depthFuncCalls=3)");
        check(executor.getDepthTestEnabled() == true, "Cmd 8: Executor cached depthTestEnabled == true");
        check(executor.getDepthFunc() == GL_LEQUAL, "Cmd 8: Executor cached depthFunc == GL_LEQUAL");

        // External mutation via ExternalFunction callback
        ux3d::renderer::Command extCmd(gl, ux3d::renderer::CommandTag::ExternalFunction);
        extCmd.externalFunction = []() {
            glDisable(GL_DEPTH_TEST);
            glDepthFunc(GL_GREATER);
        };
        executor.execute(extCmd);
        check(!executor.getDepthTestEnabled().has_value(), "ExternalFunction: Callback invalidated depthTestEnabled to nullopt");
        check(!executor.getDepthFunc().has_value(), "ExternalFunction: Callback invalidated depthFunc to nullopt");

        // Command 9: Restoration after external mutation
        executor.execute(cmdEnable);
        check(counters.depthTestEnableCalls == de0 + 7, "Cmd 9 (Post-Callback Restoration): Re-established glEnable(GL_DEPTH_TEST) (enableCalls=7)");
        check(counters.depthFuncCalls == df0 + 4, "Cmd 9 (Post-Callback Restoration): Re-established glDepthFunc(GL_LEQUAL) (depthFuncCalls=4)");
        check(executor.getDepthTestEnabled() == true, "Cmd 9: Executor cached depthTestEnabled == true");
        check(executor.getDepthFunc() == GL_LEQUAL, "Cmd 9: Executor cached depthFunc == GL_LEQUAL");

        GLboolean dtDriver = GL_FALSE;
        glGetBooleanv(GL_DEPTH_TEST, &dtDriver);
        check(dtDriver == GL_TRUE, "Driver query confirms GL_DEPTH_TEST == GL_TRUE");
        GLint dfDriver = 0;
        glGetIntegerv(GL_DEPTH_FUNC, &dfDriver);
        check(dfDriver == GL_LEQUAL, "Driver query confirms GL_DEPTH_FUNC == GL_LEQUAL");

        // Baseline comparison quantification:
        // Baseline issued enable/disable on every command (9 total).
        // Baseline issued glDepthFunc(GL_LEQUAL) on every enabled command (Commands 1, 2, 5, 6, 8, 9 = 6 total).
        // Total baseline calls: 9 + 6 = 15.
        // Actual calls with cache: 7 enable/disable + 4 depthFunc = 11.
        // Calls suppressed: (9 - 7) + (6 - 4) = 2 + 2 = 4 calls suppressed (26.7% reduction).
        const uint64_t actualTotal = (counters.depthTestEnableCalls - de0) + (counters.depthFuncCalls - df0);
        check(actualTotal == 11, "Actual wrapper calls total 11 (7 enable/disable + 4 depthFunc) vs 15 baseline");
    }

    // =========================================================================
    // SECTION 2: Depth Correctness with Real FBO and Pixel Readback
    // =========================================================================
    if (runCorrectness) {
        report << "\n--- Section 2: Depth Correctness with Real FBO ---\n";

        ux3d::renderer::CommandExecutor executor(gl);
        executor.init();

        auto cmdEnable = ux3d::renderer::Command::makeDepthTestCommand(gl, true);
        auto cmdDisable = ux3d::renderer::Command::makeDepthTestCommand(gl, false);

        // Pass A: Depth test enabled. Draw Near (Green, Z=0.0) then Far (Red, Z=0.5).
        // Expected: Near Green remains visible; Far Red is rejected (0.5 <= 0.0 is False).
        clearFbo();
        executor.execute(cmdEnable);
        executor.execute(makeDrawNear());
        executor.execute(makeDrawFar());
        auto pxA = readCenterPixel();
        check(pxA[0] == 0 && pxA[1] == 255 && pxA[2] == 0 && pxA[3] == 255,
              "Pass A (Depth Enabled): Nearer geometry remains visible (Green [0, 255, 0, 255])");

        // Pass B: Depth test disabled. Draw Near (Green, Z=0.0) then Far (Red, Z=0.5).
        // Expected: Without depth testing, Far Red overwrites Near Green.
        clearFbo();
        executor.execute(cmdDisable);
        executor.execute(makeDrawNear());
        executor.execute(makeDrawFar());
        auto pxB = readCenterPixel();
        check(pxB[0] == 255 && pxB[1] == 0 && pxB[2] == 0 && pxB[3] == 255,
              "Pass B (Depth Disabled): Farther geometry overwrites earlier geometry (Red [255, 0, 0, 255])");

        // Pass C: Depth test re-enabled. Draw Near (Green, Z=0.0) then Far (Red, Z=0.5).
        // Expected: Near Green remains visible; occlusion correctly restored.
        clearFbo();
        executor.execute(cmdEnable);
        executor.execute(makeDrawNear());
        executor.execute(makeDrawFar());
        auto pxC = readCenterPixel();
        check(pxC[0] == 0 && pxC[1] == 255 && pxC[2] == 0 && pxC[3] == 255,
              "Pass C (Depth Re-Enabled): Occlusion restored, Nearer geometry remains visible (Green [0, 255, 0, 255])");

        // Pass D: Equal-depth geometry under GL_LEQUAL.
        // Draw Near (Green, Z=0.0) then Equal (Blue, Z=0.0).
        // Under GL_LEQUAL, 0.0 <= 0.0 is TRUE! Equal geometry passes and overwrites Green.
        // (Under GL_LESS, 0.0 < 0.0 would be False and Green would remain).
        clearFbo();
        executor.execute(cmdEnable);
        executor.execute(makeDrawNear());
        executor.execute(makeDrawEqual());
        auto pxD = readCenterPixel();
        check(pxD[0] == 0 && pxD[1] == 0 && pxD[2] == 255 && pxD[3] == 255,
              "Pass D (Equal Depth GL_LEQUAL): Equal-depth geometry passes and overwrites (Blue [0, 0, 255, 255])");

        // Pass E: Repeated requests preserve output while calls are suppressed.
        clearFbo();
        executor.execute(cmdEnable); // redundant, suppressed
        executor.execute(makeDrawNear());
        executor.execute(cmdEnable); // redundant, suppressed
        executor.execute(makeDrawFar());
        auto pxE = readCenterPixel();
        check(pxE[0] == 0 && pxE[1] == 255 && pxE[2] == 0 && pxE[3] == 255,
              "Pass E (Suppressed Redundant Commands): Output preserved identically (Green [0, 255, 0, 255])");
    }

    // =========================================================================
    // SECTION 3: Boundary Isolation and Restoration
    // =========================================================================
    if (runBoundary) {
        report << "\n--- Section 3: Boundary Isolation ---\n";

        ux3d::renderer::CommandExecutor executor(gl);
        executor.init();

        auto cmdEnable = ux3d::renderer::Command::makeDepthTestCommand(gl, true);

        // 1. ExternalFunction boundary
        clearFbo();
        executor.execute(cmdEnable);
        executor.execute(makeDrawNear());

        ux3d::renderer::Command extCmd(gl, ux3d::renderer::CommandTag::ExternalFunction);
        extCmd.externalFunction = []() {
            glDisable(GL_DEPTH_TEST);
            glDepthFunc(GL_GREATER);
        };
        executor.execute(extCmd);

        check(!executor.getDepthTestEnabled().has_value(), "Boundary: ExternalFunction invalidated depthTestEnabled to nullopt");
        check(!executor.getDepthFunc().has_value(), "Boundary: ExternalFunction invalidated depthFunc to nullopt");

        // Post-callback draw with DepthTest(true) command
        executor.execute(cmdEnable);
        executor.execute(makeDrawFar());

        GLboolean dtPostExt = GL_FALSE;
        glGetBooleanv(GL_DEPTH_TEST, &dtPostExt);
        check(dtPostExt == GL_TRUE, "Boundary: GL_DEPTH_TEST restored to GL_TRUE after ExternalFunction");

        GLint dfPostExt = 0;
        glGetIntegerv(GL_DEPTH_FUNC, &dfPostExt);
        check(dfPostExt == GL_LEQUAL, "Boundary: GL_DEPTH_FUNC restored to GL_LEQUAL after ExternalFunction");

        auto pxExt = readCenterPixel();
        check(pxExt[0] == 0 && pxExt[1] == 255 && pxExt[2] == 0 && pxExt[3] == 255,
              "Boundary: Nearer Green preserved after callback restoration, Far Red rejected");

        // 2. Inter-frame Renderer::draw boundary
        if (reloadAndDraw) {
            bool ok = reloadAndDraw("resources/sampleGltf/HelloCube.glb");
            if (!ok) {
                ok = reloadAndDraw("HelloCube.glb");
            }
            check(ok, "Boundary: First model reload and Renderer::draw succeeded");

            // Direct external corruption between renderer draws
            glDisable(GL_DEPTH_TEST);
            glDepthFunc(GL_ALWAYS);

            bool drawOk2 = glRenderer->draw({ commandGenerator });
            check(drawOk2, "Renderer::draw post-corruption frame succeeded");

            GLboolean dtPostRen = GL_FALSE;
            glGetBooleanv(GL_DEPTH_TEST, &dtPostRen);
            check(dtPostRen == GL_TRUE, "Renderer::draw restored GL_DEPTH_TEST to GL_TRUE");

            GLint dfPostRen = 0;
            glGetIntegerv(GL_DEPTH_FUNC, &dfPostRen);
            check(dfPostRen == GL_LEQUAL, "Renderer::draw restored GL_DEPTH_FUNC to GL_LEQUAL");

            GLint samplerBinding = -1;
            glGetIntegerv(GL_SAMPLER_BINDING, &samplerBinding);
            check(samplerBinding == 0, "Renderer::draw boundary leaves sampler 0 unbound");

            const GLenum err = glGetError();
            check(err == GL_NO_ERROR, "No GL errors across Renderer::draw boundaries");
        }
    }

    // =========================================================================
    // SECTION 4: Application Integration Case (HelloCube.glb)
    // =========================================================================
    if (runIntegration) {
        report << "\n--- Section 4: Application Integration Case ---\n";

        if (reloadAndDraw) {
            bool ok = reloadAndDraw("resources/sampleGltf/HelloCube.glb");
            if (!ok) {
                // Try fallback relative path
                ok = reloadAndDraw("HelloCube.glb");
            }
            check(ok, "Loaded and rendered HelloCube.glb cleanly");

            GLboolean dt = GL_FALSE;
            glGetBooleanv(GL_DEPTH_TEST, &dt);
            check(dt == GL_TRUE, "Post-draw driver state confirms GL_DEPTH_TEST == GL_TRUE");

            GLint df = 0;
            glGetIntegerv(GL_DEPTH_FUNC, &df);
            check(df == GL_LEQUAL, "Post-draw driver state confirms GL_DEPTH_FUNC == GL_LEQUAL");

            GLint samplerBinding = -1;
            glGetIntegerv(GL_SAMPLER_BINDING, &samplerBinding);
            check(samplerBinding == 0, "HelloCube draw leaves sampler 0 unbound");

            const GLenum err = glGetError();
            check(err == GL_NO_ERROR, "No GL errors during application integration draw");
        }
    }

    // -------------------------------------------------------------
    // Cleanup
    // -------------------------------------------------------------
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &colorTex);
    glDeleteRenderbuffers(1, &depthRbo);
    // Program wrappers own their handles and delete them on scope exit.
    glDeleteShader(vs);
    glDeleteShader(fsGreen);
    glDeleteShader(fsRed);
    glDeleteShader(fsBlue);

    report << "\nTotal assertions evaluated: " << checks << "\n";
    report << "Failed assertions: " << failures.size() << "\n";
    report << "Result: " << (result.success ? "PASS" : "FAIL") << "\n";
    result.report = report.str();
    return result;
}

} // namespace ux3d::imgui
