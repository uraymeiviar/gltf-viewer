// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRoot.h"
#include "XMP.h"
#include "animation/AnimationChannel.h"
#include "animation/AnimationSampler.h"
#include "core/LinkedList.h"

// See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#animation

namespace ux3d::gltf {

class Animation : public ChildOfRoot {
private:
    core::LinkedList<AnimationChannel> m_Channels;
    core::LinkedList<AnimationSampler> m_Samplers;

    // extensions
    std::optional<PacketContainer> m_xmpPacket = std::nullopt;

    // Meta data
    bool m_isPlaying = false;

public:
    Animation(uint32_t _id); // regular constructor
    Animation(uint32_t _id, const Animation& _source); // duplicate in same glTF

    GltfType getGltfType() const override { return GltfType::Animation; }

    Extensions getUsedExtensions() const override;

    core::LinkedList<AnimationChannel>& getChannels() { return m_Channels; }
    const core::LinkedList<AnimationChannel>& getChannels() const { return m_Channels; }

    core::LinkedList<AnimationSampler>& getSamplers() { return m_Samplers; }
    const core::LinkedList<AnimationSampler>& getSamplers() const { return m_Samplers; }

    bool isPlaying() const { return m_isPlaying; }
    void setPlaying(bool _isPlaying) { m_isPlaying = _isPlaying; }

    bool hasXMPPacket() const { return m_xmpPacket.has_value(); }
    std::optional<PacketContainer>& getXMPPacket() { return m_xmpPacket; }
    const std::optional<PacketContainer>& getXMPPacket() const { return m_xmpPacket; }
    GltfResult setXMPPacket(Packet* _xmpPacket);
    GltfResult unsetXMPPacket();

    AnimationChannel* createChannel() { return m_Channels.emplace_back(*this); }

    AnimationSampler* createSampler() { return m_Samplers.emplace_back(*this); }

    bool isValid() const override;
    GltfResult getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const override;
};
}
