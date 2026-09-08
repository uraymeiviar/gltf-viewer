// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

namespace ux3d::gltf {
class Gltf;
class Skin;
class IndexLookup;

class SkinMerger {
private:
    Gltf& m_Destination;
    const IndexLookup& m_Indices;

    void resolveInverseBindMatricesAccessor(const Skin& _SourceSkin, Skin& _Skin);
    void resolveSkeletonRoot(const Skin& _SourceSkin, Skin& _Skin);
    bool resolveJoints(const Skin& _SourceSkin, Skin& _Skin);

public:
    SkinMerger(Gltf& _Destination, const IndexLookup& _IndexLookup)
        : m_Destination(_Destination)
        , m_Indices(_IndexLookup)
    {
    }

    bool resolveDependencies(const Skin& _SourceSkin, Skin& _Skin);
};
}
