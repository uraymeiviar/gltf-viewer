// Copyright © 2022 UX3D GmbH. All rights reserved.
// Authors:  Phillip Hohenester

#pragma once

#include "GltfMapping.h"

#include <tuple>

namespace ux3d::gltf {
class AudioSource;
}

namespace ux3d::renderer {
struct AudioBuffer;

using GltfAudioSourceTuple = std::tuple<const ux3d::gltf::AudioSource*>;
using RendererAudioBufferTuple = std::tuple<AudioBuffer*>;
class GltfAudioSourceMapping : public GltfMapping<GltfAudioSourceTuple, RendererAudioBufferTuple> {
public:
    GltfAudioSourceMapping(GltfRenderLayer& scene);

protected:
    bool onUpdateMapping(GltfAudioSourceTuple gltfSourceTuple, RendererAudioBufferTuple rendererBufferTuple) final;
    RendererAudioBufferTuple onCreateMapping(GltfAudioSourceTuple gltfSourceTuple) final;
    void onDestroyMapping(GltfAudioSourceTuple gltfSourceTuple, RendererAudioBufferTuple rendererBufferTuple) final;
};
}
