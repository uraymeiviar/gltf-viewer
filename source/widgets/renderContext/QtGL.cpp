// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "widgets/renderContext/QtGL.h"
#include <stdio.h>
#include <string.h>

using namespace ux3d;
using namespace ux3d::renderer;

QtGL::QtGL()
{
    initializeOpenGLFunctions();
}

// Custom Helpers
bool QtGL::hasExtension(const GLchar* extensionName)
{
    if (!extensionsCache) {
        extensionsCache = std::unordered_set<std::string>();
        int numberOfExtensions;
        getIntegerv(GL_NUM_EXTENSIONS, &numberOfExtensions);
        for (int i = 0; i < numberOfExtensions; i++) {
            const GLchar* supportedExtension = reinterpret_cast<const GLchar*>(getStringi(GL_EXTENSIONS, i));
            extensionsCache.value().insert(std::string(supportedExtension));
        }
    }

    std::string searchString(extensionName);
    return extensionsCache.value().contains(searchString);
}

// Global:

GLenum QtGL::getError()
{
    return glGetError();
}

void QtGL::getIntegerv(GLenum pname, GLint* data)
{
    glGetIntegerv(pname, data);
}

void QtGL::getFloatv(GLenum pname, GLfloat* param)
{
    glGetFloatv(pname, param);
}

const GLubyte* QtGL::getString(GLenum name)
{
    return glGetString(name);
}

const GLubyte* QtGL::getStringi(GLenum name, GLuint index)
{
    return glGetStringi(name, index);
}

void QtGL::enable(GLenum cap)
{
    glEnable(cap);
}

void QtGL::disable(GLenum cap)
{
    glDisable(cap);
}

void QtGL::enablei(GLenum cap, GLuint index)
{
    glEnablei(cap, index);
}

void QtGL::disablei(GLenum cap, GLuint index)
{
    glDisablei(cap, index);
}

void QtGL::finish()
{
    glFinish();
}

// Shaders:

GLuint QtGL::createShader(GLenum shaderType)
{
    return glCreateShader(shaderType);
}

void QtGL::deleteShader(GLuint shader)
{
    glDeleteShader(shader);
}

void QtGL::shaderSource(GLuint shader, GLsizei count, const GLchar** string, const GLint* length)
{
    glShaderSource(shader, count, string, length);
}

void QtGL::compileShader(GLuint shader)
{
    glCompileShader(shader);
}

void QtGL::getShaderiv(GLuint shader, GLenum pname, GLint* params)
{
    glGetShaderiv(shader, pname, params);
}

void QtGL::getShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei* length, GLchar* infoLog)
{
    glGetShaderInfoLog(shader, maxLength, length, infoLog);
}

// Programs:

GLuint QtGL::createProgram()
{
    return glCreateProgram();
}

void QtGL::deleteProgram(GLuint program)
{
    glDeleteProgram(program);
}

void QtGL::attachShader(GLuint program, GLuint shader)
{
    glAttachShader(program, shader);
}

void QtGL::detachShader(GLuint program, GLuint shader)
{
    glDetachShader(program, shader);
}

void QtGL::validateProgram(GLuint program)
{
    glValidateProgram(program);
}

void QtGL::linkProgram(GLuint program)
{
    glLinkProgram(program);
}

void QtGL::useProgram(GLuint program)
{
    glUseProgram(program);
}

void QtGL::getProgramInfoLog(GLuint program, GLsizei maxLength, GLsizei* length, GLchar* infoLog)
{
    glGetProgramInfoLog(program, maxLength, length, infoLog);
}

void QtGL::getProgramiv(GLuint program, GLenum pname, GLint* params)
{
    glGetProgramiv(program, pname, params);
}

// Pipeline state:

void QtGL::viewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    glViewport(x, y, width, height);
}

void QtGL::cullFace(GLenum mode)
{
    glCullFace(mode);
}

void QtGL::polygonMode(GLenum face, GLenum mode)
{
    glPolygonMode(face, mode);
}

void QtGL::scissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
    glScissor(x, y, width, height);
}

void QtGL::frontFace(GLenum mode)
{
    glFrontFace(mode);
}

void QtGL::blendFunc(GLenum sfactor, GLenum dfactor)
{
    glBlendFunc(sfactor, dfactor);
}

void QtGL::blendEquation(GLenum mode)
{
    glBlendEquation(mode);
}

void QtGL::depthFunc(GLenum func)
{
    glDepthFunc(func);
}

