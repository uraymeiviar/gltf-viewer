// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include <glad/glad.h>
#include "imgui/GladGL.h"
#include "imgui/Profiling.h"
#include <cstdio>
#include <cstring>

namespace ux3d::renderer {

bool GladGL::hasExtension(const GLchar* extensionName)
{
    ux3d::imgui::profiling::countGlCall();
    if (!extensionsCache) {
        extensionsCache = std::unordered_set<std::string>();
        int numberOfExtensions = 0;
        getIntegerv(GL_NUM_EXTENSIONS, &numberOfExtensions);
        for (int i = 0; i < numberOfExtensions; i++) {
            const GLubyte* supportedExtension = getStringi(GL_EXTENSIONS, i);
            if (supportedExtension) {
                extensionsCache->insert(std::string(reinterpret_cast<const char*>(supportedExtension)));
            }
        }
    }

    return extensionsCache->contains(std::string(extensionName));
}

bool GladGL::supportsAnisotropicFiltering()
{
    if (!m_anisotropicFilteringQueried) {
        m_anisotropicFilteringSupported = hasExtension("GL_EXT_texture_filter_anisotropic");
        m_anisotropicFilteringQueried = true;
    }
    return m_anisotropicFilteringSupported;
}

GLfloat GladGL::maxTextureMaxAnisotropy()
{
    if (!m_maxAnisotropyQueried) {
        GLfloat value = 1.0f;
        getFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &value);
        m_maxAnisotropy = value;
        m_maxAnisotropyQueried = true;
    }
    return m_maxAnisotropy;
}

// Global:

GLenum GladGL::getError()
{
    ux3d::imgui::profiling::countGlCall();
    return glGetError();
}

void GladGL::getIntegerv(GLenum pname, GLint* data)
{
    ux3d::imgui::profiling::countGlCall();
    glGetIntegerv(pname, data);
}

void GladGL::getFloatv(GLenum pname, GLfloat* param)
{
    ux3d::imgui::profiling::countGlCall();
    glGetFloatv(pname, param);
}

const GLubyte* GladGL::getString(GLenum name)
{
    ux3d::imgui::profiling::countGlCall();
    return glGetString(name);
}

const GLubyte* GladGL::getStringi(GLenum name, GLuint index)
{
    ux3d::imgui::profiling::countGlCall();
    return glGetStringi(name, index);
}

void GladGL::enable(GLenum cap)
{
    ux3d::imgui::profiling::countGlCall();
    glEnable(cap);
}

void GladGL::disable(GLenum cap)
{
    ux3d::imgui::profiling::countGlCall();
    glDisable(cap);
}

void GladGL::enablei(GLenum cap, GLuint index)
{
    ux3d::imgui::profiling::countGlCall();
    glEnablei(cap, index);
}

void GladGL::disablei(GLenum cap, GLuint index)
{
    ux3d::imgui::profiling::countGlCall();
    glDisablei(cap, index);
}

void GladGL::finish()
{
    ux3d::imgui::profiling::countGlCall();
    glFinish();
}

// Shaders:

GLuint GladGL::createShader(GLenum shaderType)
{
    ux3d::imgui::profiling::countGlCall();
    return glCreateShader(shaderType);
}

void GladGL::deleteShader(GLuint shader)
{
    ux3d::imgui::profiling::countGlCall();
    glDeleteShader(shader);
}

void GladGL::shaderSource(GLuint shader, GLsizei count, const GLchar** string, const GLint* length)
{
    ux3d::imgui::profiling::countGlCall();
    glShaderSource(shader, count, string, length);
}

void GladGL::compileShader(GLuint shader)
{
    ux3d::imgui::profiling::countGlCall();
    ux3d::imgui::profiling::countShaderCompile();
    glCompileShader(shader);
}

void GladGL::getShaderiv(GLuint shader, GLenum pname, GLint* params)
{
    ux3d::imgui::profiling::countGlCall();
    glGetShaderiv(shader, pname, params);
}

