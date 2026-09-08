// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/merge/SceneMerger.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"

#include <set>

bool ux3d::gltf::SceneMerger::resolveDependencies(const Scene& _SourceScene, Scene& _Scene)
{
    resolveAudioEmitters(_SourceScene, _Scene);
    resolveXMP(_SourceScene, _Scene);
    return resolveNodes(_SourceScene, _Scene);
}

bool ux3d::gltf::SceneMerger::resolveNodes(const Scene& _SourceScene, Scene& _Scene)
{
    for (const Node* pSourceNode : _SourceScene.getNodes()) {
        const size_t nodeIndex = m_Indices[pSourceNode];
        Node& node = m_Destination.getNodes().at(nodeIndex);
        if (!_Scene.addNode(node, m_Destination)) {
            return false;
        }
    }
    return true;
}

void ux3d::gltf::SceneMerger::resolveAudioEmitters(const Scene& source, Scene& destination)
{
    if (source.hasAudioEmitters()) {
        const std::set<AudioEmitter*>& srcEmitters = source.getAudioEmitters();

        for (auto* srcEmitter : srcEmitters) {
            if (srcEmitter != nullptr) {
                const size_t sourceIndex = m_Indices[srcEmitter];
                AudioEmitter& dstEmitter = m_Destination.getAudioEmitters().at(sourceIndex);
                destination.addAudioEmitter(&dstEmitter);
            }
        }
    }
}

void ux3d::gltf::SceneMerger::resolveXMP(const Scene& source, Scene& dest)
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