void QtGL::colorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
    glColorMask(red, green, blue, alpha);
}

void QtGL::clearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    glClearColor(red, green, blue, alpha);
}

void QtGL::clear(GLbitfield mask)
{
    glClear(mask);
}

// Textures:

void QtGL::genTextures(GLsizei n, GLuint* textures)
{
    glGenTextures(n, textures);
}

void QtGL::deleteTextures(GLsizei n, const GLuint* textures)
{
    glDeleteTextures(n, textures);
}

void QtGL::texParameteri(GLuint texture, GLenum pname, GLint param)
{
    glTexParameteri(texture, pname, param);
}

void QtGL::texParameterf(GLuint texture, GLenum pname, GLfloat param)
{
    glTexParameterf(texture, pname, param);
}

void QtGL::bindTexture(GLenum target, GLuint texture)
{
    glBindTexture(target, texture);
}

void QtGL::pixelStorei(GLenum pname, GLint param)
{
    glPixelStorei(pname, param);
}

void QtGL::texImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* data)
{
    glTexImage2D(target, level, internalformat, width, height, border, format, type, data);
}

void QtGL::texParameteriv(GLenum target, GLenum pname, const GLint* params)
{
    glTexParameteriv(target, pname, params);
}

void QtGL::texParameterfv(GLenum target, GLenum pname, const GLfloat* params)
{
    texParameterfv(target, pname, params);
}

void QtGL::getTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint* params)
{
    glGetTexLevelParameteriv(target, level, pname, params);
}

void QtGL::activeTexture(GLenum texture)
{
    glActiveTexture(texture);
}

void QtGL::generateMipmap(GLenum target)
{
    glGenerateMipmap(target);
}

void QtGL::copyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
    glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height);
}

void QtGL::getTexImage(GLenum target, GLint level, GLenum format, GLenum type, void* pixels)
{
    glGetTexImage(target, level, format, type, pixels);
}

void QtGL::texImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* data)
{
    glTexImage3D(target, level, internalformat, width, height, depth, border, format, type, data);
}

void QtGL::texSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels)
{
    glTexSubImage3D(target, level, xoffset, yoffset, zoffset, height, width, depth, format, type, pixels);
}

// Framebuffers:

void QtGL::genFramebuffers(GLsizei n, GLuint* framebuffers)
{
    glGenFramebuffers(n, framebuffers);
}

void QtGL::deleteFramebuffers(GLsizei n, GLuint* framebuffers)
{
    glDeleteFramebuffers(n, framebuffers);
}

void QtGL::bindFramebuffer(GLenum target, GLuint framebuffer)
{
    glBindFramebuffer(target, framebuffer);
}

void QtGL::framebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
    glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

GLenum QtGL::checkFramebufferStatus(GLenum target)
{
    return glCheckFramebufferStatus(target);
}

void QtGL::clearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat* value)
{
    glClearBufferfv(buffer, drawbuffer, value);
}

void QtGL::clearBufferiv(GLenum buffer, GLint drawbuffer, const GLint* value)
{
    glClearBufferiv(buffer, drawbuffer, value);
}

void QtGL::blitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter)
{
    glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, mask, filter);
}

// Renderbuffers:

void QtGL::genRenderbuffers(GLsizei n, GLuint* renderbuffers)
{
    glGenRenderbuffers(n, renderbuffers);
}

void QtGL::bindRenderbuffer(GLenum target, GLuint renderbuffer)
{
    glBindRenderbuffer(target, renderbuffer);
}

void QtGL::renderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
    glRenderbufferStorage(target, internalformat, width, height);
}

void QtGL::framebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
    glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

void QtGL::deleteRenderbuffers(GLsizei n, GLuint* renderbuffers)
{
    glDeleteRenderbuffers(n, renderbuffers);
}

// Buffers:

void QtGL::genBuffers(GLsizei n, GLuint* buffers)
{
    glGenBuffers(n, buffers);
}

void QtGL::deleteBuffers(GLsizei n, const GLuint* buffers)
{
    glDeleteBuffers(n, buffers);
}

void QtGL::drawBuffers(GLsizei n, const GLenum* bufs)
{
    glDrawBuffers(n, bufs);
}

void QtGL::bindBuffer(GLenum target, GLuint buffer)
{
    glBindBuffer(target, buffer);
}

