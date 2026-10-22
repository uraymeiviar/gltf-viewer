// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "renderer/gl/Enums.h"
#include "renderer/gl/GL.h"
#include "renderer/gl/Sampler.h"

#include <optional>

namespace ux3d::renderer {
class Command;

class CommandExecutor {
public:
    CommandExecutor(gl::GL*);
    CommandExecutor(CommandExecutor&&) noexcept = default;
    CommandExecutor& operator=(CommandExecutor&&) = default;

    void init();
    void resetState();
    void unbindSamplers();
    void execute(const Command& command);

    gl::SamplerCache& getSamplerCache() { return m_samplerCache; }
    const gl::SamplerCache& getSamplerCache() const { return m_samplerCache; }

    std::optional<gl::PolygonMode> getActivePolygonMode() const { return m_activePolygonMode; }
    std::optional<bool> getCullingEnabled() const { return m_cullingEnabled; }
    std::optional<gl::Culling> getCullFace() const { return m_cullFace; }
    std::optional<gl::Winding> getActiveWinding() const { return m_activeWinding; }
    std::optional<bool> getDepthTestEnabled() const { return m_depthTestEnabled; }
    std::optional<GLenum> getDepthFunc() const { return m_depthFunc; }

private:
    void draw(const Command& command);
    void clear(const Command& command) const;
    bool bindFrameBuffer(const Command& command) const;
    bool activateProgram(const Command& command);

    void setupVertexArray(const Command& command);
    void setupBlendFunc(const Command& command) const;
    void setupCulling(const Command& command);
    void setupPolygonMode(const Command& command);
    void setupFrontFace(const Command& command);
    void setupDepthTest(const Command& command);
    void setupScissorTest(const Command& command) const;
    void readPixel(const Command& command) const;

private:
    gl::GL* m_gl;
    GLint m_maxTextureUnits = 0;
    GLuint m_activeVertexArray = gl::invalid;
    std::optional<gl::PolygonMode> m_activePolygonMode = std::nullopt;
    std::optional<bool> m_cullingEnabled = std::nullopt;
    std::optional<gl::Culling> m_cullFace = std::nullopt;
    std::optional<gl::Winding> m_activeWinding = std::nullopt;
    std::optional<bool> m_depthTestEnabled = std::nullopt;
    std::optional<GLenum> m_depthFunc = std::nullopt;
    GLint m_maxBoundTextureUnit = -1;
    gl::SamplerCache m_samplerCache;
};
}
