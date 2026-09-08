// Copyright © 2019 UX3D GmbH. All rights reserved.

#include "GltfPropertyPointer.h"

#include "gltf/Accessor.h"
#include "gltf/Animation.h"
#include "gltf/AudioEmitter.h"
#include "gltf/AudioSource.h"
#include "gltf/Camera.h"
#include "gltf/ChildOfRoot.h"
#include "gltf/Gltf.h"
#include "gltf/Image.h"
#include "gltf/Light.h"
#include "gltf/Material.h"
#include "gltf/MaterialVariant.h"
#include "gltf/Mesh.h"
#include "gltf/Node.h"
#include "gltf/Sampler.h"
#include "gltf/Scene.h"
#include "gltf/Skin.h"
#include "gltf/Texture.h"

#include "model/ProjectManager.h"

#include <algorithm>

using namespace ux3d;
using namespace ux3d::editor;

GltfPropertyConstPointer ux3d::editor::toConst(GltfPropertyPointer _pointer)
{
    if (_pointer.hasType<gltf::Scene*>()) {
        return _pointer.unsafeGet<gltf::Scene*>();
    }
    if (_pointer.hasType<gltf::Node*>()) {
        return _pointer.unsafeGet<gltf::Node*>();
    }
    if (_pointer.hasType<gltf::Camera*>()) {
        return _pointer.unsafeGet<gltf::Camera*>();
    }
    if (_pointer.hasType<gltf::Light*>()) {
        return _pointer.unsafeGet<gltf::Light*>();
    }
    if (_pointer.hasType<gltf::Mesh*>()) {
        return _pointer.unsafeGet<gltf::Mesh*>();
    }
    if (_pointer.hasType<gltf::Primitive*>()) {
        return _pointer.unsafeGet<gltf::Primitive*>();
    }
    if (_pointer.hasType<gltf::Material*>()) {
        return _pointer.unsafeGet<gltf::Material*>();
    }
    if (_pointer.hasType<gltf::Texture*>()) {
        return _pointer.unsafeGet<gltf::Texture*>();
    }
    if (_pointer.hasType<gltf::Image*>()) {
        return _pointer.unsafeGet<gltf::Image*>();
    }
    if (_pointer.hasType<gltf::Sampler*>()) {
        return _pointer.unsafeGet<gltf::Sampler*>();
    }
    if (_pointer.hasType<gltf::Skin*>()) {
        return _pointer.unsafeGet<gltf::Skin*>();
    }
    if (_pointer.hasType<gltf::Animation*>()) {
        return _pointer.unsafeGet<gltf::Animation*>();
    }
    if (_pointer.hasType<gltf::Accessor*>()) {
        return _pointer.unsafeGet<gltf::Accessor*>();
    }
    if (_pointer.hasType<gltf::Accessor*>()) {
        return _pointer.unsafeGet<gltf::Accessor*>();
    }
    if (_pointer.hasType<gltf::MaterialVariant*>()) {
        return _pointer.unsafeGet<gltf::MaterialVariant*>();
    }
    if (_pointer.hasType<gltf::Packet*>()) {
        return _pointer.unsafeGet<gltf::Packet*>();
    }
    if (_pointer.hasType<gltf::Gltf*>()) {
        return _pointer.unsafeGet<gltf::Gltf*>();
    }
    if (_pointer.hasType<gltf::AudioSource*>()) {
        return _pointer.unsafeGet<gltf::AudioSource*>();
    }
    if (_pointer.hasType<gltf::AudioEmitter*>()) {
        return _pointer.unsafeGet<gltf::AudioEmitter*>();
    }
    return std::monostate();
}

gltf::ChildOfRoot* editor::toChildOfRoot(GltfPropertyPointer _pointer, GltfPropertyType _type)
{
    return const_cast<gltf::ChildOfRoot*>(toChildOfRoot(toConst(_pointer), _type));
}

