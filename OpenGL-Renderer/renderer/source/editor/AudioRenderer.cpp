// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: David Labode

#include "renderer/editor/AudioRenderer.h"
#include "core/Logger.h"
#include "gltf/AudioEmitter.h"
#include "gltf/AudioSource.h"
#include "gltf/Gltf.h"
#include "gltf/Node.h"
#include "gltf/Scene.h"
#include "math/VectorFunctions.h"
#include "renderer/gltf/GltfRenderLayer.h"

#include <AL/al.h>
#include <AL/alext.h>

using namespace ux3d;
using namespace ux3d::renderer;
using namespace std::chrono_literals;

static const float M_2PI = 2.0f * math::pi<float>();

AudioRenderer::AudioRenderer(GltfRenderLayer& gltfLayer)
    : m_gltfLayer(gltfLayer)
    , m_audioSourceMapping(gltfLayer)
    , m_audioEmitterNodeMapping(gltfLayer)
    , m_audioEmitterSceneMapping(gltfLayer)
{
    if (!setupAudio()) {
        shutdownAudio();
    }

    m_devicePoller = std::thread([this]() {
        while (m_pollDevices) {
            std::this_thread::sleep_for(1s);
            pollDefaultDevice();
        }
    });
}

AudioRenderer::~AudioRenderer()
{
    m_pollDevices = false;
    m_devicePoller.join();
    shutdownAudio();
}

bool AudioRenderer::setupAudio()
{
    m_device = alcOpenDevice(nullptr);
    if (!m_device) {
        return false;
    }

    // TODO display actual default device to user
    // log device name for developer
    const ALCchar* deviceName = alcGetString(m_device, ALC_DEVICE_SPECIFIER);
    core::debugLog(core::LogLevel::Info, "Gestaltor is using this audio device: %s", deviceName);

    m_context = alcCreateContext(m_device, nullptr);
    if (!m_context) {
        return false;
    }

    if (alcMakeContextCurrent(m_context) == ALC_FALSE) {
        return false;
    }

    // KHR_audio requires a distance model per source, so we do it manually
    alDistanceModel(AL_NONE);

    ALenum error = alGetError();
    if (error != AL_NO_ERROR) {
        core::log(core::LogLevel::Error, "Audio renderer failed to initialize with: %s.", alGetString(error));

        return false;
    }

    return true;
}

void AudioRenderer::pollDefaultDevice()
{
    // poll all devices since this seems to trigger an update of the default device
    alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);

    // poll actual current default device
    const char* defaultDevice = (char*)alcGetString(m_device, ALC_DEFAULT_ALL_DEVICES_SPECIFIER);
    std::string newDefaultDevice(defaultDevice);

#ifdef UX3D_IS_WINDOWS
    // on Windows we get all device names with a prefix: "OpenAL Soft on <Audio Device Name>"
    // we can use this device name without the prefix

    newDefaultDevice = newDefaultDevice.substr(15, std::string::npos);
#endif

    // if default device has changed, trigger device change on main thread
    if (m_currentDefaultDevice != newDefaultDevice) {
        m_reopenDefaultDevice = true;
        core::logInfo("Audio device was changed to %s", newDefaultDevice.c_str());
        m_currentDefaultDevice = newDefaultDevice;
    }
}

void AudioRenderer::shutdownAudio()
{
    if (m_context) {
        alcMakeContextCurrent(nullptr);

        alcDestroyContext(m_context);
    }

    if (m_device) {
        alcCloseDevice(m_device);
    }
}

void AudioRenderer::updateScene(const gltf::Gltf& _glTF, const gltf::Scene& _scene)
{
    updateAudioSources(_glTF.getAudioSources());
    updateNodeAudioEmitters(_scene.getNodes());
    updateSceneAudioEmitters(_scene);
}

void AudioRenderer::updateAudioSources(const core::LinkedList<gltf::AudioSource>& audioSources)
{
    core::LinkedList<std::tuple<const gltf::AudioSource*>> gltfTuples;
    for (const gltf::AudioSource& glTFAudioSource : audioSources) {
        gltfTuples.emplace_back(&glTFAudioSource);
    }
    m_audioSourceMapping.update(gltfTuples);
}

void AudioRenderer::updateNodeAudioEmitters(const core::Vector<gltf::Node*>& emitterNodes)
{
    core::LinkedList<std::tuple<const gltf::Node*, const ux3d::gltf::AudioEmitter*>> gltfTuples;

    std::function<void(const gltf::Node*)> gatherNodesRecursive = [&](const gltf::Node* _glTFNode) {
        const gltf::AudioEmitter* pGltfEmitter = _glTFNode->getAudioEmitter();
        if (pGltfEmitter != nullptr && _glTFNode->getVisible()) {
            gltfTuples.emplace_back(_glTFNode, pGltfEmitter);
        }

        for (const gltf::Node* glTFChild : _glTFNode->getChildren()) {
            gatherNodesRecursive(glTFChild);
        }
    };

    for (const gltf::Node* glTFNode : emitterNodes) {
        gatherNodesRecursive(glTFNode);
    }

    m_audioEmitterNodeMapping.update(gltfTuples);
}

