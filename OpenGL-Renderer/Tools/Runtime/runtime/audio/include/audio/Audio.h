// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Vector.h"

namespace ux3d::audio {

enum class AudioChannels {
    Invalid,
    Mono,
    Stereo
};

class Audio {
public:
    explicit Audio();

    virtual ~Audio();

    Audio(const Audio& _Other);
    Audio& operator=(const Audio& _Other);

    Audio(Audio&& _Other);
    Audio& operator=(Audio&& _Other);

    void update(const core::Vector<uint8_t>& encodedAudioData);

    const core::Vector<uint8_t>& getEncodedData() const { return m_encodedData; }
    const int16_t* getDecodedData() const { return m_decodedData; }
    AudioChannels getAudioChannels() const { return m_audioChannels; }
    float getDurationInSec() const { return m_durationInSec; }
    int getHz() const { return m_hz; }
    size_t getSampleCount() const { return m_sampleCount; }

    uint64_t getEncodedByteSize() const;

private:
    // decoded information
    AudioChannels m_audioChannels = AudioChannels::Invalid;
    float m_durationInSec = 0.0f;
    int m_hz = 0;
    int16_t* m_decodedData = nullptr;
    size_t m_sampleCount = 0;

    // encoded information
    core::Vector<uint8_t> m_encodedData;
};

}