const gltf::ChildOfRoot* editor::toChildOfRoot(GltfPropertyConstPointer _pointer, GltfPropertyType _type)
{

    if (_type == GltfPropertyType::Scenes) {
        return static_cast<const gltf::ChildOfRoot*>(_pointer.unsafeGet<const gltf::Scene*>());
    }
    if (_type == GltfPropertyType::Nodes) {
        return static_cast<const gltf::ChildOfRoot*>(_pointer.unsafeGet<const gltf::Node*>());
    }
    if (_type == GltfPropertyType::Cameras) {
        return static_cast<const gltf::ChildOfRoot*>(_pointer.unsafeGet<const gltf::Camera*>());
    }
    if (_type == GltfPropertyType::Lights) {
        return static_cast<const gltf::ChildOfRoot*>(_pointer.unsafeGet<const gltf::Light*>());
    }
    if (_type == GltfPropertyType::Meshes) {
        return static_cast<const gltf::ChildOfRoot*>(_pointer.unsafeGet<const gltf::Mesh*>());
    }
    if (_type == GltfPropertyType::Primitives) {
        return nullptr;
    }
    if (_type == GltfPropertyType::Materials) {
        return static_cast<const gltf::ChildOfRoot*>(_pointer.unsafeGet<const gltf::Material*>());
    }
    if (_type == GltfPropertyType::Textures) {
        return static_cast<const gltf::ChildOfRoot*>(_pointer.unsafeGet<const gltf::Texture*>());
    }
    if (_type == GltfPropertyType::Images) {
        return static_cast<const gltf::ChildOfRoot*>(_pointer.unsafeGet<const gltf::Image*>());
    }
    if (_type == GltfPropertyType::Samplers) {
        return static_cast<const gltf::ChildOfRoot*>(_pointer.unsafeGet<const gltf::Sampler*>());
    }
    if (_type == GltfPropertyType::Skins) {
        return static_cast<const gltf::ChildOfRoot*>(_pointer.unsafeGet<const gltf::Skin*>());
    }
    if (_type == GltfPropertyType::Animations) {
        return static_cast<const gltf::ChildOfRoot*>(_pointer.unsafeGet<const gltf::Animation*>());
    }
    if (_type == GltfPropertyType::Accessors) {
        return static_cast<const gltf::ChildOfRoot*>(_pointer.unsafeGet<const gltf::Accessor*>());
    }
    if (_type == GltfPropertyType::MaterialVariants) {
        return static_cast<const gltf::ChildOfRoot*>(_pointer.unsafeGet<const gltf::MaterialVariant*>());
    }
    if (_type == GltfPropertyType::AudioSources) {
        return static_cast<const gltf::ChildOfRoot*>(_pointer.unsafeGet<const gltf::AudioSource*>());
    }
    if (_type == GltfPropertyType::AudioEmitters) {
        return static_cast<const gltf::ChildOfRoot*>(_pointer.unsafeGet<const gltf::AudioEmitter*>());
    }

    return nullptr;
}

std::tuple<GltfPropertyPointer, GltfPropertyType> editor::jsonPointerToGltfPointer(nlohmann::json::json_pointer path, gltf::Gltf& _gltf)
{
    if (path.empty()) {
        return std::make_tuple<GltfPropertyPointer, GltfPropertyType>(&_gltf, GltfPropertyType::None);
    }

    const int id = std::stoi(path.back());
    const std::string parent = path.parent_pointer().back();

    GltfPropertyPointer gltfPointer = std::monostate();
    GltfPropertyType type = GltfPropertyType::None;
    if (parent == "nodes") {
        if (_gltf.getNodes().size() > id) {
            gltfPointer = &_gltf.getNodes().at(id);
            type = GltfPropertyType::Nodes;
        }
    } else if (parent == "accessors") {
        if (_gltf.getAccessors().size() > id) {
            gltfPointer = &_gltf.getAccessors().at(id);
            type = GltfPropertyType::Accessors;
        }
    } else if (parent == "animations") {
        if (_gltf.getAnimations().size() > id) {
            gltfPointer = &_gltf.getAnimations().at(id);
            type = GltfPropertyType::Animations;
        }
    } else if (parent == "cameras") {
        if (_gltf.getCameras().size() > id) {
            gltfPointer = &_gltf.getCameras().at(id);
            type = GltfPropertyType::Cameras;
        }
    } else if (parent == "images") {
        if (_gltf.getImages().size() > id) {
            gltfPointer = &_gltf.getImages().at(id);
            type = GltfPropertyType::Images;
        }

        // also check parents parent for "KHR_lights_punctual" to ensure we have the correct lights extension
    } else if (parent == "lights" && path.parent_pointer().parent_pointer().back() == "KHR_lights_punctual") {
        if (_gltf.getLights().size() > id) {
            gltfPointer = &_gltf.getLights().at(id);
            type = GltfPropertyType::Lights;
        }
    } else if (parent == "materials") {
        if (_gltf.getMaterials().size() > id) {
            gltfPointer = &_gltf.getMaterials().at(id);
            type = GltfPropertyType::Materials;
        }
    } else if (parent == "materialvariants") {
        if (_gltf.getMaterialVariants().size() > id) {
            gltfPointer = &_gltf.getMaterialVariants().at(id);
            type = GltfPropertyType::MaterialVariants;
        }
    } else if (parent == "meshes") {
        if (_gltf.getMeshes().size() > id) {
            gltfPointer = &_gltf.getMeshes().at(id);
            type = GltfPropertyType::Meshes;
        }
    } else if (parent == "samplers") {
        if (_gltf.getSamplers().size() > id) {
            gltfPointer = &_gltf.getSamplers().at(id);
            type = GltfPropertyType::Samplers;
        }
    } else if (parent == "scenes") {
        if (_gltf.getScenes().size() > id) {
            gltfPointer = &_gltf.getScenes().at(id);
            type = GltfPropertyType::Scenes;
        }
    } else if (parent == "skins") {
        if (_gltf.getSkins().size() > id) {
            gltfPointer = &_gltf.getSkins().at(id);
            type = GltfPropertyType::Skins;
        }
    } else if (parent == "textures") {
        if (_gltf.getTextures().size() > id) {
            gltfPointer = &_gltf.getTextures().at(id);
            type = GltfPropertyType::Textures;
        }
    } else if (parent == "primitives") {
        // primitive pointer example: /meshes/2/primitives/1

        const int meshId = std::stoi(path.parent_pointer().parent_pointer().back());

        gltf::Mesh* pParentMesh = nullptr;
        if (_gltf.getMeshes().size() > meshId) {
            pParentMesh = &_gltf.getMeshes().at(meshId);
        }
        if (pParentMesh != nullptr) {
            auto& primitives = pParentMesh->getPrimitives();
            if (id < primitives.size()) {
                gltfPointer = &primitives.at(id);
                type = GltfPropertyType::Primitives;
            }
        }
    } else if (parent == "packets" && path.parent_pointer().parent_pointer().back() == "KHR_xmp_json_ld") {
        std::optional<gltf::XMP>& xmp = model::ProjectManager::getInstance()->getGltf().getXMP();
        if (xmp.has_value()) {
            gltfPointer = &xmp.value().getPackets().at(id);
            type = GltfPropertyType::XmpPackets;
        }
    }

    return std::tuple<GltfPropertyPointer, GltfPropertyType>(gltfPointer, type);
}

