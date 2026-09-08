#include "gltf/GlbBinaryResolver.h"
#include "core/Logger.h"
#include "core/String.h"
#include "gltf/Accessor.h"
#include "gltf/AudioSource.h"
#include "gltf/Buffer.h"
#include "gltf/BufferView.h"
#include "gltf/Image.h"
#include "gltf/ResourceCollector.h"
#include "gltf/StorageTypeConverter.h"
#include "gltf/json/BufferMerger.h"

using namespace ux3d;
using namespace ux3d::gltf;

void GlbBinaryResolver::writeBinaryData(nlohmann::json& _jsonRoot, core::Vector<uint8_t>& _mainBufferData)
{
    // delete all buffers and bufferviews in root json file
    clearJsonBuffers(_jsonRoot);

    BufferMerger bufferMerger(_mainBufferData, _jsonRoot, m_exportOptions);

    core::Vector<ResourceCollector::GenericResource>& genericResources = m_resourceCollector.getGenericResources();
    for (auto& genericResource : genericResources) {
        if (genericResource.bufferView->getBuffer() == nullptr) {
            continue;
        }

        // we do not want to store an inactive draco compressed buffer
        if (genericResource.bufferView->getBuffer()->getStorageFormat() != StorageFormat::NotStored) {
            bufferMerger.add(genericResource);
        }
    }

    core::Vector<const Accessor*>& accessors = m_resourceCollector.getAccessors();
    for (uint32_t index = 0; index < accessors.size(); index++) {
        if (accessors[index]->getStorageFormat() != StorageFormat::NotStored) {
            bufferMerger.add(*accessors[index], index);
        }
    }

    core::Vector<const Image*>& images = m_resourceCollector.getImages();
    for (uint32_t index = 0; index < images.size(); index++) {
        if (images[index]->getStorageFormat() != StorageFormat::NotStored) {
            bufferMerger.add(*images[index], index);
        }
    }

    core::Vector<const AudioSource*>& audioSources = m_resourceCollector.getAudioSources();
    for (uint32_t index = 0; index < audioSources.size(); index++) {
        if (audioSources[index]->getStorageFormat() != StorageFormat::NotStored) {
            bufferMerger.add(*audioSources[index], index);
        }
    }

    if (core::Failed(bufferMerger.appendMergedBuffer())) {
        core::logError("GlbBinaryResolver: not able to create merged buffer.");
    }

    if (_jsonRoot.contains("buffers") && _jsonRoot["buffers"].is_array() && _jsonRoot["buffers"].size() > 0) {
        if (_jsonRoot["buffers"][0].contains("uri")) {
            _jsonRoot["buffers"][0].erase("uri");
        }
    }
}

void GlbBinaryResolver::clearJsonBuffers(nlohmann::json& _jsonRoot)
{
    if (_jsonRoot.contains("buffers")) {
        _jsonRoot["buffers"] = nlohmann::json::array();
    }
    if (_jsonRoot.contains("bufferViews")) {
        _jsonRoot["bufferViews"] = nlohmann::json::array();
    }
}
