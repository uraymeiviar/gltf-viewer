// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/ComputeTest.h"
#include <glad/glad.h>
#include "imgui/GlCaps.h"

#include <cstdint>
#include <sstream>
#include <vector>

namespace ux3d::imgui {

SelfCheckResult runComputeSelfCheck(bool forceFailure)
{
    SelfCheckResult result;
    result.success = true;
    std::ostringstream report;
    report << "GL 4.6 compute smoke self-check\n";

    const GlCaps& caps = glCaps();
    report << "Context: OpenGL " << caps.major << "." << caps.minor << "\n";
    report << "Compute available: " << (caps.compute ? "yes" : "no") << "\n";
    report << "GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS: " << caps.maxComputeWorkGroupInvocations << "\n";

    size_t checks = 0;
    size_t failures = 0;
    auto check = [&](bool condition, const std::string& desc) {
        ++checks;
        if (!condition) {
            ++failures;
            result.success = false;
            report << "  FAIL: " << desc << "\n";
        } else {
            report << "  PASS: " << desc << "\n";
        }
    };

    if (forceFailure) {
        check(false, "forced compute self-check failure requested");
    } else if (!caps.compute) {
        report << "Result: skipped (compute shaders need an OpenGL 4.3+ context)\n";
        result.report = report.str();
        return result;
    } else {
        constexpr int kWidth = 16;
        constexpr int kHeight = 8;
        const char* source =
            "#version 430\n"
            "layout(local_size_x = 8, local_size_y = 4) in;\n"
            "layout(r32ui, binding = 0) writeonly uniform uimage2D u_image;\n"
            "void main() {\n"
            "    ivec2 p = ivec2(gl_GlobalInvocationID.xy);\n"
            "    imageStore(u_image, p, uvec4(uint(p.x) * 1000u + uint(p.y) + 7u, 0u, 0u, 0u));\n"
            "}\n";

        while (glGetError() != GL_NO_ERROR) {
        }
        const GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        GLint status = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (!status) {
            char log[512] = {};
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            report << "  shader log: " << log << "\n";
        }
        check(status != 0, "compute shader compiles");

        GLuint program = 0;
        GLuint texture = 0;
        if (status) {
            program = glCreateProgram();
            glAttachShader(program, shader);
            glLinkProgram(program);
            status = 0;
            glGetProgramiv(program, GL_LINK_STATUS, &status);
            if (!status) {
                char log[512] = {};
                glGetProgramInfoLog(program, sizeof(log), nullptr, log);
                report << "  program log: " << log << "\n";
            }
            check(status != 0, "compute program links");

            if (status) {
                glGenTextures(1, &texture);
                glBindTexture(GL_TEXTURE_2D, texture);
                glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32UI, kWidth, kHeight);
                glBindImageTexture(0, texture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32UI);
                glUseProgram(program);
                glDispatchCompute(kWidth / 8, kHeight / 4, 1);
                glMemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

                std::vector<uint32_t> pixels(static_cast<size_t>(kWidth) * kHeight, 0xFFFFFFFFu);
                glPixelStorei(GL_PACK_ALIGNMENT, 4);
                glGetTexImage(GL_TEXTURE_2D, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, pixels.data());

                size_t wrong = 0;
                for (int y = 0; y < kHeight; ++y) {
                    for (int x = 0; x < kWidth; ++x) {
                        const uint32_t expected = static_cast<uint32_t>(x) * 1000u + static_cast<uint32_t>(y) + 7u;
                        if (pixels[static_cast<size_t>(y) * kWidth + x] != expected) {
                            ++wrong;
                        }
                    }
                }
                report << "  mismatching texels: " << wrong << " of " << pixels.size() << "\n";
                check(wrong == 0, "imageStore pattern reads back exactly");
            }
        }
        check(glGetError() == GL_NO_ERROR, "no GL error raised");

        glUseProgram(0);
        glBindImageTexture(0, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32UI);
        if (texture != 0) glDeleteTextures(1, &texture);
        if (program != 0) glDeleteProgram(program);
        glDeleteShader(shader);
        while (glGetError() != GL_NO_ERROR) {
        }
    }

    report << "\nTotal assertions evaluated: " << checks << "\n";
    report << "Failed assertions: " << failures << "\n";
    report << "Result: " << (result.success ? "PASS" : "FAIL") << "\n";
    result.report = report.str();
    return result;
}

} // namespace ux3d::imgui
