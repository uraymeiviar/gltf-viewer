// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <glad/glad.h>
#include "renderer/gl/GL.h"
#include <optional>
#include <string>
#include <unordered_set>

namespace ux3d::renderer {

class GladGL : public gl::GL {
public:
    GladGL() = default;
    virtual ~GladGL() override = default;

    // Custom Helpers
    virtual bool hasExtension(const GLchar* extensionName) override;

    // Global:
    virtual GLenum getError() override;
    virtual void getIntegerv(GLenum pname, GLint* data) override;
    virtual void getFloatv(GLenum pname, GLfloat* param) override;
    virtual const GLubyte* getString(GLenum name) override;
    virtual const GLubyte* getStringi(GLenum name, GLuint index) override;
    virtual void enable(GLenum cap) override;
    virtual void disable(GLenum cap) override;
    virtual void enablei(GLenum cap, GLuint index) override;
    virtual void disablei(GLenum cap, GLuint index) override;
    virtual void finish() override;

    // Shaders:
    virtual GLuint createShader(GLenum shaderType) override;
    virtual void deleteShader(GLuint shader) override;
    virtual void shaderSource(GLuint shader, GLsizei count, const GLchar** string, const GLint* length) override;
    virtual void compileShader(GLuint shader) override;
    virtual void getShaderiv(GLuint shader, GLenum pname, GLint* params) override;
    virtual void getShaderInfoLog(GLuint shader, GLsizei maxLength, GLsizei* length, GLchar* infoLog) override;

    // Programs:
    virtual GLuint createProgram() override;
    virtual void deleteProgram(GLuint program) override;
    virtual void attachShader(GLuint program, GLuint shader) override;
    virtual void detachShader(GLuint program, GLuint shader) override;
    virtual void validateProgram(GLuint program) override;
    virtual void linkProgram(GLuint program) override;
    virtual void useProgram(GLuint program) override;
    virtual void getProgramInfoLog(GLuint program, GLsizei maxLength, GLsizei* length, GLchar* infoLog) override;
    virtual void getProgramiv(GLuint program, GLenum pname, GLint* params) override;

