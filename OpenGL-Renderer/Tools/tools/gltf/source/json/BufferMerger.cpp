#include "gltf/json/BufferMerger.h"
#include "core/MemoryRange.h"
#include "filesystem/FileSystem.h"
#include "gltf/Accessor.h"
#include "gltf/AudioSource.h"
#include "gltf/Buffer.h"
#include "gltf/BufferView.h"
#include "gltf/EmbeddedData.h"
#include "gltf/ExportOptions.h"
#include "gltf/Image.h"
#include "gltf/StorageTypeConverter.h"
#include "gltf/json/GltfObjectToJson.h"
#include "gltf/json/JsonPath.h"

using namespace ux3d;
using namespace ux3d::gltf;

void BufferMerger::add(const Image& _image, const uint32_t _jsonImageEntryID)
{
    m_imageList.emplace_back(ImageEntry { &_image, _jsonImageEntryID });
    m_bufferValid = false;
}

void BufferMerger::add(const AudioSource& audioSource, const uint32_t jsonAudioSourceEntryID)
{
    m_audioSourceList.emplace_back(AudioSourceEntry { &audioSource, jsonAudioSourceEntryID });
    m_bufferValid = false;
}

void BufferMerger::add(const Accessor& _accessor, const uint32_t _jsonAccessorEntryID)
{
    m_accessorList.emplace_back(AccessorEntry { &_accessor, _jsonAccessorEntryID });
    m_bufferValid = false;
}

void BufferMerger::add(const ResourceCollector::GenericResource& _resource)
{
    m_genericResourceList.emplace_back(_resource);
    m_bufferValid = false;
}

