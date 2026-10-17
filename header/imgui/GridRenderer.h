// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

namespace ux3d::imgui {

class GridRenderer {
public:
    GridRenderer();
    ~GridRenderer();

    GridRenderer(const GridRenderer&) = delete;
    GridRenderer& operator=(const GridRenderer&) = delete;

    void init(float size = 10.0f, float step = 1.0f);
    void draw(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos, float cameraDistance);
    void draw(const glm::mat4& viewProjection); // Fallback overload
    void cleanup();

private:
    GLuint m_vao = 0;
    GLuint m_program = 0;
    GLint m_invViewProjLoc = -1;
    GLint m_viewProjLoc = -1;
    GLint m_cameraPosLoc = -1;
    GLint m_maxDistLoc = -1;
    GLint m_blendDistLoc = -1;
    GLint m_gridColorLoc = -1;
    GLint m_axisColorXLoc = -1;
    GLint m_axisColorZLoc = -1;
};

} // namespace ux3d::imgui
