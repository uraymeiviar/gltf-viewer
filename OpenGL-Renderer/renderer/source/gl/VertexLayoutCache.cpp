// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "renderer/gl/VertexLayoutCache.h"
#include "renderer/Mesh.h"
#include "renderer/gl/GL.h"
#include "renderer/gl/Program.h"

namespace ux3d::renderer::gl {

VertexLayoutCache::VertexLayoutCache()
{
    Mesh::addMeshDestroyCallback(this, [this](uint64_t id) { invalidateMesh(id); });
}

VertexLayoutCache::VertexLayoutCache(GL* gl)
    : m_gl(gl)
{
    Mesh::addMeshDestroyCallback(this, [this](uint64_t id) { invalidateMesh(id); });
}

VertexLayoutCache::~VertexLayoutCache()
{
    Mesh::removeMeshDestroyCallback(this);
    clear();
}

VertexLayoutCache::VertexLayoutCache(VertexLayoutCache&& other) noexcept
    : m_gl(other.m_gl)
    , m_totalLayouts(other.m_totalLayouts)
    , m_meshes(std::move(other.m_meshes))
    , m_stats(other.m_stats)
{
    other.m_gl = nullptr;
    other.m_totalLayouts = 0;
    other.m_stats = {};
    Mesh::removeMeshDestroyCallback(&other);
    Mesh::addMeshDestroyCallback(this, [this](uint64_t id) { invalidateMesh(id); });
}

VertexLayoutCache& VertexLayoutCache::operator=(VertexLayoutCache&& other) noexcept
{
    if (this != &other) {
        Mesh::removeMeshDestroyCallback(this);
        clear();

        m_gl = other.m_gl;
        m_totalLayouts = other.m_totalLayouts;
        m_meshes = std::move(other.m_meshes);
        m_stats = other.m_stats;

        other.m_gl = nullptr;
        other.m_totalLayouts = 0;
        other.m_stats = {};

        Mesh::removeMeshDestroyCallback(&other);
        Mesh::addMeshDestroyCallback(this, [this](uint64_t id) { invalidateMesh(id); });
    }
    return *this;
}

void VertexLayoutCache::init(GL* gl)
{
    m_gl = gl;
}

void VertexLayoutCache::clear()
{
    m_meshes.clear();
    m_totalLayouts = 0;
}

void VertexLayoutCache::invalidateMesh(uint64_t meshId)
{
    auto it = m_meshes.find(meshId);
    if (it != m_meshes.end()) {
        m_totalLayouts -= it->second.programs.size();
        m_stats.entriesVisited += it->second.programs.size();
        m_meshes.erase(it);
    }
}

std::shared_ptr<VertexLayout> VertexLayoutCache::getOrCreate(const Mesh* mesh, const Program* program)
{
    if (mesh == nullptr || program == nullptr) {
        return nullptr;
    }

    const uint64_t meshId = mesh->getId();
    const uint64_t meshGen = mesh->getGeneration();
    const uint64_t progId = program->getId();

    auto meshIt = m_meshes.find(meshId);
    if (meshIt == m_meshes.end()) {
        // Brand-new mesh: direct creation without scanning any existing cache entries.
        auto uniqueLayout = program->createVertexLayout(mesh->attributes, mesh->indexBuffer);
        std::shared_ptr<VertexLayout> sharedLayout = std::move(uniqueLayout);

        auto& group = m_meshes[meshId];
        group.programs[progId] = VertexLayoutEntry{ meshGen, sharedLayout };
        ++m_totalLayouts;
        return sharedLayout;
    }

    // Mesh exists. Inspect only this mesh's cached program layouts (never unrelated meshes).
    auto& group = meshIt->second;
    auto progIt = group.programs.find(progId);
    if (progIt != group.programs.end()) {
        ++m_stats.entriesVisited;
        if (progIt->second.generation == meshGen) {
            // Warm cache hit!
            return progIt->second.layout;
        }

        // Generation mismatch for this mesh: purge stale generation entries for THIS mesh only.
        for (auto it = group.programs.begin(); it != group.programs.end(); ) {
            ++m_stats.entriesVisited;
            if (it->second.generation < meshGen) {
                it = group.programs.erase(it);
                --m_totalLayouts;
            } else {
                ++it;
            }
        }
    } else {
        // Different program for an existing mesh: check for stale generation entries on this mesh.
        for (auto it = group.programs.begin(); it != group.programs.end(); ) {
            ++m_stats.entriesVisited;
            if (it->second.generation < meshGen) {
                it = group.programs.erase(it);
                --m_totalLayouts;
            } else {
                ++it;
            }
        }
    }

    auto uniqueLayout = program->createVertexLayout(mesh->attributes, mesh->indexBuffer);
    std::shared_ptr<VertexLayout> sharedLayout = std::move(uniqueLayout);
    group.programs[progId] = VertexLayoutEntry{ meshGen, sharedLayout };
    ++m_totalLayouts;
    return sharedLayout;
}

std::vector<std::shared_ptr<VertexLayout>> VertexLayoutCache::getAllLayouts() const
{
    std::vector<std::shared_ptr<VertexLayout>> layouts;
    layouts.reserve(m_totalLayouts);
    for (const auto& [meshId, group] : m_meshes) {
        for (const auto& [progId, entry] : group.programs) {
            if (entry.layout) {
                layouts.push_back(entry.layout);
            }
        }
    }
    return layouts;
}

} // namespace ux3d::renderer::gl
