#include "enums/GltfPropertyTypes.h"

using namespace ux3d;
using namespace ux3d::editor;

gltf::GltfType toGltfType(GltfPropertyType _type)
{
    switch (_type) {
    case GltfPropertyType::None:
        return gltf::GltfType::Undefined;
    case GltfPropertyType::Scenes:
        return gltf::GltfType::Scene;
    case GltfPropertyType::Nodes:
        return gltf::GltfType::Node;
    case GltfPropertyType::Cameras:
        return gltf::GltfType::Camera;
    case GltfPropertyType::AudioSources:
        return gltf::GltfType::AudioSource;
    case GltfPropertyType::AudioEmitters:
        return gltf::GltfType::AudioEmitter;
    case GltfPropertyType::Lights:
        return gltf::GltfType::Light;
    case GltfPropertyType::Meshes:
        return gltf::GltfType::Mesh;
    case GltfPropertyType::Primitives:
        return gltf::GltfType::Undefined;
    case GltfPropertyType::Materials:
        return gltf::GltfType::Material;
    case GltfPropertyType::Textures:
        return gltf::GltfType::Texture;
    case GltfPropertyType::Images:
        return gltf::GltfType::Image;
    case GltfPropertyType::Samplers:
        return gltf::GltfType::Sampler;
    case GltfPropertyType::Skins:
        return gltf::GltfType::Skin;
    case GltfPropertyType::Animations:
        return gltf::GltfType::Animation;
    default:
        return gltf::GltfType::Undefined;
    }
}

GltfPropertyType toGltfPropertyType(gltf::GltfType _type)
{
    switch (_type) {
    case gltf::GltfType::Undefined:
        return GltfPropertyType::None;
    case gltf::GltfType::Scene:
        return GltfPropertyType::Scenes;
    case gltf::GltfType::Node:
        return GltfPropertyType::Nodes;
    case gltf::GltfType::Camera:
        return GltfPropertyType::Cameras;
    case gltf::GltfType::AudioSource:
        return GltfPropertyType::AudioSources;
    case gltf::GltfType::AudioEmitter:
        return GltfPropertyType::AudioEmitters;
    case gltf::GltfType::Light:
        return GltfPropertyType::Lights;
    case gltf::GltfType::Mesh:
        return GltfPropertyType::Meshes;
    //case gltf::GltfType::Primitive:
    //	return GltfPropertyType::Primitives;
    case gltf::GltfType::Material:
        return GltfPropertyType::Materials;
    case gltf::GltfType::Texture:
        return GltfPropertyType::Textures;
    case gltf::GltfType::Image:
        return GltfPropertyType::Images;
    case gltf::GltfType::Sampler:
        return GltfPropertyType::Samplers;
    case gltf::GltfType::Skin:
        return GltfPropertyType::Skins;
    case gltf::GltfType::Animation:
        return GltfPropertyType::Animations;
    default:
        return GltfPropertyType::None;
    }
}
