// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "Attribute.h"
#include "UniformBinding.h"
#include "VertexLayout.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ux3d::renderer::gl {
class Uniform;
class UniformBlock;
struct Sampler;

class Program {
public:
    Program(GL*, GLuint handle);
    Program(Program&&) noexcept;
    ~Program();

    std::unique_ptr<VertexLayout> createVertexLayout(const std::vector<Attribute>& attributes) const;

    void commitSampler(const Sampler& sampler, GLint textureUnit, GLuint samplerHandle = 0) const;
    void commitUniform(const Uniform& uniform) const;
    void commitUniformBlock(const UniformBlock& uniformBlock) const;

    void use() const;

    GLuint getHandle() const { return m_handle; }

private:
    GL* m_gl;
    GLuint m_handle;

    std::map<std::string, UniformBinding> m_uniformBindings;
    std::map<std::string, UniformBlockBinding> m_uniformBlockLocations;
    std::map<std::string, GLuint> m_attributeLocations;
};
}
