// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ConversionResult.h"
#include "core/String.h"
#include "core/Vector.h"
#include "filesystem/File.h"
#include "gltf/ResourceCollector.h"
#include "nlohmann/json.hpp"
#include <optional>
#include <unordered_set>
#include <utility>

namespace ux3d::gltf {
struct ExportOptions;
class Accessor;
class Image;
class AudioSource;
class Buffer;

template <typename T>
class MemoryRange;

class BufferMerger {
    struct AccessorEntry {
        const Accessor* accessor;
        const uint32_t jsonID;
    };

    struct ImageEntry {
        const Image* image;
        const uint32_t jsonID;
    };

    struct AudioSourceEntry {
        const AudioSource* audioSource;
        const uint32_t jsonID;
    };

public:
    BufferMerger(core::Vector<uint8_t>& _bufferData, nlohmann::json& _jsonRoot,
        const ExportOptions& _exportOptions,
        std::optional<const filesystem::File> _gltfFile = std::nullopt)
        : m_bufferData(_bufferData)
        , m_jsonRoot(_jsonRoot)
        , m_gltfFile(_gltfFile)
        , m_exportOptions(_exportOptions)
    {
    }

    void add(const Image& _image, uint32_t _jsonImageEntryID);
    void add(const AudioSource& audioSource, uint32_t jsonAudioSourceEntryID);
    void add(const Accessor& _accessor, uint32_t _jsonAccessorEntryID);
    void add(const ResourceCollector::GenericResource& _resource);

    ConversionResult appendMergedBuffer(bool _makeEmbedded = false);

    core::string getBufferUri() const;

private:
    void createBuffer();
    void updateSingleBufferUri();
    bool isJsonBufferCreated();

    ConversionResult appendToBuffer(core::MemoryRange<const uint8_t>& _data, uint32_t& _outBufferOffset, uint32_t _requiredByteAlignment = 1u);
    ConversionResult appendBufferView(
        core::MemoryRange<const uint8_t>& _data,
        const BufferView* _bufferViewInfo,
        size_t& _outBufferViewIndex,
        uint32_t _requiredByteAlignment = 1u);

    ConversionResult appendAccessorData(const Accessor& _accessor, size_t& _outBufferViewIndex);
    ConversionResult appendImageData(const Image& _image, size_t& _outBufferViewIndex);
    ConversionResult appendAudioSourceData(const AudioSource& audioSource, size_t& _outBufferViewIndex);

    ConversionResult convertBufferToEmbedded();

    size_t m_bufferIndex = 0u;

    bool m_bufferValid = false;

    core::Vector<uint8_t>& m_bufferData;
    nlohmann::json& m_jsonRoot;
    std::optional<const filesystem::File> m_gltfFile;

    core::Vector<AccessorEntry> m_accessorList;
    core::Vector<ImageEntry> m_imageList;
    core::Vector<AudioSourceEntry> m_audioSourceList;

    core::Vector<ResourceCollector::GenericResource> m_genericResourceList;

    const ExportOptions& m_exportOptions;

    // all involved Buffers (from BufferViews) are collected in this vector
    // to gather all extras and unknown resources, this list is used in finalizeBuffer()
    std::unordered_set<const Buffer*> m_involvedBuffers;
    void finalizeBuffer();
};
}
