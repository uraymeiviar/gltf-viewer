// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "OpenGL.h"

namespace ux3d::renderer::gl {
class GL;

class VertexLayout {
public:
    VertexLayout(GL* gl);
    VertexLayout(const VertexLayout&) = delete;
    VertexLayout(VertexLayout&&) = delete;
    ~VertexLayout();

    GLuint handle;

private:
    GL* m_gl;
};
}
