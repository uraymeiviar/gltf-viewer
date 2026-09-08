#include "gltf/GltfBinaryResolver.h"
#include "core/Vector.h"
#include "gltf/Accessor.h"
#include "gltf/Buffer.h"
#include "gltf/BufferView.h"
#include "gltf/EmbeddedData.h"
#include "gltf/ExportOptions.h"
#include "gltf/Gltf.h"
#include "gltf/Image.h"
#include "gltf/ResourceCollector.h"
#include "gltf/StorageTypeConverter.h"
#include "gltf/json/BufferMerger.h"
#include "gltf/json/ResourceToFile.h"

using namespace ux3d;
using namespace ux3d::gltf;

ConversionResult GltfBinaryResolver::writeBinaryData(nlohmann::json& _jsonRoot, const Gltf& _gltf)
{
    ConversionResult result;

    if (_jsonRoot.contains("buffers")) {
        _jsonRoot["buffers"] = nlohmann::json::array();
    }
    if (_jsonRoot.contains("bufferViews")) {
        _jsonRoot["bufferViews"] = nlohmann::json::array();
    }

    std::optional<filesystem::File> oGltfFile(m_gltfFile);

    core::Vector<uint8_t> mergedData;
    BufferMerger bufferMerger(mergedData, _jsonRoot, m_exportOptions, oGltfFile);

    core::Vector<uint8_t> mergedEmbeddedData;
    BufferMerger bufferMergerEmbedded(mergedEmbeddedData, _jsonRoot, m_exportOptions, oGltfFile);

    core::Vector<ResourceCollector::GenericResource>& genericResources = m_resourceCollector.getGenericResources();

    for (auto& genericResource : genericResources) {
        if (genericResource.bufferView->getBuffer() == nullptr) {
            continue;
        }
        StorageFormat storageFormat = genericResource.bufferView->getBuffer()->getStorageFormat();

        if (storageFormat == StorageFormat::Embedded) {
            bufferMergerEmbedded.add(genericResource);
        }

        if (storageFormat == StorageFormat::File || storageFormat == StorageFormat::BufferView) {
            bufferMerger.add(genericResource);
        }

        if (storageFormat == StorageFormat::Undefined) {
            core::logError("GltfBinaryResolver: Buffer storage format invalid. Skipping Buffer.");
        }
    }

    core::Vector<const Accessor*>& accessors = m_resourceCollector.getAccessors();

    for (uint32_t accessorIndex = 0; accessorIndex < accessors.size(); accessorIndex++) {
        const Accessor* accessor = accessors[accessorIndex];
        StorageFormat storageFormat = accessor->getStorageFormat();

        if (storageFormat == StorageFormat::Undefined) {
            if (accessor->getBufferView() != nullptr) {
                storageFormat = accessor->getBufferView()->getBuffer()->getStorageFormat();
            }
        }

        if (storageFormat == StorageFormat::Undefined) {
            core::logError("GltfBinaryResolver: Accessor storage format invalid. Skipping Accessor.");
        }

        if (storageFormat == StorageFormat::File || storageFormat == StorageFormat::BufferView) {
            bufferMerger.add(*accessor, accessorIndex);
        }

        if (storageFormat == StorageFormat::Embedded) {
            bufferMergerEmbedded.add(*accessor, accessorIndex);
        }

        if (storageFormat == StorageFormat::NotStored) {
            // everything fine. Draco resources are included via GenericResources
        }
    }

    core::Vector<const Image*>& images = m_resourceCollector.getImages();
    for (uint32_t imageIndex = 0; imageIndex < images.size(); imageIndex++) {
        const Image* image = images[imageIndex];
        if (image->getStorageFormat() == StorageFormat::File) {
            core::string imageUri = StorageTypeConverter::generateUri(*image, imageIndex, m_gltfFile);
            _jsonRoot["images"][imageIndex]["uri"] = imageUri;

            if (images[imageIndex]->getData().getPixels().empty()) {
                _jsonRoot["images"][imageIndex]["uri"] = "";
            }

            _jsonRoot["images"][imageIndex].erase("bufferView");

            ResourceToFile imageWriter(m_gltfFile.getParent() / imageUri, m_exportOptions);
            result = core::Max(imageWriter.write(*image), result);
        } else if (image->getStorageFormat() == StorageFormat::Embedded) {
            core::string embeddedData;
            core::MemoryRange<const uint8_t> memoryRange;
            const core::Vector<uint8_t>* encodedData = image->getEncodedBytes();
            if (encodedData == nullptr) {
                core::logError("GltfBinaryResolver: No encoded data available for Image. Skipping Image.");
                continue;
            }
            memoryRange = encodedData->getMemoryRange();
            result = core::Max(embedded_data::createBase64Data(memoryRange, imageformats::imageEncodingToString(image->getMimeType()), embeddedData), result);
            _jsonRoot["images"][imageIndex]["uri"] = embeddedData;
            _jsonRoot["images"][imageIndex].erase("bufferView");
        } else if (image->getStorageFormat() == StorageFormat::BufferView) {
            _jsonRoot["images"][imageIndex].erase("uri");

            const BufferView* imageBufferView = image->getBufferView();

            // If referenced buffer is embedded, inherit this attribute
            bool referencedBufferIsEmbedded = false;
            if (imageBufferView != nullptr) {
                if (imageBufferView->getBuffer() != nullptr) {
                    if (imageBufferView->getBuffer()->getStorageFormat() == StorageFormat::Embedded) {
                        referencedBufferIsEmbedded = true;
                    }
                }
            }

            if (referencedBufferIsEmbedded) {
                bufferMergerEmbedded.add(*image, imageIndex);
            } else {
                bufferMerger.add(*image, imageIndex);
            }
        }

        if (image->getStorageFormat() == StorageFormat::Undefined) {
            core::logError("GltfBinaryResolver: Image storage format invalid. Skipping Image.");
        }
    }

    core::Vector<const AudioSource*>& audioSources = m_resourceCollector.getAudioSources();
    for (uint32_t audioSourceIDX = 0; audioSourceIDX < audioSources.size(); audioSourceIDX++) {
        const AudioSource* audioSource = audioSources[audioSourceIDX];
        if (audioSource->getStorageFormat() == StorageFormat::File) {
            core::string sourceURI = StorageTypeConverter::generateUri(*audioSource, audioSourceIDX, m_gltfFile);
            _jsonRoot["extensions"]["KHR_audio"]["sources"][audioSourceIDX]["uri"] = sourceURI;

            if (audioSource->getEncodedBytes()->empty()) {
                _jsonRoot["extensions"]["KHR_audio"]["sources"][audioSourceIDX]["uri"] = "";
            }

            _jsonRoot["extensions"]["KHR_audio"]["sources"][audioSourceIDX].erase("bufferView");

            ResourceToFile audioSourceWriter(m_gltfFile.getParent() / sourceURI, m_exportOptions);
            result = core::Max(audioSourceWriter.write(*audioSource), result);
        } else if (audioSource->getStorageFormat() == StorageFormat::Embedded) {
            core::string embeddedData;
            core::MemoryRange<const uint8_t> memoryRange;
            const core::Vector<uint8_t>* encodedData = audioSource->getEncodedBytes();
            if (encodedData == nullptr) {
                core::logError("GltfBinaryResolver: No encoded data available for Image. Skipping Image.");
                continue;
            }
            memoryRange = encodedData->getMemoryRange();
            result = core::Max(embedded_data::createBase64Data(memoryRange, "audio/mpeg", embeddedData), result);
            _jsonRoot["extensions"]["KHR_audio"]["sources"][audioSourceIDX]["uri"] = embeddedData;
            _jsonRoot["extensions"]["KHR_audio"]["sources"][audioSourceIDX].erase("bufferView");
        } else if (audioSource->getStorageFormat() == StorageFormat::BufferView) {
            _jsonRoot["extensions"]["KHR_audio"]["sources"][audioSourceIDX].erase("uri");

            const BufferView* sourceBufferView = audioSource->getBufferView();

            // If referenced buffer is embedded, inherit this attribute
            bool referencedBufferIsEmbedded = false;
            if (sourceBufferView != nullptr) {
                if (sourceBufferView->getBuffer() != nullptr) {
                    if (sourceBufferView->getBuffer()->getStorageFormat() == StorageFormat::Embedded) {
                        referencedBufferIsEmbedded = true;
                    }
                }
            }

            if (referencedBufferIsEmbedded) {
                bufferMergerEmbedded.add(*audioSource, audioSourceIDX);
            } else {
                bufferMerger.add(*audioSource, audioSourceIDX);
            }
        }

        if (audioSource->getStorageFormat() == StorageFormat::Undefined) {
            core::logError("GltfBinaryResolver: Audio source storage format invalid. Skipping audio source.");
        }
    }

    ////////////Write//////////////

    result = core::Max(bufferMerger.appendMergedBuffer(), result);

    if (!mergedData.empty()) {
        const core::string mergedBufferUri = bufferMerger.getBufferUri();
        ResourceToFile resourceWriter(m_gltfFile.getParent() / mergedBufferUri, m_exportOptions);
        ConversionResult writeResult = resourceWriter.write(mergedData.getMemoryRange());

        if (writeResult == ConversionError::ResourceFileAlreadyExists) {
            return ConversionError::ResourceFileAlreadyExists;
        }

        result = core::Max(writeResult, result);
    }

    result = core::Max(bufferMergerEmbedded.appendMergedBuffer(true), result);

    return result;
}
