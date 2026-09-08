// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

namespace ux3d::gltf {
class Gltf;
class Node;
class IndexLookup;

class NodeMerger {
private:
    Gltf& m_Destination;
    const IndexLookup& m_Indices;

    bool resolveChildren(const Node& _SourceNode, Node& _Node);
    void resolveCamera(const Node& _SourceNode, Node& _Node);
    void resolveLight(const Node& _SourceNode, Node& _Node);
    void resolveSkin(const Node& _SourceNode, Node& _Node);
    void resolveMesh(const Node& _SourceNode, Node& _Node);
    void resolveAudioEmitter(const Node& source, Node& destination);
    void resolveXMP(const Node& _SourceNode, Node& _Node);

public:
    NodeMerger(Gltf& _Destination, const IndexLookup& _IndexLookup)
        : m_Destination(_Destination)
        , m_Indices(_IndexLookup)
    {
    }

    bool resolveDependencies(const Node& _SourceNode, Node& _Node);
};
}
