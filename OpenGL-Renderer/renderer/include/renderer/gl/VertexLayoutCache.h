// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "OpenGL.h"
#include "renderer/gl/VertexLayout.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace ux3d::renderer {
class Mesh;
}

namespace ux3d::renderer::gl {
class GL;
class Program;

struct VertexLayoutEntry {
    uint64_t generation = 0;
    std::shared_ptr<VertexLayout> layout;
};

// Layouts cached for a single Mesh instance, indexed by programId
struct MeshLayoutGroup {
    // programId -> (generation, layout)
    std::unordered_map<uint64_t, VertexLayoutEntry> programs;
};

struct CacheWorkStats {
    uint64_t entriesVisited = 0;           // Number of entries inspected across operations
    uint64_t unrelatedEntriesScanned = 0;  // Number of entries of other meshes inspected (strictly 0)
};

class VertexLayoutCache {
public:
    VertexLayoutCache();
    explicit VertexLayoutCache(GL* gl);
    ~VertexLayoutCache();

    VertexLayoutCache(const VertexLayoutCache&) = delete;
    VertexLayoutCache& operator=(const VertexLayoutCache&) = delete;
    VertexLayoutCache(VertexLayoutCache&& other) noexcept;
    VertexLayoutCache& operator=(VertexLayoutCache&& other) noexcept;

    void init(GL* gl);
    void clear();

    std::shared_ptr<VertexLayout> getOrCreate(const Mesh* mesh, const Program* program);

    void invalidateMesh(uint64_t meshId);

    size_t size() const noexcept { return m_totalLayouts; }
    size_t meshCount() const noexcept { return m_meshes.size(); }
    bool hasMesh(uint64_t meshId) const noexcept { return m_meshes.find(meshId) != m_meshes.end(); }

    std::vector<std::shared_ptr<VertexLayout>> getAllLayouts() const;

    const CacheWorkStats& getStats() const noexcept { return m_stats; }
    void resetStats() noexcept { m_stats = {}; }

private:
    GL* m_gl = nullptr;
    size_t m_totalLayouts = 0;
    std::unordered_map<uint64_t /*meshId*/, MeshLayoutGroup> m_meshes;
    mutable CacheWorkStats m_stats;
};

} // namespace ux3d::renderer::gl
