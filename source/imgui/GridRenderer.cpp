// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/GridRenderer.h"
#include "imgui/Profiling.h"
#include <cstdio>
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

namespace ux3d::imgui {

static const char* s_infiniteGridVS = R"(#version 410 core
uniform mat4 uInvViewProj;
out vec3 vNearPoint;
out vec3 vFarPoint;

const vec2 s_pos[4] = vec2[4](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0,  1.0)
);

vec3 unproject(vec2 ndc, float z) {
    vec4 p = uInvViewProj * vec4(ndc, z, 1.0);
    return p.xyz / p.w;
}

void main() {
    vec2 p = s_pos[gl_VertexID];
    vNearPoint = unproject(p, -1.0);
    vFarPoint  = unproject(p,  1.0);
    gl_Position = vec4(p, 0.0, 1.0);
}
)";

static const char* s_infiniteGridFS = R"(#version 410 core
in vec3 vNearPoint;
in vec3 vFarPoint;
uniform mat4 uViewProj;
uniform vec3 uCameraPos;
uniform float uMaxDistance;
uniform float uBlendDistance;
uniform vec4 uGridColor;
uniform vec4 uAxisColorX;
uniform vec4 uAxisColorZ;

out vec4 FragColor;

void main() {
    float rayDirY = vFarPoint.y - vNearPoint.y;
    if (abs(rayDirY) < 1e-5) {
        discard;
    }

    float t = -vNearPoint.y / rayDirY;
    if (t <= 0.0) {
        discard;
    }

    vec3 worldPos = vNearPoint + t * (vFarPoint - vNearPoint);

    // Compute depth for depth buffer testing & writing
    vec4 clip = uViewProj * vec4(worldPos, 1.0);
    float ndcZ = clip.z / clip.w;
    gl_FragDepth = (ndcZ + 1.0) * 0.5;

    // Distance falloff fading smoothly towards horizon (Gestaltor infinite fade)
    float dist = length(worldPos - uCameraPos);
    if (dist > uMaxDistance) {
        discard;
    }

    float fade = clamp((uMaxDistance - dist) / uBlendDistance, 0.0, 1.0);
    fade = smoothstep(0.0, 1.0, fade);

    // Anti-aliased grid lines using screen-space derivatives
    vec2 coord = worldPos.xz;
    vec2 d = fwidth(coord);

    // Major 1.0m grid lines
    vec2 grid1 = abs(fract(coord - 0.5) - 0.5) / d;
    float line1 = min(grid1.x, grid1.y);
    float c1 = 1.0 - min(line1, 1.0);

    // Minor 0.1m grid lines (decimeter)
    vec2 grid01 = abs(fract(coord * 10.0 - 0.5) - 0.5) / (d * 10.0);
    float line01 = min(grid01.x, grid01.y);
    float c01 = 1.0 - min(line01, 1.0);

    // Fade out minor lines as they become sub-pixel when zooming out
    float minorAlpha = clamp(1.0 - (d.x + d.y) * 12.0, 0.0, 1.0);

    // Origin Axes: X Axis (Z = 0) and Z Axis (X = 0)
    float axisPixelWidth = 1.8;
    bool isXAxis = abs(worldPos.z) < d.y * axisPixelWidth;
    bool isZAxis = abs(worldPos.x) < d.x * axisPixelWidth;

    vec4 outColor = vec4(0.0);

    if (isXAxis) {
        outColor = vec4(uAxisColorX.rgb, uAxisColorX.a * fade);
    } else if (isZAxis) {
        outColor = vec4(uAxisColorZ.rgb, uAxisColorZ.a * fade);
    } else if (c1 > 0.0) {
        float a = c1 * 0.65 * uGridColor.a * fade;
        outColor = vec4(uGridColor.rgb, a);
    } else if (c01 > 0.0 && minorAlpha > 0.0) {
        float a = c01 * 0.28 * uGridColor.a * fade * minorAlpha;
        outColor = vec4(uGridColor.rgb, a);
    } else {
        discard;
    }

    FragColor = outColor;
}
)";

GridRenderer::GridRenderer() = default;

GridRenderer::~GridRenderer()
{
    cleanup();
}

void GridRenderer::cleanup()
{
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

static GLuint compileShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        std::fprintf(stderr, "GridRenderer shader compile error: %s\n", infoLog);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

void GridRenderer::init(float /*size*/, float /*step*/)
{
    cleanup();

    GLuint vs = compileShader(GL_VERTEX_SHADER, s_infiniteGridVS);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, s_infiniteGridFS);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return;
    }

    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint success = 0;
    glGetProgramiv(m_program, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_program, sizeof(infoLog), nullptr, infoLog);
        std::fprintf(stderr, "GridRenderer program link error: %s\n", infoLog);
        glDeleteProgram(m_program);
        m_program = 0;
        return;
    }

    m_invViewProjLoc = glGetUniformLocation(m_program, "uInvViewProj");
    m_viewProjLoc    = glGetUniformLocation(m_program, "uViewProj");
    m_cameraPosLoc   = glGetUniformLocation(m_program, "uCameraPos");
    m_maxDistLoc     = glGetUniformLocation(m_program, "uMaxDistance");
    m_blendDistLoc   = glGetUniformLocation(m_program, "uBlendDistance");
    m_gridColorLoc   = glGetUniformLocation(m_program, "uGridColor");
    m_axisColorXLoc  = glGetUniformLocation(m_program, "uAxisColorX");
    m_axisColorZLoc  = glGetUniformLocation(m_program, "uAxisColorZ");

    // Empty VAO for fullscreen triangle strip rendering with gl_VertexID
    glGenVertexArrays(1, &m_vao);
    ux3d::imgui::profiling::countVaoCreation();
    ux3d::imgui::profiling::countShaderCompile();
    ux3d::imgui::profiling::countShaderCompile();
    ux3d::imgui::profiling::countProgramLink();
}

void GridRenderer::draw(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos, float cameraDistance)
{
    if (!m_vao || !m_program) {
        return;
    }

    glm::mat4 viewProj = proj * view;
    glm::mat4 invViewProj = glm::inverse(viewProj);

    glUseProgram(m_program);
    glUniformMatrix4fv(m_invViewProjLoc, 1, GL_FALSE, glm::value_ptr(invViewProj));
    glUniformMatrix4fv(m_viewProjLoc,    1, GL_FALSE, glm::value_ptr(viewProj));
    glUniform3fv(m_cameraPosLoc,         1, glm::value_ptr(cameraPos));

    float maxDist = std::max(45.0f, cameraDistance * 4.5f);
    float blendDist = maxDist * 0.5f;
    glUniform1f(m_maxDistLoc,   maxDist);
    glUniform1f(m_blendDistLoc, blendDist);

    // Color theme matching Gestaltor
    glUniform4f(m_gridColorLoc,  0.72f, 0.72f, 0.72f, 0.85f);
    glUniform4f(m_axisColorXLoc, 0.78f, 0.15f, 0.15f, 0.95f); // Red X Axis
    glUniform4f(m_axisColorZLoc, 0.25f, 0.45f, 0.85f, 0.95f); // Blue Z Axis

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE); // Test against depth buffer, but blend smoothly without occluding subsequent transparent passes
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    ux3d::imgui::profiling::countAppDrawCall();

    glDepthMask(GL_TRUE);
    glUseProgram(0);
}

void GridRenderer::draw(const glm::mat4& viewProjection)
{
    glm::mat4 dummyView(1.0f);
    draw(dummyView, viewProjection, glm::vec3(0.0f, 10.0f, 10.0f), 15.0f);
}

} // namespace ux3d::imgui
