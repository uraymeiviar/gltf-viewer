// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include <glad/glad.h>
#include "imgui/GladGL.h"
#include <cstdio>
#include <cstring>

namespace ux3d::renderer {

bool GladGL::hasExtension(const GLchar* extensionName)
{
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

// Global:

GLenum GladGL::getError()
{
    return glGetError();
}

void GladGL::getIntegerv(GLenum pname, GLint* data)
{
    glGetIntegerv(pname, data);
}

void GladGL::getFloatv(GLenum pname, GLfloat* param)
{
    glGetFloatv(pname, param);
}

const GLubyte* GladGL::getString(GLenum name)
{
    return glGetString(name);
}

const GLubyte* GladGL::getStringi(GLenum name, GLuint index)
{
    return glGetStringi(name, index);
}

void GladGL::enable(GLenum cap)
{
    glEnable(cap);
}

void GladGL::disable(GLenum cap)
{
    glDisable(cap);
}

void GladGL::enablei(GLenum cap, GLuint index)
{
    glEnablei(cap, index);
}

void GladGL::disablei(GLenum cap, GLuint index)
{
    glDisablei(cap, index);
}

void GladGL::finish()
{
    glFinish();
}

// Shaders:

GLuint GladGL::createShader(GLenum shaderType)
{
    return glCreateShader(shaderType);
}

void GladGL::deleteShader(GLuint shader)
{
    glDeleteShader(shader);
}

void GladGL::shaderSource(GLuint shader, GLsizei count, const GLchar** string, const GLint* length)
{
    glShaderSource(shader, count, string, length);
}

void GladGL::compileShader(GLuint shader)
{
    glCompileShader(shader);
}

void GladGL::getShaderiv(GLuint shader, GLenum pname, GLint* params)
{
    glGetShaderiv(shader, pname, params);
}

void GladGL::getShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei* length, GLchar* infoLog)
{
    glGetShaderInfoLog(shader, maxLength, length, infoLog);
}

// Programs:

GLuint GladGL::createProgram()
{
    return glCreateProgram();
}

void GladGL::deleteProgram(GLuint program)
{
    glDeleteProgram(program);
}

void GladGL::attachShader(GLuint program, GLuint shader)
{
    glAttachShader(program, shader);
}

void GladGL::detachShader(GLuint program, GLuint shader)
{
    glDetachShader(program, shader);
}

void GladGL::validateProgram(GLuint program)
{
    glValidateProgram(program);
}

void GladGL::linkProgram(GLuint program)
{
    glLinkProgram(program);
}

void GladGL::useProgram(GLuint program)
{
    glUseProgram(program);
}

void GladGL::getProgramInfoLog(GLuint program, GLsizei maxLength, GLsizei* length, GLchar* infoLog)
{
    glGetProgramInfoLog(program, maxLength, length, infoLog);
}

void GladGL::getProgramiv(GLuint program, GLenum pname, GLint* params)
{
    glGetProgramiv(program, pname, params);
}

// Pipeline state:

void GladGL::viewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    glViewport(x, y, width, height);
}

void GladGL::cullFace(GLenum mode)
{
    glCullFace(mode);
}

void GladGL::polygonMode(GLenum face, GLenum mode)
{
    glPolygonMode(face, mode);
}

void GladGL::scissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
    glScissor(x, y, width, height);
}

void GladGL::frontFace(GLenum mode)
{
    glFrontFace(mode);
}

void GladGL::blendFunc(GLenum sfactor, GLenum dfactor)
{
    glBlendFunc(sfactor, dfactor);
}

void GladGL::blendEquation(GLenum mode)
{
    glBlendEquation(mode);
}

void GladGL::depthFunc(GLenum func)
{
    glDepthFunc(func);
}

void GladGL::colorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
    glColorMask(red, green, blue, alpha);
}

void GladGL::clearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    glClearColor(red, green, blue, alpha);
}

void GladGL::clear(GLbitfield mask)
{
    glClear(mask);
}

// Textures:

void GladGL::genTextures(GLsizei n, GLuint* textures)
{
    glGenTextures(n, textures);
}

