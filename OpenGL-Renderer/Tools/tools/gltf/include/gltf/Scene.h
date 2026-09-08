// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRoot.h"
#include "XMP.h"
#include <set>

// See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#scene

namespace ux3d::gltf {
class Node;
class AudioEmitter;

class Scene : public ChildOfRoot {
public:
    class AudioEmitterPointerObject : public GltfObject {
    public:
        AudioEmitterPointerObject() { }
        std::set<AudioEmitter*> m_emitters;
    };

private:
    core::Vector<Node*> m_Nodes;

    // extensions
    std::optional<PacketContainer> m_xmpPacket = std::nullopt;
    std::optional<AudioEmitterPointerObject> m_audioEmitterPointerObject;

public:
    Scene(uint32_t _id); // regular constructor
    Scene(uint32_t _id, const Scene& _source); // duplicate in same glTF

    GltfType getGltfType() const override { return GltfType::Scene; }

    core::Vector<Node*>& getNodes() { return m_Nodes; }
    const core::Vector<Node*>& getNodes() const { return m_Nodes; }

    bool hasXMPPacket() const { return m_xmpPacket.has_value(); }
    std::optional<PacketContainer>& getXMPPacket() { return m_xmpPacket; }
    const std::optional<PacketContainer>& getXMPPacket() const { return m_xmpPacket; }
    GltfResult setXMPPacket(Packet* _xmpPacket);
    GltfResult unsetXMPPacket();

    GltfResult getNodesRecursively(core::Vector<Node*>& _recursiveNodes);
    GltfResult addNode(Node& _node, const Gltf& _root);
    GltfResult canAddNode(Node& _node, const Gltf& _root);
    GltfResult canAddNode(Node& _node, core::string& _message, const Gltf& _root);
    GltfResult insertNode(Node& _Node, size_t _Index, const Gltf& _root);
    GltfResult moveNode(size_t _FromIndex, size_t _ToIndex);
    GltfResult removeNode(Node& _Node);
    GltfResult removeAllNodes();

    void addAudioEmitter(AudioEmitter* emitter);
    void removeAudioEmitter(AudioEmitter* emitter);
    void unsetAudioEmitterPointerObject() { m_audioEmitterPointerObject.reset(); }
    std::optional<AudioEmitterPointerObject>& getAudioEmitterPointerObject() { return m_audioEmitterPointerObject; }
    const std::optional<AudioEmitterPointerObject>& getAudioEmitterPointerObject() const { return m_audioEmitterPointerObject; }
    AudioEmitterPointerObject& createAudioEmitterPointerObject() { return m_audioEmitterPointerObject.emplace(); }
    bool hasAudio() const { return m_audioEmitterPointerObject.has_value(); }
    bool hasAudioEmitters() const { return m_audioEmitterPointerObject.has_value() && getAudioEmitters().size() >= 0; }
    /**
     * @brief Get the Audio Emitters set.
     * ENSURE THE SCENE ACTUALLY HAS AUDIO EMITTERS FIRST BEFORE CALLING THIS METHOD!!!
     *
     * @return const std::set<AudioEmitter*>&
     */
    const std::set<AudioEmitter*>& getAudioEmitters() const;

    bool isValid() const override;
    GltfResult getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const override;

    ~Scene() override = default;
    Scene(const Scene&) = delete;
    Scene(Scene&&) = delete;
    void operator=(const Scene&) = delete;
    void operator=(Scene&&) = delete;
};
}
