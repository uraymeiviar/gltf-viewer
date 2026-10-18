// Copyright © 2019 UX3D GmbH. All rights reserved.

#include "helpers/Name.h"
#include "Constants.h"
#include "filesystem/Path.h"
#include "gltf/ChildOfRoot.h"
#include "gltf/EmbeddedData.h"
#include "gltf/Gltf.h"
#include "gltf/Image.h"
#include "gltf/Scene.h"
#include "gltf/Texture.h"
#include "helpers/QtFileSystem.h"
#include "model/ProjectManager.h"
#include <utility>

using namespace ux3d;
using namespace ux3d::editor;

Name::Name(const gltf::ChildOfRoot& _object)
{
    fillInfo(_object);
}

Name::Name(GltfPropertyConstPointer _object)
{
    if (auto* scene = _object.get<const gltf::Scene*>();
        scene != nullptr) {
        fillInfo(*scene);
    }
    if (auto* node = _object.get<const gltf::Node*>();
        node != nullptr) {
        fillInfo(*node);
    }
    if (auto* camera = _object.get<const gltf::Camera*>();
        camera != nullptr) {
        fillInfo(*camera);
    }
    if (auto* light = _object.get<const gltf::Light*>();
        light != nullptr) {
        fillInfo(*light);
    }
    if (auto* mesh = _object.get<const gltf::Mesh*>();
        mesh != nullptr) {
        fillInfo(*mesh);
    }
    if (auto* material = _object.get<const gltf::Material*>();
        material != nullptr) {
        fillInfo(*material);
    }
    if (auto* texture = _object.get<const gltf::Texture*>();
        texture != nullptr) {
        fillInfo(*texture);
    }
    if (auto* image = _object.get<const gltf::Image*>();
        image != nullptr) {
        fillInfo(*image);
    }
    if (auto* sampler = _object.get<const gltf::Sampler*>();
        sampler != nullptr) {
        fillInfo(*sampler);
    }
    if (auto* skin = _object.get<const gltf::Skin*>();
        skin != nullptr) {
        fillInfo(*skin);
    }
    if (auto* animation = _object.get<const gltf::Animation*>();
        animation != nullptr) {
        fillInfo(*animation);
    }
    if (auto* accessor = _object.get<const gltf::Accessor*>();
        accessor != nullptr) {
        fillInfo(*accessor);
    }
    if (auto* primitive = _object.get<const gltf::Primitive*>();
        primitive != nullptr) {
        fillInfo(*primitive);
    }
    if (auto* materialVariant = _object.get<const gltf::MaterialVariant*>();
        materialVariant != nullptr) {
        fillInfo(*materialVariant);
    }
    if (auto* packet = _object.get<const gltf::Packet*>();
        packet != nullptr) {
        fillInfo(*packet);
    }
    if (auto* audioSource = _object.get<const gltf::AudioSource*>();
        audioSource != nullptr) {
        fillInfo(*audioSource);
    }
    if (auto* audioEmitter = _object.get<const gltf::AudioEmitter*>();
        audioEmitter != nullptr) {
        fillInfo(*audioEmitter);
    }
}

Name::Name(GltfPropertyPointer _object)
    : Name(toConst(std::move(_object)))
{
}

void Name::fillInfo(const gltf::ChildOfRoot& _object)
{
    m_realName = _object.getName();
    m_undecoratedDisplayName = getDisplayName(_object);
    m_displayName = m_undecoratedDisplayName;

    postProcess(_object);
    postProcessInspectorDisplay();
}

void Name::fillInfo(const gltf::Primitive& _primitive)
{
    m_realName = "";
    m_undecoratedDisplayName = "Primitive";
    m_displayName = m_undecoratedDisplayName;

    postProcessInspectorDisplay();
}

void Name::fillInfo(const gltf::Packet& _packet)
{
    std::string name = "";
    gltf::Gltf& gltf = model::ProjectManager::getInstance()->getGltf();
    if (gltf.hasXMP()) {
        const core::LinkedList<gltf::Packet>& packets = gltf.getXMP().value().getPackets();
        for (size_t i = 0; i < packets.size(); ++i) {
            if (&packets.at(i) == &_packet) {
                name = std::string("Packet ") + std::to_string(i);
                break;
            }
        }
    }
    m_realName = "";
    m_undecoratedDisplayName = name;
    m_displayName = m_undecoratedDisplayName;
    postProcessInspectorDisplay();
}

std::string Name::getDisplayName(const gltf::ChildOfRoot& _object)
{
    if (_object.hasName()) {
        return _object.getName();
    }

    std::string result = getDisplayNamePrefix(_object);
    result += getID(_object);

    return result;
}

