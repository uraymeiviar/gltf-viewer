// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "renderer/gl/Util.h"
#include "core/Logger.h"

using namespace ux3d;
using namespace ux3d::renderer;

bool ux3d::renderer::gl::checkGLError(GL* gl, const char* message, bool printLog)
{
    const char* desc = "";

    switch (gl->getError()) {
    case GL_NO_ERROR:
        return true;
    case GL_INVALID_ENUM:
        desc = "Invalid enumeration value";
        break;
    case GL_INVALID_VALUE:
        desc = "Invalid value";
        break;
    case GL_INVALID_OPERATION:
        desc = "Invalid operation";
        break;
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        desc = "Invalid framebuffer operation";
        break;
    case GL_OUT_OF_MEMORY:
        desc = "Out of memory";
        break;
    default:
        desc = "Unknown GL error";
        break;
    }
    if (printLog) {
        ux3d::core::logError("[OPENGL_ERROR (%s)]: %s", desc, message);
    } else {
        core::debugLog(core::LogLevel::Error, "[OPENGL_ERROR (%s)]: %s", desc, message);
    }
    return false;
}

GLuint ux3d::renderer::gl::getByteSize(PrimitiveType type)
{
    switch (type) {
    case PrimitiveType::Bool:
    case PrimitiveType::Float:
    case PrimitiveType::Int:
        return 4u;
    case PrimitiveType::BoolVec2:
    case PrimitiveType::FloatVec2:
    case PrimitiveType::IntVec2:
        return 8u;
    case PrimitiveType::BoolVec3:
    case PrimitiveType::FloatVec3:
    case PrimitiveType::IntVec3:
        return 12u;
    case PrimitiveType::BoolVec4:
    case PrimitiveType::FloatVec4:
    case PrimitiveType::IntVec4:
    case PrimitiveType::FloatMat2:
        return 16u;
    case PrimitiveType::FloatMat3:
        return 36u;
    case PrimitiveType::FloatMat4:
        return 64u;
    default:
        return 0u;
    }
}

GLint ux3d::renderer::gl::getComponentCount(PrimitiveType type)
{
    switch (type) {
    case PrimitiveType::Bool:
    case PrimitiveType::Float:
    case PrimitiveType::Int:
    case PrimitiveType::UnsignedInt:
    case PrimitiveType::UnsignedByte:
        return 1;
    case PrimitiveType::BoolVec2:
    case PrimitiveType::FloatVec2:
    case PrimitiveType::IntVec2:
    case PrimitiveType::UnsignedIntVec2:
        return 2;
    case PrimitiveType::BoolVec3:
    case PrimitiveType::FloatVec3:
    case PrimitiveType::IntVec3:
    case PrimitiveType::UnsignedIntVec3:
        return 3;
    case PrimitiveType::BoolVec4:
    case PrimitiveType::FloatVec4:
    case PrimitiveType::IntVec4:
    case PrimitiveType::FloatMat2:
    case PrimitiveType::UnsignedIntVec4:
        return 4;
    case PrimitiveType::FloatMat3:
        return 9;
    case PrimitiveType::FloatMat4:
        return 16;
    default:
        return 0;
    }
}

GLenum ux3d::renderer::gl::getComponentType(PrimitiveType type)
{
    switch (type) {
    case PrimitiveType::Bool:
    case PrimitiveType::BoolVec2:
    case PrimitiveType::BoolVec3:
    case PrimitiveType::BoolVec4:
        return GL_BOOL;

    case PrimitiveType::Float:
    case PrimitiveType::FloatVec2:
    case PrimitiveType::FloatVec3:
    case PrimitiveType::FloatVec4:
    case PrimitiveType::FloatMat2:
    case PrimitiveType::FloatMat3:
    case PrimitiveType::FloatMat4:
        return GL_FLOAT;

    case PrimitiveType::Int:
    case PrimitiveType::IntVec2:
    case PrimitiveType::IntVec3:
    case PrimitiveType::IntVec4:
        return GL_INT;

    case PrimitiveType::UnsignedInt:
    case PrimitiveType::UnsignedIntVec2:
    case PrimitiveType::UnsignedIntVec3:
    case PrimitiveType::UnsignedIntVec4:
        return GL_UNSIGNED_INT;

    default:
        return invalid;
    }
}
