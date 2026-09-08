// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRoot.h"
#include "core/MemoryRange.h"
#include "filesystem/Dir.h"
#include "gltf/StorageFormats.h"
#include <optional>

//See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#buffer

namespace ux3d::gltf {
class Buffer : public ChildOfRoot {
public:
    Buffer(uint32_t _id); // regular constructor
    Buffer(uint32_t _id, const Buffer& _source); // duplicate in same glTF

    GltfType getGltfType() const override { return GltfType::Buffer; }

    GltfResult setUri(const core::string& _uri);
    bool hasUri() const { return m_uri.has_value(); }
    core::string getUri() const { return m_uri.value_or(""); }

    GltfResult setByteLength(uint32_t _byteLength);
    uint32_t getByteLength() const { return m_byteLength; }

    GltfResult setData(core::MemoryRange<const uint8_t> _data);
    GltfResult getData(core::MemoryRange<const uint8_t>& _outData) const;

    StorageFormat getStorageFormat() const { return m_storageFormat; }
    void setStorageFormat(StorageFormat _storageFormat) { m_storageFormat = _storageFormat; }

    GltfResult loadDataToMemory(const std::optional<filesystem::Dir>& _gltfDir);

    bool isValid() const override;
    GltfResult getInvalidProperties(core::Vector<core::string>& _invalidProperties) const override;

    std::string createUniqueURI(const gltf::Gltf& gltf) const;
    std::string createURI() const;

public:
    // yes this is a crude hack, for until we remove the gltf module alltogether.
    // Reason: we somehow need to track if the data has changed to be able to only
    // store changed data in the undo history's data store
    bool m_historyDataIsDirty = true;
    static uint32_t m_uniqueIdCounter;

private:
    core::Vector<uint8_t> m_data;
    std::optional<core::string> m_uri;
    uint32_t m_byteLength = 0u;
    StorageFormat m_storageFormat = StorageFormat::Undefined;
};
}