void GladGL::deleteTextures(GLsizei n, const GLuint* textures)
{
    glDeleteTextures(n, textures);
}

void GladGL::texParameteri(GLuint texture, GLenum pname, GLint param)
{
    glTexParameteri(texture, pname, param);
}

void GladGL::texParameterf(GLuint texture, GLenum pname, GLfloat param)
{
    glTexParameterf(texture, pname, param);
}

void GladGL::bindTexture(GLenum target, GLuint texture)
{
    glBindTexture(target, texture);
}

void GladGL::pixelStorei(GLenum pname, GLint param)
{
    glPixelStorei(pname, param);
}

void GladGL::texImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* data)
{
    glTexImage2D(target, level, internalformat, width, height, border, format, type, data);
}

void GladGL::texParameteriv(GLenum target, GLenum pname, const GLint* params)
{
    glTexParameteriv(target, pname, params);
}

void GladGL::texParameterfv(GLenum target, GLenum pname, const GLfloat* params)
{
    glTexParameterfv(target, pname, params);
}

void GladGL::getTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint* params)
{
    glGetTexLevelParameteriv(target, level, pname, params);
}

void GladGL::activeTexture(GLenum texture)
{
    glActiveTexture(texture);
}

void GladGL::generateMipmap(GLenum target)
{
    glGenerateMipmap(target);
}

void GladGL::copyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

void GladGL::getTexImage(GLenum target, GLint level, GLenum format, GLenum type, void* pixels)
{
    glGetTexImage(target, level, format, type, pixels);
}

void GladGL::texImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* data)
{
    glTexImage3D(target, level, internalformat, width, height, depth, border, format, type, data);
}

void GladGL::texSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels)
{
    glTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type, pixels);
}

// Framebuffers:

void GladGL::genFramebuffers(GLsizei n, GLuint* framebuffers)
{
    glGenFramebuffers(n, framebuffers);
}

void GladGL::deleteFramebuffers(GLsizei n, GLuint* framebuffers)
{
    glDeleteFramebuffers(n, framebuffers);
}

void GladGL::bindFramebuffer(GLenum target, GLuint framebuffer)
{
    glBindFramebuffer(target, framebuffer);
}

void GladGL::framebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

GLenum GladGL::checkFramebufferStatus(GLenum target)
{
    return glCheckFramebufferStatus(target);
}

void GladGL::clearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat* value)
{
    glClearBufferfv(buffer, drawbuffer, value);
}

void GladGL::clearBufferiv(GLenum buffer, GLint drawbuffer, const GLint* value)
{
    glClearBufferiv(buffer, drawbuffer, value);
}

void GladGL::blitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
    glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

// Renderbuffers:

void GladGL::genRenderbuffers(GLsizei n, GLuint* renderbuffers)
{
    glGenRenderbuffers(n, renderbuffers);
}

void GladGL::bindRenderbuffer(GLenum target, GLuint renderbuffer)
{
    glBindRenderbuffer(target, renderbuffer);
}

void GladGL::renderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
    glRenderbufferStorage(target, internalformat, width, height);
}

void GladGL::framebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
    glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

void GladGL::deleteRenderbuffers(GLsizei n, GLuint* renderbuffers)
{
    glDeleteRenderbuffers(n, renderbuffers);
}

// Buffers:

void GladGL::genBuffers(GLsizei n, GLuint* buffers)
{
    glGenBuffers(n, buffers);
}

void GladGL::deleteBuffers(GLsizei n, const GLuint* buffers)
{
    glDeleteBuffers(n, buffers);
}

void GladGL::drawBuffers(GLsizei n, const GLenum* bufs)
{
    glDrawBuffers(n, bufs);
}

void GladGL::bindBuffer(GLenum target, GLuint buffer)
{
    glBindBuffer(target, buffer);
}

void GladGL::bindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
    glBindBufferBase(target, index, buffer);
}

void GladGL::bindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    glBindBufferRange(target, index, buffer, offset, size);
}

void GladGL::bufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage)
{
    glBufferData(target, size, data, usage);
}

void GladGL::bufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data)
{
    glBufferSubData(target, offset, size, data);
}

