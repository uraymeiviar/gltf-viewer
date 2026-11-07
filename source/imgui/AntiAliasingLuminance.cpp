// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// The Luminance debug view's post pass (LuminanceView.h): the HDR input mapped to a false-colour heatmap of its
// luminance in cd/m^2 (or in EV around middle grey), drawn instead of bloom, lens effect and tone mapping, and the
// non-stalling 1x1 readback of the hovered pixel. Nothing here is allocated or compiled while the view is off.

#include "imgui/AntiAliasing.h"
#include "imgui/LuminanceView.h"
#include "AntiAliasingShaders.h"

namespace ux3d::imgui {

namespace {

// The Turbo colormap (Anton Mikhailov, Google, 2019, Apache-2.0) in its published polynomial approximation (Ruofei Du,
// Google, Apache-2.0); LuminanceView.h has the same polynomial for the legend and the validator computes it too. The
// values are display (sRGB-encoded) colours: they are written as they are into the 8-bit display target, which has no
// sRGB conversion, so the sRGB encoding is applied exactly once (by the colormap itself).
const char* s_luminanceFS = R"(#version 410 core
in vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_HdrTexture;
uniform int u_Scale;      // 1 Absolute (log10 cd/m^2), 2 Exposure-relative (log2 of the value over middle grey)
uniform int u_Bands;      // one band per decade / stop, drawn with the colour of its centre
uniform vec2 u_Range;     // the scale's ends: log10 cd/m^2, or EV
uniform float u_ToUnit;   // stored value to cd/m^2 (Absolute) or to the multiple of middle grey (Relative)
uniform int u_HasClearColor;
uniform vec3 u_ClearColor;

vec3 turbo(float x) {
    const vec4 kRedVec4 = vec4(0.13572138, 4.61539260, -42.66032258, 132.13108234);
    const vec4 kGreenVec4 = vec4(0.09140261, 2.19418839, 4.84296658, -14.18503333);
    const vec4 kBlueVec4 = vec4(0.10667330, 12.64194608, -60.58204836, 110.36276771);
    const vec2 kRedVec2 = vec2(-152.94239396, 59.28637943);
    const vec2 kGreenVec2 = vec2(4.27729857, 2.82956604);
    const vec2 kBlueVec2 = vec2(-89.90310912, 27.34824973);
    x = clamp(x, 0.0, 1.0);
    vec4 v4 = vec4(1.0, x, x * x, x * x * x);
    vec2 v2 = v4.zw * v4.z;
    return clamp(vec3(dot(v4, kRedVec4) + dot(v2, kRedVec2),
                      dot(v4, kGreenVec4) + dot(v2, kGreenVec2),
                      dot(v4, kBlueVec4) + dot(v2, kBlueVec2)), 0.0, 1.0);
}

void main() {
    vec4 hdr = texture(u_HdrTexture, v_TexCoord);
    vec3 c = max(hdr.rgb, vec3(0.0));
    // Without the background the target holds the clear colour where nothing covers it (the tone map's unlit
    // backdrop); it is not scene light, so it counts as zero.
    if (u_HasClearColor != 0) c = max(vec3(0.0), c - u_ClearColor * (1.0 - clamp(hdr.a, 0.0, 1.0)));
    float y = dot(c, vec3(0.2126, 0.7152, 0.0722)) * u_ToUnit;
    if (!(y > 0.0)) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    float v = (u_Scale == 2) ? log2(y) : log(y) * 0.43429448190325176; // 1 / ln 10
    float span = u_Range.y - u_Range.x;
    float t = (v - u_Range.x) / span;
    if (t < 0.0) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    if (t > 1.0) {
        FragColor = vec4(1.0, 0.0, 1.0, 1.0);
        return;
    }
    if (u_Bands != 0) {
        float bands = max(ceil(span - 1.0e-4), 1.0);
        float k = min(floor(v - u_Range.x), bands - 1.0);
        t = min((k + 0.5) / span, 1.0);
    }
    FragColor = vec4(turbo(t), 1.0);
}
)";

} // namespace

