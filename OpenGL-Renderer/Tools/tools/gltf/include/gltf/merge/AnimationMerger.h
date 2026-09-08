// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

namespace ux3d::gltf {
class Gltf;
class Animation;
class IndexLookup;

class AnimationMerger {
private:
    Gltf& m_Destination;
    const IndexLookup& m_Indices;

    void resolveChannelTargets(const Animation& _SourceAnimation, Animation& _Animation);
    void resolveChannelSamplers(const Animation& _SourceAnimation, Animation& _Animation);
    void resolveSamplerAccessors(const Animation& _SourceAnimation, Animation& _Animation);
    void resolveXMP(const Animation& _SourceAnimation, Animation& _Animation);

public:
    AnimationMerger(Gltf& _Destination, const IndexLookup& _IndexLookup)
        : m_Destination(_Destination)
        , m_Indices(_IndexLookup)
    {
    }

    bool resolveDependencies(const Animation& _SourceAnimation, Animation& _Animation);
};
}
