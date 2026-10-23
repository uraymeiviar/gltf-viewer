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
    // planeMesh selects the rasterized plane (P7.4) instead of the full-screen ray cast.
    void draw(const glm::mat4& view, const glm::mat4& proj, const glm::vec3& cameraPos, float cameraDistance, bool planeMesh = false);
    void draw(const glm::mat4& viewProjection); // Fallback overload
    void cleanup();

    struct Program {
        GLuint id = 0;
        GLint invViewProjLoc = -1;
        GLint viewProjLoc = -1;
        GLint cameraPosLoc = -1;
        GLint maxDistLoc = -1;
        GLint blendDistLoc = -1;
        GLint gridColorLoc = -1;
        GLint axisColorXLoc = -1;
        GLint axisColorZLoc = -1;
    };

private:
    GLuint m_vao = 0;
    Program m_rayProgram;
    Program m_planeProgram;
};

} // namespace ux3d::imgui