void AudioRenderer::updateSceneAudioEmitters(const gltf::Scene& scene)
{
    core::LinkedList<std::tuple<const gltf::Scene*, const gltf::AudioEmitter*>> gltfTuples;

    if (scene.hasAudioEmitters()) {
        const std::set<gltf::AudioEmitter*>& audioEmitters = scene.getAudioEmitters();
        for (gltf::AudioEmitter* audioEmitter : audioEmitters) {
            gltfTuples.emplace_back(&scene, audioEmitter);
        }
    }

    m_audioEmitterSceneMapping.update(gltfTuples);
}

void AudioRenderer::reopenDevice()
{
    static LPALCREOPENDEVICESOFT alcReopenDeviceSOFT = (LPALCREOPENDEVICESOFT)alcGetProcAddress(m_device, "alcReopenDeviceSOFT");
    static LPALCRESETDEVICESOFT alcResetDeviceSOFT = (LPALCRESETDEVICESOFT)alcGetProcAddress(m_device, "alcResetDeviceSOFT");
    if (alcIsExtensionPresent(m_device, "ALC_SOFT_reopen_device")) {
        if (alcReopenDeviceSOFT) {
            if (alcReopenDeviceSOFT(m_device, m_currentDefaultDevice.c_str(), nullptr)) {
                if (alcResetDeviceSOFT) {
                    if (alcResetDeviceSOFT(m_device, nullptr)) {
                        return;
                    } else {
                        core::logError("Could not reset audio device: %hs", m_currentDefaultDevice.c_str());
                    }
                }
            } else {
                core::logError("Could not reopen audio device: %hs, Error: %u", m_currentDefaultDevice.c_str(), alGetError());
            }
        }
    }
}

void AudioRenderer::update(float deltaTime)
{
    if (m_reopenDefaultDevice) {
        reopenDevice();
        m_reopenDefaultDevice = false;
    }

    if (m_isPlaying == false) {
        return;
    }

    math::vec3 listenerPosition = m_gltfLayer.getActiveCamera()->getPositon();
    math::vec3 listenerVelocity = (listenerPosition - m_lastListenerPosition) * deltaTime;
    // view vector seems to point exactly backwards so we multiply with -1 to flip it
    math::vec3 listenerForward = m_gltfLayer.getActiveCamera()->getView() * -1.0f;
    math::vec3 listenerUp = m_gltfLayer.getActiveCamera()->getUp();

    m_lastListenerPosition = listenerPosition;

    alListenerfv(AL_POSITION, math::value_ptr(listenerPosition));
    alListenerfv(AL_VELOCITY, math::value_ptr(listenerVelocity));
    ALfloat listenerOrientation[6] = { listenerForward.x, listenerForward.y, listenerForward.z, listenerUp.x, listenerUp.y, listenerUp.z };
    alListenerfv(AL_ORIENTATION, listenerOrientation);

    for (auto [gltfTuple, rendererTuple] : m_audioEmitterNodeMapping.map) {
        auto [gltfNode, gltfAudioEmitter] = gltfTuple;
        updateOpenALSource(gltfAudioEmitter, rendererTuple, gltfNode);
    }

    for (auto [gltfTuple, rendererTuple] : m_audioEmitterSceneMapping.map) {
        auto [gltfScene, gltfAudioEmitter] = gltfTuple;
        updateOpenALSource(gltfAudioEmitter, rendererTuple);
    }

    for (auto bufferID : m_buffersToBeDeleted) {
        alDeleteBuffers(1, &bufferID);
    }
    m_buffersToBeDeleted.clear();
}

