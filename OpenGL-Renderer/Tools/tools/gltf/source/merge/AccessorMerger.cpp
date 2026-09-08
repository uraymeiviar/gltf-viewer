// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/merge/AccessorMerger.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"

using namespace ux3d;
using namespace ux3d::gltf;

bool ux3d::gltf::AccessorMerger::resolveDependencies(const Accessor& _SourceAccessor, Accessor& _Accessor)
{
    resolveBufferView(_SourceAccessor, _Accessor);

    if (const auto& oSourceSparse = _SourceAccessor.getSparse(); oSourceSparse.has_value()) {
        resolveSparseIndices(oSourceSparse.value(), _Accessor.getSparse().value());
        resolveSparseValues(oSourceSparse.value(), _Accessor.getSparse().value());
    }

    return true;
}

void ux3d::gltf::AccessorMerger::resolveBufferView(const Accessor& _SourceAccessor, Accessor& _Accessor)
{
    const BufferView* pSourceBufferView = _SourceAccessor.getBufferView();
    if (pSourceBufferView != nullptr) {
        const size_t bufferViewIndex = m_Indices[pSourceBufferView];
        BufferView& bufferView = m_Destination.getBufferViews().at(bufferViewIndex);
        _Accessor.setBufferView(bufferView);
    }
}

void AccessorMerger::resolveSparseIndices(const Sparse& _sourceSparse, Sparse& _sparse)
{
    const BufferView* sourceBufferView = _sourceSparse.getIndices().getBufferView();
    if (sourceBufferView != nullptr) {
        const size_t bufferViewIndex = m_Indices[sourceBufferView];
        BufferView& bufferView = m_Destination.getBufferViews().at(bufferViewIndex);
        _sparse.getIndices().setBufferView(bufferView);
    }
}

void AccessorMerger::resolveSparseValues(const Sparse& _sourceSparse, Sparse& _sparse)
{
    const BufferView* sourceBufferView = _sourceSparse.getValues().getBufferView();
    if (sourceBufferView != nullptr) {
        const size_t bufferViewIndex = m_Indices[sourceBufferView];
        BufferView& bufferView = m_Destination.getBufferViews().at(bufferViewIndex);
        _sparse.getValues().setBufferView(bufferView);
    }
}
