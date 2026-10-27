#pragma once

#include "renderer/gltf/GltfRenderLayer.h"
#include "renderer/gl/Texture.h"
#include "renderer/gl/Buffer.h"
#include "gltf/Mesh.h"
#include <set>

namespace ux3d::imgui {
inline uint64_t imageGpuBytes(const ux3d::gltf::Image* image, const ux3d::renderer::GltfRenderLayer* layer)
{
    uint64_t bytes = 0;
    if (layer) for (const auto& [key,texture] : layer->m_textureMapping.map)
        if (std::get<0>(key) == image && texture) bytes += texture->residentBytes;
    return bytes;
}
inline uint64_t meshGpuBytes(const ux3d::gltf::Mesh& mesh, const ux3d::renderer::GltfRenderLayer* layer)
{
    if (!layer) return 0;
    std::set<const ux3d::gltf::Accessor*> accessors;
    for (const auto& primitive : mesh.getPrimitives()) {
        if (primitive.getIndicesAccessor()) accessors.insert(primitive.getIndicesAccessor());
        for (const auto& attribute : primitive.getAttributes()) if (attribute.getAccessor()) accessors.insert(attribute.getAccessor());
    }
    uint64_t bytes = 0;
    for (auto* accessor : accessors) {
        auto it = layer->m_bufferMapping.map.find(accessor);
        if (it != layer->m_bufferMapping.map.end() && it->second) bytes += it->second->residentBytes;
    }
    return bytes;
}
} // namespace ux3d::imgui
