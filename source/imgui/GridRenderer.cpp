// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/GridRenderer.h"
#include <vector>
#include <cstdio>
#include <glm/gtc/type_ptr.hpp>

namespace ux3d::imgui {

struct GridVertex {
    glm::vec3 position;
    glm::vec4 color;
};

static const char* s_gridVertexShader = R"(#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
uniform mat4 uViewProj;
out vec4 vColor;
void main() {
    vColor = aColor;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
)";

static const char* s_gridFragmentShader = R"(#version 410 core
in vec4 vColor;
out vec4 FragColor;
void main() {
    FragColor = vColor;
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
    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_program) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
    m_vertexCount = 0;
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

void GridRenderer::init(float size, float step)
{
    cleanup();

    // Compile shader program
    GLuint vs = compileShader(GL_VERTEX_SHADER, s_gridVertexShader);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, s_gridFragmentShader);
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

    m_viewProjLoc = glGetUniformLocation(m_program, "uViewProj");

    // Generate grid vertices
    std::vector<GridVertex> vertices;

    const glm::vec4 minorColor(0.25f, 0.25f, 0.27f, 0.6f);
    const glm::vec4 majorColor(0.40f, 0.40f, 0.44f, 0.9f);
    const glm::vec4 axisColorX(0.85f, 0.25f, 0.25f, 1.0f); // Red
    const glm::vec4 axisColorZ(0.25f, 0.45f, 0.85f, 1.0f); // Blue
    const glm::vec4 axisColorY(0.25f, 0.85f, 0.35f, 1.0f); // Green

    int numLines = static_cast<int>(size / step);
    for (int i = -numLines; i <= numLines; ++i) {
        float pos = i * step;
        bool isCenter = (i == 0);
        bool isMajor = (i % 5 == 0);

        glm::vec4 colorZ = isCenter ? axisColorZ : (isMajor ? majorColor : minorColor);
        glm::vec4 colorX = isCenter ? axisColorX : (isMajor ? majorColor : minorColor);

        // Line parallel to Z axis (varying X)
        vertices.push_back({ glm::vec3(pos, 0.0f, -size), colorZ });
        vertices.push_back({ glm::vec3(pos, 0.0f,  size), colorZ });

        // Line parallel to X axis (varying Z)
        vertices.push_back({ glm::vec3(-size, 0.0f, pos), colorX });
        vertices.push_back({ glm::vec3( size, 0.0f, pos), colorX });
    }

    // Y Axis (up)
    vertices.push_back({ glm::vec3(0.0f, 0.0f, 0.0f), axisColorY });
    vertices.push_back({ glm::vec3(0.0f, size * 0.5f, 0.0f), axisColorY });

    m_vertexCount = static_cast<GLsizei>(vertices.size());

    // Create VAO and VBO
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GridVertex), vertices.data(), GL_STATIC_DRAW);

    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GridVertex), reinterpret_cast<void*>(offsetof(GridVertex, position)));

    // Color attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GridVertex), reinterpret_cast<void*>(offsetof(GridVertex, color)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GridRenderer::draw(const glm::mat4& viewProjection)
{
    if (!m_vao || !m_program || m_vertexCount == 0) {
        return;
    }

    glUseProgram(m_program);
    glUniformMatrix4fv(m_viewProjLoc, 1, GL_FALSE, glm::value_ptr(viewProjection));

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_LINES, 0, m_vertexCount);
    glBindVertexArray(0);

    glUseProgram(0);
}

} // namespace ux3d::imgui
