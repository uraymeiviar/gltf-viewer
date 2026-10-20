// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "renderer/gl/GL.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace ux3d::renderer {

struct GpuPassSample {
    std::string name;
    double milliseconds = 0.0;
    bool available = false;
};

struct GpuFrameSample {
    uint64_t sequence = 0;
    uint64_t frame = 0;
    uint32_t viewport = 0;
    std::string label;
    std::vector<GpuPassSample> passes;
};

// Collects OpenGL timer-query results without stalling the pipeline.
//
// A query is opened around each render pass. Results are checked only from a
// later frame, and only with GL_QUERY_RESULT_AVAILABLE. No glFinish and no
// same-frame GL_QUERY_RESULT read is performed. If a result is not ready it
// remains pending and is reported as unavailable at the capture boundary.
class GpuProfiler {
public:
    static constexpr size_t maxCompletedFrames = 512;

    explicit GpuProfiler(gl::GL* gl);
    ~GpuProfiler();

    GpuProfiler(const GpuProfiler&) = delete;
    GpuProfiler& operator=(const GpuProfiler&) = delete;
    GpuProfiler(GpuProfiler&& other) noexcept;
    GpuProfiler& operator=(GpuProfiler&& other) noexcept;

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool enabled() const { return m_enabled; }

    void beginFrame(uint64_t frame, uint32_t viewport, const std::string& label);
    void beginPass(const std::string& name);
    void endPass();
    void endFrame();
    void poll();

    const std::deque<GpuFrameSample>& completedFrames() const { return m_completed; }
    std::vector<GpuFrameSample> pendingFrames() const;
    uint64_t pendingFrameCount() const { return static_cast<uint64_t>(m_pending.size()); }
    uint64_t unavailablePollCount() const { return m_unavailablePolls; }
    uint64_t droppedFrameCount() const { return m_droppedFrames; }
    uint64_t completedFrameLimit() const { return static_cast<uint64_t>(maxCompletedFrames); }
    uint64_t completedFrameDroppedCount() const { return m_completedDropped; }

    void release();

private:
    struct PendingPass {
        std::string name;
        GLuint query = 0;
    };

    struct PendingFrame {
        uint64_t sequence = 0;
        uint64_t frame = 0;
        uint32_t viewport = 0;
        std::string label;
        std::vector<PendingPass> passes;
    };

    GLuint acquireQuery();
    void returnQuery(GLuint query);

    gl::GL* m_gl = nullptr;
    bool m_enabled = false;
    bool m_capturing = false;
    bool m_hasActivePass = false;
    std::string m_activePassName;
    GLuint m_activeQuery = 0;

    uint64_t m_nextSequence = 1;
    PendingFrame m_currentFrame;
    std::vector<PendingFrame> m_pending;
    std::deque<GpuFrameSample> m_completed;

    std::vector<GLuint> m_ownedQueries;
    std::vector<GLuint> m_freeQueries;

    uint64_t m_unavailablePolls = 0;
    uint64_t m_droppedFrames = 0;
    uint64_t m_completedDropped = 0;
};

} // namespace ux3d::renderer
