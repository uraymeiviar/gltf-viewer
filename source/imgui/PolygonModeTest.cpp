// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/PolygonModeTest.h"
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
#include "renderer/gl/VertexLayoutCache.h"

#include <array>
#include <sstream>
#include <vector>
#include <cmath>

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

SelfCheckResult runPolygonModeSelfCheck(
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
    report << "P3.4.1 CommandExecutor Polygon Mode Cache self-check\n";
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
        check(false, "forced polygon mode self-check failure requested");
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

    const bool runAll = testCase.empty() || testCase == "all";

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

    const char* fsCyanSrc =
        "#version 330 core\n"
        "out vec4 fragColor;\n"
        "void main() {\n"
        "    fragColor = vec4(0.0, 1.0, 1.0, 1.0);\n"
        "}\n";

    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fsGreen = compileShader(GL_FRAGMENT_SHADER, fsGreenSrc);
    GLuint fsCyan = compileShader(GL_FRAGMENT_SHADER, fsCyanSrc);
    GLuint progGreenHandle = linkProgram(vs, fsGreen);
    GLuint progCyanHandle = linkProgram(vs, fsCyan);

    check(progGreenHandle != 0, "Green test shader program linked successfully");
    check(progCyanHandle != 0, "Cyan test shader program linked successfully");

    ux3d::renderer::gl::Program progGreen(gl, progGreenHandle);
    ux3d::renderer::gl::Program progCyan(gl, progCyanHandle);

    // Quad geometry covering [-0.7, 0.7] x [-0.7, 0.7] (leaving clear borders)
    std::vector<float> quadPos = {
        -0.7f, -0.7f, 0.0f,
         0.7f, -0.7f, 0.0f,
         0.7f,  0.7f, 0.0f,
        -0.7f,  0.7f, 0.0f
    };
    auto quadPosBuf = makeGlBuffer(gl, quadPos);

    std::vector<uint16_t> quadIndices = { 0, 1, 2, 2, 3, 0 };
    auto quadIdxBuf = makeGlBuffer(gl, quadIndices);

    ux3d::renderer::Mesh quadMesh(gl);
    ux3d::renderer::gl::Attribute qAttr("a_position");
    qAttr.buffer = &quadPosBuf;
    qAttr.dimension = 3;
    qAttr.type = ux3d::renderer::gl::ComponentType::Float;
    quadMesh.attributes.push_back(qAttr);
    quadMesh.indexBuffer = &quadIdxBuf;
    quadMesh.indexCount = 6;
    quadMesh.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
    quadMesh.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;

    ux3d::renderer::gl::VertexLayoutCache vaoCache(gl);
    auto quadLayout = vaoCache.getOrCreate(&quadMesh, &progGreen);
    check(quadLayout != nullptr, "Quad vertex layout created");

    // Offscreen 64x64 FBO for readbacks
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLuint colorTex = 0;
    glGenTextures(1, &colorTex);
    glBindTexture(GL_TEXTURE_2D, colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

    check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
          "Offscreen test FBO is complete");

    glViewport(0, 0, 64, 64);

    // =============================================================
    // Section 1: Call Suppression and Mode Transitions
    // =============================================================
    if (runAll || testCase == "transitions") {
        report << "\n--- Section 1: Call Suppression and Mode Transitions ---\n";

        ux3d::renderer::CommandExecutor executor(gl);
        executor.init();

        check(!executor.getActivePolygonMode().has_value(),
              "Executor activePolygonMode is uninitialized/invalid on init");

        ux3d::renderer::Command cmdFill(gl, ux3d::renderer::CommandTag::Draw);
        cmdFill.program = &progGreen;
        cmdFill.vertexLayout = quadLayout;
        cmdFill.indexBuffer = &quadIdxBuf;
        cmdFill.primitiveCount = 6;
        cmdFill.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
        cmdFill.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
        cmdFill.culling = ux3d::renderer::gl::Culling::None;
        cmdFill.polygonMode = ux3d::renderer::gl::PolygonMode::Fill;

        ux3d::renderer::Command cmdLine = cmdFill;
        cmdLine.polygonMode = ux3d::renderer::gl::PolygonMode::Line;

        ux3d::imgui::profiling::setCountingEnabled(true);
        ux3d::imgui::profiling::frameCounters().polygonModeCalls = 0;

        // Draw 1: Fill (cold draw -> establishes Fill)
        executor.execute(cmdFill);
        check(ux3d::imgui::profiling::frameCounters().polygonModeCalls == 1,
              "Draw 1 (Fill): First draw issued glPolygonMode call (count=1)");
        check(executor.getActivePolygonMode().has_value() &&
              executor.getActivePolygonMode().value() == ux3d::renderer::gl::PolygonMode::Fill,
              "Executor cached PolygonMode::Fill after Draw 1");

        // Draw 2: Fill (redundant mode -> must suppress call)
        executor.execute(cmdFill);
        check(ux3d::imgui::profiling::frameCounters().polygonModeCalls == 1,
              "Draw 2 (Fill): Redundant Fill draw suppressed glPolygonMode call (count remains 1)");

        // Draw 3: Line (transition -> issues call)
        executor.execute(cmdLine);
        check(ux3d::imgui::profiling::frameCounters().polygonModeCalls == 2,
              "Draw 3 (Line): Transition from Fill to Line issued glPolygonMode call (count=2)");
        check(executor.getActivePolygonMode().has_value() &&
              executor.getActivePolygonMode().value() == ux3d::renderer::gl::PolygonMode::Line,
              "Executor cached PolygonMode::Line after Draw 3");

        // Draw 4: Line (redundant mode -> must suppress call)
        executor.execute(cmdLine);
        check(ux3d::imgui::profiling::frameCounters().polygonModeCalls == 2,
              "Draw 4 (Line): Redundant Line draw suppressed glPolygonMode call (count remains 2)");

        // Draw 5: Fill (transition back to Fill -> issues call)
        executor.execute(cmdFill);
        check(ux3d::imgui::profiling::frameCounters().polygonModeCalls == 3,
              "Draw 5 (Fill): Transition from Line to Fill issued glPolygonMode call (count=3)");
        check(executor.getActivePolygonMode().has_value() &&
              executor.getActivePolygonMode().value() == ux3d::renderer::gl::PolygonMode::Fill,
              "Executor cached PolygonMode::Fill after Draw 5");

        // Check resetState behavior
        executor.resetState();
        check(!executor.getActivePolygonMode().has_value(),
              "resetState() invalidated activePolygonMode cache");

        // Draw 6: Fill (requested mode matches last mode before reset, but cache is invalid -> must issue call)
        executor.execute(cmdFill);
        check(ux3d::imgui::profiling::frameCounters().polygonModeCalls == 4,
              "Draw 6 (Fill): First draw after reset issued glPolygonMode call despite matching pre-reset mode (count=4)");
        check(executor.getActivePolygonMode().has_value() &&
              executor.getActivePolygonMode().value() == ux3d::renderer::gl::PolygonMode::Fill,
              "Executor re-cached PolygonMode::Fill after Draw 6");
    }

    // =============================================================
    // Section 2: External Callback Boundary Isolation
    // =============================================================
    if (runAll || testCase == "boundary") {
        report << "\n--- Section 2: External Callback Boundary Isolation ---\n";

        ux3d::renderer::CommandExecutor executor(gl);
        executor.init();

        ux3d::renderer::Command cmdFill(gl, ux3d::renderer::CommandTag::Draw);
        cmdFill.program = &progGreen;
        cmdFill.vertexLayout = quadLayout;
        cmdFill.indexBuffer = &quadIdxBuf;
        cmdFill.primitiveCount = 6;
        cmdFill.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
        cmdFill.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
        cmdFill.culling = ux3d::renderer::gl::Culling::None;
        cmdFill.polygonMode = ux3d::renderer::gl::PolygonMode::Fill;

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Step 1: Draw in Fill mode
        executor.execute(cmdFill);
        check(executor.getActivePolygonMode().value() == ux3d::renderer::gl::PolygonMode::Fill,
              "Executor activePolygonMode is Fill before external callback");

        // Step 2: ExternalFunction directly changes driver state to GL_LINE
        ux3d::renderer::Command cmdExt(gl, ux3d::renderer::CommandTag::ExternalFunction);
        cmdExt.externalFunction = []() {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        };
        executor.execute(cmdExt);

        check(!executor.getActivePolygonMode().has_value(),
              "ExternalFunction successfully invalidated activePolygonMode in CommandExecutor");

        GLint driverMode[2] = { 0, 0 };
        glGetIntegerv(GL_POLYGON_MODE, driverMode);
        check(driverMode[0] == GL_LINE, "Driver polygon mode verified as GL_LINE inside external scope");

        // Clear FBO to black
        glClear(GL_COLOR_BUFFER_BIT);

        // Step 3: Draw again requesting Fill mode
        // If executor stale state were trusted, it would skip calling glPolygonMode and leave driver in GL_LINE.
        // Because ExternalFunction invalidated the cache, executor must issue glPolygonMode(GL_FRONT_AND_BACK, GL_FILL).
        executor.execute(cmdFill);

        glGetIntegerv(GL_POLYGON_MODE, driverMode);
        check(driverMode[0] == GL_FILL,
              "Driver polygon mode verified as restored to GL_FILL after draw");
        check(executor.getActivePolygonMode().value() == ux3d::renderer::gl::PolygonMode::Fill,
              "Executor activePolygonMode re-established as Fill");

        // Verify with pixel readback: center pixel must be solid green [0, 255, 0, 255]
        // (If driver had stayed in GL_LINE, center would have remained black [0, 0, 0, 255])
        std::array<uint8_t, 4> pixel = { 0, 0, 0, 0 };
        glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        const bool greenMatch = (pixel[0] == 0) && (pixel[1] >= 250) && (pixel[2] == 0) && (pixel[3] == 255);
        check(greenMatch,
              "Pixel readback at center (32, 32) is solid green [0, 255, 0, 255], proving GL_FILL was genuinely active");
    }

    // =============================================================
    // Section 3: Renderer Boundary Isolation & Inter-Pass State
    // =============================================================
    if (runAll || testCase == "renderer-boundary") {
        report << "\n--- Section 3: Renderer Boundary Isolation ---\n";

        if (glRenderer && commandGenerator && reloadAndDraw) {
            const std::string modelPath = "resources/sampleGltf/HelloCube.glb";
            bool ok = reloadAndDraw(modelPath);
            check(ok, "Initial model reload and Renderer::draw succeeded");

            // Driver state after standard draw should be GL_FILL
            GLint mode[2] = { 0, 0 };
            glGetIntegerv(GL_POLYGON_MODE, mode);
            check(mode[0] == GL_FILL, "Driver polygon mode is GL_FILL following Renderer::draw");

            // Outside Renderer::draw, directly alter OpenGL state to GL_LINE
            // (simulating wireframe overlay, AA pass, or external app code)
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glGetIntegerv(GL_POLYGON_MODE, mode);
            check(mode[0] == GL_LINE, "Driver polygon mode changed to GL_LINE outside Renderer");

            // Issue second Renderer::draw: must re-establish GL_FILL on its first draw command
            ok = glRenderer->draw({ commandGenerator });
            check(ok, "Second Renderer::draw succeeded");

            glGetIntegerv(GL_POLYGON_MODE, mode);
            check(mode[0] == GL_FILL,
                  "Renderer::draw successfully re-established GL_FILL despite external inter-frame mutation");

            // Verify clean sampler and renderer boundaries survived intact
            GLint boundSampler = 0;
            glGetIntegerv(GL_SAMPLER_BINDING, &boundSampler);
            check(boundSampler == 0, "Renderer::draw exit left sampler binding 0 on unit 0");

            GLenum err = glGetError();
            check(err == GL_NO_ERROR, "No GL errors during Renderer boundary isolation test");
        }
    }

    // =============================================================
    // Section 4: Visual Correctness with Explicit Tolerances
    // =============================================================
    if (runAll || testCase == "visual") {
        report << "\n--- Section 4: Visual Correctness with Explicit Readback Tolerances ---\n";

        ux3d::renderer::CommandExecutor executor(gl);
        executor.init();

        ux3d::renderer::Command cmdFill(gl, ux3d::renderer::CommandTag::Draw);
        cmdFill.program = &progCyan;
        cmdFill.vertexLayout = quadLayout;
        cmdFill.indexBuffer = &quadIdxBuf;
        cmdFill.primitiveCount = 6;
        cmdFill.indexFormat = ux3d::renderer::gl::PrimitiveType::UnsignedShort;
        cmdFill.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
        cmdFill.culling = ux3d::renderer::gl::Culling::None;
        cmdFill.polygonMode = ux3d::renderer::gl::PolygonMode::Fill;

        ux3d::renderer::Command cmdLine = cmdFill;
        cmdLine.polygonMode = ux3d::renderer::gl::PolygonMode::Line;

        // --- Pass 1: Solid Fill ---
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        executor.execute(cmdFill);

        // Point (43, 21) is inside triangle (0, 1, 2) and off the diagonal
        std::array<uint8_t, 4> pxInterior = { 0, 0, 0, 0 };
        std::array<uint8_t, 4> pxOutside = { 0, 0, 0, 0 };
        glReadPixels(43, 21, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pxInterior.data());
        glReadPixels(2, 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pxOutside.data());

        const bool fillInteriorMatch = (pxInterior[0] == 0) && (pxInterior[1] >= 250) && (pxInterior[2] >= 250) && (pxInterior[3] == 255);
        const bool fillOutsideMatch = (pxOutside[0] == 0) && (pxOutside[1] == 0) && (pxOutside[2] == 0) && (pxOutside[3] == 255);
        check(fillInteriorMatch, "Solid Fill pass: interior (43, 21) is filled cyan [0, 255, 255, 255]");
        check(fillOutsideMatch, "Solid Fill pass: outside (2, 2) is background black [0, 0, 0, 255]");

        // --- Pass 2: Wireframe Line ---
        glClear(GL_COLOR_BUFFER_BIT);

        executor.execute(cmdLine);

        glReadPixels(43, 21, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pxInterior.data());
        const bool lineInteriorIsBlack = (pxInterior[0] == 0) && (pxInterior[1] == 0) && (pxInterior[2] == 0) && (pxInterior[3] == 255);
        check(lineInteriorIsBlack,
              "Wireframe Line pass: interior (43, 21) is unpainted black [0, 0, 0, 255], visibly distinct from Fill");

        // Edge pixel on the quad boundary (around x = 0.7 * 32 + 32 = ~54, y = 32)
        std::array<uint8_t, 4> pxEdge = { 0, 0, 0, 0 };
        glReadPixels(54, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pxEdge.data());
        const bool edgeHasColor = (pxEdge[1] > 100 || pxEdge[2] > 100);
        check(edgeHasColor, "Wireframe Line pass: edge boundary pixel contains rendered line color");

        // --- Pass 3: Transition Back to Solid Fill ---
        glClear(GL_COLOR_BUFFER_BIT);
        executor.execute(cmdFill);

        glReadPixels(43, 21, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pxInterior.data());
        const bool refillInteriorMatch = (pxInterior[0] == 0) && (pxInterior[1] >= 250) && (pxInterior[2] >= 250) && (pxInterior[3] == 255);
        check(refillInteriorMatch,
              "Transition back to Fill: interior (43, 21) is restored to solid cyan [0, 255, 255, 255]");
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
