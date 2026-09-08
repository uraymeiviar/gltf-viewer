// Copyright © 2022 UX3D GmbH. All rights reserved.
// Authors: Phillip Hohenester

#include "renderer/gltf/GltfAudioSourceMapping.h"
#include "gltf/AudioSource.h"
#include "renderer/AudioBuffer.h"
#include "renderer/gltf/GltfRenderLayer.h"

#include "AL/al.h"
#include "AL/alext.h"

using namespace ux3d;
using namespace ux3d::renderer;

GltfAudioSourceMapping::GltfAudioSourceMapping(GltfRenderLayer& _scene)
    : GltfMapping<GltfAudioSourceTuple, RendererAudioBufferTuple>(_scene)
{
}

bool GltfAudioSourceMapping::onUpdateMapping(GltfAudioSourceTuple gltfSourceTuple, RendererAudioBufferTuple rendererBufferTuple)
{
    auto [glTFSource] = gltfSourceTuple;
    auto [rendererBuffer] = rendererBufferTuple;

    // TODO update values (uri)

    return true;
}

RendererAudioBufferTuple GltfAudioSourceMapping::onCreateMapping(GltfAudioSourceTuple gltfSourceTuple)
{
    auto [gltfSource] = gltfSourceTuple;

    const int16_t* decodedAudioData = gltfSource->getDecodedData();
    const audio::AudioChannels audioChannels = gltfSource->getAudioChannels();
    const float audioDurationInSec = gltfSource->getDurationInSec();
    const int audioHz = gltfSource->getHz();
    const size_t sampleCount = gltfSource->getSampleCount();

    auto audioBuffer = new AudioBuffer;
    alGenBuffers(1, &audioBuffer->buffer);

    audioBuffer->durInSec = audioDurationInSec;

    if (audioChannels == audio::AudioChannels::Mono) {
        alBufferData(audioBuffer->buffer, AL_FORMAT_MONO16, decodedAudioData, sampleCount * sizeof(int16_t), audioHz);
    } else if (audioChannels == audio::AudioChannels::Stereo) {
        alBufferData(audioBuffer->buffer, AL_FORMAT_STEREO16, decodedAudioData, sampleCount * sizeof(int16_t), audioHz);
    } else { // invalid
        if (audioBuffer->buffer && alIsBuffer(audioBuffer->buffer)) {
            alDeleteBuffers(1, &audioBuffer->buffer);
            audioBuffer->errorOnLoading = true;
        }
    }

    if (alGetError() != AL_NO_ERROR) {
        if (audioBuffer->buffer && alIsBuffer(audioBuffer->buffer)) {
            alDeleteBuffers(1, &audioBuffer->buffer);
            audioBuffer->errorOnLoading = true;
        }
    }

    RendererAudioBufferTuple tuple = { audioBuffer };
    onUpdateMapping(gltfSourceTuple, tuple);

    return tuple;
}

void GltfAudioSourceMapping::onDestroyMapping(GltfAudioSourceTuple gltfSourceTuple, RendererAudioBufferTuple audioBufferTuple)
{
    auto [glTFSource] = gltfSourceTuple;
    auto [audioBuffer] = audioBufferTuple;

    alDeleteBuffers(1, &audioBuffer->buffer);
    delete audioBuffer;
}
