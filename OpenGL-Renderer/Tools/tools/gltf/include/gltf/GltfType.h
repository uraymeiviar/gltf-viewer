#pragma once

namespace ux3d::gltf {
enum class GltfType {
    Undefined,

    Gltf,
    Scene,
    Node,
    Camera,
    Mesh,
    Material,
    Texture,
    Image,
    Sampler,
    Accessor,
    BufferView,
    Buffer,
    Skin,
    Animation,

    // Extensions
    Light,
    MaterialVariant,
    AudioSource,
    AudioEmitter
};

const char* gltfTypeToString(GltfType _type);
}