void GladGL::getShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei* length, GLchar* infoLog)
{
    ux3d::imgui::profiling::countGlCall();
    glGetShaderInfoLog(shader, maxLength, length, infoLog);
}

// Programs:

GLuint GladGL::createProgram()
{
    ux3d::imgui::profiling::countGlCall();
    return glCreateProgram();
}

void GladGL::deleteProgram(GLuint program)
{
    ux3d::imgui::profiling::countGlCall();
    glDeleteProgram(program);
}

void GladGL::attachShader(GLuint program, GLuint shader)
{
    ux3d::imgui::profiling::countGlCall();
    glAttachShader(program, shader);
}

void GladGL::detachShader(GLuint program, GLuint shader)
{
    ux3d::imgui::profiling::countGlCall();
    glDetachShader(program, shader);
}

void GladGL::validateProgram(GLuint program)
{
    ux3d::imgui::profiling::countGlCall();
    glValidateProgram(program);
}

void GladGL::linkProgram(GLuint program)
{
    ux3d::imgui::profiling::countGlCall();
    ux3d::imgui::profiling::countProgramLink();
    glLinkProgram(program);
}

void GladGL::useProgram(GLuint program)
{
    ux3d::imgui::profiling::countGlCall();
    glUseProgram(program);
}

void GladGL::getProgramInfoLog(GLuint program, GLsizei maxLength, GLsizei* length, GLchar* infoLog)
{
    ux3d::imgui::profiling::countGlCall();
    glGetProgramInfoLog(program, maxLength, length, infoLog);
}

void GladGL::getProgramiv(GLuint program, GLenum pname, GLint* params)
{
    ux3d::imgui::profiling::countGlCall();
    glGetProgramiv(program, pname, params);
}

// Pipeline state:

void GladGL::viewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    ux3d::imgui::profiling::countGlCall();
    glViewport(x, y, width, height);
}

void GladGL::cullFace(GLenum mode)
{
    ux3d::imgui::profiling::countGlCall();
    glCullFace(mode);
}

void GladGL::polygonMode(GLenum face, GLenum mode)
{
    ux3d::imgui::profiling::countGlCall();
    glPolygonMode(face, mode);
}

void GladGL::scissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
    ux3d::imgui::profiling::countGlCall();
    glScissor(x, y, width, height);
}

void GladGL::frontFace(GLenum mode)
{
    ux3d::imgui::profiling::countGlCall();
    glFrontFace(mode);
}

void GladGL::blendFunc(GLenum sfactor, GLenum dfactor)
{
    ux3d::imgui::profiling::countGlCall();
    glBlendFunc(sfactor, dfactor);
}

void GladGL::blendEquation(GLenum mode)
{
    ux3d::imgui::profiling::countGlCall();
    glBlendEquation(mode);
}

void GladGL::depthFunc(GLenum func)
{
    ux3d::imgui::profiling::countGlCall();
    glDepthFunc(func);
}

void GladGL::colorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
    ux3d::imgui::profiling::countGlCall();
    glColorMask(red, green, blue, alpha);
}

void GladGL::clearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    ux3d::imgui::profiling::countGlCall();
    glClearColor(red, green, blue, alpha);
}

void GladGL::clear(GLbitfield mask)
{
    ux3d::imgui::profiling::countGlCall();
    glClear(mask);
}

// Textures:

void GladGL::genTextures(GLsizei n, GLuint* textures)
{
    ux3d::imgui::profiling::countGlCall();
    ux3d::imgui::profiling::countTextureCreation(n > 0 ? static_cast<uint64_t>(n) : 0);
    glGenTextures(n, textures);
}

void GladGL::deleteTextures(GLsizei n, const GLuint* textures)
{
    ux3d::imgui::profiling::countGlCall();
    glDeleteTextures(n, textures);
}

void GladGL::texParameteri(GLuint texture, GLenum pname, GLint param)
{
    ux3d::imgui::profiling::countGlCall();
    glTexParameteri(texture, pname, param);
}

