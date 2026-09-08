#include "gltf/json/NodesFromJson.h"
#include "gltf/Gltf.h"
#include "gltf/json/GltfObjectFromJson.h"
#include "gltf/json/ParsingUtils.h"
#include "gltf/json/PointerLookup.h"

using namespace ux3d;
using namespace ux3d::gltf;

ConversionResult NodesFromJson::convert(const nlohmann::json& _jsonNode)
{
    Node* pNode = m_gltf.create<Node>(getID(_jsonNode));
    if (pNode == nullptr) {
        return ConversionError::AllocationFailure;
    }

    if (!m_pointerLookup.add(pNode)) {
        return ConversionError::AllocationFailure;
    }

    bool containsErrors = false;
    for (const auto& [key, value] : _jsonNode.items()) {
        if (key == "scale") {
            if (checkVector(value, 3, "node.scale")) {
                pNode->setScale(doublesToVector<3>(value.get<std::vector<double>>()));
            } else {
                containsErrors = true;
            }
        } else if (key == "matrix") {
            if (checkVector(value, 4 * 4, "node.matrix")) {
                pNode->decomposeTransformMatrix(doublesToMatrix<4, 4>(value.get<std::vector<double>>()));
                pNode->storeAsMatrix(true);
            } else {
                containsErrors = true;
            }
        } else if (key == "rotation") {
            if (checkVector(value, 4, "node.rotation")) {
                pNode->setRotation(doublesToQuat(value.get<std::vector<double>>()));
            } else {
                containsErrors = true;
            }
        } else if (key == "translation") {
            if (checkVector(value, 3, "node.translation")) {
                pNode->setTranslation(doublesToVector<3>(value.get<std::vector<double>>()));
            } else {
                containsErrors = true;
            }
        } else if (key == "name") {
            if (value.is_string()) {
                pNode->setName(value.get<std::string>());
            } else {
                containsErrors = true;
                core::logWarning("node.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
            }
        } else if (key == "weights") {
            if (checkNumberArray(value, "node.weights")) {
                std::vector<double> weights;
                for (const auto& jsonWeight : value) {
                    weights.emplace_back(jsonWeight.get<float>());
                }
                pNode->weights = weights;
            } else {
                containsErrors = true;
            }
        } else if (key == "extensions") {
            if (value.is_object()) {
                for (const auto& [extensionName, extension] : value.items()) {
                    if (extensionName == "KHR_lights_punctual") {
                        // handled in resolve
                    }
                }
            }
        }
    }
    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonNode, pNode, "node")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonNode, pNode);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult NodesFromJson::resolve(const nlohmann::json& _jsonNode, Node& _node)
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonNode.items()) {
        if (key == "camera") {
            if (checkIndex(value, m_pointerLookup.Cameras.size(), "node.camera")) {
                Camera* pCamera = m_pointerLookup.Cameras[value];
                _node.setCamera(*pCamera);
            } else {
                containsErrors = true;
            }
        } else if (key == "skin") {
            if (checkIndex(value, m_pointerLookup.Skins.size(), "node.skin")) {
                Skin* pSkin = m_pointerLookup.Skins[value];
                _node.setSkin(*pSkin);
            } else {
                containsErrors = true;
            }
        } else if (key == "extensions") {
            if (value.is_object()) {
                for (const auto& [extensionName, extension] : value.items()) {
                    if (extensionName == "KHR_lights_punctual") {
                        if (extension.is_object()) {
                            GltfObject& lightPointerObject = _node.createLightPointerObject();
                            for (const auto& [jsonLight, lightIndex] : extension.items()) {
                                if (jsonLight == "light") {
                                    if (checkIndex(lightIndex, m_pointerLookup.Lights.size(), "node.extensions.KHR_lights_punctual.light")) {
                                        Light* pLight = m_pointerLookup.Lights[lightIndex];
                                        _node.setLight(*pLight);
                                    } else {
                                        containsErrors = true;
                                    }
                                }
                            }
                            if (!GltfObjectFromJson::convertUnknownExtensions(extension, &lightPointerObject, "node.extensions.KHR_lights_punctual")) {
                                containsErrors = true;
                            }
                            GltfObjectFromJson::convertExtras(extension, &lightPointerObject);
                        } else {
                            core::logWarning("node.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension.dump().c_str());
                            containsErrors = true;
                        }
                    } else if (extensionName == "KHR_xmp_json_ld") {
                        if (extension.is_object()) {
                            for (const auto& [xmpKey, xmpValue] : extension.items()) {
                                if (xmpKey == "packet") {
                                    if (m_gltf.getXMP().has_value() == false) {
                                        core::logWarning("NodesFromJson: Node.XMP is defined, but the root extension KHR_xmp_json_ld is missing");
                                        containsErrors = true;
                                        continue;
                                    }

                                    XMP& xmp = m_gltf.getXMP().value();
                                    core::LinkedList<Packet>& packets = xmp.getPackets();
                                    const size_t index = xmpValue.get<size_t>();

                                    Packet* pPacket = &packets.at(index);
                                    _node.setXMPPacket(pPacket);
                                }
                            }
                            if (_node.hasXMPPacket()) {
                                if (!GltfObjectFromJson::convertUnknownExtensions(extension, &_node.getXMPPacket().value(), "node.extensions.KHR_xmp_json_ld")) {
                                    containsErrors = true;
                                }
                                GltfObjectFromJson::convertExtras(extension, &_node.getXMPPacket().value());
                            }
                        } else {
                            core::logWarning("node.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension.dump().c_str());
                            containsErrors = true;
                        }
                    } else if (extensionName == "KHR_audio") {
                        if (extension.is_object()) {
                            GltfObject& audioEmitterPointerObject = _node.createAudioEmitterPointerObject();
                            for (const auto& [key, value] : extension.items()) {
                                if (key == "emitter") {
                                    if (checkIndex(value, m_pointerLookup.AudioEmitters.size(), "node.extensions.KHR_audio.emitter")) {
                                        AudioEmitter* pEmitter = m_pointerLookup.AudioEmitters[value];
                                        _node.setAudioEmitter(*pEmitter);
                                    } else {
                                        containsErrors = true;
                                    }
                                }
                            }
                            if (!GltfObjectFromJson::convertUnknownExtensions(extension, &audioEmitterPointerObject, "node.extensions.KHR_audio")) {
                                containsErrors = true;
                            }
                            GltfObjectFromJson::convertExtras(extension, &audioEmitterPointerObject);
                        } else {
                            core::logWarning("node.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension.dump().c_str());
                            containsErrors = true;
                        }
                    }
                }
            }
        } else if (key == "mesh") {
            if (checkIndex(value, m_pointerLookup.Meshes.size(), "node.mesh")) {
                Mesh* pMesh = m_pointerLookup.Meshes[value];
                _node.setMesh(*pMesh);
            } else {
                containsErrors = true;
            }
        } else if (key == "children") {
            if (value.is_array()) {
                for (const auto& jsonChild : value) {
                    if (checkIndex(jsonChild, m_pointerLookup.Nodes.size(), "node.children[?]")) {
                        Node* pChild = m_pointerLookup.Nodes[jsonChild];
                        _node.addChild(*pChild, m_gltf);
                    } else {
                        containsErrors = true;
                    }
                }
            } else {
                core::logWarning("node.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}
