// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

namespace ux3d::gltf {
class Gltf;
class BufferView;
class IndexLookup;

class BufferViewMerger {
    Gltf& m_Destination;
    const IndexLookup& m_Indices;

    void resolveBuffer(const BufferView& _SourceBufferView, BufferView& _BufferView);

public:
    BufferViewMerger(Gltf& _Destination, const IndexLookup& _IndexLookup)
        : m_Destination(_Destination)
        , m_Indices(_IndexLookup)
    {
    }

    bool resolveDependencies(const BufferView& _SourceBufferView, BufferView& _BufferView);
};
}