ConversionResult BufferMerger::appendMergedBuffer(bool _makeEmbedded)
{
    ConversionResult result = ConversionResult::Success;
    if (m_imageList.empty() && m_accessorList.empty() && m_genericResourceList.empty() && m_audioSourceList.empty()) {
        return result;
    }

    createBuffer();

    for (auto& entry : m_genericResourceList) {
        size_t bufferViewIndex = 0u;
        core::MemoryRange<const uint8_t> memoryRange;
        entry.bufferView->getData(memoryRange);
        result = appendBufferView(
            memoryRange,
            entry.bufferView,
            bufferViewIndex, 4u);

        if (core::Failed(result)) {
            core::logWarning("BufferMerger: appending of data failed");
        }

        nlohmann::json* jsonBufferViewIndex = JsonPathResolver::getJsonReference(entry.jsonBufferViewField, m_jsonRoot);
        if (jsonBufferViewIndex != nullptr) {
            *jsonBufferViewIndex = bufferViewIndex;
        } else {
            core::logWarning("BufferMerger: failed resolving index of BufferView");
        }
    }

    for (ImageEntry entry : m_imageList) {
        size_t bufferViewIndex = 0u;
        result = appendImageData(*entry.image, bufferViewIndex);

        if (core::Failed(result)) {
            core::logWarning("BufferMerger: appending of image failed");
        } else {
            m_jsonRoot["images"][entry.jsonID]["bufferView"] = bufferViewIndex;
            m_jsonRoot["images"][entry.jsonID].erase("uri");
        }
    }

    for (AudioSourceEntry entry : m_audioSourceList) {
        size_t bufferViewIndex = 0u;
        result = appendAudioSourceData(*entry.audioSource, bufferViewIndex);

        if (core::Failed(result)) {
            core::logWarning("BufferMerger: appending of audio source failed");
        } else {
            m_jsonRoot["extensions"]["KHR_audio"]["sources"][entry.jsonID]["bufferView"] = bufferViewIndex;
            m_jsonRoot["extensions"]["KHR_audio"]["sources"][entry.jsonID].erase("uri");
        }
    }

    for (AccessorEntry entry : m_accessorList) {
        size_t bufferViewIndex = 0u;
        result = appendAccessorData(*entry.accessor, bufferViewIndex);

        if (core::Failed(result)) {
            core::logWarning("BufferMerger: appending of accessor failed");
        } else {
            m_jsonRoot["accessors"][entry.jsonID]["bufferView"] = bufferViewIndex;
            m_jsonRoot["accessors"][entry.jsonID]["byteOffset"] = 0;
        }

        const auto& sparse = entry.accessor->getSparse();
        if (sparse.has_value()) {
            uint32_t requiredByteAlignment = static_cast<uint32_t>(entry.accessor->getByteLengthFrom(entry.accessor->getComponentType()));

            if (requiredByteAlignment == 0) {
                core::logWarning("BufferMerger: invalid byte alignment. Setting byte alignment: 4");
                requiredByteAlignment = 4;
            }

            const BufferView* indicesBufferView = sparse->getIndices().getBufferView();
            if (indicesBufferView != nullptr) {
                size_t indicesBufferViewIndex = 0u;
                core::MemoryRange<const uint8_t> memoryRange;
                indicesBufferView->getData(memoryRange);

                result = appendBufferView(memoryRange, indicesBufferView, indicesBufferViewIndex, requiredByteAlignment);

                if (core::Failed(result)) {
                    core::logWarning("BufferMerger: appending of sparse accessor indices failed");
                } else {
                    m_jsonRoot["accessors"][entry.jsonID]["sparse"]["indices"]["bufferView"] = indicesBufferViewIndex;
                    m_jsonRoot["accessors"][entry.jsonID]["sparse"]["indices"]["byteOffset"] = 0u;
                }
            }

            const BufferView* valuesBufferView = sparse->getValues().getBufferView();
            if (valuesBufferView != nullptr) {
                size_t valuesBufferViewIndex = 0u;
                core::MemoryRange<const uint8_t> memoryRange;
                valuesBufferView->getData(memoryRange);

                result = appendBufferView(memoryRange, valuesBufferView, valuesBufferViewIndex, requiredByteAlignment);

                if (core::Failed(result)) {
                    core::logWarning("BufferMerger: appending of sparse accessor values failed");
                } else {
                    m_jsonRoot["accessors"][entry.jsonID]["sparse"]["values"]["bufferView"] = valuesBufferViewIndex;
                    m_jsonRoot["accessors"][entry.jsonID]["sparse"]["values"]["byteOffset"] = 0u;
                }
            }
        }
    }

    if (m_bufferData.empty()) {
        core::logWarning("BufferMerger: empty buffer");
    }

    m_bufferValid = true;

    if (_makeEmbedded) {
        return convertBufferToEmbedded();
    }

    finalizeBuffer();

    if (m_involvedBuffers.size() == 1) {
        updateSingleBufferUri();
    }

    return ConversionResult::Success;
}

void BufferMerger::createBuffer()
{
    if (m_jsonRoot.contains("buffers") == false) {
        m_jsonRoot["buffers"] = nlohmann::json::array();
    }

    m_bufferIndex = m_jsonRoot["buffers"].size();

    core::string bufferUri = getBufferUri();

    nlohmann::json jsonBuffer;
    jsonBuffer["byteLength"] = 0;
    if (bufferUri != "") {
        jsonBuffer["uri"] = bufferUri;
    }

    m_jsonRoot["buffers"].emplace_back(jsonBuffer);

    m_bufferData.clear();
}

void BufferMerger::updateSingleBufferUri()
{
    if (!isJsonBufferCreated()) {
        return;
    }

    m_jsonRoot["buffers"][m_bufferIndex]["uri"] = getBufferUri();
}

bool BufferMerger::isJsonBufferCreated()
{
    if (m_jsonRoot.contains("buffers") == false) {
        return false;
    }
    if (m_jsonRoot["buffers"].size() <= m_bufferIndex) {
        return false;
    }
    if (m_jsonRoot["buffers"][m_bufferIndex].contains("byteLength") == false) {
        return false;
    }
    return true;
}