void QtGL::bindBufferBase(GLenum target, GLuint index, GLuint buffer)
{
    glBindBufferBase(target, index, buffer);
}

void QtGL::bindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)
{
    glBindBufferRange(target, index, buffer, offset, size);
}

void QtGL::bufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage)
{
    glBufferData(target, size, data, usage);
}

void QtGL::bufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data)
{
    glBufferSubData(target, offset, size, data);
}

void QtGL::getBufferParameteriv(GLenum target, GLenum value, GLint* data)
{
    glGetBufferParameteriv(target, value, data);
}

// Vertex arrays:

void QtGL::genVertexArrays(GLsizei n, GLuint* arrays)
{
    glGenVertexArrays(n, arrays);
}

void QtGL::deleteVertexArrays(GLsizei n, const GLuint* arrays)
{
    glDeleteVertexArrays(n, arrays);
}

void QtGL::bindVertexArray(GLuint array)
{
    glBindVertexArray(array);
}

// Vertex attributes:

GLint QtGL::getAttribLocation(GLuint program, const GLchar* name)
{
    return glGetAttribLocation(program, name);
}

void QtGL::getActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name)
{
    glGetActiveAttrib(program, index, bufSize, length, size, type, name);
}

void QtGL::vertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer)
{
    glVertexAttribPointer(index, size, type, normalized, stride, pointer);
}

void QtGL::enableVertexAttribArray(GLuint index)
{
    glEnableVertexAttribArray(index);
}

// Uniforms:

GLint QtGL::getUniformLocation(GLuint program, const GLchar* name)
{
    return glGetUniformLocation(program, name);
}

void QtGL::getActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name)
{
    glGetActiveUniform(program, index, bufSize, length, size, type, name);
}

void QtGL::getActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformBlockName)
{
    glGetActiveUniformBlockName(program, uniformBlockIndex, bufSize, length, uniformBlockName);
}

void QtGL::uniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
    glUniformBlockBinding(program, uniformBlockIndex, uniformBlockBinding);
}

void QtGL::getActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params)
{
    glGetActiveUniformBlockiv(program, uniformBlockIndex, pname, params);
}

void QtGL::uniform1i(GLint location, GLint v0)
{
    glUniform1i(location, v0);
}

void QtGL::uniform1fv(GLint location, GLsizei count, const GLfloat* value)
{
    glUniform1fv(location, count, value);
}

void QtGL::uniform2fv(GLint location, GLsizei count, const GLfloat* value)
{
    glUniform2fv(location, count, value);
}

void QtGL::uniform3fv(GLint location, GLsizei count, const GLfloat* value)
{
    glUniform3fv(location, count, value);
}

void QtGL::uniform4fv(GLint location, GLsizei count, const GLfloat* value)
{
    glUniform4fv(location, count, value);
}

void QtGL::uniform1iv(GLint location, GLsizei count, const GLint* value)
{
    glUniform1iv(location, count, value);
}

void QtGL::uniform2iv(GLint location, GLsizei count, const GLint* value)
{
    glUniform2iv(location, count, value);
}

void QtGL::uniform3iv(GLint location, GLsizei count, const GLint* value)
{
    glUniform3iv(location, count, value);
}

void QtGL::uniform4iv(GLint location, GLsizei count, const GLint* value)
{
    glUniform4iv(location, count, value);
}

void QtGL::uniform1uiv(GLint location, GLsizei count, const GLuint* value)
{
    glUniform1uiv(location, count, value);
}

void QtGL::uniform2uiv(GLint location, GLsizei count, const GLuint* value)
{
    glUniform2uiv(location, count, value);
}

void QtGL::uniform3uiv(GLint location, GLsizei count, const GLuint* value)
{
    glUniform3uiv(location, count, value);
}

void QtGL::uniform4uiv(GLint location, GLsizei count, const GLuint* value)
{
    glUniform4uiv(location, count, value);
}

void QtGL::uniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    glUniformMatrix2fv(location, count, transpose, value);
}

void QtGL::uniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    glUniformMatrix3fv(location, count, transpose, value);
}

void QtGL::uniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    glUniformMatrix4fv(location, count, transpose, value);
}

// Enqueuing work:

void QtGL::drawArrays(GLenum mode, GLint first, GLsizei count)
{
    glDrawArrays(mode, first, count);
}

void QtGL::drawElements(GLenum mode, GLsizei count, GLenum type, const void* indices)
{
    glDrawElements(mode, count, type, indices);
}
