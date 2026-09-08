
#include "gltf/AudioSource.h"
#include "filesystem/File.h"
#include "filesystem/FileSystem.h"
#include "filesystem/Path.h"
#include "gltf/BufferView.h"
#include "gltf/EmbeddedData.h"
#include "gltf/Utils.h"

using namespace ux3d::gltf;

Extensions AudioSource::getUsedExtensions() const
{
    Extensions usedExtensions = Extension::Audio;
    return usedExtensions;
}

bool AudioSource::isValid() const
{
    if (m_storageFormat == StorageFormat::File) {
        if (hasUri() == false) {
            return false;
        } else if (hasBufferView()) {
            return false;
        }
    }

    if (m_storageFormat == StorageFormat::Embedded) {
        if (hasBufferView()) {
            return false;
        }
    }

    if (m_storageFormat == StorageFormat::BufferView) {
        if (hasUri()) {
            return false;
        }

        if (hasMimeType() == false) {
            return false;
        }

        if (m_mimeType.value() != "audio/mpeg") {
            return false;
        }
    }

    return true;
}

GltfResult AudioSource::getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const
{
    if (m_mimeType.has_value() && m_mimeType.value() != "audio/mpeg") {
        if (_InvalidProperties.push_back("mimeType") == nullptr) {
            return GltfError::AllocationFailure;
        }
    }
    if (hasUri() && hasBufferView()) {
        if (_InvalidProperties.push_back("uri") == nullptr) {
            return GltfError::AllocationFailure;
        }
        if (_InvalidProperties.push_back("bufferView") == nullptr) {
            return GltfError::AllocationFailure;
        }
    }
    return GltfResult::Success;
}

void AudioSource::setUri(const std::string& uri)
{
    m_uri = uri;

    // audio source cant have uri and bufferview at the same time.
    unsetBufferView();
    // mime type is only alolowed when set to buffer view
    unsetMimeType();

    setStorageFormat(StorageFormat::File);
}

void AudioSource::setBufferView(BufferView& bufferView)
{
    m_bufferView = &bufferView;

    // set correct mime type
    m_mimeType = "audio/mpeg";

    // audio source cant have uri and bufferview at the same time.
    unsetUri();

    setStorageFormat(StorageFormat::BufferView);
};

void AudioSource::convertToStorageFormat(StorageFormat _storageFormat)
{
    if (_storageFormat == m_storageFormat) {
        return;
    }
    // set storage format also sets m_historyDataIsDirty to true
    // which is needed to avoid crahses in the state history when the new uri is not stored in there
    setStorageFormat(_storageFormat);

    if (_storageFormat == StorageFormat::File) {
        std::string newUri = "";
        if (m_Name.has_value()) {
            newUri = m_Name.value() + ".mp3";
        } else {
            newUri = createURI();
        }

        // clears mime type and buffer view
        setUri(newUri);
    } else {
        setMimeType("audio/mpeg");
        unsetUri();
    }
}

uint32_t AudioSource::m_uniqueIdCounter { 1 };

std::string AudioSource::createURI() const
{
    std::string newUri = "";
    if (m_storageFormat != StorageFormat::File) {
        return newUri;
    }
    newUri += "audioSource_";
    newUri += std::to_string(m_uniqueIdCounter++);
    newUri += ".mp3";
    return newUri;
}

std::string AudioSource::createUniqueURI(const gltf::Gltf& gltf) const
{
    std::string uniqueUri = "";

    for (auto uri = createURI(); uri != uniqueUri; uri = createURI()) {
        if (gltf::utils::isURIUsed(uri, gltf) == false) {
            return uri;
        }
    }

    return uniqueUri;
}

GltfResult AudioSource::loadDataToMemory(const std::optional<filesystem::Dir>& _gltfDir)
{
    if (m_data.getEncodedData().empty() == false) {
        return GltfError::ObjectAlreadyExists;
    }

    if (hasBufferView()) {
        setStorageFormat(StorageFormat::BufferView);
        return loadDataFromBufferView();
    }

    if (hasUri() && embedded_data::isDataUri(m_uri.value())) {
        setStorageFormat(StorageFormat::Embedded);
        return loadDataFromEmbedded();
    }

    if (hasUri() && m_uri.value().empty() == false) {
        if (_gltfDir.has_value()) {
            setStorageFormat(StorageFormat::File);
            loadDataFromUri(*_gltfDir);
            return GltfResult::Success;
        } else {
            core::logError("Cannot resolve data from files if no base directory is given");
        }
    }

    return GltfError::ObjectNotFound;
}

GltfResult AudioSource::loadDataFromBufferView()
{
    if (hasBufferView() == false) {
        return GltfError::ObjectNotFound;
    }

    core::MemoryRange<const uint8_t> bufferViewData;
    getBufferView()->getData(bufferViewData);

    if (bufferViewData.empty()) {
        return GltfError::ObjectNotFound;
    }

    core::Vector<uint8_t> rawData;
    if (rawData.assign(bufferViewData, nullptr) == false) {
        return GltfError::AllocationFailure;
    }

    m_data.update(rawData);
    return GltfResult::Success;
}

GltfResult AudioSource::loadDataFromUri(const filesystem::Dir& _gltfDir)
{
    if (m_uri.has_value() == false) {
        return GltfError::ObjectNotFound;
    }
    std::string uri = getUri();

    filesystem::File file = _gltfDir.getFilesystem().fromURI(uri);

    if (file.isRelative()) {
        file = _gltfDir / file.toString();
    }

    core::Vector<uint8_t> rawData;
    if (core::Failed(file.load(rawData))) {
        core::logError("Cannot resolve audio data from file. File not found at location given by uri: %s", file.toString().c_str());

        return GltfError::ObjectNotFound;
    }

    m_data.update(rawData);
    return GltfResult::Success;
}

GltfResult AudioSource::loadDataFromEmbedded()
{
    if (hasUri() == false) {
        return GltfError::ObjectNotFound;
    }

    core::string mimeType = "";
    core::Vector<uint8_t> rawData;
    embedded_data::extractBase64Data(getUri(), mimeType, rawData);

    m_data.update(rawData);
    return GltfResult::Success;
}
