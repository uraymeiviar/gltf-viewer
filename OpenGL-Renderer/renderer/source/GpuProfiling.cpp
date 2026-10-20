// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "renderer/GpuProfiling.h"

#include <cstddef>
#include <utility>

namespace ux3d::renderer {

namespace {
constexpr uint64_t kCollectionDelayFrames = 2;
constexpr size_t kMaxPendingFrames = 8;
} // namespace

GpuProfiler::GpuProfiler(gl::GL* gl)
    : m_gl(gl)
{
}

GpuProfiler::~GpuProfiler()
{
    release();
}

GpuProfiler::GpuProfiler(GpuProfiler&& other) noexcept
{
    *this = std::move(other);
}

GpuProfiler& GpuProfiler::operator=(GpuProfiler&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    release();

    m_gl = other.m_gl;
    m_enabled = other.m_enabled;
    m_capturing = other.m_capturing;
    m_hasActivePass = other.m_hasActivePass;
    m_activePassName = std::move(other.m_activePassName);
    m_activeQuery = other.m_activeQuery;
    m_nextSequence = other.m_nextSequence;
    m_currentFrame = std::move(other.m_currentFrame);
    m_pending = std::move(other.m_pending);
    m_completed = std::move(other.m_completed);
    m_ownedQueries = std::move(other.m_ownedQueries);
    m_freeQueries = std::move(other.m_freeQueries);
    m_unavailablePolls = other.m_unavailablePolls;
    m_droppedFrames = other.m_droppedFrames;
    m_completedDropped = other.m_completedDropped;

    other.m_gl = nullptr;
    other.m_capturing = false;
    other.m_hasActivePass = false;
    other.m_activeQuery = 0;
    other.m_ownedQueries.clear();
    other.m_freeQueries.clear();
    other.m_completedDropped = 0;

    return *this;
}

void GpuProfiler::beginFrame(uint64_t frame, uint32_t viewport, const std::string& label)
{
    if (!m_enabled || m_gl == nullptr) {
        return;
    }

    poll();

    m_capturing = false;
    m_currentFrame = PendingFrame();
    m_currentFrame.sequence = m_nextSequence++;
    m_currentFrame.frame = frame;
    m_currentFrame.viewport = viewport;
    m_currentFrame.label = label;

    if (m_pending.size() >= kMaxPendingFrames) {
        ++m_droppedFrames;
        return;
    }

    m_capturing = true;
}

void GpuProfiler::beginPass(const std::string& name)
{
    if (!m_enabled || !m_capturing || m_hasActivePass || m_gl == nullptr) {
        return;
    }

    const GLuint query = acquireQuery();
    if (query == 0) {
        return;
    }

    m_activePassName = name.empty() ? "unnamed" : name;
    m_activeQuery = query;
    m_hasActivePass = true;
    m_gl->beginQuery(GL_TIME_ELAPSED, m_activeQuery);
}

void GpuProfiler::endPass()
{
    if (!m_hasActivePass || m_gl == nullptr) {
        return;
    }

    m_gl->endQuery(GL_TIME_ELAPSED);
    m_currentFrame.passes.push_back(PendingPass{ m_activePassName, m_activeQuery });
    m_activePassName.clear();
    m_activeQuery = 0;
    m_hasActivePass = false;
}

void GpuProfiler::endFrame()
{
    if (!m_capturing) {
        return;
    }

    endPass();
    if (!m_currentFrame.passes.empty()) {
        m_pending.push_back(std::move(m_currentFrame));
    }
    m_currentFrame = PendingFrame();
    m_capturing = false;
}

void GpuProfiler::poll()
{
    if (m_gl == nullptr || m_pending.empty()) {
        return;
    }

    const uint64_t latestSubmitted = m_nextSequence > 0 ? m_nextSequence - 1 : 0;
    size_t index = 0;
    while (index < m_pending.size()) {
        PendingFrame& pending = m_pending[index];

        // Do not inspect a query in the frame that submitted it. Waiting for
        // two later submissions also keeps the first read away from the
        // driver's most recent submissions.
        if (pending.sequence + kCollectionDelayFrames > latestSubmitted) {
            ++index;
            continue;
        }

        bool allAvailable = true;
        for (const PendingPass& pass : pending.passes) {
            GLint available = 0;
            m_gl->getQueryObjectiv(pass.query, GL_QUERY_RESULT_AVAILABLE, &available);
            if (available == 0) {
                allAvailable = false;
                ++m_unavailablePolls;
                break;
            }
        }

        if (!allAvailable) {
            ++index;
            continue;
        }

        GpuFrameSample sample;
        sample.sequence = pending.sequence;
        sample.frame = pending.frame;
        sample.viewport = pending.viewport;
        sample.label = pending.label;
        sample.passes.reserve(pending.passes.size());
        for (const PendingPass& pass : pending.passes) {
            GLuint64 nanoseconds = 0;
            m_gl->getQueryObjectui64v(pass.query, GL_QUERY_RESULT, &nanoseconds);
            sample.passes.push_back(GpuPassSample{
                pass.name,
                static_cast<double>(nanoseconds) / 1'000'000.0,
                true
            });
            returnQuery(pass.query);
        }
        m_completed.push_back(std::move(sample));
        if (m_completed.size() > maxCompletedFrames) {
            m_completed.pop_front();
            ++m_completedDropped;
        }
        m_pending.erase(m_pending.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

std::vector<GpuFrameSample> GpuProfiler::pendingFrames() const
{
    std::vector<GpuFrameSample> result;
    result.reserve(m_pending.size());
    for (const PendingFrame& pending : m_pending) {
        GpuFrameSample sample;
        sample.sequence = pending.sequence;
        sample.frame = pending.frame;
        sample.viewport = pending.viewport;
        sample.label = pending.label;
        sample.passes.reserve(pending.passes.size());
        for (const PendingPass& pass : pending.passes) {
            sample.passes.push_back(GpuPassSample{ pass.name, 0.0, false });
        }
        result.push_back(std::move(sample));
    }
    return result;
}

GLuint GpuProfiler::acquireQuery()
{
    if (!m_freeQueries.empty()) {
        const GLuint query = m_freeQueries.back();
        m_freeQueries.pop_back();
        return query;
    }

    GLuint query = 0;
    m_gl->genQueries(1, &query);
    if (query != 0) {
        m_ownedQueries.push_back(query);
    }
    return query;
}

void GpuProfiler::returnQuery(GLuint query)
{
    if (query != 0) {
        m_freeQueries.push_back(query);
    }
}

void GpuProfiler::release()
{
    if (m_gl == nullptr) {
        return;
    }

    if (m_hasActivePass) {
        m_gl->endQuery(GL_TIME_ELAPSED);
        m_hasActivePass = false;
    }

    if (!m_ownedQueries.empty()) {
        m_gl->deleteQueries(static_cast<GLsizei>(m_ownedQueries.size()), m_ownedQueries.data());
    }

    m_ownedQueries.clear();
    m_freeQueries.clear();
    m_pending.clear();
    m_completed.clear();
    m_currentFrame = PendingFrame();
    m_activeQuery = 0;
    m_activePassName.clear();
    m_capturing = false;
    m_completedDropped = 0;
}

} // namespace ux3d::renderer