ConversionResult BufferMerger::appendToBuffer(
    core::MemoryRange<const uint8_t>& _data,
    uint32_t& _outBufferOffset,
    const uint32_t _requiredByteAlignment)
{

    if (!isJsonBufferCreated()) {
        return ConversionError::BufferInvalid;
    }

    uint32_t previousSize = m_jsonRoot["buffers"][m_bufferIndex]["byteLength"];

    if (m_bufferData.size() != previousSize) {
        core::logWarning("BufferMerger: inconsistent buffer size");
        return ConversionError::BufferInvalid;
    }

    if (_requiredByteAlignment == 0u) {
        core::logWarning("BufferMerger: invalid byte alignment: %d", _requiredByteAlignment);
        return ConversionError::InvalidArgument;
    }

    uint32_t offset = 0u;
    while (m_bufferData.size() % _requiredByteAlignment != 0) {
        m_bufferData.push_back(0u);
        offset++;
    }

    m_jsonRoot["buffers"][m_bufferIndex]["byteLength"] = m_bufferData.size() + _data.size();

    if (!m_bufferData.append(_data)) {
        core::logError("BufferMerger: cannot append data (size=%d) to main buffer (size=%d)", _data.size(), m_bufferData.size());
        return ConversionError::AllocationFailure;
    }

    _outBufferOffset = previousSize + offset;
    return ConversionResult::Success;
}

ConversionResult BufferMerger::appendBufferView(
    core::MemoryRange<const uint8_t>& _data,
    const BufferView* _bufferViewInfo,
    size_t& _outBufferViewIndex,
    uint32_t _requiredByteAlignment)
{
    ConversionResult result = ConversionResult::Success;

    if (_data.empty()) {
        return ConversionError::BufferInvalid;
    }

    if (m_jsonRoot.contains("bufferViews") == false) {
        m_jsonRoot["bufferViews"] = nlohmann::json::array();
    }

    uint32_t bufferOffset = 0u;
    result = appendToBuffer(_data, bufferOffset, _requiredByteAlignment);
    nlohmann::json jsonBufferView;

    jsonBufferView["buffer"] = m_bufferIndex;
    jsonBufferView["byteOffset"] = bufferOffset;
    jsonBufferView["byteLength"] = _data.byteSize();

    if (_bufferViewInfo != nullptr) {
        std::optional<uint32_t> byteStride = _bufferViewInfo->getByteStride();
        std::optional<uint32_t> target = _bufferViewInfo->getTarget();

        if (byteStride.has_value()) {
            jsonBufferView["byteStride"] = byteStride.value();
        }

        if (target.has_value()) {
            jsonBufferView["target"] = target.value();
        }

        GltfObjectToJson gltfObjectToJson(m_exportOptions);
        gltfObjectToJson.convertExtras(*_bufferViewInfo, jsonBufferView);
        gltfObjectToJson.convertUnknownExtensions(*_bufferViewInfo, jsonBufferView);

        m_involvedBuffers.insert(_bufferViewInfo->getBuffer());
    }

    m_jsonRoot["bufferViews"].emplace_back(jsonBufferView);

    _outBufferViewIndex = m_jsonRoot["bufferViews"].size() - 1;

    return result;
}

ConversionResult BufferMerger::appendAccessorData(const Accessor& _accessor, size_t& _outBufferViewIndex)
{
    core::MemoryRange<const uint8_t> memoryRange;
    GltfResult result = _accessor.getData(memoryRange);
    if (core::Failed(result)) {
        return ConversionError::BufferInvalid;
    }

    // The offset of an accessor into a bufferView(i.e., accessor.byteOffset) and
    // the offset of an accessor into a buffer(i.e., accessor.byteOffset + bufferView.byteOffset)
    // must be a multiple of the size of the accessor's component type.

    uint32_t requiredByteAlignment = static_cast<uint32_t>(_accessor.getByteLengthFrom(_accessor.getComponentType()));

    if (requiredByteAlignment == 0) {
        core::logWarning("BufferMerger: invalid byte alignment. Setting byte alignment: 4");
        requiredByteAlignment = 4;
    }

    const BufferView* bufferViewInfo = _accessor.getBufferView();

    return appendBufferView(memoryRange, bufferViewInfo, _outBufferViewIndex, requiredByteAlignment);
}

ConversionResult BufferMerger::appendImageData(const Image& _image, size_t& _outBufferViewIndex)
{
    if (_image.getEncodedBytes() == nullptr) {
        return ConversionError::InvalidVectorLength;
    }
    core::MemoryRange<const uint8_t> memoryRange = _image.getEncodedBytes()->getMemoryRange();

    return appendBufferView(memoryRange, nullptr, _outBufferViewIndex);
}

