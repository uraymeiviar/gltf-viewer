// Copyright (c) 2021 UX3D GmbH. All rights reserved.

#include "gltf/merge/AudioSourceMerger.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"

using namespace ux3d;
using namespace ux3d::gltf;

bool AudioSourceMerger::resolveDependencies(const AudioSource& _srcSource, AudioSource& _dstSource)
{
    if (_srcSource.getBufferView() == nullptr) {
        return true;
    }

    const size_t bufferViewIndex = m_indices[_srcSource.getBufferView()];
    BufferView& bufferView = m_destination.getBufferViews().at(bufferViewIndex);
    _dstSource.setBufferView(bufferView);

    return true;
}
