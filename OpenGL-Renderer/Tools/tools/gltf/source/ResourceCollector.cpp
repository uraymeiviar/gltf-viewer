// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/ResourceCollector.h"
#include "core/Logger.h"
#include "core/Vector.h"
#include "gltf/Accessor.h"
#include "gltf/Buffer.h"
#include "gltf/BufferView.h"
#include "gltf/Image.h"

using namespace ux3d;
using namespace ux3d::gltf;

void ResourceCollector::add(const Image& _image)
{
    m_images.emplace_back(&_image);
}

void ResourceCollector::add(const AudioSource& audioSource)
{
    m_audioSources.emplace_back(&audioSource);
}

void ResourceCollector::add(const Accessor& _accessor)
{
    m_accessors.emplace_back(&_accessor);
}

void ResourceCollector::addGenericResource(const BufferView* _bufferView, const JsonPath& _jsonBufferViewField)
{
    if (_bufferView == nullptr) {
        core::logError("cannot create a generic resource with a nullptr for buffer view");
        return;
    }
    m_genericResources.push_back(GenericResource { _bufferView, _jsonBufferViewField });
}

core::Vector<const gltf::Image*>& ResourceCollector::getImages()
{
    return m_images;
}

core::Vector<const gltf::AudioSource*>& ResourceCollector::getAudioSources()
{
    return m_audioSources;
}

core::Vector<const gltf::Accessor*>& ResourceCollector::getAccessors()
{
    return m_accessors;
}

core::Vector<ResourceCollector::GenericResource>& ResourceCollector::getGenericResources()
{
    return m_genericResources;
}

size_t ResourceCollector::getByteSize()
{
    size_t accumulatedSize = 0;
    for (auto& genericResource : m_genericResources) {
        if (genericResource.bufferView->getBuffer() == nullptr) {
            continue;
        }
        StorageFormat storageFormat = genericResource.bufferView->getBuffer()->getStorageFormat();

        if (storageFormat == StorageFormat::Embedded || storageFormat == StorageFormat::File || storageFormat == StorageFormat::BufferView) {
            accumulatedSize += genericResource.bufferView->getByteLength();
        }
    }

    for (uint32_t accessorIndex = 0; accessorIndex < m_accessors.size(); accessorIndex++) {
        const Accessor* accessor = m_accessors[accessorIndex];
        StorageFormat storageFormat = accessor->getStorageFormat();

        if (storageFormat == StorageFormat::Undefined) {
            if (accessor->getBufferView() != nullptr) {
                storageFormat = accessor->getBufferView()->getBuffer()->getStorageFormat();
            }
        }

        if (storageFormat == StorageFormat::Embedded || storageFormat == StorageFormat::File || storageFormat == StorageFormat::BufferView) {
            core::MemoryRange<const uint8_t> memoryRange;
            accessor->getData(memoryRange);
            accumulatedSize += memoryRange.size();
        }
    }

    for (const gltf::Image* image : m_images) {

        if (image->getStorageFormat() == StorageFormat::File || image->getStorageFormat() == StorageFormat::Embedded || image->getStorageFormat() == StorageFormat::BufferView) {

            const core::Vector<uint8_t>* encodedData = image->getEncodedBytes();
            if (encodedData == nullptr) {
                continue;
            }
            size_t imageSize = encodedData->size();
            accumulatedSize += imageSize;
        }
    }

    return accumulatedSize;
}
