// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: David Labode

#pragma once

#include "GltfMapping.h"

#include <tuple>

namespace ux3d::gltf {
class Scene;
class AudioEmitter;
}

namespace ux3d::renderer {
struct AudioEmitter;

using GltfAudioEmitterSceneTuple = std::tuple<const gltf::Scene*, const gltf::AudioEmitter*>;
using RendererAudioEmitterTuple = std::tuple<AudioEmitter*>;
class GltfAudioEmitterSceneMapping : public GltfMapping<GltfAudioEmitterSceneTuple, RendererAudioEmitterTuple> {
public:
    GltfAudioEmitterSceneMapping(GltfRenderLayer& scene);

protected:
    bool onUpdateMapping(GltfAudioEmitterSceneTuple gltfTuple, RendererAudioEmitterTuple rendererTuple) final;
    RendererAudioEmitterTuple onCreateMapping(GltfAudioEmitterSceneTuple gltfTuple) final;
    void onDestroyMapping(GltfAudioEmitterSceneTuple gltfTuple, RendererAudioEmitterTuple rendererTuple) final;
};
}
