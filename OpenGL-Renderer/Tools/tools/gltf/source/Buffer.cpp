// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Buffer.h"
#include "filesystem/File.h"
#include "filesystem/FileSystem.h"
#include "filesystem/Path.h"
#include "gltf/EmbeddedData.h"
#include "gltf/Gltf.h"
#include "gltf/Utils.h"

using namespace ux3d;
using namespace ux3d::gltf;

ux3d::gltf::Buffer::Buffer(uint32_t _id)
    : ChildOfRoot(_id)
{
}

ux3d::gltf::Buffer::Buffer(uint32_t _id, const Buffer& _source)
    : ChildOfRoot(_id, _source)
    , m_historyDataIsDirty(true)
    , m_data(_source.m_data)
    , m_uri(_source.m_uri)
    , m_byteLength(_source.m_byteLength)
    , m_storageFormat(_source.m_storageFormat)
{
}

ux3d::gltf::GltfResult ux3d::gltf::Buffer::setData(core::MemoryRange<const uint8_t> _data)
{
    if (_data.size() == 0) {
        return GltfError::InvalidInput;
    }

    if (!m_data.assign(_data.begin(), _data.end())) {
        return GltfError::AllocationFailure;
    }

    m_historyDataIsDirty = true;

    m_byteLength = uint32_t(_data.size());

    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Buffer::getData(core::MemoryRange<const uint8_t>& _outData) const
{
    if (m_data.empty()) {
        return GltfError::ObjectNotFound;
    }

    _outData = m_data.getMemoryRange();
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Buffer::setUri(const core::string& _uri)
{
    if (_uri.empty()) {
        return GltfError::InvalidInput;
    }

    m_uri = _uri;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Buffer::setByteLength(uint32_t _byteLength)
{
    if (_byteLength == 0) {
        return GltfError::InvalidInput;
    }

    m_byteLength = _byteLength;
    return GltfResult::Success;
}

ux3d::gltf::GltfResult ux3d::gltf::Buffer::loadDataToMemory(const std::optional<filesystem::Dir>& _gltfDir)
{
    if (!m_data.empty()) {
        return GltfError::ObjectAlreadyExists;
    }

    if (getUri().empty()) {
        core::logError("Cannot resolve data for buffer with empty uri");
        m_data.resize(m_byteLength);
        return GltfError::ObjectNotFound;
    }

    core::string uri = getUri();

    if (embedded_data::isDataUri(uri)) {
        core::string mimeType = "";
        if (embedded_data::extractBase64Data(uri, mimeType, m_data) == ConversionResult::Success) {
            m_storageFormat = StorageFormat::Embedded;
            return GltfResult::Success;
        } else {
            m_data.resize(m_byteLength);
            return GltfError::ObjectNotFound;
        }
    }

    if (_gltfDir.has_value() == false) {
        core::logError("Cannot resolve data from files if no base directory is given");
        m_data.resize(m_byteLength);
        return GltfError::ObjectNotFound;
    }

    filesystem::File file = _gltfDir.value().getFilesystem().fromURI(uri);

    if (file.isRelative()) {
        file = _gltfDir.value() / file.toString();
    }

    if (filesystem::FileLoadResult result = file.load(m_data);
        core::Failed(result)) {
        if (result == filesystem::FileLoadResultE::AllocationFailed) {
            return GltfError::AllocationFailure;
        }

        core::logError("Cannot resolve data from file. File not found at location given by uri: %s", file.toString().c_str());
        m_data.resize(m_byteLength);
        return GltfError::ObjectNotFound;
    }

    m_storageFormat = StorageFormat::File;

    return GltfResult::Success;
}

bool ux3d::gltf::Buffer::isValid() const
{
    return m_data.size() > 0u && m_byteLength > 0u;
}

ux3d::gltf::GltfResult ux3d::gltf::Buffer::getInvalidProperties(core::Vector<core::string>& _invalidProperties) const
{
    if (m_data.empty() || m_byteLength == 0u) {
        if (_invalidProperties.emplace_back("byteLength") == nullptr) {
            return GltfError::AllocationFailure;
        }
    }

    return GltfResult::Success;
}

uint32_t Buffer::m_uniqueIdCounter { 1 };

std::string Buffer::createURI() const
{
    std::string newUri = "";
    newUri += "buffer_";
    newUri += std::to_string(m_uniqueIdCounter++);
    newUri += ".bin";
    return newUri;
}

std::string Buffer::createUniqueURI(const gltf::Gltf& gltf) const
{
    std::string uniqueUri = "";

    for (auto uri = createURI(); uri != uniqueUri; uri = createURI()) {
        if (gltf::utils::isURIUsed(uri, gltf) == false) {
            return uri;
        }
    }

    return uniqueUri;
}