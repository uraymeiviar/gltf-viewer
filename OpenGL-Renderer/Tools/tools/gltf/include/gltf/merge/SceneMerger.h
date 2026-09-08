// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

namespace ux3d::gltf {
class Gltf;
class Scene;
class IndexLookup;

class SceneMerger {
private:
    Gltf& m_Destination;
    const IndexLookup& m_Indices;

    void resolveXMP(const Scene& _SourceScene, Scene& _Scene);
    bool resolveNodes(const Scene& _SourceScene, Scene& _Scene);
    void resolveAudioEmitters(const Scene& source, Scene& destination);

public:
    SceneMerger(Gltf& _Destination, const IndexLookup& _IndexLookup)
        : m_Destination(_Destination)
        , m_Indices(_IndexLookup)
    {
    }

    bool resolveDependencies(const Scene& _SourceScene, Scene& _Scene);
};
}
