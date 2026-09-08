// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: David Labode

#pragma once
#include "core/Vector.h"
#include "renderer/gltf/GltfAudioEmitterNodeMapping.h"
#include "renderer/gltf/GltfAudioEmitterSceneMapping.h"
#include "renderer/gltf/GltfAudioSourceMapping.h"

#include "math/VectorTypes.h"

#include <set>
#include <thread>

typedef unsigned int ALuint;

class ALCdevice;
class ALCcontext;

namespace ux3d::gltf {
class Scene;
class Gltf;
class AudioEmitter;
}

namespace ux3d::renderer {
class GltfRenderLayer;

class AudioRenderer {
public:
    AudioRenderer(GltfRenderLayer& gltfLayer);
    ~AudioRenderer();

    GltfRenderLayer& m_gltfLayer;

    void updateScene(const gltf::Gltf& _glTF, const gltf::Scene& _scene);
    void update(float deltaTime);

    void play();
    void stop();
    void rewind();
    void pause();
    void toggleMute(bool unmute);
    void setPlayBackTime(float t);

    math::vec3 m_lastListenerPosition;

private:
    bool setupAudio();
    void shutdownAudio();

    void updateAudioSources(const core::LinkedList<gltf::AudioSource>& audioSources);
    void updateNodeAudioEmitters(const core::Vector<gltf::Node*>& emitterNodes);
    void updateSceneAudioEmitters(const gltf::Scene& scene);

    void updateOpenALSource(const gltf::AudioEmitter* gltfAudioEmitter, RendererAudioEmitterTuple rendererTuple, const gltf::Node* gltfNode = nullptr);

    void pollDefaultDevice();
    void reopenDevice();

    GltfAudioSourceMapping m_audioSourceMapping;
    GltfAudioEmitterNodeMapping m_audioEmitterNodeMapping;
    GltfAudioEmitterSceneMapping m_audioEmitterSceneMapping;

    std::set<ALuint> m_buffersToBeDeleted; // gather all openal buffers for late deletion.

    // OpenAL
    ALCdevice* m_device = nullptr;
    ALCcontext* m_context = nullptr;

    bool m_isPlaying = false;
    bool m_isMute = false;

    // devicePoller is a thread for polling the default audio device every x seconds
    std::thread m_devicePoller;
    bool m_pollDevices = true;
    bool m_reopenDefaultDevice = false;
    std::string m_currentDefaultDevice = "";
};
}
