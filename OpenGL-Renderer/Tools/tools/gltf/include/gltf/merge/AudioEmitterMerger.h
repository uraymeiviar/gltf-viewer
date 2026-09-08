// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

namespace ux3d::gltf {
class Gltf;
class AudioEmitter;
class IndexLookup;

class AudioEmitterMerger {
private:
    Gltf& m_Destination;
    const IndexLookup& m_Indices;

    void resolveSource(const AudioEmitter& _Source, AudioEmitter& _Target);

public:
    AudioEmitterMerger(Gltf& _Destination, const IndexLookup& _IndexLookup)
        : m_Destination(_Destination)
        , m_Indices(_IndexLookup)
    {
    }

    bool resolveDependencies(const AudioEmitter& _Source, AudioEmitter& _Target);
};
}
