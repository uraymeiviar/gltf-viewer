// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "AudioSource.h"
#include "ChildOfRoot.h"
#include "GltfObject.h"
#include <optional>

namespace ux3d::gltf {

class AudioEmitterPositional : public GltfObject {
public:
    enum class EmitterDistanceModel : uint8_t {
        Linear,
        Inverse,
        Exponential
    };

public:
    double getConeInnerAngle() const { return m_coneInnerAngle.value_or(6.283185307179586); }
    bool hasConeInnerAngle() const { return m_coneInnerAngle.has_value(); }
    void setConeInnerAngle(double coneInnerAngle);
    void unsetConeInnerAngle() { m_coneInnerAngle.reset(); }

    double getConeOuterAngle() const { return m_coneOuterAngle.value_or(6.283185307179586); }
    bool hasConeOuterAngle() const { return m_coneOuterAngle.has_value(); }
    void setConeOuterAngle(double coneOuterAngle);
    void unsetConeOuterAngle() { m_coneOuterAngle.reset(); }

    double getConeOuterGain() const { return m_coneOuterGain.value_or(0.0); }
    bool hasConeOuterGain() const { return m_coneOuterGain.has_value(); }
    void setConeOuterGain(double coneOuterGain);
    void unsetConeOuterGain() { m_coneOuterGain.reset(); }

    EmitterDistanceModel getDistanceModel() const { return m_distanceModel.value_or(EmitterDistanceModel::Inverse); }
    bool hasDistanceModel() const { return m_distanceModel.has_value(); }
    void setDistanceModel(EmitterDistanceModel _distanceModel) { m_distanceModel = _distanceModel; }
    void unsetDistanceModel() { m_distanceModel.reset(); }

    double getMaxDistance() const { return m_maxDistance.value_or(10000.0); }
    bool hasMaxDistance() const { return m_maxDistance.has_value(); }
    void setMaxDistance(double maxDistance);
    void unsetMaxDistance() { m_maxDistance.reset(); }

    double getRefDistance() const { return m_refDistance.value_or(1.0); }
    bool hasRefDistance() const { return m_refDistance.has_value(); }
    void setRefDistance(double refDistance);
    void unsetRefDistance() { m_refDistance.reset(); }

    double getRolloffFactor() const { return m_rollofFactor.value_or(1.0); }
    bool hasRolloffFactor() const { return m_rollofFactor.has_value(); }
    void setRolloffFactor(double rolloffFactor);
    void unsetRolloffFactor() { m_rollofFactor.reset(); }

private:
    std::optional<double> m_coneInnerAngle;
    std::optional<double> m_coneOuterAngle;
    std::optional<double> m_coneOuterGain;
    std::optional<EmitterDistanceModel> m_distanceModel;
    std::optional<double> m_maxDistance;
    std::optional<double> m_refDistance;
    std::optional<double> m_rollofFactor;
};

class AudioEmitter : public ChildOfRoot {

public:
    enum class EmitterType : uint8_t {
        Positional = 0u,
        Global
    };

public:
    AudioEmitter(uint32_t id); // regular constructor
    AudioEmitter(uint32_t id, EmitterType type); // regular constructor
    AudioEmitter(uint32_t id, const AudioEmitter& source); // duplicate in same glTF

    GltfType getGltfType() const override { return GltfType::AudioEmitter; }

    EmitterType getType() { return m_type; }
    EmitterType getType() const { return m_type; }
    void setType(EmitterType type);

    double getGain() const { return m_gain.value_or(1.0); }
    bool hasGain() const { return m_gain.has_value(); }
    void setGain(double gain);
    void unsetGain() { m_gain.reset(); }

    bool isLoop() const { return m_loop.value_or(false); }
    bool hasLoop() const { return m_loop.has_value(); }
    void setLoop(bool loop) { m_loop = loop; }
    void unsetLoop() { m_loop.reset(); }

    bool isPlaying() const { return m_playing.value_or(false); }
    bool hasPlaying() const { return m_playing.has_value(); }
    void setPlaying(bool playing) { m_playing = playing; }
    void unsetPlaying() { m_playing.reset(); }

    AudioSource* getSource() { return m_source; }
    const AudioSource* getSource() const { return m_source; }
    void setSource(AudioSource* source) { m_source = source; }
    void unsetSource() { m_source = nullptr; }

    AudioEmitterPositional& getPositional() { return m_positional.value(); }
    const AudioEmitterPositional& getPositional() const { return m_positional.value(); }
    bool hasPositional() const { return m_positional.has_value(); }
    void createPositional();
    void unsetPositional() { m_positional.reset(); }

    Extensions getUsedExtensions() const override;

    bool isValid() const override;
    GltfResult getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const override;

private:
    EmitterType m_type = EmitterType::Global;
    std::optional<double> m_gain;
    std::optional<bool> m_loop;
    std::optional<bool> m_playing;
    std::optional<AudioEmitterPositional> m_positional;

    AudioSource* m_source = nullptr;
};

}
