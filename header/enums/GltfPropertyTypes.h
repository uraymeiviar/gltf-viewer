#pragma once

#include <cstdint>

#include "gltf/GltfType.h"

namespace ux3d::editor {
enum class GltfPropertyType : uint32_t {
    // use None for sending deselection events
    None,
    Scenes,
    Nodes,
    Cameras,
    AudioSources,
    AudioEmitters,
    Lights,
    Meshes,
    Primitives,
    Materials,
    Textures,
    Images,
    Samplers,
    Skins,
    Animations,
    Accessors,
    MaterialVariants,
    XmpPackets
};

gltf::GltfType toGltfType(GltfPropertyType _type);
GltfPropertyType toGltfPropertyType(gltf::GltfType _type);
}
