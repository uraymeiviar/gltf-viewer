#pragma once

#include "OpenGL.h"

namespace ux3d::renderer::gl {
// Encapsulates all OpenGL calls the renderer needs.
// Function prototypes as per https://www.khronos.org/registry/OpenGL-Refpages/gl4/
class GL {
public:
    virtual ~GL() = default;

    // Custom Helpers
    virtual bool hasExtension(const GLchar* extensionName) = 0;

    // Global:
    virtual GLenum getError() = 0;
    virtual void getIntegerv(GLenum pname, GLint* data) = 0;
    virtual void getFloatv(GLenum pname, GLfloat* param) = 0;
    virtual const GLubyte* getString(GLenum name) = 0;
    virtual const GLubyte* getStringi(GLenum name, GLuint index) = 0;
    virtual void enable(GLenum cap) = 0;
    virtual void disable(GLenum cap) = 0;
    virtual void enablei(GLenum cap, GLuint index) = 0;
    virtual void disablei(GLenum cap, GLuint index) = 0;
    virtual void finish() = 0;

    // Shaders:
    virtual GLuint createShader(GLenum shaderType) = 0;
    virtual void deleteShader(GLuint shader) = 0;
    virtual void shaderSource(GLuint shader, GLsizei count, const GLchar** string, const GLint* length) = 0;
    virtual void compileShader(GLuint shader) = 0;
    virtual void getShaderiv(GLuint shader, GLenum pname, GLint* params) = 0;
    virtual void getShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei* length, GLchar* infoLog) = 0;

    // Programs:
    virtual GLuint createProgram() = 0;
    virtual void deleteProgram(GLuint program) = 0;
    virtual void attachShader(GLuint program, GLuint shader) = 0;
    virtual void detachShader(GLuint program, GLuint shader) = 0;
    virtual void validateProgram(GLuint program) = 0;
    virtual void linkProgram(GLuint program) = 0;
    virtual void useProgram(GLuint program) = 0;
    virtual void getProgramInfoLog(GLuint program, GLsizei maxLength, GLsizei* length, GLchar* infoLog) = 0;
    virtual void getProgramiv(GLuint program, GLenum pname, GLint* params) = 0;

