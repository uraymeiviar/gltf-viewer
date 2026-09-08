// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRoot.h"
#include "audio/Audio.h"
#include "filesystem/Dir.h"
#include "gltf/StorageFormats.h"
#include <optional>
#include <string>

namespace ux3d::gltf {
class BufferView;

class AudioSource : public ChildOfRoot {
public:
    AudioSource(uint32_t _id) // regular constructor
        : ChildOfRoot(_id)
    {
    }

    AudioSource(uint32_t _id, const AudioSource& source) // duplicate in same glTF
        : ChildOfRoot(_id, source)
        , m_data(source.m_data)
        , m_uri(source.m_uri)
        , m_bufferView(source.m_bufferView)
        , m_mimeType(source.m_mimeType)
        , m_storageFormat(source.m_storageFormat)
    {
    }

    GltfType getGltfType() const override { return GltfType::AudioSource; }

    const std::string getUri() const { return m_uri.value_or(""); }
    bool hasUri() const { return m_uri.has_value(); }
    void setUri(const std::string& uri);

    std::string getMimeType() const { return m_mimeType.value_or(""); }
    bool hasMimeType() const { return m_mimeType.has_value(); }
    void setMimeType(std::string mimeType) { m_mimeType = mimeType; }

    BufferView* getBufferView() { return m_bufferView.value_or(nullptr); }
    const BufferView* getBufferView() const { return m_bufferView.value_or(nullptr); }
    bool hasBufferView() const { return m_bufferView.has_value(); }
    void setBufferView(BufferView& bufferView);

    Extensions getUsedExtensions() const override;

    bool isValid() const override;
    GltfResult getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const override;

    // handling of binary data
    GltfResult loadDataToMemory(const std::optional<filesystem::Dir>& _gltfDir);

    StorageFormat getStorageFormat() const { return m_storageFormat; }
    void setStorageFormat(StorageFormat _storageFormat)
    {
        m_storageFormat = _storageFormat;
        m_historyDataIsDirty = true;
    }
    void convertToStorageFormat(StorageFormat _storageFormat);
    std::string createUniqueURI(const gltf::Gltf& gltf) const;

    const core::Vector<uint8_t>* getEncodedBytes() const { return &m_data.getEncodedData(); }
    GltfResult setEncodedBytes(core::Vector<uint8_t>& _data)
    {
        m_data.update(_data);
        m_historyDataIsDirty = true;
        return GltfResult::Success;
    }

    const int16_t* getDecodedData() const { return m_data.getDecodedData(); }
    audio::AudioChannels getAudioChannels() const { return m_data.getAudioChannels(); }
    float getDurationInSec() const { return m_data.getDurationInSec(); }
    int getHz() const { return m_data.getHz(); }
    size_t getSampleCount() const { return m_data.getSampleCount(); }
    uint64_t getEncodedByteSize() const { return m_data.getEncodedByteSize(); }

    // yes this is a crude hack, for until we remove the gltf module alltogether.
    // Reason: we somehow need to track if the data has changed to be able to only
    // store changed data in the undo history's data store
    bool m_historyDataIsDirty = true;

private:
    audio::Audio m_data = audio::Audio();

    std::optional<std::string> m_uri = std::nullopt;
    std::optional<BufferView*> m_bufferView = std::nullopt;
    std::optional<std::string> m_mimeType = std::nullopt;

    void unsetUri() { m_uri.reset(); }
    void unsetMimeType() { m_mimeType.reset(); }
    void unsetBufferView() { m_bufferView.reset(); }

    GltfResult loadDataFromBufferView();
    GltfResult loadDataFromUri(const filesystem::Dir& _gltfDir);
    GltfResult loadDataFromEmbedded();
    StorageFormat m_storageFormat = StorageFormat::Undefined;
    std::string createURI() const;
    static uint32_t m_uniqueIdCounter;
};
}
