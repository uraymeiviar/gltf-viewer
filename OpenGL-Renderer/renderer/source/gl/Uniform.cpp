// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/gl/Uniform.h"
#include "core/Logger.h"
#include "renderer/gl/UniformBinding.h"
#include "renderer/gl/Util.h"

ux3d::renderer::gl::Uniform::Uniform(const std::string& name)
    : name(name)
{
}

void ux3d::renderer::gl::Uniform::write(bool value)
{
    write<uint32_t>(value ? GL_TRUE : GL_FALSE);
}

void ux3d::renderer::gl::Uniform::commit(GL* gl, const UniformBinding& binding) const
{
    if (m_bytes.empty()) {
        return;
    }

    const auto* floats = reinterpret_cast<const GLfloat*>(m_bytes.data());
    const auto* ints = reinterpret_cast<const GLint*>(m_bytes.data());
    const auto* uints = reinterpret_cast<const GLuint*>(m_bytes.data());

    switch (binding.type) {
    case gl::PrimitiveType::Float:
        gl->uniform1fv(binding.location, m_count, floats);
        break;

    case gl::PrimitiveType::FloatVec2:
        gl->uniform2fv(binding.location, m_count, floats);
        break;

    case gl::PrimitiveType::FloatVec3:
        gl->uniform3fv(binding.location, m_count, floats);
        break;

    case gl::PrimitiveType::FloatVec4:
        gl->uniform4fv(binding.location, m_count, floats);
        break;

    case gl::PrimitiveType::Int:
        gl->uniform1iv(binding.location, m_count, ints);
        break;

    case gl::PrimitiveType::IntVec2:
        gl->uniform2iv(binding.location, m_count, ints);
        break;

    case gl::PrimitiveType::IntVec3:
        gl->uniform3iv(binding.location, m_count, ints);
        break;

    case gl::PrimitiveType::IntVec4:
        gl->uniform4iv(binding.location, m_count, ints);
        break;

    case gl::PrimitiveType::Bool:
    case gl::PrimitiveType::UnsignedInt:
        gl->uniform1uiv(binding.location, m_count, uints);
        break;

    case gl::PrimitiveType::BoolVec2:
    case gl::PrimitiveType::UnsignedIntVec2:
        gl->uniform2uiv(binding.location, m_count, uints);
        break;

    case gl::PrimitiveType::BoolVec3:
    case gl::PrimitiveType::UnsignedIntVec3:
        gl->uniform3uiv(binding.location, m_count, uints);
        break;

    case gl::PrimitiveType::BoolVec4:
    case gl::PrimitiveType::UnsignedIntVec4:
        gl->uniform3uiv(binding.location, m_count, uints);
        break;

    case gl::PrimitiveType::FloatMat2:
        gl->uniformMatrix2fv(binding.location, m_count, GL_FALSE, floats);
        break;

    case gl::PrimitiveType::FloatMat3:
        gl->uniformMatrix3fv(binding.location, m_count, GL_FALSE, floats);
        break;

    case gl::PrimitiveType::FloatMat4:
        gl->uniformMatrix4fv(binding.location, m_count, GL_FALSE, floats);
        break;

    default:
        core::logError("Unknown gl type %d", binding.type);
        break;
    }

    GLIDER_HOT_GL_CHECK(gl, "Failed to commit uniform");

    return;
}