void Name::postProcess(const gltf::ChildOfRoot& _object)
{
    if (_object.getGltfType() == gltf::GltfType::Scene) {
        scenePostProcess(*static_cast<const gltf::Scene*>(&_object));
    } else if (_object.getGltfType() == gltf::GltfType::Image) {
        imagePostProcess(*static_cast<const gltf::Image*>(&_object));
    } else if (_object.getGltfType() == gltf::GltfType::Texture) {
        texturePostProcess(*static_cast<const gltf::Texture*>(&_object));
    } else if (_object.getGltfType() == gltf::GltfType::AudioSource) {
        audioSourcePostProcess(*static_cast<const gltf::AudioSource*>(&_object));
    }
}

void Name::scenePostProcess(const gltf::Scene& _scene)
{
    if (model::ProjectManager::getInstance()->getGltf().getDefaultScene() == &_scene) {
        m_displayName = "* " + m_displayName;
    }
}

void Name::imagePostProcess(const gltf::Image& _image)
{
    if (!m_realName.empty()) {
        return;
    }

    if (std::string fromUri = nameFromUri(_image.getUri());
        !fromUri.empty()) {
        m_displayName = fromUri;
        m_undecoratedDisplayName = fromUri;
    }
}

void Name::audioSourcePostProcess(const gltf::AudioSource& source)
{
    if (!m_realName.empty()) {
        return;
    }

    if (std::string fromUri = nameFromUri(source.getUri());
        !fromUri.empty()) {
        m_displayName = fromUri;
        m_undecoratedDisplayName = fromUri;
    }
}

void Name::texturePostProcess(const gltf::Texture& _texture)
{
    if (!m_realName.empty()) {
        return;
    }

    gltf::Image* pImage;
    if (_texture.getBasisU().has_value() && _texture.getBasisU()->getSource() != nullptr) {
        pImage = _texture.getBasisU()->getSource();
    } else if (_texture.getWebP().has_value() && _texture.getWebP()->getSource() != nullptr) {
        pImage = _texture.getWebP()->getSource();
    } else {
        pImage = _texture.getSource();
    }
    if (pImage == nullptr) {
        return;
    }

    if (std::string imageName = pImage->getName();
        !imageName.empty()) {
        m_displayName = imageName;
        m_undecoratedDisplayName = imageName;
        return;
    }
    if (std::string fromUri = nameFromUri(pImage->getUri());
        !fromUri.empty()) {
        m_displayName = fromUri;
        m_undecoratedDisplayName = fromUri;
        return;
    }
}

std::string Name::nameFromUri(const std::string& _uri)
{
    if (_uri.empty() || gltf::embedded_data::isDataUri(_uri)) {
        return "";
    }

    return QtFileSystem::instance().toFile(_uri).getStem();
}

std::string Name::getID(const gltf::ChildOfRoot& _object)
{
    return std::to_string(_object.m_ID);
}

void Name::postProcessInspectorDisplay()
{
    m_displayNameInspector = "'" + m_displayName + "'";
}

std::string Name::getDisplayNamePrefix(const gltf::ChildOfRoot& _object)
{
    switch (_object.getGltfType()) {
    case gltf::GltfType::Scene:
        return std::string(constants::unnamedSceneName) + " ";
    case gltf::GltfType::Node:
        return std::string(constants::unnamedNodeName) + " ";
    case gltf::GltfType::Camera:
        return std::string(constants::unnamedCameraName) + " ";
    case gltf::GltfType::Mesh:
        return std::string(constants::unnamedMeshName) + " ";
    case gltf::GltfType::Material:
        return std::string(constants::unnamedMaterialName) + " ";
    case gltf::GltfType::Texture:
        return std::string(constants::unnamedTextureName) + " ";
    case gltf::GltfType::Image:
        return std::string(constants::unnamedImageName) + " ";
    case gltf::GltfType::Sampler:
        return std::string(constants::unnamedSamplerName) + " ";
    case gltf::GltfType::Accessor:
        return std::string(constants::unnamedAccessorName) + " ";
    case gltf::GltfType::BufferView:
        return std::string(constants::unnamedBufferViewName) + " ";
    case gltf::GltfType::Buffer:
        return std::string(constants::unnamedBufferName) + " ";
    case gltf::GltfType::Skin:
        return std::string(constants::unnamedSkinName) + " ";
    case gltf::GltfType::Animation:
        return std::string(constants::unnamedAnimationName) + " ";
    case gltf::GltfType::Light:
        return std::string(constants::unnamedLightName) + " ";
    case gltf::GltfType::AudioSource:
        return std::string(constants::unnamedAudioSource) + " ";
    case gltf::GltfType::AudioEmitter:
        return std::string(constants::unnamedAudioEmitter) + " ";
    default:
        break;
    }

    return "";
}
