// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Animation.h"
#include "gltf/XMP.h"

using namespace ux3d;
using namespace ux3d::gltf;

Animation::Animation(uint32_t _id)
    : ChildOfRoot(_id)
{
}

Animation::Animation(uint32_t _id, const Animation& _source)
    : ChildOfRoot(_id, _source)
    , m_xmpPacket(_source.m_xmpPacket)
{
    for (const AnimationChannel& sourceChannel : _source.getChannels()) {
        AnimationChannel* pChannel = m_Channels.emplace_back(*this, sourceChannel);
        if (const AnimationSampler* pSampler = sourceChannel.getSampler();
            pSampler != nullptr) {
            pChannel->setSampler(*const_cast<AnimationSampler*>(pSampler));
        }
        if (const Node* pTargetNode = sourceChannel.getTarget().getNode();
            pTargetNode != nullptr) {
            pChannel->getTarget().setNode(*const_cast<Node*>(pTargetNode));
        }
        if (sourceChannel.getTarget().m_animationPointer.has_value()) {
            pChannel->getTarget().setAnimationPointer(sourceChannel.getTarget().m_animationPointer.value());
        }
    }

    for (const AnimationSampler& sourceSampler : _source.getSamplers()) {
        AnimationSampler* pSampler = m_Samplers.emplace_back(*this, sourceSampler);
        if (const Accessor* pInput = sourceSampler.getInputAccessor();
            pInput != nullptr) {
            pSampler->setInputAccessor(*const_cast<Accessor*>(pInput));
        }
        if (const Accessor* pOutput = sourceSampler.getOutputAccessor();
            pOutput != nullptr) {
            pSampler->setOutputAccessor(*const_cast<Accessor*>(pOutput));
        }
    }
}

Extensions Animation::getUsedExtensions() const
{
    for (const AnimationChannel& sourceChannel : getChannels()) {
        if (sourceChannel.getTarget().m_animationPointer.has_value())
            return Extension::AnimationPointer;
    }

    return Extension::None;
}

GltfResult Animation::setXMPPacket(Packet* _xmpPacket)
{
    if (m_xmpPacket.has_value() == false) {
        m_xmpPacket.emplace(PacketContainer());
    }
    m_xmpPacket.value().m_packet = _xmpPacket;
    return GltfResult::Success;
}

GltfResult Animation::unsetXMPPacket()
{
    m_xmpPacket.reset();
    return GltfResult::Success;
}

bool Animation::isValid() const
{
    // Different channels of the same animation can't have equal targets.

    const size_t animChannelCount = m_Channels.size();
    for (size_t x = 0; x < animChannelCount; x++) {
        const AnimationChannel& animationChannel = m_Channels.at(x);
        const AnimationChannelTarget& channelTarget = animationChannel.getTarget();

        if (channelTarget.getNode() == nullptr) {
            // When node isn't defined, channel should be ignored
            continue;
        }

        for (size_t y = x + 1; y < animChannelCount; y++) {
            const AnimationChannel& otherChannel = m_Channels.at(y);
            const AnimationChannelTarget& otherTarget = otherChannel.getTarget();
            if (channelTarget == otherTarget) {
                return false;
            }
        }
    }

    return !m_Channels.empty() && !m_Samplers.empty();
}

GltfResult Animation::getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const
{
    // Different channels of the same animation can't have equal targets.

    const size_t animChannelCount = m_Channels.size();
    for (size_t x = 0; x < animChannelCount; x++) {
        const AnimationChannel& animationChannel = m_Channels.at(x);
        const AnimationChannelTarget& channelTarget = animationChannel.getTarget();

        if (channelTarget.getNode() == nullptr) {
            // When node isn't defined, channel should be ignored
            continue;
        }

        for (size_t y = x + 1; y < animChannelCount; y++) {
            const AnimationChannel& otherChannel = m_Channels.at(y);
            const AnimationChannelTarget& otherTarget = otherChannel.getTarget();
            if (channelTarget == otherTarget) {
                if (_InvalidProperties.push_back("channels") == nullptr) {
                    return GltfError::AllocationFailure;
                }
            }
        }
    }

    if (m_Channels.empty()) {
        if (_InvalidProperties.push_back("channels") == nullptr) {
            return GltfError::AllocationFailure;
        }
    }
    if (m_Samplers.empty()) {
        if (_InvalidProperties.push_back("samplers") == nullptr) {
            return GltfError::AllocationFailure;
        }
    }

    return GltfResult::Success;
}
