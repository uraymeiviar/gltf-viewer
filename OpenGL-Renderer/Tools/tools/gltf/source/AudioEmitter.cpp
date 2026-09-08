#include "gltf/AudioEmitter.h"

using namespace ux3d::gltf;

AudioEmitter::AudioEmitter(uint32_t id, EmitterType type)
    : ChildOfRoot(id)
    , m_type(type)
{
}
AudioEmitter::AudioEmitter(uint32_t id)
    : ChildOfRoot(id)
{
}

AudioEmitter::AudioEmitter(uint32_t id, const AudioEmitter& source)
    : ChildOfRoot(id, source)
    , m_type(source.m_type)
    , m_gain(source.m_gain)
    , m_loop(source.m_loop)
    , m_playing(source.m_playing)
    , m_positional(source.m_positional)
    , m_source(source.m_source)
{
}

void AudioEmitter::setType(EmitterType type)
{
    m_type = type;

    // positional is not allowed for type global -> remove it
    if (type == EmitterType::Global && hasPositional()) {
        unsetPositional();
    }
}

void AudioEmitter::setGain(double gain)
{
    if (gain >= 0.0f) {
        m_gain = gain;
    }
}

bool AudioEmitter::isValid() const
{
    return true;
}

GltfResult AudioEmitter::getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const
{
    return GltfResult::Success;
}

Extensions AudioEmitter::getUsedExtensions() const
{
    Extensions usedExtensions = Extension::Audio;
    return usedExtensions;
}

void AudioEmitter::createPositional()
{
    if (hasPositional()) {
        return;
    }

    m_positional = AudioEmitterPositional();
}

void AudioEmitterPositional::setConeInnerAngle(double coneInnerAngle)
{
    // enforce minimum and maximum
    if (coneInnerAngle < 0.0 || coneInnerAngle > 6.283185307179586) {
        return;
    }
    m_coneInnerAngle = coneInnerAngle;
}

void AudioEmitterPositional::setConeOuterAngle(double coneOuterAngle)
{
    // enforce minimum and maximum
    if (coneOuterAngle < 0.0 || coneOuterAngle > 6.283185307179586) {
        return;
    }
    m_coneOuterAngle = coneOuterAngle;
}

void AudioEmitterPositional::setConeOuterGain(double coneOuterGain)
{
    // enforce minimum and maximum
    if (coneOuterGain < 0.0 || coneOuterGain > 1.0) {
        return;
    }
    m_coneOuterGain = coneOuterGain;
}

void AudioEmitterPositional::setMaxDistance(double maxDistance)
{
    if (maxDistance <= 0.0) {
        return;
    }
    m_maxDistance = maxDistance;
}

void AudioEmitterPositional::setRefDistance(double refDistance)
{
    if (refDistance < 0.0) {
        return;
    }
    m_refDistance = refDistance;
}

void AudioEmitterPositional::setRolloffFactor(double rolloffFactor)
{
    if (rolloffFactor < 0.0) {
        return;
    }
    m_rollofFactor = rolloffFactor;
}