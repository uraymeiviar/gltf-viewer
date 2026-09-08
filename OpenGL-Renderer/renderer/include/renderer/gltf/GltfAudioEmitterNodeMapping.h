// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: David Labode

#pragma once

#include "GltfMapping.h"

#include <tuple>

namespace ux3d::gltf {
class Node;
class AudioEmitter;
}

namespace ux3d::renderer {
struct AudioEmitter;

using GltfAudioEmitterNodeTuple = std::tuple<const gltf::Node*, const gltf::AudioEmitter*>;
using RendererAudioEmitterTuple = std::tuple<AudioEmitter*>;
class GltfAudioEmitterNodeMapping : public GltfMapping<GltfAudioEmitterNodeTuple, RendererAudioEmitterTuple> {
public:
    GltfAudioEmitterNodeMapping(GltfRenderLayer& scene);

protected:
    bool onUpdateMapping(GltfAudioEmitterNodeTuple gltfTuple, RendererAudioEmitterTuple rendererTuple) final;
    RendererAudioEmitterTuple onCreateMapping(GltfAudioEmitterNodeTuple gltfTuple) final;
    void onDestroyMapping(GltfAudioEmitterNodeTuple gltfTuple, RendererAudioEmitterTuple rendererTuple) final;
};
}
