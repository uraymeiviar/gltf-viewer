#include "gltf/json/ScenesFromJson.h"
#include "gltf/Gltf.h"
#include "gltf/json/GltfObjectFromJson.h"
#include "gltf/json/PointerLookup.h"

using namespace ux3d;
using namespace ux3d::gltf;

ConversionResult ScenesFromJson::convert(const nlohmann::json& _jsonScene)
{
    Scene* pScene = m_gltf.create<Scene>(getID(_jsonScene));
    if (pScene == nullptr) {
        return ConversionError::AllocationFailure;
    }

    if (!m_pointerLookup.add(pScene)) {
        return ConversionError::AllocationFailure;
    }

    bool containsErrors = false;
    for (const auto& [key, value] : _jsonScene.items()) {
        if (key == "name") {
            if (value.is_string()) {
                pScene->setName(value.get<std::string>());
            } else {
                core::logWarning("scene.name with value '%s' has invalid type", value.dump().c_str());
                containsErrors = true;
            }
        }
    }
    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonScene, pScene, "scene")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonScene, pScene);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult ScenesFromJson::resolve(const nlohmann::json& _jsonScene, Scene& _scene)
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonScene.items()) {
        if (key == "nodes") {
            if (value.is_array()) {
                for (const auto& jsonNode : value) {
                    if (checkIndex(jsonNode, m_pointerLookup.Nodes.size(), "scene.nodes[?]")) {
                        Node* pNode = m_pointerLookup.Nodes[jsonNode.get<uint32_t>()];
                        _scene.addNode(*pNode, m_gltf);
                    } else {
                        containsErrors = true;
                    }
                }
            } else {
                core::logWarning("scene.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "extensions") {
            if (value.is_object()) {
                for (const auto& [extensionName, extension] : value.items()) {
                    if (extensionName == "KHR_xmp_json_ld") {
                        if (extension.is_object()) {
                            for (const auto& [xmpKey, xmpValue] : extension.items()) {
                                if (xmpKey == "packet") {
                                    if (m_gltf.getXMP().has_value() == false) {
                                        core::logWarning("ScenesFromJson: Scene.XMP is defined, but the root extension KHR_xmp_json_ld is missing");
                                        containsErrors = true;
                                        continue;
                                    }

                                    XMP& xmp = m_gltf.getXMP().value();
                                    core::LinkedList<Packet>& packets = xmp.getPackets();
                                    const size_t index = xmpValue.get<size_t>();

                                    Packet* pPacket = &packets.at(index);
                                    _scene.setXMPPacket(pPacket);
                                }
                            }
                            if (_scene.hasXMPPacket()) {
                                if (!GltfObjectFromJson::convertUnknownExtensions(extension, &_scene.getXMPPacket().value(), "scenes.extensions.KHR_xmp_json_ld")) {
                                    containsErrors = true;
                                }
                                GltfObjectFromJson::convertExtras(extension, &_scene.getXMPPacket().value());
                            }
                        } else {
                            core::logWarning("scene.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension.dump().c_str());
                            containsErrors = true;
                        }

                    } else if (extensionName == "KHR_audio") {
                        if (extension.is_object()) {
                            GltfObject& audioEmitterPointerObject = _scene.createAudioEmitterPointerObject();

                            for (const auto& [keyAudio, valueAudio] : extension.items()) {
                                if (keyAudio == "emitters") {
                                    if (valueAudio.is_array() == false) {
                                        containsErrors = true;
                                        continue;
                                    }

                                    for (const auto& [emitterKey, emitterValue] : valueAudio.items()) {
                                        if (checkIndex(emitterValue, m_pointerLookup.AudioEmitters.size(), "scenes.extensions.KHR_audio.emitter")) {
                                            AudioEmitter* pEmitter = m_pointerLookup.AudioEmitters[emitterValue];
                                            _scene.addAudioEmitter(pEmitter);
                                        } else {
                                            containsErrors = true;
                                        }
                                    }
                                }
                            }

                            if (!GltfObjectFromJson::convertUnknownExtensions(extension, &audioEmitterPointerObject, "scenes.extensions.KHR_audio")) {
                                containsErrors = true;
                            }
                            GltfObjectFromJson::convertExtras(extension, &audioEmitterPointerObject);
                        } else {
                            core::logWarning("scene.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension.dump().c_str());
                            containsErrors = true;
                        }
                    } // end khr audio
                }
            }
        }
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}
