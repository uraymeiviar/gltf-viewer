// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/merge/AudioEmitterMerger.h"
#include "gltf/AudioEmitter.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"

bool ux3d::gltf::AudioEmitterMerger::resolveDependencies(const AudioEmitter& _Source, AudioEmitter& _Destination)
{
    resolveSource(_Source, _Destination);
    return true;
}

void ux3d::gltf::AudioEmitterMerger::resolveSource(const AudioEmitter& source, AudioEmitter& destination)
{
    const AudioSource* pSrcSource = source.getSource();
    if (pSrcSource != nullptr) {
        const size_t sourceIndex = m_Indices[pSrcSource];
        AudioSource& dstSource = m_Destination.getAudioSources().at(sourceIndex);
        destination.setSource(&dstSource);
    }
}