void GladGL::texParameterf(GLuint texture, GLenum pname, GLfloat param)
{
    ux3d::imgui::profiling::countGlCall();
    glTexParameterf(texture, pname, param);
}

void GladGL::bindTexture(GLenum target, GLuint texture)
{
    ux3d::imgui::profiling::countGlCall();
    glBindTexture(target, texture);
}

void GladGL::pixelStorei(GLenum pname, GLint param)
{
    ux3d::imgui::profiling::countGlCall();
    glPixelStorei(pname, param);
}

void GladGL::texImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* data)
{
    ux3d::imgui::profiling::countGlCall();
    const uint64_t texels = width > 0 && height > 0
        ? static_cast<uint64_t>(width) * static_cast<uint64_t>(height)
        : 0ull;
    if (texels > 0) {
        if (const auto texelBytes = ux3d::imgui::profiling::internalFormatTexelBytes(static_cast<GLenum>(internalformat))) {
            ux3d::imgui::profiling::countTextureAllocation(texels * *texelBytes);
        } else {
            ux3d::imgui::profiling::countTextureAllocationUnknown();
        }
    }
    if (data != nullptr && texels > 0) {
        if (const auto pixelBytes = ux3d::imgui::profiling::pixelTransferBytes(format, type)) {
            ux3d::imgui::profiling::countTextureUpload(texels * *pixelBytes);
        } else {
            ux3d::imgui::profiling::countTextureUploadUnknown();
        }
    }
    glTexImage2D(target, level, internalformat, width, height, border, format, type, data);
}

void GladGL::texParameteriv(GLenum target, GLenum pname, const GLint* params)
{
    ux3d::imgui::profiling::countGlCall();
    glTexParameteriv(target, pname, params);
}

void GladGL::texParameterfv(GLenum target, GLenum pname, const GLfloat* params)
{
    ux3d::imgui::profiling::countGlCall();
    glTexParameterfv(target, pname, params);
}

void GladGL::getTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint* params)
{
    ux3d::imgui::profiling::countGlCall();
    glGetTexLevelParameteriv(target, level, pname, params);
}

void GladGL::activeTexture(GLenum texture)
{
    ux3d::imgui::profiling::countGlCall();
    glActiveTexture(texture);
}

void GladGL::generateMipmap(GLenum target)
{
    ux3d::imgui::profiling::countGlCall();
    glGenerateMipmap(target);
}

void GladGL::copyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    ux3d::imgui::profiling::countGlCall();
    glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

void GladGL::getTexImage(GLenum target, GLint level, GLenum format, GLenum type, void* pixels)
{
    ux3d::imgui::profiling::countGlCall();
    glGetTexImage(target, level, format, type, pixels);
}

void GladGL::texImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* data)
{
    ux3d::imgui::profiling::countGlCall();
    const uint64_t texels = width > 0 && height > 0 && depth > 0
        ? static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * static_cast<uint64_t>(depth)
        : 0ull;
    if (texels > 0) {
        if (const auto texelBytes = ux3d::imgui::profiling::internalFormatTexelBytes(static_cast<GLenum>(internalformat))) {
            ux3d::imgui::profiling::countTextureAllocation(texels * *texelBytes);
        } else {
            ux3d::imgui::profiling::countTextureAllocationUnknown();
        }
    }
    if (data != nullptr && texels > 0) {
        if (const auto pixelBytes = ux3d::imgui::profiling::pixelTransferBytes(format, type)) {
            ux3d::imgui::profiling::countTextureUpload(texels * *pixelBytes);
        } else {
            ux3d::imgui::profiling::countTextureUploadUnknown();
        }
    }
    glTexImage3D(target, level, internalformat, width, height, depth, border, format, type, data);
}

void GladGL::texSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels)
{
    ux3d::imgui::profiling::countGlCall();
    const uint64_t texels = width > 0 && height > 0 && depth > 0
        ? static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * static_cast<uint64_t>(depth)
        : 0ull;
    if (pixels != nullptr && texels > 0) {
        if (const auto pixelBytes = ux3d::imgui::profiling::pixelTransferBytes(format, type)) {
            ux3d::imgui::profiling::countTextureUpload(texels * *pixelBytes);
        } else {
            ux3d::imgui::profiling::countTextureUploadUnknown();
        }
    }
    glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}

