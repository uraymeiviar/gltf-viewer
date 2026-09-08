// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/merge/BufferViewMerger.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"

bool ux3d::gltf::BufferViewMerger::resolveDependencies(const BufferView& _SourceBufferView, BufferView& _BufferView)
{
    resolveBuffer(_SourceBufferView, _BufferView);
    return true;
}

void ux3d::gltf::BufferViewMerger::resolveBuffer(const BufferView& _SourceBufferView, BufferView& _BufferView)
{
    const Buffer* pSourceBuffer = _SourceBufferView.getBuffer();
    const size_t bufferIndex = m_Indices[pSourceBuffer];
    Buffer& buffer = m_Destination.getBuffers().at(bufferIndex);
    _BufferView.setBuffer(buffer);
}
