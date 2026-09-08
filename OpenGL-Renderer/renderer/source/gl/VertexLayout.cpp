// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/gl/VertexLayout.h"
#include "renderer/gl/GL.h"

ux3d::renderer::gl::VertexLayout::VertexLayout(GL* gl)
    : m_gl(gl)
{
    gl->genVertexArrays(1, &handle);
}

ux3d::renderer::gl::VertexLayout::~VertexLayout()
{
    m_gl->deleteVertexArrays(1, &handle);
}
