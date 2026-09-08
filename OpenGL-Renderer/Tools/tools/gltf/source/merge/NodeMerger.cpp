// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/merge/NodeMerger.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"

bool ux3d::gltf::NodeMerger::resolveDependencies(const Node& _SourceNode, Node& _Node)
{
    if (!resolveChildren(_SourceNode, _Node)) {
        return false;
    }
    resolveCamera(_SourceNode, _Node);
    resolveLight(_SourceNode, _Node);
    resolveSkin(_SourceNode, _Node);
    resolveMesh(_SourceNode, _Node);
    resolveAudioEmitter(_SourceNode, _Node);
    resolveXMP(_SourceNode, _Node);
    return true;
}

bool ux3d::gltf::NodeMerger::resolveChildren(const Node& _SourceNode, Node& _Node)
{
    for (const Node* pSourceChild : _SourceNode.getChildren()) {
        const size_t childIndex = m_Indices[pSourceChild];
        Node& child = m_Destination.getNodes().at(childIndex);
        if (!_Node.addChild(child, m_Destination)) {
            return false;
        }
    }
    return true;
}

void ux3d::gltf::NodeMerger::resolveCamera(const Node& _SourceNode, Node& _Node)
{
    const Camera* pSourceCamera = _SourceNode.getCamera();
    if (pSourceCamera != nullptr) {
        const size_t cameraIndex = m_Indices[pSourceCamera];
        _Node.setCamera(m_Destination.getCameras().at(cameraIndex));
    }
}

void ux3d::gltf::NodeMerger::resolveLight(const Node& _SourceNode, Node& _Node)
{
    const Light* pSourceLight = _SourceNode.getLight();
    if (pSourceLight != nullptr) {
        const size_t lightIndex = m_Indices[pSourceLight];
        _Node.setLight(m_Destination.getLights().at(lightIndex));
    }
}

void ux3d::gltf::NodeMerger::resolveSkin(const Node& _SourceNode, Node& _Node)
{
    const Skin* pSourceSkin = _SourceNode.getSkin();
    if (pSourceSkin != nullptr) {
        const size_t skinIndex = m_Indices[pSourceSkin];
        Skin& skin = m_Destination.getSkins().at(skinIndex);
        _Node.setSkin(skin);
    }
}

void ux3d::gltf::NodeMerger::resolveMesh(const Node& _SourceNode, Node& _Node)
{
    const Mesh* pSourceMesh = _SourceNode.getMesh();
    if (pSourceMesh != nullptr) {
        const size_t meshIndex = m_Indices[pSourceMesh];
        _Node.setMesh(m_Destination.getMeshes().at(meshIndex));
    }
}

void ux3d::gltf::NodeMerger::resolveAudioEmitter(const Node& source, Node& destination)
{
    const AudioEmitter* pSrcEmitter = source.getAudioEmitter();
    if (pSrcEmitter != nullptr) {
        const size_t sourceIndex = m_Indices[pSrcEmitter];
        AudioEmitter& dstEmitter = m_Destination.getAudioEmitters().at(sourceIndex);
        destination.setAudioEmitter(dstEmitter);
    }
}

void ux3d::gltf::NodeMerger::resolveXMP(const Node& _SourceNode, Node& _Node)
{
    if (_SourceNode.hasXMPPacket()) {
        const Packet* pSourcePacket = _SourceNode.getXMPPacket().value().m_packet;
        const size_t packetIndex = m_Indices[pSourcePacket];

        if (m_Destination.hasXMP() == false) {
            m_Destination.createXMP();
        }

        _Node.setXMPPacket(&(m_Destination.getXMP().value().getPackets().at(packetIndex)));
    }
}
