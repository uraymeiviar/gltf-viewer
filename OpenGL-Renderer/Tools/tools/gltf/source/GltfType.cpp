#include "gltf/GltfType.h"

using namespace ux3d;
using namespace ux3d::gltf;

const char* gltf::gltfTypeToString(GltfType _type)
{
    switch (_type) {
    case GltfType::Undefined:
        return "Undefined GltfType";
    case GltfType::Gltf:
        return "Gltf";
    case GltfType::Scene:
        return "Scene";
    case GltfType::Node:
        return "Node";
    case GltfType::Camera:
        return "Camera";
    case GltfType::Mesh:
        return "Mesh";
    case GltfType::Material:
        return "Material";
    case GltfType::Texture:
        return "Texture";
    case GltfType::Image:
        return "Image";
    case GltfType::Sampler:
        return "Sampler";
    case GltfType::Accessor:
        return "Accessor";
    case GltfType::BufferView:
        return "BufferView";
    case GltfType::Buffer:
        return "Buffer";
    case GltfType::Skin:
        return "Skin";
    case GltfType::Animation:
        return "Animation";
    case GltfType::Light:
        return "Light";
    case GltfType::AudioEmitter:
        return "AudioEmitter";
    case GltfType::AudioSource:
        return "AudioSource";
    default:
        return "Unknown GltfType";
    }
}
