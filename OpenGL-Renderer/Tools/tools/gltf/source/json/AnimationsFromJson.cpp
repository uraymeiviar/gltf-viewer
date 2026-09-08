#include "gltf/json/AnimationsFromJson.h"
#include "gltf/Gltf.h"
#include "gltf/animation/AnimationChannel.h"
#include "gltf/animation/AnimationSampler.h"
#include "gltf/json/GltfObjectFromJson.h"
#include "gltf/json/PointerLookup.h"
#include <string>
#include <vector>

using namespace ux3d;
using namespace ux3d::gltf;

ConversionResult AnimationsFromJson::convert(const nlohmann::json& _jsonAnimation)
{
    Animation* pAnimation = m_gltf.create<Animation>(getID(_jsonAnimation));
    if (pAnimation == nullptr) {
        return ConversionError::AllocationFailure;
    }

    if (!m_pointerLookup.add(pAnimation)) {
        return ConversionError::AllocationFailure;
    }

    bool containsErrors = false;
    for (const auto& [key, value] : _jsonAnimation.items()) {
        if (key == "name") {
            if (value.is_string()) {
                pAnimation->setName(value.get<std::string>());
            } else {
                core::logWarning("animation.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "channels") {
            if (value.is_array()) {
                for (const auto& jsonChannel : value) {
                    if (jsonChannel.is_object()) {
                        AnimationChannel* pChannel = pAnimation->createChannel();
                        convertChannel(jsonChannel, pChannel);
                    } else {
                        core::logWarning("animation.%s has element with value '%s' of invalid type", key.c_str(), jsonChannel.dump().c_str());
                        containsErrors = true;
                    }
                }
            } else {
                core::logWarning("animation.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "samplers") {
            if (value.is_array()) {
                for (const auto& jsonSampler : value) {
                    if (jsonSampler.is_object()) {
                        AnimationSampler* pSampler = pAnimation->createSampler();
                        convertSampler(jsonSampler, pSampler);
                    } else {
                        core::logWarning("animation.%s has element with value '%s' of invalid type", key.c_str(), jsonSampler.dump().c_str());
                        containsErrors = true;
                    }
                }
            } else {
                core::logWarning("animation.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "extensions") {
            if (value.is_object()) {
                for (const auto& [extensionName, extension] : value.items()) {
                    if (extensionName == "ux3d_gestaltor") {
                        if (extension.is_object()) {
                            if (core::Failed(convertIsPlaying(extension, *pAnimation))) {
                                containsErrors = true;
                            }
                        } else {
                            core::logWarning("extensions.%s with value '%s' has invalid type", extensionName.c_str(), extension.dump().c_str());
                            containsErrors = true;
                        }
                    }
                    if (extensionName == "KHR_xmp_json_ld") {
                        if (extension.is_object()) {
                            resolveXMP(extension, *pAnimation);
                        } else {
                            core::logWarning("animation.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension.dump().c_str());
                            containsErrors = true;
                        }
                    }
                }
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonAnimation, pAnimation, "animation")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonAnimation, pAnimation);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult AnimationsFromJson::resolve(const nlohmann::json& _jsonAnimation, Animation& _animation)
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonAnimation.items()) {
        if (key == "channels") {
            if (value.is_array()) {
                auto jsonIt = value.begin();
                auto jsonEnd = value.end();
                auto gltfIt = _animation.getChannels().begin();
                auto gltfEnd = _animation.getChannels().end();

                for (; jsonIt != jsonEnd && gltfIt != gltfEnd; jsonIt++, gltfIt++) {
                    if (!resolveChannel(*jsonIt, *gltfIt, _animation)) {
                        containsErrors = true;
                    }
                }
            }
        } else if (key == "samplers") {
            if (value.is_array()) {
                auto jsonIt = value.begin();
                auto jsonEnd = value.end();
                auto gltfIt = _animation.getSamplers().begin();
                auto gltfEnd = _animation.getSamplers().end();

                for (; jsonIt != jsonEnd && gltfIt != gltfEnd; jsonIt++, gltfIt++) {
                    if (!resolveSampler(*jsonIt, *gltfIt)) {
                        containsErrors = true;
                    }
                }
            }
        }
    }
    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }
    return ConversionResult::Success;
}

ConversionResult AnimationsFromJson::convertIsPlaying(const nlohmann::json& _editorExtension, Animation& _animation)
{
    if (_editorExtension.contains("isPlaying") == false) {
        return ConversionResult::Success;
    }

    auto jsonIsPlaying = _editorExtension["isPlaying"];
    if (jsonIsPlaying.is_boolean() == false) {
        core::logWarning("ux3d_gestaltor.isPlaying must be a bool");
        return ConversionError::InvalidType;
    }

    _animation.setPlaying(jsonIsPlaying.get<bool>());

    return ConversionResult::Success;
}

ConversionResult AnimationsFromJson::convertChannel(const nlohmann::json& _jsonChannel, AnimationChannel* _channel)
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonChannel.items()) {
        if (key == "target") {
            if (value.is_object()) {
                for (const auto& [keyTarget, valueTarget] : value.items()) {
                    if (keyTarget == "path") {
                        if (valueTarget.is_string()) {
                            _channel->getTarget().setPath(animationPathFromString(valueTarget));
                        } else {
                            core::logWarning("animation.channel.%s.%s with value '%s' has invalid type", key.c_str(), keyTarget.c_str(), valueTarget.dump().c_str());
                            containsErrors = true;
                        }
                    }
                    if (keyTarget == "extensions") {
                        if (valueTarget.contains("KHR_animation_pointer")) {
                            if (valueTarget.at("KHR_animation_pointer").contains("path")) {
                                std::string globalPointer = valueTarget["KHR_animation_pointer"]["path"].get<std::string>();
                            }
                        }
                    }
                }
                if (!GltfObjectFromJson::convertUnknownExtensions(value, &_channel->getTarget(), "animation.channel.target")) {
                    containsErrors = true;
                }
                GltfObjectFromJson::convertExtras(value, &_channel->getTarget());

            } else {
                core::logWarning("animation.channel.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonChannel, _channel, "animation.channel")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonChannel, _channel);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult AnimationsFromJson::convertSampler(const nlohmann::json& _jsonSampler, AnimationSampler* _sampler)
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonSampler.items()) {
        if (key == "interpolation") {
            if (value.is_string()) {
                std::string interpolation = value.get<std::string>();
                _sampler->setInterpolation(animationInterpolationFromString(interpolation));
            } else {
                core::logWarning("animation.sampler.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonSampler, _sampler, "animation.sampler")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonSampler, _sampler);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult AnimationsFromJson::resolveChannel(const nlohmann::json& _jsonChannel, AnimationChannel& _channel, Animation& _animation)
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonChannel.items()) {
        if (key == "sampler") {
            if (core::Succeeded(checkIndex(value, _animation.getSamplers().size(), "animation.channel.sampler"))) {
                AnimationSampler& sampler = _animation.getSamplers().at(value.get<uint32_t>());
                _channel.setSampler(sampler);
            } else {
                containsErrors = true;
            }
        } else if (key == "target") {
            if (value.is_object()) {
                if (!resolveChannelTarget(value, _channel.getTarget())) {
                    containsErrors = true;
                }
            } else {
                core::logWarning("animation.channel.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        }
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult AnimationsFromJson::resolveSampler(const nlohmann::json& _jsonSampler, AnimationSampler& _sampler)
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonSampler.items()) {
        if (key == "input") {
            if (core::Succeeded(checkIndex(value, m_pointerLookup.Accessors.size(), "animation.sampler.input"))) {
                Accessor* pAccessor = m_pointerLookup.Accessors[value.get<uint32_t>()];
                _sampler.setInputAccessor(*pAccessor);
                if (pAccessor->getBufferView() != nullptr) {
                    pAccessor->getBufferView()->unsetByteStride();
                }
            } else {
                containsErrors = true;
            }
        } else if (key == "output") {
            if (core::Succeeded(checkIndex(value, m_pointerLookup.Accessors.size(), "animation.sampler.output"))) {
                Accessor* pAccessor = m_pointerLookup.Accessors[value.get<uint32_t>()];
                _sampler.setOutputAccessor(*pAccessor);
                if (pAccessor->getBufferView() != nullptr) {
                    pAccessor->getBufferView()->unsetByteStride();
                }
            } else {
                containsErrors = true;
            }
        }
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult AnimationsFromJson::resolveChannelTarget(const nlohmann::json& _jsonTarget, AnimationChannelTarget& _target)
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonTarget.items()) {
        // Without extension, we only can target nodes
        if (key == "node") {
            if (core::Succeeded(checkIndex(value, m_pointerLookup.Nodes.size(), "animation.channel.target.node"))) {
                Node* pNode = m_pointerLookup.Nodes[value.get<uint32_t>()];
                if (pNode->getStoreAsMatrix()) {
                    pNode->storeAsMatrix(false);
                    core::logInfo("Node %s transform matrix is converted to TRS components since node is an animation target");
                }
                _target.setNode(*pNode);
            } else {
                containsErrors = true;
            }
        }

        if (key == "extensions") {
            if (value.contains("KHR_animation_pointer")) {
                std::string globalPointer = "";
                if (value.at("KHR_animation_pointer").contains("pointer")) {
                    globalPointer = value["KHR_animation_pointer"]["pointer"].get<std::string>();
                } else if (value.at("KHR_animation_pointer").contains("path")) {
                    globalPointer = value["KHR_animation_pointer"]["path"].get<std::string>();
                    core::logWarning("Invalid KHR_animation_pointer property: 'path' (should be 'pointer')");
                } else {
                    continue;
                }

                _target.m_animationPointer = _target.fromAnimationPointer(m_pointerLookup, globalPointer);
            }
        }
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult AnimationsFromJson::resolveXMP(const nlohmann::json& _jsonXMP, Animation& _animation)
{
    bool containsErrors = false;

    for (const auto& [xmpKey, xmpValue] : _jsonXMP.items()) {
        if (xmpKey == "packet") {
            if (m_gltf.getXMP().has_value() == false) {
                core::logWarning("AnimationsFromJson: Animation.XMP is defined, but the root extension KHR_xmp_json_ld is missing");
                containsErrors = true;
                continue;
            }

            XMP& xmp = m_gltf.getXMP().value();
            core::LinkedList<Packet>& packets = xmp.getPackets();
            const size_t index = xmpValue.get<size_t>();

            Packet* pPacket = &packets.at(index);
            _animation.setXMPPacket(pPacket);
        }
    }
    if (_animation.hasXMPPacket()) {
        if (!GltfObjectFromJson::convertUnknownExtensions(_jsonXMP, &_animation.getXMPPacket().value(), "animation.extensions.KHR_xmp_json_ld")) {
            containsErrors = true;
        }
        GltfObjectFromJson::convertExtras(_jsonXMP, &_animation.getXMPPacket().value());
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}
