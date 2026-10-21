// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/merge/AnimationMerger.h"
#include "core/Algorithms.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"

using namespace ux3d;
using namespace ux3d::gltf;

bool AnimationMerger::resolveDependencies(const Animation& _SourceAnimation, Animation& _Animation)
{
    resolveChannelTargets(_SourceAnimation, _Animation);
    resolveChannelSamplers(_SourceAnimation, _Animation);
    resolveSamplerAccessors(_SourceAnimation, _Animation);
    resolveXMP(_SourceAnimation, _Animation);
    return true;
}

void AnimationMerger::resolveChannelTargets(const Animation& _SourceAnimation, Animation& _Animation)
{
    auto channelIter = _Animation.getChannels().begin();
    for (const AnimationChannel& sourceChannel : _SourceAnimation.getChannels()) {
        AnimationChannel& channel = *channelIter;

        const Node* pSourceTargetNode = sourceChannel.getTarget().getNode();
        if (pSourceTargetNode != nullptr) {
            const size_t targetIndex = m_Indices[pSourceTargetNode];
            channel.getTarget().setNode(m_Destination.getNodes().at(targetIndex));
        }
        if (sourceChannel.getTarget().m_animationPointer.has_value()) {
            auto& [gltfObject, localPath] = sourceChannel.getTarget().m_animationPointer.value();
            std::string animationPointer = AnimationChannelTarget::toAnimationPointer(m_Indices, channel.getTarget().m_animationPointer.value());
            ChildOfRoot* newTargetObject = nullptr;

            size_t targetIndex = 0;
            if (gltfObject->getGltfType() == GltfType::Node) {
                targetIndex = m_Indices[static_cast<Node*>(gltfObject)];
                newTargetObject = &m_Destination.getNodes().at(targetIndex);
            } else if (gltfObject->getGltfType() == GltfType::Camera) {
                targetIndex = m_Indices[static_cast<Camera*>(gltfObject)];
                newTargetObject = &m_Destination.getCameras().at(targetIndex);
            } else if (gltfObject->getGltfType() == GltfType::Material) {
                targetIndex = m_Indices[static_cast<Material*>(gltfObject)];
                newTargetObject = &m_Destination.getMaterials().at(targetIndex);
            } else if (gltfObject->getGltfType() == GltfType::Mesh) {
                targetIndex = m_Indices[static_cast<Mesh*>(gltfObject)];
                newTargetObject = &m_Destination.getMeshes().at(targetIndex);
            } else if (gltfObject->getGltfType() == GltfType::Light) {
                targetIndex = m_Indices[static_cast<Light*>(gltfObject)];
                newTargetObject = &m_Destination.getLights().at(targetIndex);
            }
            channel.getTarget().setAnimationPointer(std::tuple<ChildOfRoot*, std::string>(newTargetObject, localPath));
        }

        channelIter++;
    }
}

void AnimationMerger::resolveChannelSamplers(const Animation& _SourceAnimation, Animation& _Animation)
{
    auto channelIter = _Animation.getChannels().begin();
    for (const AnimationChannel& sourceChannel : _SourceAnimation.getChannels()) {
        AnimationChannel& channel = *channelIter;

        const AnimationSampler* pSourceSampler = sourceChannel.getSampler();
        if (pSourceSampler != nullptr) {
            const size_t samplerIndex = core::findIndexOf(_SourceAnimation.getSamplers(), pSourceSampler);
            channel.setSampler(_Animation.getSamplers().at(samplerIndex));
        }

        channelIter++;
    }
}

void AnimationMerger::resolveSamplerAccessors(const Animation& _SourceAnimation, Animation& _Animation)
{
    auto samplerIter = _Animation.getSamplers().begin();
    for (const AnimationSampler& sourceSampler : _SourceAnimation.getSamplers()) {
        AnimationSampler& sampler = *samplerIter;

        const Accessor* pSourceInput = sourceSampler.getInputAccessor();
        if (pSourceInput != nullptr) {
            const size_t inputIndex = m_Indices[pSourceInput];
            sampler.setInputAccessor(m_Destination.getAccessors().at(inputIndex));
        }

        const Accessor* pSourceOutput = sourceSampler.getOutputAccessor();
        if (pSourceOutput != nullptr) {
            const size_t outputIndex = m_Indices[pSourceOutput];
            sampler.setOutputAccessor(m_Destination.getAccessors().at(outputIndex));
        }

        samplerIter++;
    }
}

void AnimationMerger::resolveXMP(const Animation& source, Animation& dest)
{
    if (source.hasXMPPacket()) {
        const Packet* pSourcePacket = source.getXMPPacket().value().m_packet;
        const size_t packetIndex = m_Indices[pSourcePacket];

        if (m_Destination.hasXMP() == false) {
            m_Destination.createXMP();
        }

        dest.setXMPPacket(&(m_Destination.getXMP().value().getPackets().at(packetIndex)));
    }
}