// Framebuffers:

void GladGL::genFramebuffers(GLsizei n, GLuint* framebuffers)
{
    ux3d::imgui::profiling::countGlCall();
    ux3d::imgui::profiling::countFramebufferCreation(n > 0 ? static_cast<uint64_t>(n) : 0);
    glGenFramebuffers(n, framebuffers);
}

void GladGL::deleteFramebuffers(GLsizei n, GLuint* framebuffers)
{
    ux3d::imgui::profiling::countGlCall();
    glDeleteFramebuffers(n, framebuffers);
}

void GladGL::bindFramebuffer(GLenum target, GLuint framebuffer)
{
    ux3d::imgui::profiling::countGlCall();
    glBindFramebuffer(target, framebuffer);
}

void GladGL::framebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    ux3d::imgui::profiling::countGlCall();
    glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

GLenum GladGL::checkFramebufferStatus(GLenum target)
{
    ux3d::imgui::profiling::countGlCall();
    return glCheckFramebufferStatus(target);
}

void GladGL::clearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat* value)
{
    ux3d::imgui::profiling::countGlCall();
    glClearBufferfv(buffer, drawbuffer, value);
}

void GladGL::clearBufferiv(GLenum buffer, GLint drawbuffer, const GLint* value)
{
    ux3d::imgui::profiling::countGlCall();
    glClearBufferiv(buffer, drawbuffer, value);
}

void GladGL::blitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
    ux3d::imgui::profiling::countGlCall();
    glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

// Renderbuffers:

void GladGL::genRenderbuffers(GLsizei n, GLuint* renderbuffers)
{
    ux3d::imgui::profiling::countGlCall();
    ux3d::imgui::profiling::countRenderbufferCreation(n > 0 ? static_cast<uint64_t>(n) : 0);
    glGenRenderbuffers(n, renderbuffers);
}

void GladGL::bindRenderbuffer(GLenum target, GLuint renderbuffer)
{
    ux3d::imgui::profiling::countGlCall();
    glBindRenderbuffer(target, renderbuffer);
}

void GladGL::renderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
    ux3d::imgui::profiling::countGlCall();
    if (width > 0 && height > 0) {
        const uint64_t texels = static_cast<uint64_t>(width) * static_cast<uint64_t>(height);
        if (const auto texelBytes = ux3d::imgui::profiling::internalFormatTexelBytes(internalformat)) {
            ux3d::imgui::profiling::countRenderbufferAllocation(texels * *texelBytes);
        } else {
            ux3d::imgui::profiling::countRenderbufferAllocationUnknown();
        }
    }
    glRenderbufferStorage(target, internalformat, width, height);
}

void GladGL::framebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
    ux3d::imgui::profiling::countGlCall();
    glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

void GladGL::deleteRenderbuffers(GLsizei n, GLuint* renderbuffers)
{
    ux3d::imgui::profiling::countGlCall();
    glDeleteRenderbuffers(n, renderbuffers);
}

// Buffers:

void GladGL::genBuffers(GLsizei n, GLuint* buffers)
{
    ux3d::imgui::profiling::countGlCall();
    ux3d::imgui::profiling::countBufferCreation(n > 0 ? static_cast<uint64_t>(n) : 0);
    glGenBuffers(n, buffers);
}

void GladGL::deleteBuffers(GLsizei n, const GLuint* buffers)
{
    ux3d::imgui::profiling::countGlCall();
    glDeleteBuffers(n, buffers);
}

void GladGL::drawBuffers(GLsizei n, const GLenum* bufs)
{
    ux3d::imgui::profiling::countGlCall();
    glDrawBuffers(n, bufs);
}

void GladGL::bindBuffer(GLenum target, GLuint buffer)
{
    ux3d::imgui::profiling::countGlCall();
    glBindBuffer(target, buffer);
}

