// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/json/AnimationsToJson.h"
#include "gltf/Animation.h"
#include "gltf/Gltf.h"
#include "gltf/XMP.h"
#include "gltf/json/AnimationChannelsToJson.h"
#include "gltf/json/AnimationSamplersToJson.h"
#include <gltf/ExportOptions.h>

using namespace ux3d;
using namespace ux3d::gltf;

AnimationsToJson::AnimationsToJson(const IndexLookup& _indices, const ExportOptions& _exportOptions, const Gltf& _gltf)
    : ChildOfRootToJson(_indices, _exportOptions)
    , m_gltf(_gltf)
{
}

void AnimationsToJson::convert(const Animation& _animation, nlohmann::json& _jsonAnimation, const JsonPath& _jsonPath) const
{
    convertBase(_animation, _jsonAnimation);
    convertChannels(_animation, _jsonAnimation);
    convertSamplers(_animation, _jsonAnimation);
    convertXMP(_animation, _jsonAnimation);

    if (m_exportOptions.conversionUsage == ConversionUsage::StateHistory || m_exportOptions.conversionUsage == ConversionUsage::ProjectFile) {
        convertIsPlaying(_animation, _jsonAnimation);
    }
}

void AnimationsToJson::convertIsPlaying(const Animation& _animation, nlohmann::json& _jsonAnimation) const
{
    _jsonAnimation["extensions"]["ux3d_gestaltor"]["isPlaying"] = _animation.isPlaying();
}

void AnimationsToJson::convertChannels(const Animation& _animation, nlohmann::json& _jsonAnimation) const
{
    if (_animation.getChannels().empty()) {
        return;
    }

    nlohmann::json jsonChannels = nlohmann::json::array();
    AnimationChannelsToJson converter(m_indices, m_exportOptions);

    for (const AnimationChannel& channel : _animation.getChannels()) {
        nlohmann::json jsonChannel = nlohmann::json::value_t::null;
        converter.convert(channel, jsonChannel);
        if (!jsonChannel.is_null()) {
            jsonChannels.emplace_back(jsonChannel);
        }
    }

    _jsonAnimation["channels"] = jsonChannels;
}

void AnimationsToJson::convertSamplers(const Animation& _animation, nlohmann::json& _jsonAnimation) const
{
    if (_animation.getSamplers().empty()) {
        return;
    }

    nlohmann::json jsonSamplers = nlohmann::json::array();
    AnimationSamplersToJson converter(m_indices, m_exportOptions);

    for (const AnimationSampler& sampler : _animation.getSamplers()) {
        nlohmann::json jsonSampler;
        converter.convert(sampler, jsonSampler);
        jsonSamplers.emplace_back(jsonSampler);
    }

    _jsonAnimation["samplers"] = jsonSamplers;
}

void AnimationsToJson::convertXMP(const Animation& _animation, nlohmann::json& _jsonAnimation) const
{
    if (_animation.hasXMPPacket()) {
        const PacketContainer& container = _animation.getXMPPacket().value();
        Packet* pPacket = container.m_packet;
        if (m_gltf.getXMP().has_value()) {
            const XMP& xmp = m_gltf.getXMP().value();
            const core::LinkedList<Packet>& packets = xmp.getPackets();

            for (size_t i = 0; i < packets.size(); ++i) {
                if (&packets.at(i) == pPacket) {
                    _jsonAnimation["extensions"]["KHR_xmp_json_ld"]["packet"] = i;
                    break;
                }
            }
        }

        // convert extras and extensions
        GltfObjectToJson gltfObjectToJson(m_exportOptions);
        gltfObjectToJson.convertExtras(container, _jsonAnimation["extensions"]["KHR_xmp_json_ld"]);
        gltfObjectToJson.convertUnknownExtensions(container, _jsonAnimation["extensions"]["KHR_xmp_json_ld"]);
    }
}