void AudioRenderer::updateOpenALSource(const gltf::AudioEmitter* gltfAudioEmitter, RendererAudioEmitterTuple rendererTuple, const gltf::Node* gltfNode)
{
    AudioEmitter* audioEmitter = std::get<AudioEmitter*>(rendererTuple);

    float finalGain = audioEmitter->gain;

    // Positional audio emitter
    if (audioEmitter->type == AudioEmitter::Type::Positional) {

        if (gltfNode != nullptr) {
            // update translation and orientation of audio emitter
            auto nodeIt = m_gltfLayer.m_transformHierarchyMapping.map.find(gltfNode);
            if (nodeIt != m_gltfLayer.m_transformHierarchyMapping.map.end()) {
                TransformHierarchyNode* rendererNode = nodeIt->second;
                audioEmitter->updateTransform(rendererNode->getWorldTransform());
            }
        }
        // Apply distance model
        ALfloat distance = math::distance(audioEmitter->position, m_lastListenerPosition);

        if (audioEmitter->distanceModel == AudioEmitter::DistanceModel::Linear) {
            finalGain *= 1.0f - audioEmitter->rollofFactor * (distance - audioEmitter->refDistance) / (audioEmitter->maxDistance - audioEmitter->refDistance);
        } else if (audioEmitter->distanceModel == AudioEmitter::DistanceModel::Inverse) {
            finalGain *= audioEmitter->refDistance / (audioEmitter->refDistance + audioEmitter->rollofFactor * (math::max(distance, audioEmitter->refDistance) - audioEmitter->refDistance));
        } else if (audioEmitter->distanceModel == AudioEmitter::DistanceModel::Exponential) {
            finalGain *= powf(math::max(distance, audioEmitter->refDistance) / audioEmitter->refDistance, -audioEmitter->rollofFactor);
        }

        // Apply cone gain
        float coneGain = 1.0f;
        if (math::length(audioEmitter->orientation) != 0.0f && ((audioEmitter->coneInnerAngle != M_2PI) || (audioEmitter->coneOuterAngle != M_2PI))) {
            // Take sound cone into account

            math::vec3 sourceToListener = math::normalize(math::vec3(m_lastListenerPosition - audioEmitter->position));
            math::vec3 normalizedSourceOrientation = math::normalize(audioEmitter->orientation);

            float angle = acosf(math::dot(sourceToListener, normalizedSourceOrientation));
            float absAngle = fabs(angle);

            float absInnerAngle = fabs(audioEmitter->coneInnerAngle) * 0.5f;
            float absOuterAngle = fabs(audioEmitter->coneOuterAngle) * 0.5f;

            if (absAngle <= absInnerAngle) {
                // No attenuation
                coneGain = 1.0;
            } else if (absAngle >= absOuterAngle) {
                // Max attenuation
                coneGain = audioEmitter->coneOuterGain; // coneOuterGain is in range [0..1]
            } else {
                // Between inner and outer cones
                // inner -> outer, x goes from 0 -> 1
                float x = (absAngle - absInnerAngle) / (absOuterAngle - absInnerAngle);

                coneGain = (1.0f - x) + audioEmitter->coneOuterGain * x;
            }
        }
        finalGain *= coneGain; // coneGain is in range [0..1]
    }

    ALenum playState;
    alGetSourcei(audioEmitter->source, AL_SOURCE_STATE, &playState);

    ALint bufferIndex = -1;
    const gltf::AudioSource* gltfAudioSource = gltfAudioEmitter->getSource();

    // if source was deleted gather buffer to be deleted later
    if (gltfAudioSource == nullptr) {
        if (audioEmitter->buffer != 0) {
            m_buffersToBeDeleted.insert(audioEmitter->buffer);
            if (playState == AL_PLAYING) {
                alSourceStop(audioEmitter->source);
                alSourcei(audioEmitter->source, AL_BUFFER, 0);
                audioEmitter->buffer = 0;
            }
        }
        return;
    }

    auto audioSourceMapping = m_audioSourceMapping.map.find(gltfAudioSource);
    auto [audioBuffer] = audioSourceMapping->second;
    if (audioBuffer != nullptr && !audioBuffer->errorOnLoading) {
        bufferIndex = (ALint)audioBuffer->buffer;
    }

    if (bufferIndex > -1) {
        ALenum oldBufferIndex;
        alGetSourcei(audioEmitter->source, AL_BUFFER, &oldBufferIndex);

        // set new buffer
        if (oldBufferIndex != bufferIndex) {
            // if already playing stop and start again
            if (playState == AL_PLAYING) {
                alSourceStop(audioEmitter->source);
                alGetSourcei(audioEmitter->source, AL_SOURCE_STATE, &playState);
            }
            alSourcei(audioEmitter->source, AL_BUFFER, bufferIndex);
            audioEmitter->buffer = bufferIndex;
        }

        // play if not playing and should play now
        if (playState != AL_PLAYING && audioEmitter->playing) {
            // start if not yet played
            if (audioEmitter->hasPlayed == false || audioEmitter->isPaused) {
                alSourcePlay(audioEmitter->source);
                audioEmitter->hasPlayed = true;
                audioEmitter->isPaused = false;
            } else if (audioEmitter->loop) {
                // also start if loop is true and we are not playing
                alSourcePlay(audioEmitter->source);
            }
        } else if (playState == AL_PLAYING && !audioEmitter->playing) {
            // stop if playing and should not play anymore
            alSourceStop(audioEmitter->source);
            audioEmitter->hasPlayed = false;
        }
    } else {
        // stop if bufferindex invalid
        alSourceStop(audioEmitter->source);
        audioEmitter->hasPlayed = false;
    }

    alSourcef(audioEmitter->source, AL_LOOPING, audioEmitter->loop);
    alSourcefv(audioEmitter->source, AL_POSITION, math::value_ptr(audioEmitter->position));

    if (m_isMute) {
        alSourcef(audioEmitter->source, AL_GAIN, 0.0f);
    } else {
        alSourcef(audioEmitter->source, AL_GAIN, finalGain);
    }
}