    // Pipeline state:
    virtual void viewport(GLint x, GLint y, GLsizei width, GLsizei height) override;
    virtual void cullFace(GLenum mode) override;
    virtual void polygonMode(GLenum face, GLenum mode) override;
    virtual void scissor(GLint x, GLint y, GLsizei width, GLsizei height) override;
    virtual void frontFace(GLenum mode) override;
    virtual void blendFunc(GLenum sfactor, GLenum dfactor) override;
    virtual void blendEquation(GLenum mode) override;
    virtual void depthFunc(GLenum func) override;
    virtual void colorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) override;
    virtual void clearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) override;
    virtual void clear(GLbitfield mask) override;

    // Textures:
    virtual void genTextures(GLsizei n, GLuint* textures) override;
    virtual void deleteTextures(GLsizei n, const GLuint* textures) override;
    virtual void texParameteri(GLuint texture, GLenum pname, GLint param) override;
    virtual void texParameterf(GLuint texture, GLenum pname, GLfloat param) override;
    virtual void bindTexture(GLenum target, GLuint texture) override;
    virtual void pixelStorei(GLenum pname, GLint param) override;
    virtual void texImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* data) override;
    virtual void texParameteriv(GLenum target, GLenum pname, const GLint* params) override;
    virtual void texParameterfv(GLenum target, GLenum pname, const GLfloat* params) override;
    virtual void getTexLevelParameteriv(GLenum target, GLint level, GLenum pname, GLint* params) override;
    virtual void activeTexture(GLenum texture) override;
    virtual void generateMipmap(GLenum target) override;
    virtual void copyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height) override;
    virtual void getTexImage(GLenum target, GLint level, GLenum format, GLenum type, void* pixels) override;
    virtual void texImage3D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void* data) override;
    virtual void texSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pixels) override;

    // Framebuffers:
    virtual void genFramebuffers(GLsizei n, GLuint* framebuffers) override;
    virtual void deleteFramebuffers(GLsizei n, GLuint* framebuffers) override;
    virtual void bindFramebuffer(GLenum target, GLuint framebuffer) override;
    virtual void framebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) override;
    virtual GLenum checkFramebufferStatus(GLenum target) override;
    virtual void clearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat* value) override;
    virtual void clearBufferiv(GLenum buffer, GLint drawbuffer, const GLint* value) override;
    virtual void blitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) override;

    // Renderbuffers:
    virtual void genRenderbuffers(GLsizei n, GLuint* renderbuffers) override;
    virtual void bindRenderbuffer(GLenum target, GLuint renderbuffer) override;
    virtual void renderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) override;
    virtual void framebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) override;
    virtual void deleteRenderbuffers(GLsizei n, GLuint* renderbuffers) override;

    // Buffers:
    virtual void genBuffers(GLsizei n, GLuint* buffers) override;
    virtual void deleteBuffers(GLsizei n, const GLuint* buffers) override;
    virtual void drawBuffers(GLsizei n, const GLenum* bufs) override;
    virtual void bindBuffer(GLenum target, GLuint buffer) override;
    virtual void bindBufferBase(GLenum target, GLuint index, GLuint buffer) override;
    virtual void bindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) override;
    virtual void bufferData(GLenum target, GLsizeiptr size, const void* data, GLenum usage) override;
    virtual void bufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data) override;
    virtual void getBufferParameteriv(GLenum target, GLenum value, GLint* data) override;

    // Vertex arrays:
    virtual void genVertexArrays(GLsizei n, GLuint* arrays) override;
    virtual void deleteVertexArrays(GLsizei n, const GLuint* arrays) override;
    virtual void bindVertexArray(GLuint array) override;

    // Vertex attributes:
    virtual GLint getAttribLocation(GLuint program, const GLchar* name) override;
    virtual void getActiveAttrib(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name) override;
    virtual void vertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer) override;
    virtual void enableVertexAttribArray(GLuint index) override;

    // Uniforms:
    virtual GLint getUniformLocation(GLuint program, const GLchar* name) override;
    virtual void getActiveUniform(GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name) override;
    virtual void getActiveUniformBlockName(GLuint program, GLuint uniformBlockIndex, GLsizei bufSize, GLsizei* length, GLchar* uniformBlockName) override;
    virtual void uniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding) override;
    virtual void getActiveUniformBlockiv(GLuint program, GLuint uniformBlockIndex, GLenum pname, GLint* params) override;
    virtual void uniform1i(GLint location, GLint v0) override;
    virtual void uniform1fv(GLint location, GLsizei count, const GLfloat* value) override;
    virtual void uniform2fv(GLint location, GLsizei count, const GLfloat* value) override;
    virtual void uniform3fv(GLint location, GLsizei count, const GLfloat* value) override;
    virtual void uniform4fv(GLint location, GLsizei count, const GLfloat* value) override;
    virtual void uniform1iv(GLint location, GLsizei count, const GLint* value) override;
    virtual void uniform2iv(GLint location, GLsizei count, const GLint* value) override;
    virtual void uniform3iv(GLint location, GLsizei count, const GLint* value) override;
    virtual void uniform4iv(GLint location, GLsizei count, const GLint* value) override;
    virtual void uniform1uiv(GLint location, GLsizei count, const GLuint* value) override;
    virtual void uniform2uiv(GLint location, GLsizei count, const GLuint* value) override;
    virtual void uniform3uiv(GLint location, GLsizei count, const GLuint* value) override;
    virtual void uniform4uiv(GLint location, GLsizei count, const GLuint* value) override;
    virtual void uniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) override;
    virtual void uniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) override;
    virtual void uniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) override;

    // Queries:
    virtual void genQueries(GLsizei n, GLuint* ids) override;
    virtual void deleteQueries(GLsizei n, const GLuint* ids) override;
    virtual void beginQuery(GLenum target, GLuint id) override;
    virtual void endQuery(GLenum target) override;
    virtual void getQueryObjectiv(GLuint id, GLenum pname, GLint* params) override;
    virtual void getQueryObjectui64v(GLuint id, GLenum pname, GLuint64* params) override;

    // Enqueuing work:
    virtual void drawArrays(GLenum mode, GLint first, GLsizei count) override;
    virtual void drawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) override;

private:
    std::optional<std::unordered_set<std::string>> extensionsCache;
};

} // namespace ux3d::renderer