ConversionResult BufferMerger::appendAudioSourceData(const AudioSource& audioSource, size_t& _outBufferViewIndex)
{
    if (audioSource.getEncodedBytes() == nullptr) {
        return ConversionError::InvalidVectorLength;
    }
    core::MemoryRange<const uint8_t> memoryRange = audioSource.getEncodedBytes()->getMemoryRange();

    return appendBufferView(memoryRange, nullptr, _outBufferViewIndex);
}

ConversionResult BufferMerger::convertBufferToEmbedded()
{
    if (m_bufferData.empty()) {
        return ConversionResult::Success;
    }
    if (!m_jsonRoot.contains("buffers")) {
        return ConversionError::EmptyBuffer;
    }
    if (m_jsonRoot["buffers"].size() <= m_bufferIndex) {
        return ConversionError::EmptyBuffer;
    }
    if (m_bufferValid == false) {
        return ConversionError::BufferInvalid;
    }

    core::MemoryRange<const uint8_t> memoryRange;
    core::string embeddedData;
    memoryRange = m_bufferData.getMemoryRange();
    if (Failed(embedded_data::createBase64Data(memoryRange, "application/octet-stream", embeddedData))) {
        return ConversionError::EncodeFailure;
    }
    m_jsonRoot["buffers"][m_bufferIndex]["uri"] = embeddedData;

    return ConversionResult::Success;
}

void BufferMerger::finalizeBuffer()
{
    if (!isJsonBufferCreated()) {
        return;
    }

    uint32_t buffersWithExtras = 0;
    uint32_t buffersWithUnknownExtensions = 0;

    // Step 1: Analyze situation
    for (auto buffer : m_involvedBuffers) {
        if (buffer->getExtras().empty() == false) {
            buffersWithExtras++;
        }
        if (buffer->getUnknownExtensions().size() != 0) {
            buffersWithUnknownExtensions++;
        }
    }

    // Step 2: Warnings if we merge multiple extras/unknown extensions
    if (buffersWithExtras > 1) {
        core::logWarning("BufferMerger: merging multiple extra fields");
    }
    if (buffersWithUnknownExtensions > 1) {
        core::logWarning("BufferMerger: merging multiple unknown extensions");
    }

    // Step 3: Do it
    core::string extras = "";
    nlohmann::json& jsonBuffer = m_jsonRoot["buffers"][m_bufferIndex];

    for (auto buffer : m_involvedBuffers) {
        // Handle extras:
        core::string moreExtras = buffer->getExtras();
        if (!extras.empty() && !moreExtras.empty()) {
            extras += ", ";
        }
        extras += moreExtras;

        // Handle unknown extensions:

        GltfObjectToJson gltfObjectToJson(m_exportOptions);
        gltfObjectToJson.convertUnknownExtensions(*buffer, jsonBuffer);
    }

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(extras, jsonBuffer);
}

core::string BufferMerger::getBufferUri() const
{
    if (m_gltfFile.has_value()) {
        if (m_exportOptions.replaceOriginalBufferUri) {
            return m_gltfFile->getStem() + ".bin";
        }
        if (m_involvedBuffers.size() == 1 && (*m_involvedBuffers.begin())->hasUri()) {
            filesystem::File newFile = m_gltfFile.value().getFilesystem().fromURI((*m_involvedBuffers.begin())->getUri());
            if (newFile.isRelative()) {
                return newFile.toString();
            } else {
                // we do not store absolute paths, so just use files name
                return newFile.getName();
            }
        } else {
            return StorageTypeConverter::generateUri(static_cast<uint32_t>(m_bufferIndex), m_gltfFile.value(), nullptr, !m_exportOptions.writeToMemory);
        }
    } else {
        if (!m_exportOptions.writeToMemory && m_exportOptions.fileContainer != FileContainer::Glb) {
            core::logWarning("BufferMerger: no glTF file given. Cannot create URI");
        }
    }

    return "";
}
