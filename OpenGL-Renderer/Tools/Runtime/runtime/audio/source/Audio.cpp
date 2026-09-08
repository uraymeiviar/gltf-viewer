// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "audio/Audio.h"
#include "core/Logger.h"

#define MINIMP3_ONLY_MP3
// #define MINIMP3_FLOAT_OUTPUT
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#include <minimp3_ex.h>

using namespace ux3d;
using namespace ux3d::audio;

Audio::Audio()
    : m_decodedData()
{
}

Audio::~Audio()
{
    free(m_decodedData);
}

Audio::Audio(const Audio& other)
    : m_audioChannels(other.m_audioChannels)
    , m_durationInSec(other.m_durationInSec)
    , m_hz(other.m_hz)
{
    update(other.m_encodedData);
}

Audio::Audio(Audio&& other)
    : m_audioChannels(other.m_audioChannels)
    , m_durationInSec(other.m_durationInSec)
    , m_hz(other.m_hz)
{
    update(other.m_encodedData);
}

Audio& Audio::operator=(const Audio& other)
{
    *this = Audio(other);
    return *this;
}

Audio& Audio::operator=(Audio&& other)
{
    m_audioChannels = other.m_audioChannels;
    m_durationInSec = other.m_durationInSec;
    m_hz = other.m_hz;
    m_decodedData = std::move(other.m_decodedData);
    m_encodedData = std::move(other.m_encodedData);
    return *this;
}

void Audio::update(const core::Vector<uint8_t>& encodedAudioData)
{
    // update encoded data
    m_encodedData = encodedAudioData;

    // update decoded  data
    mp3dec_t mp3Decoder;
    mp3dec_file_info_t info {};
    if (mp3dec_load_buf(&mp3Decoder, m_encodedData.data(), m_encodedData.size(), &info, NULL, NULL)) {
        core::logError("Could not load MP3");
        return;
    }
    m_hz = info.hz;
    m_sampleCount = info.samples;

    double duration = 0;
    mp3dec_iterate_buf(
        m_encodedData.data(), m_encodedData.size(), [](void* user_data, const uint8_t*, int, int, size_t, uint64_t, mp3dec_frame_info_t* info) -> int {
            double frameDuration = static_cast<double>(info->frame_bytes) / static_cast<double>(info->bitrate_kbps) / 1000.0 * 8.0;
            *reinterpret_cast<double*>(user_data) += frameDuration;
            return 0;
        },
        &duration);
    m_durationInSec = static_cast<float>(duration);

    // info.buffer points to memory that was allocated during decoding by minimp3
    m_decodedData = info.buffer;

    if (info.channels == 1) {
        m_audioChannels = AudioChannels::Mono;
    } else if (info.channels == 2) {
        m_audioChannels = AudioChannels::Stereo;
    } else {
        m_audioChannels = AudioChannels::Invalid;
    }

    return;
}

uint64_t Audio::getEncodedByteSize() const
{
    return m_encodedData.size();
}