void AntiAliasing::renderLuminance(GLuint inputHdrTexture, GLuint targetLdrFbo, int width, int height,
                                   const HdrPostProcessParams& params, float exposureLinear)
{
    if (!m_luminanceProgramTried) {
        m_luminanceProgramTried = true;
        ux3d::imgui::profiling::countShaderCompile();
        ux3d::imgui::profiling::countShaderCompile();
        ux3d::imgui::profiling::countProgramLink();
        m_luminanceProgram = linkProgram(s_quadVS, s_luminanceFS);
        if (m_luminanceProgram) {
            m_lumHdrTexLoc = glGetUniformLocation(m_luminanceProgram, "u_HdrTexture");
            m_lumScaleLoc = glGetUniformLocation(m_luminanceProgram, "u_Scale");
            m_lumBandsLoc = glGetUniformLocation(m_luminanceProgram, "u_Bands");
            m_lumRangeLoc = glGetUniformLocation(m_luminanceProgram, "u_Range");
            m_lumToUnitLoc = glGetUniformLocation(m_luminanceProgram, "u_ToUnit");
            m_lumHasClearColorLoc = glGetUniformLocation(m_luminanceProgram, "u_HasClearColor");
            m_lumClearColorLoc = glGetUniformLocation(m_luminanceProgram, "u_ClearColor");
        }
    }

    const bool relative = params.luminanceView == 2;
    LuminanceInput input;
    input.texture = inputHdrTexture;
    input.width = width;
    input.height = height;
    input.preExposure = params.preExposure;
    input.cdPerUnit = luminance::kCdPerEnvironmentUnit / params.preExposure;
    input.relativePerUnit = exposureLinear / luminance::kMiddleGrey;
    input.hasClearColor = params.hasClearColor;
    input.clearColor = params.clearColor;
    m_luminanceInput = input;

    glViewport(0, 0, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, targetLdrFbo);
    if (m_luminanceProgram == 0) {
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDepthMask(GL_TRUE);
        return;
    }
    glUseProgram(m_luminanceProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputHdrTexture);
    glUniform1i(m_lumHdrTexLoc, 0);
    glUniform1i(m_lumScaleLoc, relative ? 2 : 1);
    glUniform1i(m_lumBandsLoc, params.luminanceBands ? 1 : 0);
    const luminance::Scale scale = luminance::Scale::of(relative, params.luminanceMin, params.luminanceMax,
                                                        params.luminanceEvMin, params.luminanceEvMax);
    glUniform2f(m_lumRangeLoc, scale.low, scale.high);
    glUniform1f(m_lumToUnitLoc, relative ? input.relativePerUnit : input.cdPerUnit);
    glUniform1i(m_lumHasClearColorLoc, params.hasClearColor ? 1 : 0);
    glUniform3f(m_lumClearColorLoc, params.clearColor.r, params.clearColor.g, params.clearColor.b);

    glBindVertexArray(m_quadVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    ux3d::imgui::profiling::countAppDrawCall("Luminance view");

    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDepthMask(GL_TRUE);
}

void AntiAliasing::sampleLuminance(int x, int y)
{
    GLint previousRead = 0, previousPackBuffer = 0, previousPack = 4, previousReadBuffer = GL_BACK;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousRead);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &previousPackBuffer);
    glGetIntegerv(GL_PACK_ALIGNMENT, &previousPack);

    // The read issued earlier: consumed once the GPU has finished it, never waited for.
    if (m_luminanceFence != nullptr) {
        const GLenum status = glClientWaitSync(m_luminanceFence, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
        if (status != GL_ALREADY_SIGNALED && status != GL_CONDITION_SATISFIED) {
            return;
        }
        glDeleteSync(m_luminanceFence);
        m_luminanceFence = nullptr;
        float pixel[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        glBindBuffer(GL_PIXEL_PACK_BUFFER, m_luminanceProbePbo);
        if (const void* mapped = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, sizeof(pixel), GL_MAP_READ_BIT)) {
            std::memcpy(pixel, mapped, sizeof(pixel));
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
            const LuminanceInput& in = m_luminancePendingInput;
            float rgb[3] = {std::max(pixel[0], 0.0f), std::max(pixel[1], 0.0f), std::max(pixel[2], 0.0f)};
            if (in.hasClearColor) {
                const float uncovered = 1.0f - std::clamp(pixel[3], 0.0f, 1.0f);
                for (int c = 0; c < 3; ++c) rgb[c] = std::max(0.0f, rgb[c] - in.clearColor[c] * uncovered);
            }
            const float stored = 0.2126f * rgb[0] + 0.7152f * rgb[1] + 0.0722f * rgb[2];
            LuminanceSample sample;
            sample.valid = true;
            sample.x = m_luminancePendingX;
            sample.y = m_luminancePendingY;
            std::memcpy(sample.hdr, pixel, sizeof(pixel));
            sample.luminance = stored / in.preExposure;
            sample.cd = stored * in.cdPerUnit;
            const float relative = stored * in.relativePerUnit;
            sample.ev = relative > 0.0f ? std::log2(relative) : -std::numeric_limits<float>::infinity();
            sample.reads = m_luminanceSample.reads + 1;
            m_luminanceSample = sample;
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(previousPackBuffer));
    }

    const LuminanceInput& in = m_luminanceInput;
    if (in.texture != 0 && glIsTexture(in.texture) == GL_TRUE && x >= 0 && y >= 0 && x < in.width && y < in.height) {
        if (m_luminanceProbeFbo == 0) glGenFramebuffers(1, &m_luminanceProbeFbo);
        if (m_luminanceProbePbo == 0) {
            glGenBuffers(1, &m_luminanceProbePbo);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, m_luminanceProbePbo);
            glBufferData(GL_PIXEL_PACK_BUFFER, 4 * sizeof(float), nullptr, GL_STREAM_READ);
        }
        glGetIntegerv(GL_READ_BUFFER, &previousReadBuffer);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_luminanceProbeFbo);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, in.texture, 0);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        if (glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, m_luminanceProbePbo);
            glReadPixels(x, y, 1, 1, GL_RGBA, GL_FLOAT, nullptr);
            m_luminanceFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
            m_luminancePendingInput = in;
            m_luminancePendingX = x;
            m_luminancePendingY = y;
        }
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousRead));
        glReadBuffer(static_cast<GLenum>(previousReadBuffer));
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(previousPackBuffer));
    glPixelStorei(GL_PACK_ALIGNMENT, previousPack);
}

void AntiAliasing::releaseLuminanceProbe()
{
    if (m_luminanceFence != nullptr) {
        glDeleteSync(m_luminanceFence);
        m_luminanceFence = nullptr;
    }
    if (m_luminanceProbePbo != 0) {
        glDeleteBuffers(1, &m_luminanceProbePbo);
        m_luminanceProbePbo = 0;
    }
    if (m_luminanceProbeFbo != 0) {
        glDeleteFramebuffers(1, &m_luminanceProbeFbo);
        m_luminanceProbeFbo = 0;
    }
    m_luminanceSample = LuminanceSample{};
    m_luminanceInput = LuminanceInput{};
}

void AntiAliasing::releaseLuminanceProgram()
{
    if (m_luminanceProgram != 0) {
        glDeleteProgram(m_luminanceProgram);
        m_luminanceProgram = 0;
    }
    m_luminanceProgramTried = false;
}

} // namespace ux3d::imgui
