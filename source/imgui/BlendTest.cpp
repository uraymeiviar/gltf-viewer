// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/BlendTest.h"
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
#include "renderer/gltf/GltfResourceManager.h"

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

SelfCheckResult runBlendSelfCheck(
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
    report << "P3.4.5 CommandExecutor Blending State Cache self-check\n";
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
        check(false, "forced blend self-check failure requested");
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
    const bool runIntegration = (filter == "all" || filter == "integration" || filter == "gltf" || filter == "combined");

    // -------------------------------------------------------------
    // Shaders & Shared Geometry Fixtures
    // -------------------------------------------------------------
    const char* vsSrc =
        "#version 330 core\n"
        "layout(location = 0) in vec3 a_position;\n"
        "void main() {\n"
        "    gl_Position = vec4(a_position, 1.0);\n"
        "}\n";

    // Opaque Green
    const char* fsOpaqueGreenSrc =
        "#version 330 core\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    fragColor = vec4(0.0, 1.0, 0.0, 1.0);\n"
        "}\n";

    // Half-Transparent Red (50% Alpha)
    const char* fsTransRedSrc =
        "#version 330 core\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    fragColor = vec4(1.0, 0.0, 0.0, 0.5);\n"
        "}\n";

    // Half-Transparent Blue (50% Alpha)
    const char* fsTransBlueSrc =
        "#version 330 core\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    fragColor = vec4(0.0, 0.0, 1.0, 0.5);\n"
        "}\n";

    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fsOpaqueGreen = compileShader(GL_FRAGMENT_SHADER, fsOpaqueGreenSrc);
    GLuint fsTransRed = compileShader(GL_FRAGMENT_SHADER, fsTransRedSrc);
    GLuint fsTransBlue = compileShader(GL_FRAGMENT_SHADER, fsTransBlueSrc);

    GLuint progGreenHandle = linkProgram(vs, fsOpaqueGreen);
    GLuint progTransRedHandle = linkProgram(vs, fsTransRed);
    GLuint progTransBlueHandle = linkProgram(vs, fsTransBlue);

    check(progGreenHandle != 0, "Opaque green shader program linked");
    check(progTransRedHandle != 0, "Transparent red shader program linked");
    check(progTransBlueHandle != 0, "Transparent blue shader program linked");

    ux3d::renderer::gl::Program progGreen(gl, progGreenHandle);
    ux3d::renderer::gl::Program progTransRed(gl, progTransRedHandle);
    ux3d::renderer::gl::Program progTransBlue(gl, progTransBlueHandle);

    // Quad Near at Z = 0.0f
    std::vector<float> quadMidPos = {
        -0.8f, -0.8f, 0.0f,
         0.8f, -0.8f, 0.0f,
         0.8f,  0.8f, 0.0f,
        -0.8f,  0.8f, 0.0f
    };
    auto quadMidPosBuf = makeGlBuffer(gl, quadMidPos);
    std::vector<uint16_t> quadIndices = { 0, 1, 2, 0, 2, 3 };
    auto quadIdxBuf = makeGlBuffer(gl, quadIndices);

    ux3d::renderer::Mesh meshQuadMid(gl);
    ux3d::renderer::gl::Attribute attrMid("a_position");
    attrMid.buffer = &quadMidPosBuf;
    attrMid.dimension = 3;
    attrMid.type = ux3d::renderer::gl::ComponentType::Float;
    meshQuadMid.attributes.push_back(attrMid);
    meshQuadMid.indexBuffer = &quadIdxBuf;
    meshQuadMid.indexCount = 6;
    meshQuadMid.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
    meshQuadMid.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
    std::shared_ptr<ux3d::renderer::gl::VertexLayout> layoutQuadMid = progTransRed.createVertexLayout(meshQuadMid.attributes);
    check(layoutQuadMid != nullptr && layoutQuadMid->handle != 0, "Quad vertex layout created");

    // Quad Front at Z = -0.2f
    std::vector<float> quadFrontPos = {
        -0.8f, -0.8f, -0.2f,
         0.8f, -0.8f, -0.2f,
         0.8f,  0.8f, -0.2f,
        -0.8f,  0.8f, -0.2f
    };
    auto quadFrontPosBuf = makeGlBuffer(gl, quadFrontPos);
    ux3d::renderer::Mesh meshQuadFront(gl);
    ux3d::renderer::gl::Attribute attrFront("a_position");
    attrFront.buffer = &quadFrontPosBuf;
    attrFront.dimension = 3;
    attrFront.type = ux3d::renderer::gl::ComponentType::Float;
    meshQuadFront.attributes.push_back(attrFront);
    meshQuadFront.indexBuffer = &quadIdxBuf;
    meshQuadFront.indexCount = 6;
    meshQuadFront.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
    meshQuadFront.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
    std::shared_ptr<ux3d::renderer::gl::VertexLayout> layoutQuadFront = progTransRed.createVertexLayout(meshQuadFront.attributes);

    // Quad Back at Z = 0.5f
    std::vector<float> quadBackPos = {
        -0.8f, -0.8f, 0.5f,
         0.8f, -0.8f, 0.5f,
         0.8f,  0.8f, 0.5f,
        -0.8f,  0.8f, 0.5f
    };
    auto quadBackPosBuf = makeGlBuffer(gl, quadBackPos);
    ux3d::renderer::Mesh meshQuadBack(gl);
    ux3d::renderer::gl::Attribute attrBack("a_position");
    attrBack.buffer = &quadBackPosBuf;
    attrBack.dimension = 3;
    attrBack.type = ux3d::renderer::gl::ComponentType::Float;
    meshQuadBack.attributes.push_back(attrBack);
    meshQuadBack.indexBuffer = &quadIdxBuf;
    meshQuadBack.indexCount = 6;
    meshQuadBack.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
    meshQuadBack.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
    std::shared_ptr<ux3d::renderer::gl::VertexLayout> layoutQuadBack = progTransBlue.createVertexLayout(meshQuadBack.attributes);

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
    check(fboStatus == GL_FRAMEBUFFER_COMPLETE, "Offscreen RGBA8 + Depth FBO is complete");

    auto clearFboSolidBlue = [&]() {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, 64, 64);
        glDepthMask(GL_TRUE);
        glClearColor(0.0f, 0.0f, 1.0f, 1.0f); // Solid Blue
        glClearDepth(1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    };

    auto clearFboBlack = [&]() {
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

    auto makeDrawTransRed = [&]() -> ux3d::renderer::Command {
        ux3d::renderer::Command cmd(gl, ux3d::renderer::CommandTag::Draw);
        cmd.program = &progTransRed;
        cmd.vertexLayout = layoutQuadMid;
        cmd.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
        cmd.primitiveCount = 6;
        cmd.primitiveOffset = 0;
        cmd.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
        cmd.indexBuffer = &quadIdxBuf;
        cmd.polygonMode = ux3d::renderer::gl::PolygonMode::Fill;
        return cmd;
    };

    // =========================================================================
    // SECTION 1: Actual Call Counts and State Transitions (P3.4.5)
    // =========================================================================
    if (runTransitions) {
        report << "\n--- Section 1: Call Counts and Mode Transitions ---\n";

        ux3d::renderer::CommandExecutor executor(gl);
        executor.init();

        check(!executor.getBlendEnabled().has_value(), "Executor blendEnabled is uninitialized on init");
        check(!executor.getBlendSrcFactor().has_value(), "Executor blendSrcFactor is uninitialized on init");
        check(!executor.getBlendDstFactor().has_value(), "Executor blendDstFactor is uninitialized on init");
        check(!executor.getBlendEquation().has_value(), "Executor blendEquation is uninitialized on init");

        ux3d::imgui::profiling::setCountingEnabled(true);
        const auto& counters = ux3d::imgui::profiling::frameCounters();

        const uint64_t be0 = counters.blendEnableCalls;
        const uint64_t bf0 = counters.blendFuncCalls;
        const uint64_t bq0 = counters.blendEquationCalls;

        auto cmdEnable = ux3d::renderer::Command::makeBlendCommand(gl, true);
        auto cmdDisable = ux3d::renderer::Command::makeBlendCommand(gl, false);

        // Command 1: Initial enabled request
        executor.execute(cmdEnable);
        check(counters.blendEnableCalls == be0 + 1, "Cmd 1 (Enabled): Initial blend enable issued glEnable(GL_BLEND) (enableCalls=1)");
        check(counters.blendFuncCalls == bf0 + 1, "Cmd 1 (Enabled): Initial blend factors issued glBlendFunc (funcCalls=1)");
        check(counters.blendEquationCalls == bq0 + 1, "Cmd 1 (Enabled): Initial blend equation issued glBlendEquation (equationCalls=1)");
        check(executor.getBlendEnabled() == true, "Cmd 1: Executor cached blendEnabled == true");
        check(executor.getBlendSrcFactor() == GL_SRC_ALPHA, "Cmd 1: Executor cached blendSrcFactor == GL_SRC_ALPHA");
        check(executor.getBlendDstFactor() == GL_ONE_MINUS_SRC_ALPHA, "Cmd 1: Executor cached blendDstFactor == GL_ONE_MINUS_SRC_ALPHA");
        check(executor.getBlendEquation() == GL_FUNC_ADD, "Cmd 1: Executor cached blendEquation == GL_FUNC_ADD");

        // Command 2: Enabled -> Enabled (redundant)
        executor.execute(cmdEnable);
        check(counters.blendEnableCalls == be0 + 1, "Cmd 2 (Enabled -> Enabled): Redundant blend enable suppressed (enableCalls remains 1)");
        check(counters.blendFuncCalls == bf0 + 1, "Cmd 2 (Enabled -> Enabled): Redundant blend factors suppressed (funcCalls remains 1)");
        check(counters.blendEquationCalls == bq0 + 1, "Cmd 2 (Enabled -> Enabled): Redundant blend equation suppressed (equationCalls remains 1)");
        check(executor.getBlendEnabled() == true, "Cmd 2: Executor retains blendEnabled == true");

        // Command 3: Enabled -> Disabled
        executor.execute(cmdDisable);
        check(counters.blendEnableCalls == be0 + 2, "Cmd 3 (Enabled -> Disabled): Issued glDisable(GL_BLEND) (enableCalls=2)");
        check(counters.blendFuncCalls == bf0 + 1, "Cmd 3 (Enabled -> Disabled): Disabling blend does not call glBlendFunc (funcCalls remains 1)");
        check(counters.blendEquationCalls == bq0 + 1, "Cmd 3 (Enabled -> Disabled): Disabling blend does not call glBlendEquation (equationCalls remains 1)");
        check(executor.getBlendEnabled() == false, "Cmd 3: Executor cached blendEnabled == false");
        check(executor.getBlendSrcFactor() == GL_SRC_ALPHA, "Cmd 3: Factors retained in cache while disabled");
        check(executor.getBlendEquation() == GL_FUNC_ADD, "Cmd 3: Equation retained in cache while disabled");

        // Command 4: Disabled -> Disabled (redundant)
        executor.execute(cmdDisable);
        check(counters.blendEnableCalls == be0 + 2, "Cmd 4 (Disabled -> Disabled): Redundant blend disable suppressed (enableCalls remains 2)");
        check(counters.blendFuncCalls == bf0 + 1, "Cmd 4 (Disabled -> Disabled): Factor call suppressed (funcCalls remains 1)");
        check(counters.blendEquationCalls == bq0 + 1, "Cmd 4 (Disabled -> Disabled): Equation call suppressed (equationCalls remains 1)");
        check(executor.getBlendEnabled() == false, "Cmd 4: Executor retains blendEnabled == false");

        // Command 5: Disabled -> Enabled (factors and equation already known)
        executor.execute(cmdEnable);
        check(counters.blendEnableCalls == be0 + 3, "Cmd 5 (Disabled -> Enabled): Re-enabling blend issued glEnable(GL_BLEND) (enableCalls=3)");
        check(counters.blendFuncCalls == bf0 + 1, "Cmd 5 (Disabled -> Enabled): Factors already match cache, suppressed glBlendFunc (funcCalls remains 1)");
        check(counters.blendEquationCalls == bq0 + 1, "Cmd 5 (Disabled -> Enabled): Equation already matches cache, suppressed glBlendEquation (equationCalls remains 1)");
        check(executor.getBlendEnabled() == true, "Cmd 5: Executor cached blendEnabled == true");

        // Invalidation via resetState()
        executor.resetState();
        check(!executor.getBlendEnabled().has_value(), "ResetState: blendEnabled invalidated to nullopt");
        check(!executor.getBlendSrcFactor().has_value(), "ResetState: blendSrcFactor invalidated to nullopt");
        check(!executor.getBlendDstFactor().has_value(), "ResetState: blendDstFactor invalidated to nullopt");
        check(!executor.getBlendEquation().has_value(), "ResetState: blendEquation invalidated to nullopt");

        // Command 6: First command after reset with identical requested state (enabled)
        executor.execute(cmdEnable);
        check(counters.blendEnableCalls == be0 + 4, "Cmd 6 (Post-Reset Enabled): First draw re-issues glEnable(GL_BLEND) (enableCalls=4)");
        check(counters.blendFuncCalls == bf0 + 2, "Cmd 6 (Post-Reset Enabled): First draw re-issues glBlendFunc (funcCalls=2)");
        check(counters.blendEquationCalls == bq0 + 2, "Cmd 6 (Post-Reset Enabled): First draw re-issues glBlendEquation (equationCalls=2)");
        check(executor.getBlendEnabled() == true, "Cmd 6: Executor cached blendEnabled == true");

        // Invalidation via resetState(), then disabled command first
        executor.resetState();
        check(!executor.getBlendEnabled().has_value(), "ResetState: blendEnabled invalidated");
        check(!executor.getBlendSrcFactor().has_value(), "ResetState: blendSrcFactor invalidated");

        // Command 7: Disabled command when factors/equation are unknown
        executor.execute(cmdDisable);
        check(counters.blendEnableCalls == be0 + 5, "Cmd 7 (Post-Reset Disabled): Issued glDisable(GL_BLEND) (enableCalls=5)");
        check(counters.blendFuncCalls == bf0 + 2, "Cmd 7 (Post-Reset Disabled): Did not call glBlendFunc (funcCalls remains 2)");
        check(counters.blendEquationCalls == bq0 + 2, "Cmd 7 (Post-Reset Disabled): Did not call glBlendEquation (equationCalls remains 2)");
        check(executor.getBlendEnabled() == false, "Cmd 7: Executor cached blendEnabled == false");
        check(!executor.getBlendSrcFactor().has_value(), "Cmd 7: Initially disabled command does NOT mark factors as known (remains nullopt)");
        check(!executor.getBlendEquation().has_value(), "Cmd 7: Initially disabled command does NOT mark equation as known (remains nullopt)");

        // Command 8: Re-enabling after disabled when factors/equation were unknown
        executor.execute(cmdEnable);
        check(counters.blendEnableCalls == be0 + 6, "Cmd 8 (Unknown Factors Re-Enabled): Issued glEnable(GL_BLEND) (enableCalls=6)");
        check(counters.blendFuncCalls == bf0 + 3, "Cmd 8 (Unknown Factors Re-Enabled): Factors were unknown, issued glBlendFunc (funcCalls=3)");
        check(counters.blendEquationCalls == bq0 + 3, "Cmd 8 (Unknown Factors Re-Enabled): Equation was unknown, issued glBlendEquation (equationCalls=3)");
        check(executor.getBlendEnabled() == true, "Cmd 8: Executor cached blendEnabled == true");

        // External mutation via ExternalFunction callback
        ux3d::renderer::Command extCmd(gl, ux3d::renderer::CommandTag::ExternalFunction);
        extCmd.externalFunction = []() {
            glDisable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            glBlendEquation(GL_FUNC_SUBTRACT);
        };
        executor.execute(extCmd);
        check(!executor.getBlendEnabled().has_value(), "ExternalFunction: Callback invalidated blendEnabled to nullopt");
        check(!executor.getBlendSrcFactor().has_value(), "ExternalFunction: Callback invalidated blendSrcFactor to nullopt");
        check(!executor.getBlendEquation().has_value(), "ExternalFunction: Callback invalidated blendEquation to nullopt");

        // Command 9: Restoration after external mutation
        executor.execute(cmdEnable);
        check(counters.blendEnableCalls == be0 + 7, "Cmd 9 (Post-Callback Restoration): Re-established glEnable(GL_BLEND) (enableCalls=7)");
        check(counters.blendFuncCalls == bf0 + 4, "Cmd 9 (Post-Callback Restoration): Re-established glBlendFunc (funcCalls=4)");
        check(counters.blendEquationCalls == bq0 + 4, "Cmd 9 (Post-Callback Restoration): Re-established glBlendEquation (equationCalls=4)");
        check(executor.getBlendEnabled() == true, "Cmd 9: Executor cached blendEnabled == true");

        GLboolean blDriver = GL_FALSE;
        glGetBooleanv(GL_BLEND, &blDriver);
        check(blDriver == GL_TRUE, "Driver query confirms GL_BLEND == GL_TRUE");
        GLint sFactorDriver = 0, dFactorDriver = 0;
        glGetIntegerv(GL_BLEND_SRC_RGB, &sFactorDriver);
        glGetIntegerv(GL_BLEND_DST_RGB, &dFactorDriver);
        check(sFactorDriver == GL_SRC_ALPHA && dFactorDriver == GL_ONE_MINUS_SRC_ALPHA,
              "Driver query confirms GL_BLEND_SRC_RGB == GL_SRC_ALPHA and GL_BLEND_DST_RGB == GL_ONE_MINUS_SRC_ALPHA");
        GLint eqDriver = 0;
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &eqDriver);
        check(eqDriver == GL_FUNC_ADD, "Driver query confirms GL_BLEND_EQUATION_RGB == GL_FUNC_ADD");

        // Baseline comparison quantification:
        // Baseline issued enable/disable on every command (9 total).
        // Baseline issued blendFunc on every enabled command (Commands 1, 2, 5, 6, 8, 9 = 6 total).
        // Baseline issued blendEquation on every enabled command (6 total).
        // Total baseline calls: 9 + 6 + 6 = 21.
        // Actual calls with cache: 7 enable/disable + 4 blendFunc + 4 blendEquation = 15.
        // Calls suppressed: (9 - 7) + (6 - 4) + (6 - 4) = 2 + 2 + 2 = 6 calls suppressed (28.6% reduction).
        const uint64_t actualTotal = (counters.blendEnableCalls - be0) + (counters.blendFuncCalls - bf0) + (counters.blendEquationCalls - bq0);
        check(actualTotal == 15, "Actual wrapper calls total 15 (7 enable/disable + 4 func + 4 equation) vs 21 baseline");
    }

    // =========================================================================
    // SECTION 2: Pixel Correctness with Real RGBA FBO (P3.4.5)
    // =========================================================================
    if (runCorrectness) {
        report << "\n--- Section 2: Pixel Correctness with Real RGBA FBO ---\n";

        ux3d::renderer::CommandExecutor executor(gl);
        executor.init();

        auto cmdEnable = ux3d::renderer::Command::makeBlendCommand(gl, true);
        auto cmdDisable = ux3d::renderer::Command::makeBlendCommand(gl, false);

        // Pass A: Blending enabled over solid blue [0, 0, 255, 255].
        // Source is 50% red [255, 0, 0, 128].
        // Expected blended output:
        // R = 1.0 * 0.5 + 0.0 * 0.5 = 0.5 -> ~128
        // G = 0
        // B = 0.0 * 0.5 + 1.0 * 0.5 = 0.5 -> ~128
        // A = 0.5 * 0.5 + 1.0 * 0.5 = 0.75 -> ~191
        clearFboSolidBlue();
        executor.execute(cmdEnable);
        executor.execute(makeDrawTransRed());
        auto pxA = readCenterPixel();
        check(pxA[0] >= 126 && pxA[0] <= 130 && pxA[1] == 0 && pxA[2] >= 126 && pxA[2] <= 130 && pxA[3] >= 190 && pxA[3] <= 193,
              "Pass A (Blend Enabled): Alpha blending produced expected violet [128, 0, 128, 191]");

        // Pass B: Blending disabled over solid blue.
        // Source should unconditionally overwrite destination:
        // R = 255, G = 0, B = 0, A = 128
        clearFboSolidBlue();
        executor.execute(cmdDisable);
        executor.execute(makeDrawTransRed());
        auto pxB = readCenterPixel();
        check(pxB[0] == 255 && pxB[1] == 0 && pxB[2] == 0 && pxB[3] >= 126 && pxB[3] <= 130,
              "Pass B (Blend Disabled): Source overwrites destination [255, 0, 0, 128]");

        // Pass C: Blending re-enabled over solid blue.
        // Expected blended violet output restored.
        clearFboSolidBlue();
        executor.execute(cmdEnable);
        executor.execute(makeDrawTransRed());
        auto pxC = readCenterPixel();
        check(pxC[0] >= 126 && pxC[0] <= 130 && pxC[1] == 0 && pxC[2] >= 126 && pxC[2] <= 130 && pxC[3] >= 190 && pxC[3] <= 193,
              "Pass C (Blend Re-Enabled): Blending restored violet [128, 0, 128, 191]");

        // Pass D: Repeated requests preserve output while calls are suppressed.
        clearFboSolidBlue();
        executor.execute(cmdEnable); // redundant, suppressed
        executor.execute(makeDrawTransRed());
        auto pxD = readCenterPixel();
        check(pxD[0] >= 126 && pxD[0] <= 130 && pxD[1] == 0 && pxD[2] >= 126 && pxD[2] <= 130 && pxD[3] >= 190 && pxD[3] <= 193,
              "Pass D (Suppressed Redundant Commands): Output preserved identically [128, 0, 128, 191]");

        // Pass E: Restoration after external factor/equation corruption.
        glBlendFunc(GL_ONE, GL_ONE);
        glBlendEquation(GL_FUNC_SUBTRACT);
        ux3d::renderer::Command extCmd(gl, ux3d::renderer::CommandTag::ExternalFunction);
        extCmd.externalFunction = []() {};
        executor.execute(extCmd); // invalidates cache

        clearFboSolidBlue();
        executor.execute(cmdEnable);
        executor.execute(makeDrawTransRed());
        auto pxE = readCenterPixel();
        check(pxE[0] >= 126 && pxE[0] <= 130 && pxE[1] == 0 && pxE[2] >= 126 && pxE[2] <= 130 && pxE[3] >= 190 && pxE[3] <= 193,
              "Pass E (Post-Corruption Restoration): Blending factors/equation restored violet [128, 0, 128, 191]");
    }

    // =========================================================================
    // SECTION 3: Boundary Isolation and Restoration
    // =========================================================================
    if (runBoundary) {
        report << "\n--- Section 3: Boundary Isolation ---\n";

        ux3d::renderer::CommandExecutor executor(gl);
        executor.init();

        auto cmdEnable = ux3d::renderer::Command::makeBlendCommand(gl, true);

        // 1. ExternalFunction boundary
        clearFboSolidBlue();
        executor.execute(cmdEnable);
        executor.execute(makeDrawTransRed());

        ux3d::renderer::Command extCmd(gl, ux3d::renderer::CommandTag::ExternalFunction);
        extCmd.externalFunction = []() {
            glDisable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            glBlendEquation(GL_FUNC_SUBTRACT);
        };
        executor.execute(extCmd);

        check(!executor.getBlendEnabled().has_value(), "Boundary: ExternalFunction invalidated blendEnabled to nullopt");
        check(!executor.getBlendSrcFactor().has_value(), "Boundary: ExternalFunction invalidated blendSrcFactor to nullopt");
        check(!executor.getBlendEquation().has_value(), "Boundary: ExternalFunction invalidated blendEquation to nullopt");

        // Post-callback draw with Blend(true) command
        clearFboSolidBlue();
        executor.execute(cmdEnable);
        executor.execute(makeDrawTransRed());

        GLboolean blPostExt = GL_FALSE;
        glGetBooleanv(GL_BLEND, &blPostExt);
        check(blPostExt == GL_TRUE, "Boundary: GL_BLEND restored to GL_TRUE after ExternalFunction");

        GLint sFactorPostExt = 0, dFactorPostExt = 0;
        glGetIntegerv(GL_BLEND_SRC_RGB, &sFactorPostExt);
        glGetIntegerv(GL_BLEND_DST_RGB, &dFactorPostExt);
        check(sFactorPostExt == GL_SRC_ALPHA && dFactorPostExt == GL_ONE_MINUS_SRC_ALPHA,
              "Boundary: Blend factors restored to GL_SRC_ALPHA and GL_ONE_MINUS_SRC_ALPHA");

        GLint eqPostExt = 0;
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &eqPostExt);
        check(eqPostExt == GL_FUNC_ADD, "Boundary: Blend equation restored to GL_FUNC_ADD");

        auto pxExt = readCenterPixel();
        check(pxExt[0] >= 126 && pxExt[0] <= 130 && pxExt[2] >= 126 && pxExt[2] <= 130,
              "Boundary: Meaningful blended pixel output verified after callback restoration");

        // 2. Inter-frame Renderer::draw boundary
        if (reloadAndDraw) {
            bool ok = reloadAndDraw("resources/sampleGltf/HelloCube.glb");
            if (!ok) {
                ok = reloadAndDraw("HelloCube.glb");
            }
            check(ok, "Boundary: First model reload and Renderer::draw succeeded");

            // Direct external corruption between renderer draws
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);

            bool drawOk2 = glRenderer->draw({ commandGenerator });
            check(drawOk2, "Renderer::draw post-corruption frame succeeded");

            GLboolean blPostRen = GL_TRUE;
            glGetBooleanv(GL_BLEND, &blPostRen);
            check(blPostRen == GL_FALSE, "Renderer::draw restored GL_BLEND to GL_FALSE (Opaque pass default)");

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
                ok = reloadAndDraw("HelloCube.glb");
            }
            check(ok, "Loaded and rendered HelloCube.glb cleanly");

            GLboolean bl = GL_TRUE;
            glGetBooleanv(GL_BLEND, &bl);
            check(bl == GL_FALSE, "Post-draw driver state confirms GL_BLEND == GL_FALSE");

            GLint samplerBinding = -1;
            glGetIntegerv(GL_SAMPLER_BINDING, &samplerBinding);
            check(samplerBinding == 0, "HelloCube draw leaves sampler 0 unbound");

            const GLenum err = glGetError();
            check(err == GL_NO_ERROR, "No GL errors during application integration draw");
        }
    }

    // =========================================================================
    // SECTION 5: Combined P3.4 Integration Scenarios (PART B)
    // =========================================================================
    if (runIntegration) {
        report << "\n--- Section 5: Combined P3.4 Integration Scenarios ---\n";

        ux3d::renderer::CommandExecutor executor(gl);
        executor.init();

        // Scenario 1: Opaque & Transparent Geometry Overlap with Depth & Blend
        // Step 1: Draw Opaque Quad Near (Green, Z=0.0). DepthTest=True, Blend=False, Cull=Back, Winding=CCW, Poly=Fill.
        clearFboBlack();
        auto cmdDepthTrue = ux3d::renderer::Command::makeDepthTestCommand(gl, true);
        auto cmdBlendFalse = ux3d::renderer::Command::makeBlendCommand(gl, false);
        auto cmdBlendTrue = ux3d::renderer::Command::makeBlendCommand(gl, true);

        ux3d::renderer::Command cmdOpaque(gl, ux3d::renderer::CommandTag::Draw);
        cmdOpaque.program = &progGreen;
        cmdOpaque.vertexLayout = layoutQuadMid;
        cmdOpaque.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
        cmdOpaque.primitiveCount = 6;
        cmdOpaque.primitiveOffset = 0;
        cmdOpaque.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
        cmdOpaque.indexBuffer = &quadIdxBuf;
        cmdOpaque.polygonMode = ux3d::renderer::gl::PolygonMode::Fill;
        cmdOpaque.culling = ux3d::renderer::gl::Culling::Back;
        cmdOpaque.winding = ux3d::renderer::gl::Winding::CounterClockwise;

        executor.execute(cmdDepthTrue);
        executor.execute(cmdBlendFalse);
        executor.execute(cmdOpaque);

        auto pxOpaque = readCenterPixel();
        check(pxOpaque[0] == 0 && pxOpaque[1] == 255 && pxOpaque[2] == 0,
              "Combined Scenario 1: Opaque pass drew solid green [0, 255, 0, 255]");

        // Step 2: Draw Transparent Quad Front (Red, Z=-0.2f, in front of green). DepthTest=True, Blend=True.
        ux3d::renderer::Command cmdTransFront(gl, ux3d::renderer::CommandTag::Draw);
        cmdTransFront.program = &progTransRed;
        cmdTransFront.vertexLayout = layoutQuadFront;
        cmdTransFront.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
        cmdTransFront.primitiveCount = 6;
        cmdTransFront.primitiveOffset = 0;
        cmdTransFront.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
        cmdTransFront.indexBuffer = &quadIdxBuf;
        cmdTransFront.polygonMode = ux3d::renderer::gl::PolygonMode::Fill;
        cmdTransFront.culling = ux3d::renderer::gl::Culling::None;
        cmdTransFront.winding = ux3d::renderer::gl::Winding::CounterClockwise;

        executor.execute(cmdBlendTrue);
        executor.execute(cmdTransFront);

        auto pxBlended = readCenterPixel();
        // Red blended over Green: R ≈ 128, G ≈ 128, B = 0, A ≈ 191
        check(pxBlended[0] >= 126 && pxBlended[0] <= 130 && pxBlended[1] >= 126 && pxBlended[1] <= 130 && pxBlended[2] == 0,
              "Combined Scenario 1: Front transparent quad blended over opaque green [128, 128, 0]");

        // Step 3: Draw Transparent Quad Back (Blue, Z=0.5f, behind green). DepthTest=True, Blend=True.
        // Depth test should reject it; pixel must remain [128, 128, 0].
        ux3d::renderer::Command cmdTransBack(gl, ux3d::renderer::CommandTag::Draw);
        cmdTransBack.program = &progTransBlue;
        cmdTransBack.vertexLayout = layoutQuadBack;
        cmdTransBack.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
        cmdTransBack.primitiveCount = 6;
        cmdTransBack.primitiveOffset = 0;
        cmdTransBack.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
        cmdTransBack.indexBuffer = &quadIdxBuf;
        cmdTransBack.polygonMode = ux3d::renderer::gl::PolygonMode::Fill;
        cmdTransBack.culling = ux3d::renderer::gl::Culling::None;
        cmdTransBack.winding = ux3d::renderer::gl::Winding::CounterClockwise;

        executor.execute(cmdTransBack);
        auto pxRejected = readCenterPixel();
        check(pxRejected[0] >= 126 && pxRejected[0] <= 130 && pxRejected[1] >= 126 && pxRejected[1] <= 130 && pxRejected[2] == 0,
              "Combined Scenario 1: Back transparent quad occluded by depth test, output unchanged");

        // Scenario 2: Wireframe Overlay Interleaving Simulation
        // Transition from Fill to Line, then back to Fill with polygon offset
        cmdOpaque.polygonMode = ux3d::renderer::gl::PolygonMode::Line;
        executor.execute(cmdOpaque);
        check(executor.getActivePolygonMode() == ux3d::renderer::gl::PolygonMode::Line,
              "Combined Scenario 2: Switched to PolygonMode::Line for wireframe overlay");

        cmdOpaque.polygonMode = ux3d::renderer::gl::PolygonMode::Fill;
        executor.execute(cmdOpaque);
        check(executor.getActivePolygonMode() == ux3d::renderer::gl::PolygonMode::Fill,
              "Combined Scenario 2: Restored to PolygonMode::Fill after overlay");

        // Scenario 3: Multi-Viewport Simulation (Viewport A vs Viewport B)
        // Viewport A: Back-cull, CCW, Fill, Blend=False, Depth=True
        // Viewport B: None-cull, CW, Line, Blend=True, Depth=True
        executor.resetState(); // Simulates Viewport A entry
        executor.execute(cmdDepthTrue);
        executor.execute(cmdBlendFalse);
        cmdOpaque.culling = ux3d::renderer::gl::Culling::Back;
        cmdOpaque.winding = ux3d::renderer::gl::Winding::CounterClockwise;
        cmdOpaque.polygonMode = ux3d::renderer::gl::PolygonMode::Fill;
        executor.execute(cmdOpaque);
        check(executor.getCullingEnabled() == true && executor.getCullFace() == ux3d::renderer::gl::Culling::Back,
              "Combined Scenario 3: Viewport A established Back culling");

        executor.resetState(); // Simulates Viewport B entry
        executor.execute(cmdDepthTrue);
        executor.execute(cmdBlendTrue);
        cmdOpaque.culling = ux3d::renderer::gl::Culling::None;
        cmdOpaque.winding = ux3d::renderer::gl::Winding::Clockwise;
        cmdOpaque.polygonMode = ux3d::renderer::gl::PolygonMode::Line;
        executor.execute(cmdOpaque);
        check(executor.getCullingEnabled() == false && executor.getActiveWinding() == ux3d::renderer::gl::Winding::Clockwise,
              "Combined Scenario 3: Viewport B established None culling and CW winding");

        // Scenario 4: External Post-Processing AA / ImGui Simulation
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        executor.resetState(); // Next frame entry
        executor.execute(cmdDepthTrue);
        executor.execute(cmdBlendTrue);
        cmdOpaque.culling = ux3d::renderer::gl::Culling::Back;
        cmdOpaque.winding = ux3d::renderer::gl::Winding::CounterClockwise;
        cmdOpaque.polygonMode = ux3d::renderer::gl::PolygonMode::Fill;
        executor.execute(cmdOpaque);
        check(executor.getDepthTestEnabled() == true && executor.getBlendEnabled() == true,
              "Combined Scenario 4: State restored cleanly after external AA/ImGui passes");

        const GLenum errCombined = glGetError();
        check(errCombined == GL_NO_ERROR, "No GL errors across combined integration scenarios");
    }

    if (runIntegration) {
        GLuint borrowed[2];
        glGenTextures(2, borrowed);
        for (GLuint handle : borrowed) glBindTexture(GL_TEXTURE_2D, handle);
        {
            ux3d::renderer::GltfResourceManager resources(gl);
            resources.createFramebufferTexture(borrowed[0]);
            auto* wrapper = resources.getFramebufferTexture();
            resources.createFramebufferTexture(borrowed[1]);
            check(resources.getFramebufferTexture() == wrapper && wrapper->handle == borrowed[1],
                  "Transmission wrapper identity survives viewport texture replacement");
            check(glIsTexture(borrowed[0]) == GL_TRUE, "Replacing borrowed texture preserves original owner");
        }
        check(glIsTexture(borrowed[1]) == GL_TRUE, "Resource manager destruction preserves borrowed texture");
        {
            ux3d::renderer::gl::Texture borrowedWrapper(gl, borrowed[1], ux3d::renderer::gl::TextureTarget::Texture2d, false);
            auto moved = std::move(borrowedWrapper);
        }
        check(glIsTexture(borrowed[1]) == GL_TRUE, "Moving borrowed wrapper preserves ownership");
        glDeleteTextures(2, borrowed);
        check(glIsTexture(borrowed[0]) == GL_FALSE && glIsTexture(borrowed[1]) == GL_FALSE,
              "Framebuffer owner can release both borrowed textures");
        check(glGetError() == GL_NO_ERROR, "No GL errors during transmission ownership checks");
    }

    // -------------------------------------------------------------
    // Cleanup
    // -------------------------------------------------------------
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &colorTex);
    glDeleteRenderbuffers(1, &depthRbo);
    // Note: Shader programs are RAII-owned by ux3d::renderer::gl::Program
    // wrappers (progGreen, progTransRed, progTransBlue) which release program handles on destruction.
    glDeleteShader(vs);
    glDeleteShader(fsOpaqueGreen);
    glDeleteShader(fsTransRed);
    glDeleteShader(fsTransBlue);

    report << "\nTotal assertions evaluated: " << checks << "\n";
    report << "Failed assertions: " << failures.size() << "\n";
    report << "Result: " << (result.success ? "PASS" : "FAIL") << "\n";
    result.report = report.str();
    return result;
}

} // namespace ux3d::imgui