void GladGL::bindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
    ux3d::imgui::profiling::countGlCall();
    glBindBufferBase(target, index, buffer);
}

void GladGL::bindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    ux3d::imgui::profiling::countGlCall();
    glBindBufferRange(target, index, buffer, offset, size);
}

void GladGL::bufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage)
{
    ux3d::imgui::profiling::countGlCall();
    if (size > 0) {
        ux3d::imgui::profiling::countBufferAllocation(static_cast<uint64_t>(size));
        if (data != nullptr) {
            ux3d::imgui::profiling::countBufferUpload(static_cast<uint64_t>(size));
        }
    }
    glBufferData(target, size, data, usage);
}

void GladGL::bufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data)
{
    ux3d::imgui::profiling::countGlCall();
    if (data != nullptr && size > 0) {
        ux3d::imgui::profiling::countBufferUpload(static_cast<uint64_t>(size));
    }
    glBufferSubData(target, offset, size, data);
}

void GladGL::getBufferParameteriv(GLenum target, GLenum value, GLint* data)
{
    ux3d::imgui::profiling::countGlCall();
    glGetBufferParameteriv(target, value, data);
}

// Vertex arrays:

void GladGL::genVertexArrays(GLsizei n, GLuint* arrays)
{
    ux3d::imgui::profiling::countGlCall();
    ux3d::imgui::profiling::countVaoCreation(n > 0 ? static_cast<uint64_t>(n) : 0);
    glGenVertexArrays(n, arrays);
}

void GladGL::deleteVertexArrays(GLsizei n, const GLuint* arrays)
{
    ux3d::imgui::profiling::countGlCall();
    glDeleteVertexArrays(n, arrays);
}

void GladGL::bindVertexArray(GLuint array)
{
    ux3d::imgui::profiling::countGlCall();
    glBindVertexArray(array);
}

// Vertex attributes:

GLint GladGL::getAttribLocation(GLuint program, const GLchar* name)
{
    ux3d::imgui::profiling::countGlCall();
    return glGetAttribLocation(program, name);
}

void GladGL::getActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name)
{
    ux3d::imgui::profiling::countGlCall();
    glGetActiveAttrib(program, index, bufSize, length, size, type, name);
}

void GladGL::vertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer)
{
    ux3d::imgui::profiling::countGlCall();
    glVertexAttribPointer(index, size, type, normalized, stride, pointer);
}

void GladGL::enableVertexAttribArray(GLuint index)
{
    ux3d::imgui::profiling::countGlCall();
    glEnableVertexAttribArray(index);
}

// Uniforms:

GLint GladGL::getUniformLocation(GLuint program, const GLchar* name)
{
    ux3d::imgui::profiling::countGlCall();
    return glGetUniformLocation(program, name);
}

void GladGL::getActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name)
{
    ux3d::imgui::profiling::countGlCall();
    glGetActiveUniform(program, index, bufSize, length, size, type, name);
}

void GladGL::getActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformBlockName)
{
    ux3d::imgui::profiling::countGlCall();
    glGetActiveUniformBlockName(program, uniformBlockIndex, bufSize, length, uniformBlockName);
}

void GladGL::uniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
    ux3d::imgui::profiling::countGlCall();
    glUniformBlockBinding(program, uniformBlockIndex, uniformBlockBinding);
}

void GladGL::getActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params)
{
    ux3d::imgui::profiling::countGlCall();
    glGetActiveUniformBlockiv(program, uniformBlockIndex, pname, params);
}

void GladGL::uniform1i(GLint location, GLint v0)
{
    ux3d::imgui::profiling::countGlCall();
    glUniform1i(location, v0);
}

void GladGL::uniform1fv(GLint location, GLsizei count, const GLfloat* value)
{
    ux3d::imgui::profiling::countGlCall();
    glUniform1fv(location, count, value);
}

void GladGL::uniform2fv(GLint location, GLsizei count, const GLfloat* value)
{
    ux3d::imgui::profiling::countGlCall();
    glUniform2fv(location, count, value);
}

