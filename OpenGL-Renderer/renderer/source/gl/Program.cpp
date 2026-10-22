// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/gl/Program.h"
#include "core/Logger.h"
#include "renderer/gl/Buffer.h"
#include "renderer/gl/Sampler.h"
#include "renderer/gl/Uniform.h"
#include "renderer/gl/UniformBlock.h"

#include <algorithm>
#include <atomic>

namespace {
static std::atomic<uint64_t> s_nextProgramId{ 1 };
}

ux3d::renderer::gl::Program::Program(GL* gl, GLuint handle)
    : m_gl(gl)
    , m_handle(handle)
    , m_id(s_nextProgramId++)
{
    if (handle == invalid) {
        return;
    }

    GLint count = 0;
    GLchar stringBuffer[256] { '\0' };

    m_gl->getProgramiv(handle, GL_ACTIVE_UNIFORMS, &count);
    for (GLint i = 0; i < count; ++i) {
        UniformBinding binding;
        GLsizei size;
        m_gl->getActiveUniform(handle, i, sizeof(stringBuffer), nullptr, &size, reinterpret_cast<GLenum*>(&binding.type), stringBuffer);
        binding.location = static_cast<GLuint>(m_gl->getUniformLocation(handle, stringBuffer));
        std::string name = stringBuffer;
        if (name.find_last_of(']') != name.length() - 1) {
            m_uniformBindings[name] = binding;
        } else {
            // The uniform is an array of non-aggregates.
            // This requires special handling, as only `uniform[0]` is reported. The highest accessed index is reported in `size`.
            // We emit one binding per array element.
            name = name.substr(0, name.find_last_of('['));
            for (int i = 0; i < size; ++i) {
                std::string nameArrayElement = name + "[" + std::to_string(i) + "]";
                binding.location = gl->getUniformLocation(handle, nameArrayElement.data());
                m_uniformBindings[nameArrayElement] = binding;
            }
        }
    }

    m_gl->getProgramiv(handle, GL_ACTIVE_UNIFORM_BLOCKS, &count);
    for (GLint i = 0; i < count; ++i) {
        UniformBlockBinding binding;
        binding.blockIndex = i;
        GLint nameLength = 0;
        m_gl->getActiveUniformBlockName(handle, i, sizeof(stringBuffer), &nameLength, stringBuffer);
        m_uniformBlockLocations[stringBuffer] = binding;
    }

    m_gl->getProgramiv(handle, GL_ACTIVE_ATTRIBUTES, &count);
    for (GLint i = 0; i < count; ++i) {
        GLint size;
        GLenum type;
        m_gl->getActiveAttrib(handle, i, sizeof(stringBuffer), nullptr, &size, &type, stringBuffer);
        GLuint location = static_cast<GLuint>(m_gl->getAttribLocation(handle, stringBuffer));
        m_attributeLocations[stringBuffer] = location;
    }
}

ux3d::renderer::gl::Program::Program(Program&& other) noexcept
    : m_gl(other.m_gl)
    , m_handle(other.m_handle)
    , m_id(other.m_id)
    , m_uniformBindings(std::move(other.m_uniformBindings))
    , m_uniformBlockLocations(std::move(other.m_uniformBlockLocations))
    , m_attributeLocations(std::move(other.m_attributeLocations))
{
    other.m_handle = invalid;
    other.m_id = 0;
}

ux3d::renderer::gl::Program::~Program()
{
    if (m_handle != invalid) {
        m_gl->deleteProgram(m_handle);
        m_handle = invalid;
    }
}

void ux3d::renderer::gl::Program::commitSampler(const Sampler& sampler, GLint textureUnit, GLuint samplerHandle) const
{
    auto iter = m_uniformBindings.find(sampler.name);
    if (iter == m_uniformBindings.end()) {
        return;
    }
    sampler.commit(m_gl, iter->second, textureUnit, samplerHandle);
}

void ux3d::renderer::gl::Program::commitUniform(const Uniform& uniform) const
{
    auto iter = m_uniformBindings.find(uniform.name);
    if (iter == m_uniformBindings.end()) {
        return;
    }
    uniform.commit(m_gl, iter->second);
}

void ux3d::renderer::gl::Program::commitUniformBlock(const UniformBlock& uniformBlock) const
{
    auto iter = m_uniformBlockLocations.find(uniformBlock.name);
    if (iter == m_uniformBlockLocations.end()) {
        return;
    }
    uniformBlock.commit(m_gl, iter->second);
}

auto ux3d::renderer::gl::Program::createVertexLayout(const std::vector<Attribute>& attributes, const Buffer* indexBuffer) const -> std::unique_ptr<VertexLayout>
{
    auto vertexLayout = std::make_unique<VertexLayout>(m_gl);
    m_gl->bindVertexArray(vertexLayout->handle);

    for (const auto& attribute : attributes) {
        GLuint location;
        if (auto it = m_attributeLocations.find(attribute.name); it != m_attributeLocations.end()) {
            location = it->second;
        } else {
            continue;
        }
        if (attribute.buffer == nullptr) {
            continue;
        }

        m_gl->bindBuffer(GL_ARRAY_BUFFER, attribute.buffer->handle);
        m_gl->enableVertexAttribArray(location);
        m_gl->vertexAttribPointer(
            location,
            attribute.dimension,
            static_cast<GLenum>(attribute.type),
            attribute.normalized,
            attribute.stride,
            reinterpret_cast<const void*>(static_cast<size_t>(attribute.offset)));
    }

    if (indexBuffer != nullptr) {
        m_gl->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->handle);
    } else {
        m_gl->bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    m_gl->bindVertexArray(0);
    return vertexLayout;
}

void ux3d::renderer::gl::Program::use() const
{
    m_gl->useProgram(m_handle);
}