    // Pipeline state:
    virtual void viewport(GLint x, GLint y, GLsizei width, GLsizei height) = 0;
    virtual void cullFace(GLenum mode) = 0;
    virtual void polygonMode(GLenum face, GLenum mode) = 0;
    virtual void scissor(GLint x, GLint y, GLsizei width, GLsizei height) = 0;
    virtual void frontFace(GLenum mode) = 0;
    virtual void blendFunc(GLenum sfactor, GLenum dfactor) = 0;
    virtual void blendEquation(GLenum mode) = 0;
    virtual void depthFunc(GLenum func) = 0;
    virtual void colorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) = 0;
    virtual void clearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) = 0;
    virtual void clear(GLbitfield mask) = 0;

    // Textures:
    virtual void genTextures(GLsizei n, GLuint* textures) = 0;
    virtual void deleteTextures(GLsizei n, const GLuint* textures) = 0;
    virtual void texParameteri(GLuint texture, GLenum pname, GLint param) = 0;
    virtual void texParameterf(GLuint texture, GLenum pname, GLfloat param) = 0;
    virtual void bindTexture(GLenum target, GLuint texture) = 0;
    virtual void pixelStorei(GLenum pname, GLint param) = 0;
    virtual void texImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* data) = 0;
    virtual void texParameteriv(GLenum target, GLenum pname, const GLint* params) = 0;
    virtual void texParameterfv(GLenum target, GLenum pname, const GLfloat* params) = 0;
    virtual void getTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint* params) = 0;
    virtual void activeTexture(GLenum texture) = 0;
    virtual void generateMipmap(GLenum target) = 0;
    virtual void copyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) = 0;
    virtual void getTexImage(GLenum target, GLint level, GLenum format, GLenum type, void* pixels) = 0;
    virtual void texImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* data) = 0;
    virtual void texSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels) = 0;

    // Framebuffers:
    virtual void genFramebuffers(GLsizei n, GLuint* framebuffers) = 0;
    virtual void deleteFramebuffers(GLsizei n, GLuint* framebuffers) = 0;
    virtual void bindFramebuffer(GLenum target, GLuint framebuffer) = 0;
    virtual void framebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) = 0;
    virtual GLenum checkFramebufferStatus(GLenum target) = 0;
    virtual void clearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat* value) = 0;
    virtual void clearBufferiv(GLenum buffer, GLint drawbuffer, const GLint* value) = 0;
    virtual void blitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) = 0;

    // Renderbuffers:
    virtual void genRenderbuffers(GLsizei n, GLuint* renderbuffers) = 0;
    virtual void bindRenderbuffer(GLenum target, GLuint renderbuffer) = 0;
    virtual void renderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) = 0;
    virtual void framebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) = 0;
    virtual void deleteRenderbuffers(GLsizei n, GLuint* renderbuffers) = 0;

    // Buffers:
    virtual void genBuffers(GLsizei n, GLuint* buffers) = 0;
    virtual void deleteBuffers(GLsizei n, const GLuint* buffers) = 0;
    virtual void drawBuffers(GLsizei n, const GLenum* bufs) = 0;
    virtual void bindBuffer(GLenum target, GLuint buffer) = 0;
    virtual void bindBufferBase(GLenum target, GLuint index, GLuint buffer) = 0;
    virtual void bindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) = 0;
    virtual void bufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) = 0;
    virtual void bufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data) = 0;
    virtual void getBufferParameteriv(GLenum target, GLenum value, GLint* data) = 0;

    // Vertex arrays:
    virtual void genVertexArrays(GLsizei n, GLuint* arrays) = 0;
    virtual void deleteVertexArrays(GLsizei n, const GLuint* arrays) = 0;
    virtual void bindVertexArray(GLuint array) = 0;

    // Vertex attributes:
    virtual GLint getAttribLocation(GLuint program, const GLchar* name) = 0;
    virtual void getActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name) = 0;
    virtual void vertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer) = 0;
    virtual void enableVertexAttribArray(GLuint index) = 0;

    // Uniforms:
    virtual GLint getUniformLocation(GLuint program, const GLchar* name) = 0;
    virtual void getActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name) = 0;
    virtual void getActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformBlockName) = 0;
    virtual void uniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding) = 0;
    virtual void getActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params) = 0;
    virtual void uniform1i(GLint location, GLint v0) = 0;
    virtual void uniform1fv(GLint location, GLsizei count, const GLfloat* value) = 0;
    virtual void uniform2fv(GLint location, GLsizei count, const GLfloat* value) = 0;
    virtual void uniform3fv(GLint location, GLsizei count, const GLfloat* value) = 0;
    virtual void uniform4fv(GLint location, GLsizei count, const GLfloat* value) = 0;
    virtual void uniform1iv(GLint location, GLsizei count, const GLint* value) = 0;
    virtual void uniform2iv(GLint location, GLsizei count, const GLint* value) = 0;
    virtual void uniform3iv(GLint location, GLsizei count, const GLint* value) = 0;
    virtual void uniform4iv(GLint location, GLsizei count, const GLint* value) = 0;
    virtual void uniform1uiv(GLint location, GLsizei count, const GLuint* value) = 0;
    virtual void uniform2uiv(GLint location, GLsizei count, const GLuint* value) = 0;
    virtual void uniform3uiv(GLint location, GLsizei count, const GLuint* value) = 0;
    virtual void uniform4uiv(GLint location, GLsizei count, const GLuint* value) = 0;
    virtual void uniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) = 0;
    virtual void uniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) = 0;
    virtual void uniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) = 0;

    // Queries:
    virtual void genQueries(GLsizei n, GLuint* ids) = 0;
    virtual void deleteQueries(GLsizei n, const GLuint* ids) = 0;
    virtual void beginQuery(GLenum target, GLuint id) = 0;
    virtual void endQuery(GLenum target) = 0;
    virtual void getQueryObjectiv(GLuint id, GLenum pname, GLint* params) = 0;
    virtual void getQueryObjectui64v(GLuint id, GLenum pname, GLuint64* params) = 0;

    // Enqueuing work:
    virtual void drawArrays(GLenum mode, GLint first, GLsizei count) = 0;
    virtual void drawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) = 0;
};
}
