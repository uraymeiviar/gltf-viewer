// Copyright (c) 2021 UX3D GmbH. All rights reserved.

#pragma once

namespace ux3d::gltf {
class Gltf;
class IndexLookup;
class AudioSource;

class AudioSourceMerger {
private:
    Gltf& m_destination;
    const IndexLookup& m_indices;

public:
    AudioSourceMerger(Gltf& _destination, const IndexLookup& _indexLookup)
        : m_destination(_destination)
        , m_indices(_indexLookup)
    {
    }

    bool resolveDependencies(const AudioSource& _srcSource, AudioSource& _dstSource);
};
}