void AudioRenderer::play()
{
    m_isPlaying = true;
}

void AudioRenderer::stop()
{
    for (auto [gltfTuple, rendererTuple] : m_audioEmitterNodeMapping.map) {
        auto [audioEmitter] = rendererTuple;
        alSourceStop(audioEmitter->source);
        audioEmitter->hasPlayed = false;
        audioEmitter->isPaused = false;
    }

    for (auto [gltfTuple, rendererTuple] : m_audioEmitterSceneMapping.map) {
        auto [audioEmitter] = rendererTuple;
        alSourceStop(audioEmitter->source);
        audioEmitter->hasPlayed = false;
        audioEmitter->isPaused = false;
    }

    m_isPlaying = false;
}

void AudioRenderer::rewind()
{
    for (auto [gltfTuple, rendererTuple] : m_audioEmitterNodeMapping.map) {
        auto [audioEmitter] = rendererTuple;
        alSourceRewind(audioEmitter->source);
        audioEmitter->hasPlayed = false;
        audioEmitter->isPaused = false;
    }

    for (auto [gltfTuple, rendererTuple] : m_audioEmitterSceneMapping.map) {
        auto [audioEmitter] = rendererTuple;
        alSourceRewind(audioEmitter->source);
        audioEmitter->hasPlayed = false;
        audioEmitter->isPaused = false;
    }

    m_isPlaying = false;
}

void AudioRenderer::pause()
{
    for (auto [gltfTuple, rendererTuple] : m_audioEmitterNodeMapping.map) {
        auto [audioEmitter] = rendererTuple;

        ALenum playState;
        alGetSourcei(audioEmitter->source, AL_SOURCE_STATE, &playState);

        if (playState == AL_PLAYING) {
            alSourcePause(audioEmitter->source);
            audioEmitter->isPaused = true;
        }
    }

    for (auto [gltfTuple, rendererTuple] : m_audioEmitterSceneMapping.map) {
        auto [audioEmitter] = rendererTuple;

        ALenum playState;
        alGetSourcei(audioEmitter->source, AL_SOURCE_STATE, &playState);

        if (playState == AL_PLAYING) {
            alSourcePause(audioEmitter->source);
            audioEmitter->isPaused = true;
        }
    }

    m_isPlaying = false;
}

void AudioRenderer::toggleMute(bool unmute)
{
    if (unmute) {
        m_isMute = false;
    } else {
        m_isMute = true;
    }
}

void AudioRenderer::setPlayBackTime(float t)
{
    for (auto [gltfTuple, rendererTuple] : m_audioEmitterNodeMapping.map) {
        auto [gltfNode, gltfAudioEmitter] = gltfTuple;
        auto [audioEmitter] = rendererTuple;

        const gltf::AudioSource* gltfAudioSource = gltfAudioEmitter->getSource();
        auto audioSourceMapping = m_audioSourceMapping.map.find(gltfAudioSource);
        auto [audioBuffer] = audioSourceMapping->second;

        if (audioBuffer->durInSec < t) {
            if (audioEmitter->loop) {
                t = fmodf(audioBuffer->durInSec, t);

            } else {
                t = audioBuffer->durInSec;
            }
        }

        alSourcef(audioEmitter->source, AL_SEC_OFFSET, t);
        audioEmitter->hasPlayed = false;
        audioEmitter->isPaused = false;
    }

    for (auto [gltfTuple, rendererTuple] : m_audioEmitterSceneMapping.map) {
        auto [gltfNode, gltfAudioEmitter] = gltfTuple;
        auto [audioEmitter] = rendererTuple;

        const gltf::AudioSource* gltfAudioSource = gltfAudioEmitter->getSource();
        auto audioSourceMapping = m_audioSourceMapping.map.find(gltfAudioSource);
        auto [audioBuffer] = audioSourceMapping->second;

        if (audioBuffer->durInSec < t) {
            if (audioEmitter->loop) {
                t = fmodf(audioBuffer->durInSec, t);

            } else {
                t = audioBuffer->durInSec;
            }
        }

        alSourcef(audioEmitter->source, AL_SEC_OFFSET, t);
        audioEmitter->hasPlayed = false;
        audioEmitter->isPaused = false;
    }
}
