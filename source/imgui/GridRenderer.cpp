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

// Line, axis and fade shading shared by both grid algorithms.
static const char* s_gridShadingGLSL = R"(
uniform vec3 uCameraPos;
uniform float uMaxDistance;
uniform float uBlendDistance;
uniform vec4 uGridColor;
uniform vec4 uAxisColorX;
uniform vec4 uAxisColorZ;

out vec4 FragColor;

void shadeGrid(vec3 worldPos) {
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

static const char* s_infiniteGridFS = R"(
in vec3 vNearPoint;
in vec3 vFarPoint;
uniform mat4 uViewProj;
uniform float uHeight; // A3.6: world Y of the grid plane (0 for the ordinary grid)

void main() {
    float rayDirY = vFarPoint.y - vNearPoint.y;
    if (abs(rayDirY) < 1e-5) {
        discard;
    }

    float t = (uHeight - vNearPoint.y) / rayDirY;
    if (t <= 0.0) {
        discard;
    }

    vec3 worldPos = vNearPoint + t * (vFarPoint - vNearPoint);

    // Compute depth for depth buffer testing & writing
    vec4 clip = uViewProj * vec4(worldPos, 1.0);
    float ndcZ = clip.z / clip.w;
    gl_FragDepth = (ndcZ + 1.0) * 0.5;

    shadeGrid(worldPos);
}
)";

// P7.4 plane mesh: a square on y = 0 around the camera, as wide as the fade distance.
// Rasterized depth replaces gl_FragDepth, so early depth testing stays enabled and
// pixels off the plane are never shaded.
static const char* s_planeGridVS = R"(#version 410 core
uniform mat4 uViewProj;
uniform vec3 uCameraPos;
uniform float uMaxDistance;
uniform float uHeight; // A3.6: world Y of the grid plane (0 for the ordinary grid)
out vec3 vWorldPos;

const vec2 s_pos[4] = vec2[4](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2(-1.0,  1.0),
    vec2( 1.0,  1.0)
);

void main() {
    vec2 xz = uCameraPos.xz + s_pos[gl_VertexID] * uMaxDistance;
    vWorldPos = vec3(xz.x, uHeight, xz.y);
    gl_Position = uViewProj * vec4(vWorldPos, 1.0);
}
)";

static const char* s_planeGridFS = R"(
in vec3 vWorldPos;

void main() {
    shadeGrid(vWorldPos);
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
    for (Program* program : { &m_rayProgram, &m_planeProgram }) {
        if (program->id) {
            glDeleteProgram(program->id);
            program->id = 0;
        }
    }
}

static GLuint compileShader(GLenum type, const char* source, const char* body = nullptr)
{
    // Fragment shaders are the version line, the shared shading code and a main body.
    const char* sources[3] = { "#version 410 core\n", s_gridShadingGLSL, body };
    GLuint shader = glCreateShader(type);
    if (body != nullptr) {
        glShaderSource(shader, 3, sources, nullptr);
    } else {
        glShaderSource(shader, 1, &source, nullptr);
    }
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

static bool linkProgram(GridRenderer::Program& program, const char* vsSource, const char* fsBody)
{
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, nullptr, fsBody);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }

    program.id = glCreateProgram();
    glAttachShader(program.id, vs);
    glAttachShader(program.id, fs);
    glLinkProgram(program.id);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint success = 0;
    glGetProgramiv(program.id, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(program.id, sizeof(infoLog), nullptr, infoLog);
        std::fprintf(stderr, "GridRenderer program link error: %s\n", infoLog);
        glDeleteProgram(program.id);
        program.id = 0;
        return false;
    }

    program.invViewProjLoc = glGetUniformLocation(program.id, "uInvViewProj");
    program.viewProjLoc    = glGetUniformLocation(program.id, "uViewProj");
    program.cameraPosLoc   = glGetUniformLocation(program.id, "uCameraPos");
    program.heightLoc      = glGetUniformLocation(program.id, "uHeight");
    program.maxDistLoc     = glGetUniformLocation(program.id, "uMaxDistance");
    program.blendDistLoc   = glGetUniformLocation(program.id, "uBlendDistance");
    program.gridColorLoc   = glGetUniformLocation(program.id, "uGridColor");
    program.axisColorXLoc  = glGetUniformLocation(program.id, "uAxisColorX");
    program.axisColorZLoc  = glGetUniformLocation(program.id, "uAxisColorZ");

    ux3d::imgui::profiling::countShaderCompile();
    ux3d::imgui::profiling::countShaderCompile();
    ux3d::imgui::profiling::countProgramLink();
    return true;
}

void GridRenderer::init(float /*size*/, float /*step*/)
{
    cleanup();

    if (!linkProgram(m_rayProgram, s_infiniteGridVS, s_infiniteGridFS)
        || !linkProgram(m_planeProgram, s_planeGridVS, s_planeGridFS)) {
        cleanup();
        return;
    }

    // Empty VAO for triangle strip rendering with gl_VertexID
    glGenVertexArrays(1, &m_vao);
    ux3d::imgui::profiling::countVaoCreation();
}

void GridRenderer::draw(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos, float cameraDistance, bool planeMesh, float height)
{
    const Program& program = planeMesh ? m_planeProgram : m_rayProgram;
    if (!m_vao || !program.id) {
        return;
    }

    glm::mat4 viewProj = proj * view;
    glm::mat4 invViewProj = glm::inverse(viewProj);

    glUseProgram(program.id);
    glUniformMatrix4fv(program.invViewProjLoc, 1, GL_FALSE, glm::value_ptr(invViewProj));
    glUniformMatrix4fv(program.viewProjLoc,    1, GL_FALSE, glm::value_ptr(viewProj));
    glUniform3fv(program.cameraPosLoc,         1, glm::value_ptr(cameraPos));
    glUniform1f(program.heightLoc,             height);

    float maxDist = std::max(45.0f, cameraDistance * 4.5f);
    float blendDist = maxDist * 0.5f;
    glUniform1f(program.maxDistLoc,   maxDist);
    glUniform1f(program.blendDistLoc, blendDist);

    // Color theme matching Gestaltor
    glUniform4f(program.gridColorLoc,  0.72f, 0.72f, 0.72f, 0.85f);
    glUniform4f(program.axisColorXLoc, 0.78f, 0.15f, 0.15f, 0.95f); // Red X Axis
    glUniform4f(program.axisColorZLoc, 0.25f, 0.45f, 0.85f, 0.95f); // Blue Z Axis

    glEnable(GL_DEPTH_TEST);
    // The plane is two-sided. Face culling and winding were otherwise inherited from the last
    // scene command, so a mirrored (negatively scaled) last drawable hid the grid (P5.5).
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE); // Test against depth buffer, but blend smoothly without occluding subsequent transparent passes
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // The plane reaches past the far plane when zoomed out. Depth clamping keeps that part
    // at depth 1, as the ray cast's clamped gl_FragDepth does, instead of clipping it.
    if (planeMesh) {
        glEnable(GL_DEPTH_CLAMP);
    }
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    if (planeMesh) {
        glDisable(GL_DEPTH_CLAMP);
    }
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