nlohmann::json::json_pointer editor::gltfPointerToJsonPointer(GltfPropertyConstPointer _gltfPointer, GltfPropertyType _type)
{
    std::string path = "";
    switch (_type) {
    case GltfPropertyType::Accessors:
        path = "/accessors/" + std::to_string(_gltfPointer.get<const gltf::Accessor*>()->m_ID);
        break;
    case GltfPropertyType::Animations:
        path = "/animations/" + std::to_string(_gltfPointer.get<const gltf::Animation*>()->m_ID);
        break;
    case GltfPropertyType::Cameras:
        path = "/cameras/" + std::to_string(_gltfPointer.get<const gltf::Camera*>()->m_ID);
        break;
    case GltfPropertyType::Images:
        path = "/images/" + std::to_string(_gltfPointer.get<const gltf::Image*>()->m_ID);
        break;
    case GltfPropertyType::Lights:
        path = "/extensions/KHR_lights_punctual/lights/" + std::to_string(_gltfPointer.get<const gltf::Light*>()->m_ID);
        break;
    case GltfPropertyType::MaterialVariants:
        path = "/extensions/KHR_materials_variants/variants/" + std::to_string(_gltfPointer.get<const gltf::MaterialVariant*>()->m_ID);
        break;
    case GltfPropertyType::Materials:
        path = "/materials/" + std::to_string(_gltfPointer.get<const gltf::Material*>()->m_ID);
        break;
    case GltfPropertyType::Meshes:
        path = "/meshes/" + std::to_string(_gltfPointer.get<const gltf::Mesh*>()->m_ID);
        break;
    case GltfPropertyType::Nodes:
        path = "/nodes/" + std::to_string(_gltfPointer.get<const gltf::Node*>()->m_ID);
        break;
    case GltfPropertyType::Primitives: {
        const gltf::Primitive* prim = _gltfPointer.get<const gltf::Primitive*>();
        const gltf::Mesh* parentMesh = prim->parentMesh;
        if (parentMesh != nullptr) {
            int primitiveIndex = prim->parentMesh->getPrimitiveIndex(*prim);
            path = "/meshes/" + std::to_string(parentMesh->m_ID) + "/primitives/" + std::to_string(primitiveIndex);
        }
    } break;
    case GltfPropertyType::Samplers:
        path = "/samplers/" + std::to_string(_gltfPointer.get<const gltf::Sampler*>()->m_ID);
        break;
    case GltfPropertyType::Scenes:
        path = "/scenes/" + std::to_string(_gltfPointer.get<const gltf::Scene*>()->m_ID);
        break;
    case GltfPropertyType::Skins:
        path = "/skins/" + std::to_string(_gltfPointer.get<const gltf::Skin*>()->m_ID);
        break;
    case GltfPropertyType::Textures:
        path = "/textures/" + std::to_string(_gltfPointer.get<const gltf::Texture*>()->m_ID);
        break;
    case GltfPropertyType::XmpPackets: {
        const gltf::Packet* xmpPacket = _gltfPointer.get<const gltf::Packet*>();
        std::optional<gltf::XMP>& xmp = model::ProjectManager::getInstance()->getGltf().getXMP();
        if (xmp.has_value()) {
            uint32_t packetIndexCounter = 0;
            const auto& packets = xmp.value().getPackets();
            for (auto it = packets.begin(); it != packets.end(); ++it) {
                if (&*it == xmpPacket) {
                    break;
                }
                packetIndexCounter++;
            }
            // use -1 if not found
            int packetIndex = packetIndexCounter != packets.size() ? packetIndexCounter : -1;
            path = "/extensions/KHR_xmp_json_ld/packets/" + std::to_string(packetIndex);
        }
    } break;
    default:
        break;
    }

    return nlohmann::json::json_pointer(path);
}
