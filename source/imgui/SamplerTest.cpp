// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/SamplerTest.h"
#include <glad/glad.h>
#include "imgui/GladGL.h"
#include "imgui/stb_image_write.h"
#include "renderer/Renderer.h"
#include "renderer/CommandExecutor.h"
#include "renderer/Command.h"
#include "renderer/gl/Sampler.h"
#include "renderer/gl/Program.h"
#include "renderer/gl/Texture.h"
#include "renderer/gl/VertexLayout.h"

#include <array>
#include <sstream>
#include <vector>
#include <filesystem>
#include <fstream>
#include <algorithm>

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

} // namespace

SelfCheckResult runSamplerSelfCheck(
    ux3d::renderer::gl::GL* gl,
    bool forceFailure,
    const std::string& testCase,
    const std::string& imageDumpPath,
    ux3d::renderer::Renderer* glRenderer,
    std::function<bool(const std::string&)> reloadAndDraw
)
{
    SelfCheckResult result;
    result.success = true;
    std::ostringstream report;
    report << "P3.2 GL sampler objects, fallback, and production coverage self-check\n";
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
        check(false, "forced sampler self-check failure requested");
        result.success = false;
        result.report = report.str();
        return result;
    }

    if (gl == nullptr) {
        check(false, "GL wrapper pointer is null");
        result.success = false;
        result.report = report.str();
        return result;
    }

    const bool runAll = (testCase.empty() || testCase == "all");
    const bool runFallback = runAll || (testCase == "fallback");
    const bool runUnit = runAll || (testCase == "unit");
    const bool runProduction = runAll || (testCase == "production-draw");
    const bool runBoundary = runAll || (testCase == "boundary");
    const bool runLifecycle = runAll || (testCase == "lifecycle");

    // ------------------------------------------------------------------
    // 1. Finding 1 Regression Check: Stale Sampler in Zero-Handle Fallback
    // ------------------------------------------------------------------
    if (runFallback) {
        report << "[Section 1] Zero-handle fallback stale sampler regression check\n";

        // Create a test texture with texture-owned parameters
        GLuint fallbackTex = 0;
        glGenTextures(1, &fallbackTex);
        glBindTexture(GL_TEXTURE_2D, fallbackTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Create a stale sampler with deliberately different parameters (Linear, Repeat)
        GLuint staleSampler = 0;
        gl->genSamplers(1, &staleSampler);
        gl->samplerParameteri(staleSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl->samplerParameteri(staleSampler, GL_TEXTURE_WRAP_S, GL_REPEAT);

        // Bind the stale sampler to texture unit 0
        gl->activeTexture(GL_TEXTURE0);
        gl->bindSampler(0, staleSampler);
        GLint initialBound = -1;
        glGetIntegerv(GL_SAMPLER_BINDING, &initialBound);
        check(initialBound == static_cast<GLint>(staleSampler), "Fallback test: stale sampler initially bound to unit 0");

        // Construct a production gl::Sampler requesting Nearest / ClampToEdge
        ux3d::renderer::gl::Texture texWrapper(gl, fallbackTex, ux3d::renderer::gl::TextureTarget::Texture2d);
        ux3d::renderer::gl::SamplerState requestedState;
        requestedState.minFilter = ux3d::renderer::gl::TextureMinFilter::Nearest;
        requestedState.magFilter = ux3d::renderer::gl::TextureMagFilter::Nearest;
        requestedState.wrapS = ux3d::renderer::gl::TextureWrap::ClampToEdge;
        requestedState.wrapT = ux3d::renderer::gl::TextureWrap::ClampToEdge;
        requestedState.wrapR = ux3d::renderer::gl::TextureWrap::ClampToEdge;

        ux3d::renderer::gl::Sampler prodSampler("u_FallbackTestSampler", &texWrapper, &requestedState);
        ux3d::renderer::gl::UniformBinding dummyBinding;
        dummyBinding.location = 0;

        // Execute production commit with samplerHandle == 0 (fallback path)
        prodSampler.commit(gl, dummyBinding, 0, 0);

        // Verify that unit 0 was explicitly unbound (GL_SAMPLER_BINDING is now 0)
        GLint postFallbackBound = -1;
        glGetIntegerv(GL_SAMPLER_BINDING, &postFallbackBound);
        check(postFallbackBound == 0, "Fallback commit explicitly unbound stale sampler (GL_SAMPLER_BINDING is 0)");

        // Verify that the requested fallback texture parameters were applied to the texture object
        GLint verifiedMin = 0, verifiedWrap = 0;
        glBindTexture(GL_TEXTURE_2D, fallbackTex);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &verifiedMin);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &verifiedWrap);
        check(verifiedMin == GL_NEAREST, "Fallback texture parameter GL_TEXTURE_MIN_FILTER is GL_NEAREST");
        check(verifiedWrap == GL_CLAMP_TO_EDGE, "Fallback texture parameter GL_TEXTURE_WRAP_S is GL_CLAMP_TO_EDGE");

        check(gl->getError() == GL_NO_ERROR, "Fallback regression check completed without GL errors");

        // Cleanup fallback resources
        gl->bindSampler(0, 0);
        gl->bindTexture(GL_TEXTURE_2D, 0);
        gl->deleteSamplers(1, &staleSampler);
        gl->deleteTextures(1, &fallbackTex);
    }

    // ------------------------------------------------------------------
    // 2. Unit / Low-level Cache & Hashing Checks
    // ------------------------------------------------------------------
    if (runUnit) {
        report << "[Section 2] SamplerState key comparison, hashing, and cache lookup\n";

        ux3d::renderer::gl::SamplerState s1;
        ux3d::renderer::gl::SamplerState s2;
        check(s1 == s2, "Identical SamplerState compare equal");
        check(!(s1 != s2), "Identical SamplerState do not compare unequal");
        check(ux3d::core::Hash<ux3d::renderer::gl::SamplerState>{}(s1) == ux3d::core::Hash<ux3d::renderer::gl::SamplerState>{}(s2),
              "Identical SamplerState produce matching hashes");

        ux3d::renderer::gl::SamplerState s_minFilter = s1;
        s_minFilter.minFilter = ux3d::renderer::gl::TextureMinFilter::Nearest;
        check(s1 != s_minFilter, "Differing minFilter compares unequal");
        check(ux3d::core::Hash<ux3d::renderer::gl::SamplerState>{}(s1) != ux3d::core::Hash<ux3d::renderer::gl::SamplerState>{}(s_minFilter),
              "Differing minFilter produces distinct hash");

        ux3d::renderer::gl::SamplerState s_wrapS = s1;
        s_wrapS.wrapS = ux3d::renderer::gl::TextureWrap::ClampToEdge;
        check(s1 != s_wrapS, "Differing wrapS compares unequal");

        ux3d::renderer::gl::SamplerCache cache(gl);
        check(cache.size() == 0, "Initial SamplerCache size is 0");

        GLuint h1 = cache.getOrCreateSampler(s1);
        check(h1 != 0, "First sampler handle is non-zero");
        check(cache.size() == 1, "Cache size is 1 after first allocation");

        GLuint h1_dup = cache.getOrCreateSampler(s1);
        check(h1 == h1_dup, "Deduplication: repeat query with same state returns identical handle");
        check(cache.size() == 1, "Cache size remains 1 on repeat query");

        GLuint h_min = cache.getOrCreateSampler(s_minFilter);
        check(h_min != 0 && h_min != h1, "Distinct minFilter produces a distinct GL sampler handle");
        check(cache.size() == 2, "Cache size increased to 2 for distinct minFilter");

        // Driver parameter verification
        GLint qMin = 0, qWrap = 0;
        gl->getSamplerParameteriv(h1, GL_TEXTURE_MIN_FILTER, &qMin);
        check(qMin == static_cast<GLint>(s1.minFilter), "h1 GL_TEXTURE_MIN_FILTER query matches state");
        gl->getSamplerParameteriv(h_min, GL_TEXTURE_MIN_FILTER, &qMin);
        check(qMin == GL_NEAREST, "h_min GL_TEXTURE_MIN_FILTER query matches GL_NEAREST");

        if (gl->supportsAnisotropicFiltering()) {
            GLfloat qAniso = 0.0f;
            gl->getSamplerParameterfv(h1, GL_TEXTURE_MAX_ANISOTROPY_EXT, &qAniso);
            check(qAniso == gl->maxTextureMaxAnisotropy(), "h1 anisotropy matches context maximum");
        }

        cache.clear();
        check(cache.size() == 0, "SamplerCache cleared");
    }

    // ------------------------------------------------------------------
    // 3. Finding 2A: Production Draw Sampling Integration
    // ------------------------------------------------------------------
    if (runProduction) {
        report << "[Section 3] Production draw sampling integration (dual unit, successive, filters, wraps, mips)\n";

        // Setup test shaders: supports dual sampler sampling, mip sampling, wrap mode UV transformation, and mode switching
        const char* vsSrc =
            "#version 330 core\n"
            "layout(location = 0) in vec2 a_pos;\n"
            "layout(location = 1) in vec2 a_uv;\n"
            "out vec2 v_uv;\n"
            "uniform vec2 u_uvScale;\n"
            "uniform vec2 u_uvOffset;\n"
            "void main() {\n"
            "    v_uv = a_uv * u_uvScale + u_uvOffset;\n"
            "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
            "}\n";

        const char* fsSrc =
            "#version 330 core\n"
            "in vec2 v_uv;\n"
            "out vec4 FragColor;\n"
            "uniform sampler2D u_Sampler0;\n"
            "uniform sampler2D u_Sampler1;\n"
            "uniform int u_Mode;\n"
            "void main() {\n"
            "    if (u_Mode == 0) {\n"
            "        FragColor = texture(u_Sampler0, v_uv);\n"
            "    } else if (u_Mode == 1) {\n"
            "        FragColor = texture(u_Sampler1, v_uv);\n"
            "    } else if (u_Mode == 2) {\n"
            "        vec4 c0 = texture(u_Sampler0, v_uv);\n"
            "        vec4 c1 = texture(u_Sampler1, v_uv);\n"
            "        FragColor = vec4(c0.r, c1.g, c0.b, 1.0);\n"
            "    } else if (u_Mode == 3) {\n"
            "        FragColor = textureLod(u_Sampler0, v_uv, 1.0);\n"
            "    } else {\n"
            "        FragColor = vec4(0.0);\n"
            "    }\n"
            "}\n";

        GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
        GLuint progHandle = linkProgram(vs, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);
        check(progHandle != 0, "Compiled and linked production test shader program");

        GLint locMode = glGetUniformLocation(progHandle, "u_Mode");
        GLint locUvScale = glGetUniformLocation(progHandle, "u_uvScale");
        GLint locUvOffset = glGetUniformLocation(progHandle, "u_uvOffset");
        glUseProgram(progHandle);
        glUniform2f(locUvScale, 1.0f, 1.0f);
        glUniform2f(locUvOffset, 0.0f, 0.0f);

        // Create patterned texture: 2x2 with mipmaps
        // Level 0 (2x2):
        //   (0,0)=Red [255, 0, 0, 255],     (1,0)=Green [0, 255, 0, 255]
        //   (0,1)=Blue [0, 0, 255, 255],    (1,1)=White [255, 255, 255, 255]
        // Level 1 (1x1):
        //   (0,0)=Yellow [255, 255, 0, 255]
        GLuint patternTex = 0;
        glGenTextures(1, &patternTex);
        glBindTexture(GL_TEXTURE_2D, patternTex);

        const uint8_t level0[16] = {
            255, 0, 0, 255,      0, 255, 0, 255,
            0, 0, 255, 255,      255, 255, 255, 255
        };
        const uint8_t level1[4] = {
            255, 255, 0, 255
        };

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, level0);
        glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, level1);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Offscreen 64x64 RGBA8 Framebuffer
        const int fboW = 64, fboH = 64;
        GLuint fbo = 0, fboColor = 0;
        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &fboColor);
        glBindTexture(GL_TEXTURE_2D, fboColor);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fboW, fboH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboColor, 0);
        check(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Test FBO complete");

        // Quad VBO and VAO: covering [-1, 1] with UV [0, 1]
        const float quadVerts[] = {
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f,

            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f,
            -1.0f,  1.0f,  0.0f, 1.0f,
        };
        GLuint vaoHandle = 0, vboHandle = 0;
        glGenVertexArrays(1, &vaoHandle);
        glGenBuffers(1, &vboHandle);
        glBindVertexArray(vaoHandle);
        glBindBuffer(GL_ARRAY_BUFFER, vboHandle);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
        glBindVertexArray(0);

        // Wrap into engine objects
        auto prog = std::make_unique<ux3d::renderer::gl::Program>(gl, progHandle);
        ux3d::renderer::gl::Texture glPatternTex(gl, patternTex, ux3d::renderer::gl::TextureTarget::Texture2d);
        auto vaoLayout = std::make_shared<ux3d::renderer::gl::VertexLayout>(gl);
        vaoLayout->handle = vaoHandle;

        ux3d::renderer::CommandExecutor executor(gl);
        executor.init();

        // Sampler states
        ux3d::renderer::gl::SamplerState stateNearest;
        stateNearest.minFilter = ux3d::renderer::gl::TextureMinFilter::Nearest;
        stateNearest.magFilter = ux3d::renderer::gl::TextureMagFilter::Nearest;
        stateNearest.wrapS = ux3d::renderer::gl::TextureWrap::ClampToEdge;
        stateNearest.wrapT = ux3d::renderer::gl::TextureWrap::ClampToEdge;

        ux3d::renderer::gl::SamplerState stateLinear;
        stateLinear.minFilter = ux3d::renderer::gl::TextureMinFilter::Linear;
        stateLinear.magFilter = ux3d::renderer::gl::TextureMagFilter::Linear;
        stateLinear.wrapS = ux3d::renderer::gl::TextureWrap::ClampToEdge;
        stateLinear.wrapT = ux3d::renderer::gl::TextureWrap::ClampToEdge;

        ux3d::renderer::gl::SamplerState stateMipLinear;
        stateMipLinear.minFilter = ux3d::renderer::gl::TextureMinFilter::LinearMipmapLinear;
        stateMipLinear.magFilter = ux3d::renderer::gl::TextureMagFilter::Linear;
        stateMipLinear.wrapS = ux3d::renderer::gl::TextureWrap::ClampToEdge;
        stateMipLinear.wrapT = ux3d::renderer::gl::TextureWrap::ClampToEdge;

        ux3d::renderer::gl::SamplerState stateRepeat;
        stateRepeat.minFilter = ux3d::renderer::gl::TextureMinFilter::Nearest;
        stateRepeat.magFilter = ux3d::renderer::gl::TextureMagFilter::Nearest;
        stateRepeat.wrapS = ux3d::renderer::gl::TextureWrap::Repeat;
        stateRepeat.wrapT = ux3d::renderer::gl::TextureWrap::Repeat;

        ux3d::renderer::gl::SamplerState stateClamp;
        stateClamp.minFilter = ux3d::renderer::gl::TextureMinFilter::Nearest;
        stateClamp.magFilter = ux3d::renderer::gl::TextureMagFilter::Nearest;
        stateClamp.wrapS = ux3d::renderer::gl::TextureWrap::ClampToEdge;
        stateClamp.wrapT = ux3d::renderer::gl::TextureWrap::ClampToEdge;

        ux3d::renderer::gl::SamplerState stateMirroredRepeat;
        stateMirroredRepeat.minFilter = ux3d::renderer::gl::TextureMinFilter::Nearest;
        stateMirroredRepeat.magFilter = ux3d::renderer::gl::TextureMagFilter::Nearest;
        stateMirroredRepeat.wrapS = ux3d::renderer::gl::TextureWrap::MirroredRepeat;
        stateMirroredRepeat.wrapT = ux3d::renderer::gl::TextureWrap::MirroredRepeat;

        ux3d::renderer::gl::Sampler s0_nearest("u_Sampler0", &glPatternTex, &stateNearest);
        ux3d::renderer::gl::Sampler s1_linear("u_Sampler1", &glPatternTex, &stateLinear);
        ux3d::renderer::gl::Sampler s0_linear("u_Sampler0", &glPatternTex, &stateLinear);
        ux3d::renderer::gl::Sampler s0_mip("u_Sampler0", &glPatternTex, &stateMipLinear);
        ux3d::renderer::gl::Sampler s0_repeat("u_Sampler0", &glPatternTex, &stateRepeat);
        ux3d::renderer::gl::Sampler s0_clamp("u_Sampler0", &glPatternTex, &stateClamp);
        ux3d::renderer::gl::Sampler s0_mirror("u_Sampler0", &glPatternTex, &stateMirroredRepeat);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, fboW, fboH);

        // --- Test 2A.1: Dual Units on SAME Texture in ONE Production Draw ---
        glUseProgram(progHandle);
        glUniform1i(locMode, 2); // Mode 2: R from Sampler0 (Nearest), G from Sampler1 (Linear)

        ux3d::renderer::Command drawDual(gl, ux3d::renderer::CommandTag::Draw);
        drawDual.program = prog.get();
        drawDual.vertexLayout = vaoLayout;
        drawDual.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::TriangleList;
        drawDual.primitiveCount = 6;
        drawDual.culling = ux3d::renderer::gl::Culling::None;
        drawDual.samplers = { &s0_nearest, &s1_linear };

        executor.execute(drawDual);

        // Read back center pixel (32, 32)
        uint8_t pixel[4] = { 0 };
        glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);

        // Unit 0 (Nearest): UV (0.5, 0.5) lands on a single quadrant texel (e.g. Red, R=255)
        // Unit 1 (Linear): UV (0.5, 0.5) blends all 4 quadrant texels, so G is blended (~64-128)
        check(pixel[0] == 255, "Dual-unit draw: Unit 0 sampled with Nearest (pixel[0] == 255)");
        check(pixel[1] > 30 && pixel[1] < 160, "Dual-unit draw: Unit 1 sampled with Linear blend simultaneously (pixel[1] blended)");

        // Verify that GL_SAMPLER_BINDING on Unit 0 and Unit 1 hold distinct sampler handles
        GLint b0 = 0, b1 = 0;
        gl->activeTexture(GL_TEXTURE0);
        glGetIntegerv(GL_SAMPLER_BINDING, &b0);
        gl->activeTexture(GL_TEXTURE1);
        glGetIntegerv(GL_SAMPLER_BINDING, &b1);
        check(b0 != 0 && b1 != 0 && b0 != b1, "Dual-unit draw: units 0 and 1 hold distinct active sampler objects");

        // --- Test 2A.2: Successive Draws Alternating Samplers through CommandExecutor ---
        // Draw A: Unit 0 with Nearest, Mode 0
        glUniform1i(locMode, 0);
        ux3d::renderer::Command drawA(gl, ux3d::renderer::CommandTag::Draw);
        drawA.program = prog.get();
        drawA.vertexLayout = vaoLayout;
        drawA.primitiveCount = 6;
        drawA.culling = ux3d::renderer::gl::Culling::None;
        drawA.samplers = { &s0_nearest };
        executor.execute(drawA);

        uint8_t pixelA_q0[4] = { 0 };
        glReadPixels(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixelA_q0);
        uint8_t pixelA_center[4] = { 0 };
        glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixelA_center);
        check(pixelA_q0[0] == 255 && pixelA_q0[1] == 0 && pixelA_q0[2] == 0 &&
              pixelA_center[0] == 255 && pixelA_center[1] == 255 && pixelA_center[2] == 255,
              "Successive Draw A: Nearest filter verified (pure unblended quadrant and unblended center)");

        // Draw B: Unit 0 with Linear, Mode 0 (same unit, same texture, different sampler)
        ux3d::renderer::Command drawB(gl, ux3d::renderer::CommandTag::Draw);
        drawB.program = prog.get();
        drawB.vertexLayout = vaoLayout;
        drawB.primitiveCount = 6;
        drawB.culling = ux3d::renderer::gl::Culling::None;
        drawB.samplers = { &s0_linear };
        executor.execute(drawB);

        uint8_t pixelB[4] = { 0 };
        glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixelB);
        check(pixelB[0] > 30 && pixelB[0] < 160 && pixelB[1] > 30 && pixelB[1] < 160,
              "Successive Draw B: Linear filter verified (blended center pixels)");

        // --- Test 2A.3: Mipmap Filter Test (LinearMipmapLinear vs Linear) ---
        // Draw with Mode 3 (textureLod 1.0)
        glUniform1i(locMode, 3);

        // With stateMipLinear: samples Level 1 (Yellow [255, 255, 0])
        ux3d::renderer::Command drawMip(gl, ux3d::renderer::CommandTag::Draw);
        drawMip.program = prog.get();
        drawMip.vertexLayout = vaoLayout;
        drawMip.primitiveCount = 6;
        drawMip.culling = ux3d::renderer::gl::Culling::None;
        drawMip.samplers = { &s0_mip };
        executor.execute(drawMip);

        uint8_t pixelMip[4] = { 0 };
        glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixelMip);
        check(pixelMip[0] > 200 && pixelMip[1] > 200 && pixelMip[2] < 30,
              "Mipmap draw: LinearMipmapLinear sampled Level 1 (Yellow [255, 255, 0])");

        // With stateLinear: mipmaps disabled on sampler, samples Level 0 (blended center, Blue > 30)
        ux3d::renderer::Command drawNoMip(gl, ux3d::renderer::CommandTag::Draw);
        drawNoMip.program = prog.get();
        drawNoMip.vertexLayout = vaoLayout;
        drawNoMip.primitiveCount = 6;
        drawNoMip.culling = ux3d::renderer::gl::Culling::None;
        drawNoMip.samplers = { &s0_linear };
        executor.execute(drawNoMip);

        uint8_t pixelNoMip[4] = { 0 };
        glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixelNoMip);
        check(pixelNoMip[2] > 30, "No-mip draw: Linear sampler without mip filtering sampled Level 0 (Blue component present)");

        // --- Test 2A.4: Wrap Mode Sampling Coverage (Repeat, ClampToEdge, MirroredRepeat) ---
        // Shift UV coordinates outside [0, 1] into [1.0, 2.0]
        // Pixel (16, 16): U = 1.0 + 16.5/64 ~= 1.258, V = 16.5/64 ~= 0.258
        // Pixel (48, 16): U = 1.0 + 48.5/64 ~= 1.758, V = 16.5/64 ~= 0.258
        glUniform1i(locMode, 0);
        glUniform2f(locUvScale, 1.0f, 1.0f);
        glUniform2f(locUvOffset, 1.0f, 0.0f);

        // 1. Repeat: U=1.258 wraps to 0.258 (texel 0, Red), U=1.758 wraps to 0.758 (texel 1, Green)
        ux3d::renderer::Command drawRepeat(gl, ux3d::renderer::CommandTag::Draw);
        drawRepeat.program = prog.get();
        drawRepeat.vertexLayout = vaoLayout;
        drawRepeat.primitiveCount = 6;
        drawRepeat.culling = ux3d::renderer::gl::Culling::None;
        drawRepeat.samplers = { &s0_repeat };
        executor.execute(drawRepeat);

        uint8_t pixRepeat_16[4] = { 0 }, pixRepeat_48[4] = { 0 };
        glReadPixels(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixRepeat_16);
        glReadPixels(48, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixRepeat_48);
        check(pixRepeat_16[0] == 255 && pixRepeat_16[1] == 0 && pixRepeat_16[2] == 0,
              "Wrap Mode Repeat: U=1.25 wraps to U=0.25 (pure Red [255, 0, 0])");
        check(pixRepeat_48[0] == 0 && pixRepeat_48[1] == 255 && pixRepeat_48[2] == 0,
              "Wrap Mode Repeat: U=1.75 wraps to U=0.75 (pure Green [0, 255, 0])");

        // 2. ClampToEdge: both U=1.258 and U=1.758 clamp to edge texel (texel 1, Green)
        ux3d::renderer::Command drawClamp(gl, ux3d::renderer::CommandTag::Draw);
        drawClamp.program = prog.get();
        drawClamp.vertexLayout = vaoLayout;
        drawClamp.primitiveCount = 6;
        drawClamp.culling = ux3d::renderer::gl::Culling::None;
        drawClamp.samplers = { &s0_clamp };
        executor.execute(drawClamp);

        uint8_t pixClamp_16[4] = { 0 }, pixClamp_48[4] = { 0 };
        glReadPixels(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixClamp_16);
        glReadPixels(48, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixClamp_48);
        check(pixClamp_16[0] == 0 && pixClamp_16[1] == 255 && pixClamp_16[2] == 0,
              "Wrap Mode ClampToEdge: U=1.25 clamps to edge (pure Green [0, 255, 0])");
        check(pixClamp_48[0] == 0 && pixClamp_48[1] == 255 && pixClamp_48[2] == 0,
              "Wrap Mode ClampToEdge: U=1.75 clamps to edge (pure Green [0, 255, 0])");

        // 3. MirroredRepeat: U=1.258 reflects to 0.742 (texel 1, Green), U=1.758 reflects to 0.242 (texel 0, Red)
        ux3d::renderer::Command drawMirror(gl, ux3d::renderer::CommandTag::Draw);
        drawMirror.program = prog.get();
        drawMirror.vertexLayout = vaoLayout;
        drawMirror.primitiveCount = 6;
        drawMirror.culling = ux3d::renderer::gl::Culling::None;
        drawMirror.samplers = { &s0_mirror };
        executor.execute(drawMirror);

        uint8_t pixMirror_16[4] = { 0 }, pixMirror_48[4] = { 0 };
        glReadPixels(16, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixMirror_16);
        glReadPixels(48, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixMirror_48);
        check(pixMirror_16[0] == 0 && pixMirror_16[1] == 255 && pixMirror_16[2] == 0,
              "Wrap Mode MirroredRepeat: U=1.25 reflects to U=0.74 (pure Green [0, 255, 0])");
        check(pixMirror_48[0] == 255 && pixMirror_48[1] == 0 && pixMirror_48[2] == 0,
              "Wrap Mode MirroredRepeat: U=1.75 reflects to U=0.24 (pure Red [255, 0, 0])");

        // Restore standard [0, 1] UVs and re-render drawB for representative dump
        glUniform2f(locUvOffset, 0.0f, 0.0f);
        executor.execute(drawB);

        // Dump representative patterned frame if requested
        if (!imageDumpPath.empty()) {
            std::vector<uint8_t> dumpBuffer(fboW * fboH * 4);
            glReadPixels(0, 0, fboW, fboH, GL_RGBA, GL_UNSIGNED_BYTE, dumpBuffer.data());
            // Flip vertically for standard PNG orientation
            std::vector<uint8_t> flipped(fboW * fboH * 4);
            for (int y = 0; y < fboH; ++y) {
                std::memcpy(&flipped[y * fboW * 4], &dumpBuffer[(fboH - 1 - y) * fboW * 4], fboW * 4);
            }
            int ok = stbi_write_png(imageDumpPath.c_str(), fboW, fboH, 4, flipped.data(), fboW * 4);
            check(ok != 0, "Representative patterned test frame dumped to " + imageDumpPath);
        }

        check(gl->getError() == GL_NO_ERROR, "Production draw sampling completed without GL errors");

        // Cleanup production draw test resources
        executor.resetState();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &fboColor);
        glDeleteVertexArrays(1, &vaoHandle);
        glDeleteBuffers(1, &vboHandle);
        glDeleteTextures(1, &patternTex);
    }

    // ------------------------------------------------------------------
    // 4. Finding 2B: Automatic Boundary Cleanup Verification
    // ------------------------------------------------------------------
    if (runBoundary) {
        report << "[Section 4] Automatic boundary cleanup verification (Renderer::draw & ExternalFunction boundaries)\n";

        ux3d::renderer::Renderer testRenderer(gl);
        testRenderer.init(ux3d::math::vec4(0.0f, 0.0f, 0.0f, 0.0f));

        // Create a dummy texture and sampler to bind through a production command
        GLuint boundTex = 0;
        glGenTextures(1, &boundTex);
        glBindTexture(GL_TEXTURE_2D, boundTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, 0);

        ux3d::renderer::gl::Texture glBoundTex(gl, boundTex, ux3d::renderer::gl::TextureTarget::Texture2d);
        ux3d::renderer::gl::SamplerState testState;
        testState.minFilter = ux3d::renderer::gl::TextureMinFilter::LinearMipmapLinear;
        ux3d::renderer::gl::Sampler testSampler0("u_BoundarySampler0", &glBoundTex, &testState);
        ux3d::renderer::gl::Sampler testSampler1("u_BoundarySampler1", &glBoundTex, &testState);

        // Compile minimal dummy program
        const char* dVs = "#version 330 core\nvoid main() { gl_Position = vec4(0.0); }\n";
        const char* dFs = "#version 330 core\nuniform sampler2D u_BoundarySampler0;\nuniform sampler2D u_BoundarySampler1;\nout vec4 c;\nvoid main() { c = texture(u_BoundarySampler0, vec2(0.0)) + texture(u_BoundarySampler1, vec2(0.0)); }\n";
        GLuint dvs = compileShader(GL_VERTEX_SHADER, dVs);
        GLuint dfs = compileShader(GL_FRAGMENT_SHADER, dFs);
        GLuint dProg = linkProgram(dvs, dfs);
        glDeleteShader(dvs);
        glDeleteShader(dfs);

        auto progWrapper = std::make_unique<ux3d::renderer::gl::Program>(gl, dProg);
        auto dummyVao = std::make_shared<ux3d::renderer::gl::VertexLayout>(gl);

        // Build a production draw command that binds samplers on units 0 and 1
        ux3d::renderer::Command drawCmd(gl, ux3d::renderer::CommandTag::Draw);
        drawCmd.program = progWrapper.get();
        drawCmd.vertexLayout = dummyVao;
        drawCmd.primitiveTopology = ux3d::renderer::gl::PrimitiveTopology::Points;
        drawCmd.primitiveCount = 1;
        drawCmd.samplers = { &testSampler0, &testSampler1 };

        // Test ExternalFunction boundary: samplers must be automatically unbound before the callback
        bool externalFunctionVerified = false;
        ux3d::renderer::Command extCmd = ux3d::renderer::Command::makeExternalFunctionCommand(gl, [&]() {
            GLint extB0 = -1, extB1 = -1;
            gl->activeTexture(GL_TEXTURE0);
            glGetIntegerv(GL_SAMPLER_BINDING, &extB0);
            gl->activeTexture(GL_TEXTURE1);
            glGetIntegerv(GL_SAMPLER_BINDING, &extB1);
            if (extB0 == 0 && extB1 == 0) {
                externalFunctionVerified = true;
            }
        });

        // Independent Test 1: ExternalFunction callback unbind
        testRenderer.preRenderCommands = { drawCmd, extCmd };
        bool extDrawOk = testRenderer.draw({});
        check(extDrawOk && externalFunctionVerified, "Automatic cleanup: GL_SAMPLER_BINDING was 0 inside ExternalFunction callback");

        // Independent Test 2: Renderer::draw exit unbind (NO ExternalFunction callback in command list)
        testRenderer.preRenderCommands = { drawCmd };
        bool exitDrawOk = testRenderer.draw({});
        check(exitDrawOk, "Independent exit cleanup: Renderer::draw succeeded");

        GLint postDrawB0 = -1, postDrawB1 = -1;
        gl->activeTexture(GL_TEXTURE0);
        glGetIntegerv(GL_SAMPLER_BINDING, &postDrawB0);
        gl->activeTexture(GL_TEXTURE1);
        glGetIntegerv(GL_SAMPLER_BINDING, &postDrawB1);
        check(postDrawB0 == 0, "Independent exit cleanup: GL_SAMPLER_BINDING is 0 on unit 0 after Renderer::draw exit");
        check(postDrawB1 == 0, "Independent exit cleanup: GL_SAMPLER_BINDING is 0 on unit 1 after Renderer::draw exit");

        // Small subsequent consumer using texture-owned parameters
        // If a scene sampler had leaked on unit 0, texture-owned sampling would be corrupted
        GLuint consumerTex = 0;
        glGenTextures(1, &consumerTex);
        glBindTexture(GL_TEXTURE_2D, consumerTex);
        const uint8_t redTexel[4] = { 255, 0, 0, 255 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, redTexel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, consumerTex);
        GLint consumerBound = -1;
        glGetIntegerv(GL_SAMPLER_BINDING, &consumerBound);
        check(consumerBound == 0, "Subsequent consumer: unit 0 has no lingering sampler binding");

        GLint consumerMin = 0;
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &consumerMin);
        check(consumerMin == GL_NEAREST, "Subsequent consumer: texture-owned GL_NEAREST intact and uncorrupted");

        check(gl->getError() == GL_NO_ERROR, "Boundary cleanup verification completed without GL errors");

        // Cleanup
        testRenderer.uninit();
        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteTextures(1, &consumerTex);
        glDeleteTextures(1, &boundTex);
    }

    // ------------------------------------------------------------------
    // 5. Finding 2C: Reload & Destruction Ownership Verification
    // ------------------------------------------------------------------
    if (runLifecycle) {
        report << "[Section 5A] SamplerCache deduplication and direct deletion unit check\n";

        ux3d::renderer::CommandExecutor executor(gl);
        executor.init();

        ux3d::renderer::gl::SamplerState sA, sB, sC, sD;
        sA.minFilter = ux3d::renderer::gl::TextureMinFilter::Nearest;
        sB.minFilter = ux3d::renderer::gl::TextureMinFilter::Linear;
        sC.wrapS = ux3d::renderer::gl::TextureWrap::ClampToEdge;
        sD.wrapT = ux3d::renderer::gl::TextureWrap::MirroredRepeat;

        // Unit Cycle 1: initial allocation of 4 known states in unit cache
        GLuint hA = executor.getSamplerCache().getOrCreateSampler(sA);
        GLuint hB = executor.getSamplerCache().getOrCreateSampler(sB);
        GLuint hC = executor.getSamplerCache().getOrCreateSampler(sC);
        GLuint hD = executor.getSamplerCache().getOrCreateSampler(sD);
        check(executor.getSamplerCache().size() == 4, "Unit Cycle 1: 4 distinct sampler states allocated in unit cache");
        check(gl->isSampler(hA) == GL_TRUE, "Unit check: hA is valid GL sampler object via glIsSampler");
        check(gl->isSampler(hB) == GL_TRUE, "Unit check: hB is valid GL sampler object via glIsSampler");
        check(gl->isSampler(hC) == GL_TRUE, "Unit check: hC is valid GL sampler object via glIsSampler");
        check(gl->isSampler(hD) == GL_TRUE, "Unit check: hD is valid GL sampler object via glIsSampler");

        // Unit Cycle 2: Repeat query requesting sA, sB, sC
        GLuint hA2 = executor.getSamplerCache().getOrCreateSampler(sA);
        GLuint hB2 = executor.getSamplerCache().getOrCreateSampler(sB);
        GLuint hC2 = executor.getSamplerCache().getOrCreateSampler(sC);
        check(hA2 == hA && hB2 == hB && hC2 == hC, "Unit Cycle 2 (repeat): identical GL handles returned without reallocation");
        check(executor.getSamplerCache().size() == 4, "Unit Cycle 2 (repeat): cache size stabilized at 4 (zero growth)");

        // Unit Cycle 3: Second repeat query requesting sA, sD
        GLuint hA3 = executor.getSamplerCache().getOrCreateSampler(sA);
        GLuint hD3 = executor.getSamplerCache().getOrCreateSampler(sD);
        check(hA3 == hA && hD3 == hD, "Unit Cycle 3 (repeat): identical GL handles returned");
        check(executor.getSamplerCache().size() == 4, "Unit Cycle 3 (repeat): cache size remains 4");

        // Direct SamplerCache::clear check
        executor.getSamplerCache().clear();
        check(executor.getSamplerCache().size() == 0, "Unit check: direct SamplerCache::clear emptied container");

        check(gl->isSampler(hA) == GL_FALSE, "Unit check: hA confirmed destroyed from driver via glIsSampler");
        check(gl->isSampler(hB) == GL_FALSE, "Unit check: hB confirmed destroyed from driver via glIsSampler");
        check(gl->isSampler(hC) == GL_FALSE, "Unit check: hC confirmed destroyed from driver via glIsSampler");
        check(gl->isSampler(hD) == GL_FALSE, "Unit check: hD confirmed destroyed from driver via glIsSampler");

        // ------------------------------------------------------------------
        // Section 5B: Production Model Reload & Renderer::uninit Integration Check
        // ------------------------------------------------------------------
        report << "[Section 5B] Production model reload cycles and Renderer::uninit teardown integration check\n";
        if (glRenderer != nullptr && reloadAndDraw != nullptr) {
            std::filesystem::create_directories("build/p3.2");
            const std::string fixturePath = "build/p3.2/fixture_samplers.gltf";

            // Small self-contained glTF fixture with 3 known sampler states
            const char* fixtureGltf =
                "{\n"
                "  \"asset\": {\"version\": \"2.0\"},\n"
                "  \"samplers\": [\n"
                "    {\"magFilter\": 9728, \"minFilter\": 9728, \"wrapS\": 33071, \"wrapT\": 33071},\n"
                "    {\"magFilter\": 9729, \"minFilter\": 9987, \"wrapS\": 10497, \"wrapT\": 10497},\n"
                "    {\"magFilter\": 9729, \"minFilter\": 9729, \"wrapS\": 33648, \"wrapT\": 33648}\n"
                "  ],\n"
                "  \"textures\": [\n"
                "    {\"sampler\": 0, \"source\": 0},\n"
                "    {\"sampler\": 1, \"source\": 0},\n"
                "    {\"sampler\": 2, \"source\": 0}\n"
                "  ],\n"
                "  \"images\": [\n"
                "    {\"uri\": \"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==\"}\n"
                "  ],\n"
                "  \"materials\": [\n"
                "    {\n"
                "      \"pbrMetallicRoughness\": {\n"
                "        \"baseColorTexture\": {\"index\": 0},\n"
                "        \"metallicRoughnessTexture\": {\"index\": 1}\n"
                "      },\n"
                "      \"normalTexture\": {\"index\": 2}\n"
                "    }\n"
                "  ],\n"
                "  \"meshes\": [\n"
                "    {\"primitives\": [{\"attributes\": {\"POSITION\": 0}, \"material\": 0}]}\n"
                "  ],\n"
                "  \"nodes\": [{\"mesh\": 0}],\n"
                "  \"scenes\": [{\"nodes\": [0]}],\n"
                "  \"scene\": 0,\n"
                "  \"buffers\": [\n"
                "    {\"uri\": \"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA==\", \"byteLength\": 36}\n"
                "  ],\n"
                "  \"bufferViews\": [\n"
                "    {\"buffer\": 0, \"byteOffset\": 0, \"byteLength\": 36, \"target\": 34962}\n"
                "  ],\n"
                "  \"accessors\": [\n"
                "    {\"bufferView\": 0, \"byteOffset\": 0, \"componentType\": 5126, \"count\": 3, \"type\": \"VEC3\", \"max\": [1.0, 1.0, 0.0], \"min\": [0.0, 0.0, 0.0]}\n"
                "  ]\n"
                "}\n";

            {
                std::ofstream ofs(fixturePath);
                ofs << fixtureGltf;
            }

            // Integration Cycle 1
            bool ok1 = reloadAndDraw(fixturePath);
            check(ok1, "Integration Cycle 1: loadGltfModel and draw succeeded");
            size_t count1 = glRenderer->getSamplerCache().size();
            check(count1 >= 2, "Integration Cycle 1: production SamplerCache holds allocated GL samplers for fixture");

            std::vector<GLuint> capturedHandles;
            for (const auto& [st, handle] : glRenderer->getSamplerCache().getSamplers()) {
                capturedHandles.push_back(handle);
                check(gl->isSampler(handle) == GL_TRUE, "Integration Cycle 1: handle " + std::to_string(handle) + " is valid GL sampler");
            }

            // Integration Cycle 2 (Reload)
            bool ok2 = reloadAndDraw(fixturePath);
            check(ok2, "Integration Cycle 2: production reload and draw succeeded");
            check(glRenderer->getSamplerCache().size() == count1,
                  "Integration Cycle 2 (reload): production cache size stabilized at " + std::to_string(count1) + " (zero unbounded growth)");
            for (const auto& [st, handle] : glRenderer->getSamplerCache().getSamplers()) {
                check(std::find(capturedHandles.begin(), capturedHandles.end(), handle) != capturedHandles.end(),
                      "Integration Cycle 2 (reload): GL sampler handle " + std::to_string(handle) + " retained and reused across reload");
            }

            // Integration Cycle 3 (Reload)
            bool ok3 = reloadAndDraw(fixturePath);
            check(ok3, "Integration Cycle 3: production reload and draw succeeded");
            check(glRenderer->getSamplerCache().size() == count1,
                  "Integration Cycle 3 (reload): production cache size remains stabilized at " + std::to_string(count1));

            // Teardown through Renderer::uninit()
            glRenderer->uninit();
            check(glRenderer->getSamplerCache().size() == 0, "Renderer::uninit cleared production SamplerCache");
            for (GLuint handle : capturedHandles) {
                check(gl->isSampler(handle) == GL_FALSE,
                      "Renderer::uninit deleted sampler handle " + std::to_string(handle) + " from driver (glIsSampler returned false)");
            }
        } else {
            report << "  NOTE: Production reload integration skipped (glRenderer or reloadAndDraw was null)\n";
        }

        check(gl->getError() == GL_NO_ERROR, "Lifecycle ownership verification completed without GL errors");
    }

    result.success = failures.empty();
    report << "Checks passed: " << (checks - failures.size()) << " / " << checks << "\n";
    report << "Result: " << (result.success ? "PASS" : "FAIL") << "\n";
    for (const auto& fail : failures) {
        report << "Failure detail: " << fail << "\n";
    }
    result.report = report.str();
    return result;
}

} // namespace ux3d::imgui
