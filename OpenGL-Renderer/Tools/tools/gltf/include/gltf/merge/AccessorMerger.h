// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

namespace ux3d::gltf {
class Gltf;
class Accessor;
class Sparse;
class IndexLookup;

class AccessorMerger {
private:
    Gltf& m_Destination;
    const IndexLookup& m_Indices;

    void resolveBufferView(const Accessor& _SourceAccessor, Accessor& _Accessor);
    void resolveSparseIndices(const Sparse& _sourceSparse, Sparse& _sparse);
    void resolveSparseValues(const Sparse& _sourceSparse, Sparse& _sparse);

public:
    AccessorMerger(Gltf& _Destination, const IndexLookup& _IndexLookup)
        : m_Destination(_Destination)
        , m_Indices(_IndexLookup)
    {
    }

    bool resolveDependencies(const Accessor& _SourceAccessor, Accessor& _Accessor);
};
}
