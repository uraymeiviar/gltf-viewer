// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: David Labode

#include "renderer/gltf/GltfAudioEmitterNodeMapping.h"
#include "AL/al.h"
#include "gltf/AudioEmitter.h"
#include "renderer/AudioEmitter.h"
#include "renderer/TransformHierarchy.h"
#include "renderer/gltf/GltfRenderLayer.h"

using namespace ux3d;
using namespace ux3d::renderer;

GltfAudioEmitterNodeMapping::GltfAudioEmitterNodeMapping(GltfRenderLayer& _scene)
    : GltfMapping<GltfAudioEmitterNodeTuple, RendererAudioEmitterTuple>(_scene)
{
}

bool GltfAudioEmitterNodeMapping::onUpdateMapping(GltfAudioEmitterNodeTuple gltfTuple, RendererAudioEmitterTuple rendererTuple)
{
    auto [gltfNode, gltfAudioEmitter] = gltfTuple;
    auto [audioEmitter] = rendererTuple;

    audioEmitter->playing = gltfAudioEmitter->isPlaying();
    audioEmitter->loop = gltfAudioEmitter->isLoop();
    audioEmitter->gain = gltfAudioEmitter->getGain();

    switch (gltfAudioEmitter->getType()) {
    case gltf::AudioEmitter::EmitterType::Positional: {
        audioEmitter->type = AudioEmitter::Type::Positional;
        if (!gltfAudioEmitter->hasPositional()) {
            return true; // positional object is missing, but default values will be used
        }
        const gltf::AudioEmitterPositional& positional = gltfAudioEmitter->getPositional();
        audioEmitter->coneInnerAngle = positional.getConeInnerAngle();
        audioEmitter->coneOuterAngle = positional.getConeOuterAngle();
        audioEmitter->coneOuterGain = positional.getConeOuterGain();
        audioEmitter->maxDistance = positional.getMaxDistance();
        audioEmitter->refDistance = positional.getRefDistance();
        audioEmitter->rollofFactor = positional.getRolloffFactor();
        switch (positional.getDistanceModel()) {
        case gltf::AudioEmitterPositional::EmitterDistanceModel::Inverse:
            audioEmitter->distanceModel = AudioEmitter::DistanceModel::Inverse;
            break;
        case gltf::AudioEmitterPositional::EmitterDistanceModel::Linear:
            audioEmitter->distanceModel = AudioEmitter::DistanceModel::Linear;
            break;
        case gltf::AudioEmitterPositional::EmitterDistanceModel::Exponential:
            audioEmitter->distanceModel = AudioEmitter::DistanceModel::Exponential;
            break;
        }
        break;
    }
    case gltf::AudioEmitter::EmitterType::Global:
        // global emitters are not supported by nodes
        return false;
    }

    return true;
}

RendererAudioEmitterTuple GltfAudioEmitterNodeMapping::onCreateMapping(GltfAudioEmitterNodeTuple gltfTuple)
{
    auto [gltfNode, gltfAudioEmitter] = gltfTuple;

    auto audioEmitter = new AudioEmitter;

    RendererAudioEmitterTuple tuple = { audioEmitter };
    onUpdateMapping(gltfTuple, tuple);

    auto nodeIt = m_gltfRenderLayer.m_transformHierarchyMapping.map.find(gltfNode);
    if (nodeIt != m_gltfRenderLayer.m_transformHierarchyMapping.map.end()) {
        nodeIt->second->addAudioEmitter(audioEmitter);
    }

    alGenSources(1, &audioEmitter->source);

    if (alGetError() != AL_NO_ERROR) {
        if (alIsSource(audioEmitter->source)) {
            alDeleteSources(1, &audioEmitter->source);
        }
    }

    return tuple;
}

void GltfAudioEmitterNodeMapping::onDestroyMapping(GltfAudioEmitterNodeTuple gltfTuple, RendererAudioEmitterTuple rendererTuple)
{
    auto [gltfNode, gltfAudioEmitter] = gltfTuple;
    auto [audioEmitter] = rendererTuple;

    auto nodeIt = m_gltfRenderLayer.m_transformHierarchyMapping.map.find(gltfNode);
    if (nodeIt != m_gltfRenderLayer.m_transformHierarchyMapping.map.end()) {
        nodeIt->second->removeAudioEmitter(audioEmitter);
    }

    alSourceStop(audioEmitter->source);
    alDeleteSources(1, &audioEmitter->source);
    delete audioEmitter;
}
