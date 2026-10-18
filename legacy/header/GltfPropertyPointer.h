#pragma once

#include "core/VariantWrapper.h"
#include "enums/GltfPropertyTypes.h"

#include <nlohmann/json.hpp>

namespace ux3d::gltf {
class Gltf;
class Scene;
class Node;
class Camera;
class Light;
class Mesh;
class Primitive;
class Material;
class Texture;
class Image;
class Sampler;
class Skin;
class Animation;
class Accessor;
class MaterialVariant;
class Packet;
class AudioSource;
class AudioEmitter;

class ChildOfRoot;
}

namespace ux3d::editor {
using GltfPropertyPointer = core::VariantWrapper<std::monostate,
    gltf::Scene*, gltf::Node*, gltf::Camera*, gltf::Light*, gltf::Gltf*,
    gltf::Mesh*, gltf::Primitive*, gltf::Material*, gltf::Texture*,
    gltf::Image*, gltf::Sampler*, gltf::Skin*, gltf::Animation*, gltf::Accessor*, gltf::MaterialVariant*,
    gltf::Packet*, gltf::AudioSource*, gltf::AudioEmitter*>;

using GltfPropertyConstPointer = core::VariantWrapper<std::monostate,
    const gltf::Scene*, const gltf::Node*, const gltf::Camera*, const gltf::Light*, const gltf::Gltf*,
    const gltf::Mesh*, const gltf::Primitive*, const gltf::Material*, const gltf::Texture*,
    const gltf::Image*, const gltf::Sampler*, const gltf::Skin*, const gltf::Animation*,
    const gltf::Accessor*, const gltf::MaterialVariant*, const gltf::Packet*, const gltf::AudioSource*, const gltf::AudioEmitter*>;

GltfPropertyConstPointer toConst(GltfPropertyPointer _pointer);

gltf::ChildOfRoot* toChildOfRoot(GltfPropertyPointer _pointer, GltfPropertyType _type);
const gltf::ChildOfRoot* toChildOfRoot(GltfPropertyConstPointer _pointer, GltfPropertyType _type);

std::tuple<GltfPropertyPointer, GltfPropertyType> jsonPointerToGltfPointer(nlohmann::json::json_pointer path, gltf::Gltf& _gltf);
/**
 * @brief Converts gltfPointer to json pointer.
 * DOES NOT CHECK IF POINTER IS OF TYPE _TYPE!!!!
 */
nlohmann::json::json_pointer gltfPointerToJsonPointer(GltfPropertyConstPointer _gltfPointer, GltfPropertyType _type);

}