void GladGL::uniform3fv(GLint location, GLsizei count, const GLfloat* value)
{
    ux3d::imgui::profiling::countGlCall();
    glUniform3fv(location, count, value);
}

void GladGL::uniform4fv(GLint location, GLsizei count, const GLfloat* value)
{
    ux3d::imgui::profiling::countGlCall();
    glUniform4fv(location, count, value);
}

void GladGL::uniform1iv(GLint location, GLsizei count, const GLint* value)
{
    ux3d::imgui::profiling::countGlCall();
    glUniform1iv(location, count, value);
}

void GladGL::uniform2iv(GLint location, GLsizei count, const GLint* value)
{
    ux3d::imgui::profiling::countGlCall();
    glUniform2iv(location, count, value);
}

void GladGL::uniform3iv(GLint location, GLsizei count, const GLint* value)
{
    ux3d::imgui::profiling::countGlCall();
    glUniform3iv(location, count, value);
}

void GladGL::uniform4iv(GLint location, GLsizei count, const GLint* value)
{
    ux3d::imgui::profiling::countGlCall();
    glUniform4iv(location, count, value);
}

void GladGL::uniform1uiv(GLint location, GLsizei count, const GLuint* value)
{
    ux3d::imgui::profiling::countGlCall();
    glUniform1uiv(location, count, value);
}

void GladGL::uniform2uiv(GLint location, GLsizei count, const GLuint* value)
{
    ux3d::imgui::profiling::countGlCall();
    glUniform2uiv(location, count, value);
}

void GladGL::uniform3uiv(GLint location, GLsizei count, const GLuint* value)
{
    ux3d::imgui::profiling::countGlCall();
    glUniform3uiv(location, count, value);
}

void GladGL::uniform4uiv(GLint location, GLsizei count, const GLuint* value)
{
    ux3d::imgui::profiling::countGlCall();
    glUniform4uiv(location, count, value);
}

void GladGL::uniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    ux3d::imgui::profiling::countGlCall();
    glUniformMatrix2fv(location, count, transpose, value);
}

void GladGL::uniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    ux3d::imgui::profiling::countGlCall();
    glUniformMatrix3fv(location, count, transpose, value);
}

void GladGL::uniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    ux3d::imgui::profiling::countGlCall();
    glUniformMatrix4fv(location, count, transpose, value);
}

// Queries:

void GladGL::genQueries(GLsizei n, GLuint* ids)
{
    ux3d::imgui::profiling::countGlCall();
    glGenQueries(n, ids);
}

void GladGL::deleteQueries(GLsizei n, const GLuint* ids)
{
    ux3d::imgui::profiling::countGlCall();
    glDeleteQueries(n, ids);
}

void GladGL::beginQuery(GLenum target, GLuint id)
{
    ux3d::imgui::profiling::countGlCall();
    glBeginQuery(target, id);
}

void GladGL::endQuery(GLenum target)
{
    ux3d::imgui::profiling::countGlCall();
    glEndQuery(target);
}

void GladGL::getQueryObjectiv(GLuint id, GLenum pname, GLint* params)
{
    ux3d::imgui::profiling::countGlCall();
    glGetQueryObjectiv(id, pname, params);
}

void GladGL::getQueryObjectui64v(GLuint id, GLenum pname, GLuint64* params)
{
    ux3d::imgui::profiling::countGlCall();
    glGetQueryObjectui64v(id, pname, params);
}

// Enqueuing work:

void GladGL::drawArrays(GLenum mode, GLint first, GLsizei count)
{
    ux3d::imgui::profiling::countGlCall();
    ux3d::imgui::profiling::countRendererDrawCall();
    glDrawArrays(mode, first, count);
}

void GladGL::drawElements(GLenum mode, GLsizei count, GLenum type, const void* indices)
{
    ux3d::imgui::profiling::countGlCall();
    ux3d::imgui::profiling::countRendererDrawCall();
    glDrawElements(mode, count, type, indices);
}

} // namespace ux3d::renderer
