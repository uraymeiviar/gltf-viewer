// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

namespace ux3d::imgui {

class BoundingBoxRenderer {
public:
    BoundingBoxRenderer();
    ~BoundingBoxRenderer();

    BoundingBoxRenderer(const BoundingBoxRenderer&) = delete;
    BoundingBoxRenderer& operator=(const BoundingBoxRenderer&) = delete;

    void init();
    void cleanup();

    void draw(const glm::vec3& minBounds, const glm::vec3& maxBounds, const glm::vec4& color, const glm::mat4& viewProj);

private:
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_program = 0;
    GLint m_viewProjLoc = -1;
    GLint m_colorLoc = -1;
};

} // namespace ux3d::imgui