void GladGL::getBufferParameteriv(GLenum target, GLenum value, GLint* data)
{
    glGetBufferParameteriv(target, value, data);
}

// Vertex arrays:

void GladGL::genVertexArrays(GLsizei n, GLuint* arrays)
{
    glGenVertexArrays(n, arrays);
}

void GladGL::deleteVertexArrays(GLsizei n, const GLuint* arrays)
{
    glDeleteVertexArrays(n, arrays);
}

void GladGL::bindVertexArray(GLuint array)
{
    glBindVertexArray(array);
}

// Vertex attributes:

GLint GladGL::getAttribLocation(GLuint program, const GLchar* name)
{
    return glGetAttribLocation(program, name);
}

void GladGL::getActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name)
{
    glGetActiveAttrib(program, index, bufSize, length, size, type, name);
}

void GladGL::vertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer)
{
    glVertexAttribPointer(index, size, type, normalized, stride, pointer);
}

void GladGL::enableVertexAttribArray(GLuint index)
{
    glEnableVertexAttribArray(index);
}

// Uniforms:

GLint GladGL::getUniformLocation(GLuint program, const GLchar* name)
{
    return glGetUniformLocation(program, name);
}

void GladGL::getActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name)
{
    glGetActiveUniform(program, index, bufSize, length, size, type, name);
}

void GladGL::getActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformBlockName)
{
    glGetActiveUniformBlockName(program, uniformBlockIndex, bufSize, length, uniformBlockName);
}

void GladGL::uniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
    glUniformBlockBinding(program, uniformBlockIndex, uniformBlockBinding);
}

void GladGL::getActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params)
{
    glGetActiveUniformBlockiv(program, uniformBlockIndex, pname, params);
}

void GladGL::uniform1i(GLint location, GLint v0)
{
    glUniform1i(location, v0);
}

void GladGL::uniform1fv(GLint location, GLsizei count, const GLfloat* value)
{
    glUniform1fv(location, count, value);
}

void GladGL::uniform2fv(GLint location, GLsizei count, const GLfloat* value)
{
    glUniform2fv(location, count, value);
}

void GladGL::uniform3fv(GLint location, GLsizei count, const GLfloat* value)
{
    glUniform3fv(location, count, value);
}

void GladGL::uniform4fv(GLint location, GLsizei count, const GLfloat* value)
{
    glUniform4fv(location, count, value);
}

void GladGL::uniform1iv(GLint location, GLsizei count, const GLint* value)
{
    glUniform1iv(location, count, value);
}

void GladGL::uniform2iv(GLint location, GLsizei count, const GLint* value)
{
    glUniform2iv(location, count, value);
}

void GladGL::uniform3iv(GLint location, GLsizei count, const GLint* value)
{
    glUniform3iv(location, count, value);
}

void GladGL::uniform4iv(GLint location, GLsizei count, const GLint* value)
{
    glUniform4iv(location, count, value);
}

void GladGL::uniform1uiv(GLint location, GLsizei count, const GLuint* value)
{
    glUniform1uiv(location, count, value);
}

void GladGL::uniform2uiv(GLint location, GLsizei count, const GLuint* value)
{
    glUniform2uiv(location, count, value);
}

void GladGL::uniform3uiv(GLint location, GLsizei count, const GLuint* value)
{
    glUniform3uiv(location, count, value);
}

void GladGL::uniform4uiv(GLint location, GLsizei count, const GLuint* value)
{
    glUniform4uiv(location, count, value);
}

void GladGL::uniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    glUniformMatrix2fv(location, count, transpose, value);
}

void GladGL::uniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    glUniformMatrix3fv(location, count, transpose, value);
}

void GladGL::uniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    glUniformMatrix4fv(location, count, transpose, value);
}

// Enqueuing work:

void GladGL::drawArrays(GLenum mode, GLint first, GLsizei count)
{
    glDrawArrays(mode, first, count);
}

void GladGL::drawElements(GLenum mode, GLsizei count, GLenum type, const void* indices)
{
    glDrawElements(mode, count, type, indices);
}

} // namespace ux3d::renderer
