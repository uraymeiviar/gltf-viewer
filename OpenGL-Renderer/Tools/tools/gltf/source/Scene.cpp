// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Scene.h"
#include "gltf/AudioEmitter.h"
#include "gltf/Node.h"
#include "gltf/XMP.h"

using namespace ux3d;
using namespace ux3d::gltf;

Scene::Scene(uint32_t _id)
    : ChildOfRoot(_id)
{
}

Scene::Scene(uint32_t _id, const Scene& _source)
    : ChildOfRoot(_id, _source)
    , m_xmpPacket(_source.m_xmpPacket)
    , m_audioEmitterPointerObject(_source.m_audioEmitterPointerObject)
{
    // we can't copy the nodes because Nodes can only belong to one Scene
}

GltfResult Scene::addNode(Node& _node, const Gltf& _root)
{
    core::string message;
    GltfResult result = canAddNode(_node, message, _root);

    if (core::Failed(result)) {
        core::logWarning(message.c_str());
        return result;
    }

    if (m_Nodes.emplace_back(&_node) == nullptr) {
        return GltfError::AllocationFailure;
    }

    return GltfResult::Success;
}

GltfResult Scene::canAddNode(Node& _node, const Gltf& _root)
{
    core::string message;
    return canAddNode(_node, message, _root);
}

GltfResult Scene::canAddNode(Node& _node, core::string& _message, const Gltf& _root)
{
    if (core::contains(m_Nodes, &_node)) {
        _message = "Cannot add node to scene because it is already in this scene";
        return GltfError::ObjectAlreadyExists;
    }
    if (_node.isParentAScene(_root) == false && _node.hasParent(_root)) {
        _message = "Cannot add node to scene because it already has a parent.";
        return GltfError::NodeHasParent;
    }

    return GltfResult::Success;
}

GltfResult Scene::insertNode(Node& _Node, const size_t _Index, const Gltf& _root)
{
    GltfResult result = addNode(_Node, _root);
    if (result != GltfResult::Success) {
        return result;
    }

    return moveNode(m_Nodes.size() - 1, _Index);
}

GltfResult Scene::moveNode(const size_t _FromIndex, const size_t _ToIndex)
{
    if (_FromIndex >= m_Nodes.size() || _ToIndex >= m_Nodes.size()) {
        return GltfError::ObjectNotFound;
    }

    bool increase = _FromIndex < _ToIndex;

    for (size_t i = _FromIndex; i != _ToIndex; i = increase ? i + 1 : i - 1) {
        size_t swapIndex = increase ? i + 1 : i - 1;
        Node* pTemp = m_Nodes[i];
        m_Nodes[i] = m_Nodes[swapIndex];
        m_Nodes[swapIndex] = pTemp;
    }

    return GltfResult::Success;
}

GltfResult Scene::removeNode(Node& _Node)
{
    for (auto it = m_Nodes.begin(), end = m_Nodes.end(); it != end; ++it) {
        if (*it == &_Node) {
            m_Nodes.erase_keep_order(it);
            return GltfResult::Success;
        }
    }

    return GltfError::ObjectNotFound;
}

GltfResult Scene::removeAllNodes()
{
    m_Nodes.clear();
    return GltfResult::Success;
}

GltfResult Scene::setXMPPacket(Packet* _xmpPacket)
{
    if (m_xmpPacket.has_value() == false) {
        m_xmpPacket.emplace(PacketContainer());
    }
    m_xmpPacket.value().m_packet = _xmpPacket;
    return GltfResult::Success;
}

GltfResult Scene::unsetXMPPacket()
{
    m_xmpPacket.reset();
    return GltfResult::Success;
}

GltfResult Scene::getNodesRecursively(ux3d::core::Vector<Node*>& _recursiveNodes)
{
    for (Node* pNode : m_Nodes) {
        if (!_recursiveNodes.push_back(pNode)) {
            return GltfError::AllocationFailure;
        }
        GltfResult result = pNode->getChildrenRecursively(_recursiveNodes);
        if (result != GltfResult::Success) {
            return result;
        }
    }

    return GltfResult::Success;
}

bool Scene::isValid() const
{
    // nothing is required of a scene
    return true;
}

GltfResult Scene::getInvalidProperties(core::Vector<ux3d::core::string>&) const
{
    return GltfResult::Success;
}

void Scene::addAudioEmitter(AudioEmitter* emitter)
{
    if (!m_audioEmitterPointerObject.has_value()) {
        m_audioEmitterPointerObject.emplace();
    }

    m_audioEmitterPointerObject.value().m_emitters.insert(emitter);
}

void Scene::removeAudioEmitter(AudioEmitter* emitter)
{
    if (!m_audioEmitterPointerObject.has_value()) {
        return;
    }
    m_audioEmitterPointerObject.value().m_emitters.erase(emitter);

    if (m_audioEmitterPointerObject.value().m_emitters.size() == 0) {
        unsetAudioEmitterPointerObject();
    }
}

const std::set<AudioEmitter*>& Scene::getAudioEmitters() const
{
    return m_audioEmitterPointerObject.value().m_emitters;
}
