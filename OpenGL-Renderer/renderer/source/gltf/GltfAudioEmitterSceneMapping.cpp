// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: David Labode

#include "renderer/gltf/GltfAudioEmitterSceneMapping.h"
#include "AL/al.h"
#include "gltf/AudioEmitter.h"
#include "renderer/AudioEmitter.h"

using namespace ux3d;
using namespace ux3d::renderer;

GltfAudioEmitterSceneMapping::GltfAudioEmitterSceneMapping(GltfRenderLayer& _scene)
    : GltfMapping<GltfAudioEmitterSceneTuple, RendererAudioEmitterTuple>(_scene)
{
}

bool GltfAudioEmitterSceneMapping::onUpdateMapping(GltfAudioEmitterSceneTuple gltfTuple, RendererAudioEmitterTuple rendererTuple)
{
    auto [gltfScene, gltfAudioEmitter] = gltfTuple;
    auto [audioEmitter] = rendererTuple;

    audioEmitter->playing = gltfAudioEmitter->isPlaying();
    audioEmitter->loop = gltfAudioEmitter->isLoop();
    audioEmitter->gain = gltfAudioEmitter->getGain();

    switch (gltfAudioEmitter->getType()) {
    case gltf::AudioEmitter::EmitterType::Positional: {
        // positional emitters are not supported by scenes
        return false;
    }
    case gltf::AudioEmitter::EmitterType::Global:
        audioEmitter->type = AudioEmitter::Type::Global;
        break;
    }

    return true;
}

RendererAudioEmitterTuple GltfAudioEmitterSceneMapping::onCreateMapping(GltfAudioEmitterSceneTuple gltfTuple)
{
    auto [gltfScene, gltfAudioEmitter] = gltfTuple;

    auto audioEmitter = new AudioEmitter;

    RendererAudioEmitterTuple tuple = { audioEmitter };
    onUpdateMapping(gltfTuple, tuple);

    alGenSources(1, &audioEmitter->source);

    if (alGetError() != AL_NO_ERROR) {
        if (alIsSource(audioEmitter->source)) {
            alDeleteSources(1, &audioEmitter->source);
        }
    }

    return tuple;
}

void GltfAudioEmitterSceneMapping::onDestroyMapping(GltfAudioEmitterSceneTuple gltfTuple, RendererAudioEmitterTuple rendererTuple)
{
    auto [gltfScene, gltfAudioEmitter] = gltfTuple;
    auto [audioEmitter] = rendererTuple;

    alSourceStop(audioEmitter->source);
    alDeleteSources(1, &audioEmitter->source);
    delete audioEmitter;
}
