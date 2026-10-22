// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/CullWindingTest.h"
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

SelfCheckResult runCullWindingSelfCheck(
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
    report << "P3.4.2 + P3.4.3 CommandExecutor Culling and Winding Cache self-check\n";
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
        check(false, "forced culling and winding self-check failure requested");
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

    const char* fsBlueSrc =
        "#version 330 core\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    fragColor = vec4(0.0, 0.0, 1.0, 1.0);\n"
        "}\n";

    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fsGreen = compileShader(GL_FRAGMENT_SHADER, fsGreenSrc);
    GLuint fsBlue = compileShader(GL_FRAGMENT_SHADER, fsBlueSrc);
    GLuint progGreenHandle = linkProgram(vs, fsGreen);
    GLuint progBlueHandle = linkProgram(vs, fsBlue);

    check(progGreenHandle != 0, "Green test shader program linked successfully");
    check(progBlueHandle != 0, "Blue test shader program linked successfully");

    ux3d::renderer::gl::Program progGreen(gl, progGreenHandle);
    ux3d::renderer::gl::Program progBlue(gl, progBlueHandle);

    // Geometry: Left Triangle (CCW) covering [-0.8, -0.1]
    // V0=(-0.8, -0.7), V1=(-0.1, -0.7), V2=(-0.45, 0.7) -> Counter-Clockwise
    std::vector<float> triLeftPos = {
        -0.8f, -0.7f, 0.0f,
        -0.1f, -0.7f, 0.0f,
        -0.45f, 0.7f, 0.0f
    };
    auto triLeftPosBuf = makeGlBuffer(gl, triLeftPos);
    std::vector<uint16_t> triLeftIndices = { 0, 1, 2 };
    auto triLeftIdxBuf = makeGlBuffer(gl, triLeftIndices);

    ux3d::renderer::Mesh meshTriLeft(gl);
    ux3d::renderer::gl::Attribute attrLeft("a_position");
    attrLeft.buffer = &triLeftPosBuf;
    attrLeft.dimension = 3;
    attrLeft.type = ux3d::renderer::gl::ComponentType::Float;
    meshTriLeft.attributes.push_back(attrLeft);
    meshTriLeft.indexBuffer = &triLeftIdxBuf;
    meshTriLeft.indexCount = 3;
    meshTriLeft.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
    meshTriLeft.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
    std::shared_ptr<ux3d::renderer::gl::VertexLayout> layoutTriLeft = progGreen.createVertexLayout(meshTriLeft.attributes);
    check(layoutTriLeft != nullptr && layoutTriLeft->handle != 0, "Left CCW triangle vertex layout created");

    // Geometry: Right Triangle (CW) covering [0.1, 0.8]
    // W0=(0.1, -0.7), W1=(0.45, 0.7), W2=(0.8, -0.7) -> Clockwise
    std::vector<float> triRightPos = {
        0.1f, -0.7f, 0.0f,
        0.45f, 0.7f, 0.0f,
        0.8f, -0.7f, 0.0f
    };
    auto triRightPosBuf = makeGlBuffer(gl, triRightPos);
    std::vector<uint16_t> triRightIndices = { 0, 1, 2 };
    auto triRightIdxBuf = makeGlBuffer(gl, triRightIndices);

    ux3d::renderer::Mesh meshTriRight(gl);
    ux3d::renderer::gl::Attribute attrRight("a_position");
    attrRight.buffer = &triRightPosBuf;
    attrRight.dimension = 3;
    attrRight.type = ux3d::renderer::gl::ComponentType::Float;
    meshTriRight.attributes.push_back(attrRight);
    meshTriRight.indexBuffer = &triRightIdxBuf;
    meshTriRight.indexCount = 3;
    meshTriRight.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
    meshTriRight.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
    std::shared_ptr<ux3d::renderer::gl::VertexLayout> layoutTriRight = progBlue.createVertexLayout(meshTriRight.attributes);
    check(layoutTriRight != nullptr && layoutTriRight->handle != 0, "Right CW triangle vertex layout created");

    // Offscreen 64x64 FBO
    GLuint fbo = 0;
    GLuint colorTex = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenTextures(1, &colorTex);
    glBindTexture(GL_TEXTURE_2D, colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);
    glViewport(0, 0, 64, 64);

    const GLenum fboStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    check(fboStatus == GL_FRAMEBUFFER_COMPLETE, "Offscreen test FBO is complete");

    // =========================================================================
    // SECTION 1: Actual Call Counts and State Transitions (P3.4.2 & P3.4.3)
    // =========================================================================
    if (runTransitions) {
        report << "\n--- Section 1: Call Counts and Mode Transitions ---\n";

        ux3d::renderer::CommandExecutor executor(gl);
        executor.init();

        check(!executor.getCullingEnabled().has_value(), "Executor cullingEnabled is uninitialized on init");
        check(!executor.getCullFace().has_value(), "Executor cullFace is uninitialized on init");
        check(!executor.getActiveWinding().has_value(), "Executor activeWinding is uninitialized on init");

        ux3d::imgui::profiling::setCountingEnabled(true);
        const auto& counters = ux3d::imgui::profiling::frameCounters();

        const uint64_t e0 = counters.cullFaceEnableCalls;
        const uint64_t s0 = counters.cullFaceSelectionCalls;
        const uint64_t w0 = counters.frontFaceCalls;

        // Base command
        ux3d::renderer::Command cmd(gl, ux3d::renderer::CommandTag::Draw);
        cmd.program = &progGreen;
        cmd.vertexLayout = layoutTriLeft;
        cmd.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
        cmd.primitiveCount = 3;
        cmd.primitiveOffset = 0;
        cmd.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
        cmd.indexBuffer = &triLeftIdxBuf;
        cmd.polygonMode = ux3d::renderer::gl::PolygonMode::Fill;

        // Draw 1: culling = Back, winding = CCW
        cmd.culling = ux3d::renderer::gl::Culling::Back;
        cmd.winding = ux3d::renderer::gl::Winding::CounterClockwise;
        executor.execute(cmd);

        check(counters.cullFaceEnableCalls == e0 + 1, "Draw 1: Initial culling enable issued glEnable(GL_CULL_FACE) (enableCalls=1)");
        check(counters.cullFaceSelectionCalls == s0 + 1, "Draw 1: Initial cull-face selection issued glCullFace(GL_BACK) (selectionCalls=1)");
        check(counters.frontFaceCalls == w0 + 1, "Draw 1: Initial front-face winding issued glFrontFace(GL_CCW) (frontFaceCalls=1)");
        check(executor.getCullingEnabled() == true, "Draw 1: Executor cached cullingEnabled == true");
        check(executor.getCullFace() == ux3d::renderer::gl::Culling::Back, "Draw 1: Executor cached cullFace == Back");
        check(executor.getActiveWinding() == ux3d::renderer::gl::Winding::CounterClockwise, "Draw 1: Executor cached activeWinding == CCW");

        // Draw 2: Identical request (culling = Back, winding = CCW)
        executor.execute(cmd);
        check(counters.cullFaceEnableCalls == e0 + 1, "Draw 2: Redundant culling enable suppressed (remains 1)");
        check(counters.cullFaceSelectionCalls == s0 + 1, "Draw 2: Redundant cull-face selection suppressed (remains 1)");
        check(counters.frontFaceCalls == w0 + 1, "Draw 2: Redundant front-face winding suppressed (remains 1)");

        // Draw 3: Disable culling (culling = None, winding = CCW)
        cmd.culling = ux3d::renderer::gl::Culling::None;
        cmd.winding = ux3d::renderer::gl::Winding::CounterClockwise;
        executor.execute(cmd);
        check(counters.cullFaceEnableCalls == e0 + 2, "Draw 3: Transition to Culling::None issued glDisable(GL_CULL_FACE) (enableCalls=2)");
        check(counters.cullFaceSelectionCalls == s0 + 1, "Draw 3: Disabling culling did not touch selected cull face (selectionCalls remains 1)");
        check(counters.frontFaceCalls == w0 + 1, "Draw 3: Winding unchanged, suppressed glFrontFace (frontFaceCalls remains 1)");
        check(executor.getCullingEnabled() == false, "Draw 3: Executor cached cullingEnabled == false");
        check(executor.getCullFace() == ux3d::renderer::gl::Culling::Back, "Draw 3: Driver cull face retained as Back in cache");

        // Draw 4: Winding change while culling disabled (culling = None, winding = CW)
        cmd.culling = ux3d::renderer::gl::Culling::None;
        cmd.winding = ux3d::renderer::gl::Winding::Clockwise;
        executor.execute(cmd);
        check(counters.cullFaceEnableCalls == e0 + 2, "Draw 4: Redundant disable suppressed (enableCalls remains 2)");
        check(counters.cullFaceSelectionCalls == s0 + 1, "Draw 4: Cull face untouched while disabled (selectionCalls remains 1)");
        check(counters.frontFaceCalls == w0 + 2, "Draw 4: Winding transition while disabled issued glFrontFace(GL_CW) (frontFaceCalls=2)");
        check(executor.getActiveWinding() == ux3d::renderer::gl::Winding::Clockwise, "Draw 4: Executor cached activeWinding == CW");

        // Draw 5: Re-enable culling with winding established while disabled (culling = Back, winding = CW)
        cmd.culling = ux3d::renderer::gl::Culling::Back;
        cmd.winding = ux3d::renderer::gl::Winding::Clockwise;
        executor.execute(cmd);
        check(counters.cullFaceEnableCalls == e0 + 3, "Draw 5: Re-enabling culling issued glEnable(GL_CULL_FACE) (enableCalls=3)");
        check(counters.cullFaceSelectionCalls == s0 + 1, "Draw 5: Cull face matches driver state, suppressed glCullFace (selectionCalls remains 1)");
        check(counters.frontFaceCalls == w0 + 2, "Draw 5: Winding matches established CW state, suppressed glFrontFace (frontFaceCalls remains 2)");
        check(executor.getCullingEnabled() == true, "Draw 5: Executor cached cullingEnabled == true");

        // Draw 6: Cull face selection transition (culling = Front, winding = CW)
        cmd.culling = ux3d::renderer::gl::Culling::Front;
        cmd.winding = ux3d::renderer::gl::Winding::Clockwise;
        executor.execute(cmd);
        check(counters.cullFaceEnableCalls == e0 + 3, "Draw 6: Culling already enabled, suppressed glEnable (enableCalls remains 3)");
        check(counters.cullFaceSelectionCalls == s0 + 2, "Draw 6: Transition from Back to Front issued glCullFace(GL_FRONT) (selectionCalls=2)");
        check(counters.frontFaceCalls == w0 + 2, "Draw 6: Winding unchanged, suppressed glFrontFace (frontFaceCalls remains 2)");
        check(executor.getCullFace() == ux3d::renderer::gl::Culling::Front, "Draw 6: Executor cached cullFace == Front");

        // Draw 7: Redundant Front draw (culling = Front, winding = CW)
        executor.execute(cmd);
        check(counters.cullFaceEnableCalls == e0 + 3, "Draw 7: Redundant enable suppressed (enableCalls remains 3)");
        check(counters.cullFaceSelectionCalls == s0 + 2, "Draw 7: Redundant Front selection suppressed (selectionCalls remains 2)");
        check(counters.frontFaceCalls == w0 + 2, "Draw 7: Redundant winding suppressed (frontFaceCalls remains 2)");

        // Draw 8: Disable culling again (culling = None, winding = CW)
        cmd.culling = ux3d::renderer::gl::Culling::None;
        cmd.winding = ux3d::renderer::gl::Winding::Clockwise;
        executor.execute(cmd);
        check(counters.cullFaceEnableCalls == e0 + 4, "Draw 8: Disabling culling issued glDisable(GL_CULL_FACE) (enableCalls=4)");
        check(counters.cullFaceSelectionCalls == s0 + 2, "Draw 8: Cull face untouched while disabled (selectionCalls remains 2)");
        check(counters.frontFaceCalls == w0 + 2, "Draw 8: Winding unchanged (frontFaceCalls remains 2)");

        // Draw 9: Re-enable with Front and CCW (culling = Front, winding = CCW)
        cmd.culling = ux3d::renderer::gl::Culling::Front;
        cmd.winding = ux3d::renderer::gl::Winding::CounterClockwise;
        executor.execute(cmd);
        check(counters.cullFaceEnableCalls == e0 + 5, "Draw 9: Re-enabling culling issued glEnable(GL_CULL_FACE) (enableCalls=5)");
        check(counters.cullFaceSelectionCalls == s0 + 2, "Draw 9: Cull face still Front, suppressed glCullFace (selectionCalls remains 2)");
        check(counters.frontFaceCalls == w0 + 3, "Draw 9: Winding transition to CCW issued glFrontFace(GL_CCW) (frontFaceCalls=3)");

        // Draw 10: Transition cull face back to Back (culling = Back, winding = CCW)
        cmd.culling = ux3d::renderer::gl::Culling::Back;
        cmd.winding = ux3d::renderer::gl::Winding::CounterClockwise;
        executor.execute(cmd);
        check(counters.cullFaceEnableCalls == e0 + 5, "Draw 10: Culling already enabled, suppressed glEnable (enableCalls remains 5)");
        check(counters.cullFaceSelectionCalls == s0 + 3, "Draw 10: Transition to Back issued glCullFace(GL_BACK) (selectionCalls=3)");
        check(counters.frontFaceCalls == w0 + 3, "Draw 10: Winding unchanged, suppressed glFrontFace (frontFaceCalls remains 3)");

        // Invalidation across resetState()
        executor.resetState();
        check(!executor.getCullingEnabled().has_value(), "resetState() invalidated cullingEnabled cache");
        check(!executor.getCullFace().has_value(), "resetState() invalidated cullFace cache");
        check(!executor.getActiveWinding().has_value(), "resetState() invalidated activeWinding cache");

        // Draw 11: First draw after reset (matching pre-reset mode: Back, CCW)
        executor.execute(cmd);
        check(counters.cullFaceEnableCalls == e0 + 6, "Draw 11: First draw after reset re-issued glEnable(GL_CULL_FACE) (enableCalls=6)");
        check(counters.cullFaceSelectionCalls == s0 + 4, "Draw 11: First draw after reset re-issued glCullFace(GL_BACK) (selectionCalls=4)");
        check(counters.frontFaceCalls == w0 + 4, "Draw 11: First draw after reset re-issued glFrontFace(GL_CCW) (frontFaceCalls=4)");
        check(executor.getCullingEnabled() == true, "Draw 11: Executor re-cached cullingEnabled == true");
        check(executor.getCullFace() == ux3d::renderer::gl::Culling::Back, "Draw 11: Executor re-cached cullFace == Back");
        check(executor.getActiveWinding() == ux3d::renderer::gl::Winding::CounterClockwise, "Draw 11: Executor re-cached activeWinding == CCW");
    }

    // =========================================================================
    // SECTION 2: Rendering Correctness with Controlled FBO & Pixel Readbacks
    // =========================================================================
    if (runCorrectness) {
        report << "\n--- Section 2: Rendering Correctness with Pixel Readbacks ---\n";

        ux3d::renderer::CommandExecutor executor(gl);
        executor.init();

        ux3d::renderer::Command cmdLeft(gl, ux3d::renderer::CommandTag::Draw);
        cmdLeft.program = &progGreen;
        cmdLeft.vertexLayout = layoutTriLeft;
        cmdLeft.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
        cmdLeft.primitiveCount = 3;
        cmdLeft.primitiveOffset = 0;
        cmdLeft.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
        cmdLeft.indexBuffer = &triLeftIdxBuf;
        cmdLeft.polygonMode = ux3d::renderer::gl::PolygonMode::Fill;

        ux3d::renderer::Command cmdRight(gl, ux3d::renderer::CommandTag::Draw);
        cmdRight.program = &progBlue;
        cmdRight.vertexLayout = layoutTriRight;
        cmdRight.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
        cmdRight.primitiveCount = 3;
        cmdRight.primitiveOffset = 0;
        cmdRight.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
        cmdRight.indexBuffer = &triRightIdxBuf;
        cmdRight.polygonMode = ux3d::renderer::gl::PolygonMode::Fill;

        auto readPixel = [](int x, int y) -> std::array<uint8_t, 4> {
            std::array<uint8_t, 4> px = { 0, 0, 0, 0 };
            glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
            return px;
        };

        auto isGreen = [](const std::array<uint8_t, 4>& px) {
            return (px[0] < 10 && px[1] >= 240 && px[2] < 10 && px[3] == 255);
        };
        auto isBlue = [](const std::array<uint8_t, 4>& px) {
            return (px[0] < 10 && px[1] < 10 && px[2] >= 240 && px[3] == 255);
        };
        auto isBlack = [](const std::array<uint8_t, 4>& px) {
            return (px[0] < 10 && px[1] < 10 && px[2] < 10 && px[3] == 255);
        };

        // --- Pass A: Culling Disabled (Culling::None, Winding CCW) ---
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        cmdLeft.culling = ux3d::renderer::gl::Culling::None;
        cmdLeft.winding = ux3d::renderer::gl::Winding::CounterClockwise;
        cmdRight.culling = ux3d::renderer::gl::Culling::None;
        cmdRight.winding = ux3d::renderer::gl::Winding::CounterClockwise;
        executor.execute(cmdLeft);
        executor.execute(cmdRight);

        auto pxL = readPixel(20, 20);
        auto pxR = readPixel(44, 20);
        auto pxBg = readPixel(32, 55);
        check(isGreen(pxL), "Pass A (Culling Disabled): Left CCW triangle (20, 20) is visible Green [0, 255, 0, 255]");
        check(isBlue(pxR), "Pass A (Culling Disabled): Right CW triangle (44, 20) is visible Blue [0, 0, 255, 255]");
        check(isBlack(pxBg), "Pass A (Culling Disabled): Background (32, 55) is clear Black");

        // --- Pass B: Standard glTF Culling (Culling::Back, Winding CCW) ---
        glClear(GL_COLOR_BUFFER_BIT);
        cmdLeft.culling = ux3d::renderer::gl::Culling::Back;
        cmdLeft.winding = ux3d::renderer::gl::Winding::CounterClockwise;
        cmdRight.culling = ux3d::renderer::gl::Culling::Back;
        cmdRight.winding = ux3d::renderer::gl::Winding::CounterClockwise;
        executor.execute(cmdLeft);
        executor.execute(cmdRight);

        pxL = readPixel(20, 20);
        pxR = readPixel(44, 20);
        check(isGreen(pxL), "Pass B (Cull Back, CCW): Left CCW front-facing triangle (20, 20) is visible Green");
        check(isBlack(pxR), "Pass B (Cull Back, CCW): Right CW back-facing triangle (44, 20) is culled Black");

        // --- Pass C: Mirrored Winding (Culling::Back, Winding CW) ---
        glClear(GL_COLOR_BUFFER_BIT);
        cmdLeft.culling = ux3d::renderer::gl::Culling::Back;
        cmdLeft.winding = ux3d::renderer::gl::Winding::Clockwise;
        cmdRight.culling = ux3d::renderer::gl::Culling::Back;
        cmdRight.winding = ux3d::renderer::gl::Winding::Clockwise;
        executor.execute(cmdLeft);
        executor.execute(cmdRight);

        pxL = readPixel(20, 20);
        pxR = readPixel(44, 20);
        check(isBlack(pxL), "Pass C (Cull Back, CW): Left CCW triangle is now back-facing and culled Black");
        check(isBlue(pxR), "Pass C (Cull Back, CW): Right CW triangle is now front-facing and visible Blue");

        // --- Pass D: Front Culling (Culling::Front, Winding CCW) ---
        glClear(GL_COLOR_BUFFER_BIT);
        cmdLeft.culling = ux3d::renderer::gl::Culling::Front;
        cmdLeft.winding = ux3d::renderer::gl::Winding::CounterClockwise;
        cmdRight.culling = ux3d::renderer::gl::Culling::Front;
        cmdRight.winding = ux3d::renderer::gl::Winding::CounterClockwise;
        executor.execute(cmdLeft);
        executor.execute(cmdRight);

        pxL = readPixel(20, 20);
        pxR = readPixel(44, 20);
        check(isBlack(pxL), "Pass D (Cull Front, CCW): Left CCW front-facing triangle is culled Black");
        check(isBlue(pxR), "Pass D (Cull Front, CCW): Right CW back-facing triangle is NOT culled, visible Blue");

        // --- Pass E: Restoring Disabled Culling (Culling::None, Winding CCW) ---
        glClear(GL_COLOR_BUFFER_BIT);
        cmdLeft.culling = ux3d::renderer::gl::Culling::None;
        cmdLeft.winding = ux3d::renderer::gl::Winding::CounterClockwise;
        cmdRight.culling = ux3d::renderer::gl::Culling::None;
        cmdRight.winding = ux3d::renderer::gl::Winding::CounterClockwise;
        executor.execute(cmdLeft);
        executor.execute(cmdRight);

        pxL = readPixel(20, 20);
        pxR = readPixel(44, 20);
        check(isGreen(pxL), "Pass E (Culling Restored None): Left triangle (20, 20) restored to Green");
        check(isBlue(pxR), "Pass E (Culling Restored None): Right triangle (44, 20) restored to Blue");

        // --- Pass F: Repeated Draw with Redundant Call Suppression ---
        glClear(GL_COLOR_BUFFER_BIT);
        cmdLeft.culling = ux3d::renderer::gl::Culling::Back;
        cmdLeft.winding = ux3d::renderer::gl::Winding::CounterClockwise;
        cmdRight.culling = ux3d::renderer::gl::Culling::Back;
        cmdRight.winding = ux3d::renderer::gl::Winding::CounterClockwise;
        executor.execute(cmdLeft);
        executor.execute(cmdRight);

        pxL = readPixel(20, 20);
        pxR = readPixel(44, 20);
        check(isGreen(pxL), "Pass F (Suppressed Draw): Output retained Left Green while calls suppressed");
        check(isBlack(pxR), "Pass F (Suppressed Draw): Output retained Right culled Black while calls suppressed");
    }

    // =========================================================================
    // SECTION 3: Boundary Isolation
    // =========================================================================
    if (runBoundary) {
        report << "\n--- Section 3: Boundary Isolation ---\n";

        ux3d::renderer::CommandExecutor executor(gl);
        executor.init();

        ux3d::renderer::Command cmdLeft(gl, ux3d::renderer::CommandTag::Draw);
        cmdLeft.program = &progGreen;
        cmdLeft.vertexLayout = layoutTriLeft;
        cmdLeft.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
        cmdLeft.primitiveCount = 3;
        cmdLeft.primitiveOffset = 0;
        cmdLeft.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
        cmdLeft.indexBuffer = &triLeftIdxBuf;
        cmdLeft.polygonMode = ux3d::renderer::gl::PolygonMode::Fill;
        cmdLeft.culling = ux3d::renderer::gl::Culling::Back;
        cmdLeft.winding = ux3d::renderer::gl::Winding::CounterClockwise;

        ux3d::renderer::Command cmdRight = cmdLeft;
        cmdRight.program = &progBlue;
        cmdRight.vertexLayout = layoutTriRight;
        cmdRight.indexBuffer = &triRightIdxBuf;

        // Draw initially in Back/CCW
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        executor.execute(cmdLeft);
        check(executor.getCullingEnabled() == true, "Boundary: Executor cullingEnabled is true before callback");
        check(executor.getCullFace() == ux3d::renderer::gl::Culling::Back, "Boundary: Executor cullFace is Back before callback");
        check(executor.getActiveWinding() == ux3d::renderer::gl::Winding::CounterClockwise, "Boundary: Executor winding is CCW before callback");

        // Submit ExternalFunction that mutates driver GL state
        bool callbackExecuted = false;
        auto externalCmd = ux3d::renderer::Command::makeExternalFunctionCommand(gl, [&]() {
            callbackExecuted = true;
            glDisable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            glFrontFace(GL_CW);
        });
        executor.execute(externalCmd);

        check(callbackExecuted, "Boundary: ExternalFunction callback executed");
        check(!executor.getCullingEnabled().has_value(), "Boundary: ExternalFunction invalidated cullingEnabled");
        check(!executor.getCullFace().has_value(), "Boundary: ExternalFunction invalidated cullFace");
        check(!executor.getActiveWinding().has_value(), "Boundary: ExternalFunction invalidated activeWinding");
        check(!executor.getActivePolygonMode().has_value(), "Boundary: ExternalFunction invalidated activePolygonMode");

        // Re-draw requesting original state (Back, CCW)
        glClear(GL_COLOR_BUFFER_BIT);
        executor.execute(cmdLeft);
        executor.execute(cmdRight);

        GLboolean cullEnabled = GL_FALSE;
        glGetBooleanv(GL_CULL_FACE, &cullEnabled);
        GLint cullMode = 0;
        glGetIntegerv(GL_CULL_FACE_MODE, &cullMode);
        GLint frontWinding = 0;
        glGetIntegerv(GL_FRONT_FACE, &frontWinding);

        check(cullEnabled == GL_TRUE, "Boundary: Driver GL_CULL_FACE verified as restored to GL_TRUE");
        check(cullMode == GL_BACK, "Boundary: Driver GL_CULL_FACE_MODE verified as restored to GL_BACK");
        check(frontWinding == GL_CCW, "Boundary: Driver GL_FRONT_FACE verified as restored to GL_CCW");

        std::array<uint8_t, 4> pxL = { 0, 0, 0, 0 };
        std::array<uint8_t, 4> pxR = { 0, 0, 0, 0 };
        glReadPixels(20, 20, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pxL.data());
        glReadPixels(44, 20, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pxR.data());
        const bool passLeft = (pxL[0] < 10 && pxL[1] >= 240 && pxL[2] < 10 && pxL[3] == 255);
        const bool passRightCulled = (pxR[0] < 10 && pxR[1] < 10 && pxR[2] < 10 && pxR[3] == 255);
        check(passLeft, "Boundary: Left triangle (20, 20) is solid Green [0, 255, 0, 255]");
        check(passRightCulled, "Boundary: Right CW triangle (44, 20) is culled Black [0, 0, 0, 255]");

        // Part 2: Inter-frame Renderer boundary isolation
        if (reloadAndDraw) {
            bool ok = reloadAndDraw("build/p3.4/fixtures/cull_winding_fixture.gltf");
            check(ok, "Boundary: First model reload and Renderer::draw succeeded");

            // Direct external GL state corruption between renderer calls
            glDisable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            glFrontFace(GL_CCW);

            ok = glRenderer->draw({ commandGenerator });
            check(ok, "Boundary: Second Renderer::draw succeeded despite inter-frame state corruption");

            GLint restoredCullMode = 0;
            GLint restoredWinding = 0;
            glGetIntegerv(GL_CULL_FACE_MODE, &restoredCullMode);
            glGetIntegerv(GL_FRONT_FACE, &restoredWinding);
            check(glIsEnabled(GL_CULL_FACE) == GL_TRUE,
                  "Boundary: Second Renderer::draw restored culling enablement");
            check(restoredCullMode == GL_BACK,
                  "Boundary: Second Renderer::draw restored back-face culling");
            check(restoredWinding == GL_CW,
                  "Boundary: Second Renderer::draw restored mirrored fixture winding");

            GLint samplerUnit0 = -1;
            glGetIntegeri_v(GL_SAMPLER_BINDING, 0, &samplerUnit0);
            check(samplerUnit0 == 0, "Boundary: Renderer::draw exit left sampler binding 0 on unit 0");

            GLenum err = glGetError();
            check(err == GL_NO_ERROR, "Boundary: No GL errors during Renderer boundary isolation test");
        }
    }

    // =========================================================================
    // SECTION 4: Application Integration Case
    // =========================================================================
    if (runIntegration) {
        report << "\n--- Section 4: Application Integration Case ---\n";

        if (reloadAndDraw) {
            bool ok = reloadAndDraw("build/p3.4/fixtures/cull_winding_fixture.gltf");
            check(ok, "Integration: cull_winding_fixture.gltf loaded and rendered successfully");

            GLenum err = glGetError();
            check(err == GL_NO_ERROR, "Integration: Zero GL errors following cull_winding_fixture render");

            // The fixture ends with Node_Mirrored_SingleSided (scale: [-1, 1, 1], frontFace: Clockwise, culling: Back)
            GLboolean isCullEnabled = GL_FALSE;
            glGetBooleanv(GL_CULL_FACE, &isCullEnabled);
            GLint cullFaceMode = 0;
            glGetIntegerv(GL_CULL_FACE_MODE, &cullFaceMode);
            GLint frontWinding = 0;
            glGetIntegerv(GL_FRONT_FACE, &frontWinding);

            check(isCullEnabled == GL_TRUE, "Integration: Driver GL_CULL_FACE is enabled after rendering fixture");
            check(cullFaceMode == GL_BACK, "Integration: Driver GL_CULL_FACE_MODE is GL_BACK");
            check(frontWinding == GL_CW, "Integration: Driver GL_FRONT_FACE is GL_CW from mirrored geometry");

            // Baseline comparison: render HelloCube.glb
            ok = reloadAndDraw("resources/sampleGltf/HelloCube.glb");
            check(ok, "Integration: HelloCube.glb rendered successfully with active caches");
            check(glGetError() == GL_NO_ERROR, "Integration: Zero GL errors following HelloCube baseline render");
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteTextures(1, &colorTex);
    glDeleteFramebuffers(1, &fbo);

    report << "\nTotal assertions evaluated: " << checks << "\n";
    report << "Failed assertions: " << failures.size() << "\n";
    report << "Result: " << (failures.empty() ? "PASS" : "FAIL") << "\n";

    result.report = report.str();
    result.success = (failures.empty());
    return result;
}

} // namespace ux3d::imgui
