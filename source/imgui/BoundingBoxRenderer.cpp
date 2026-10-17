// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/BoundingBoxRenderer.h"
#include <cstdio>
#include <glm/gtc/type_ptr.hpp>

namespace ux3d::imgui {

static const char* s_bboxVS = R"(#version 410 core
layout(location = 0) in vec3 aPos;
uniform mat4 uViewProj;
void main() {
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
)";

static const char* s_bboxFS = R"(#version 410 core
uniform vec4 uColor;
out vec4 FragColor;
void main() {
    FragColor = uColor;
}
)";

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
        std::fprintf(stderr, "BoundingBoxRenderer shader error: %s\n", infoLog);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

BoundingBoxRenderer::BoundingBoxRenderer() = default;

BoundingBoxRenderer::~BoundingBoxRenderer()
{
    cleanup();
}

void BoundingBoxRenderer::cleanup()
{
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
}

void BoundingBoxRenderer::init()
{
    cleanup();

    GLuint vs = compileShader(GL_VERTEX_SHADER, s_bboxVS);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, s_bboxFS);
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
        std::fprintf(stderr, "BoundingBoxRenderer link error: %s\n", infoLog);
        glDeleteProgram(m_program);
        m_program = 0;
        return;
    }

    m_viewProjLoc = glGetUniformLocation(m_program, "uViewProj");
    m_colorLoc = glGetUniformLocation(m_program, "uColor");

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    // Allocate space for 24 vec3 vertices (12 lines)
    glBufferData(GL_ARRAY_BUFFER, 24 * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void BoundingBoxRenderer::draw(const glm::vec3& minBounds, const glm::vec3& maxBounds, const glm::vec4& color, const glm::mat4& viewProj)
{
    if (!m_vao || !m_vbo || !m_program) {
        return;
    }

    // 8 box corners
    glm::vec3 c0(minBounds.x, minBounds.y, minBounds.z);
    glm::vec3 c1(maxBounds.x, minBounds.y, minBounds.z);
    glm::vec3 c2(maxBounds.x, minBounds.y, maxBounds.z);
    glm::vec3 c3(minBounds.x, minBounds.y, maxBounds.z);
    glm::vec3 c4(minBounds.x, maxBounds.y, minBounds.z);
    glm::vec3 c5(maxBounds.x, maxBounds.y, minBounds.z);
    glm::vec3 c6(maxBounds.x, maxBounds.y, maxBounds.z);
    glm::vec3 c7(minBounds.x, maxBounds.y, maxBounds.z);

    // 12 lines (24 vertices)
    glm::vec3 lineVertices[24] = {
        // Bottom
        c0, c1,  c1, c2,  c2, c3,  c3, c0,
        // Top
        c4, c5,  c5, c6,  c6, c7,  c7, c4,
        // Vertical pillars
        c0, c4,  c1, c5,  c2, c6,  c3, c7
    };

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(lineVertices), lineVertices);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glUseProgram(m_program);
    glUniformMatrix4fv(m_viewProjLoc, 1, GL_FALSE, glm::value_ptr(viewProj));
    glUniform4fv(m_colorLoc, 1, glm::value_ptr(color));

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_LINES, 0, 24);
    glBindVertexArray(0);

    glUseProgram(0);
}

} // namespace ux3d::imgui
