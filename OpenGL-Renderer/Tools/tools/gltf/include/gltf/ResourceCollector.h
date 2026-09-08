// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "core/Vector.h"
#include "gltf/json/JsonPath.h"

namespace ux3d::gltf {
class Image;
class Accessor;
class BufferView;
class AudioSource;

class ResourceCollector {
public:
    struct GenericResource {
        const BufferView* bufferView; // this is the bufferview that should be stored somehow
        const JsonPath jsonBufferViewField; // this is a reference to the json entry where the bufferview is used
    };

    void add(const Image& _image);
    void add(const Accessor& _buffer);
    void add(const AudioSource& audioSource);
    void addGenericResource(const BufferView* _bufferView, const JsonPath& _jsonBufferViewField);

    core::Vector<const Image*>& getImages();
    core::Vector<const AudioSource*>& getAudioSources();
    core::Vector<const Accessor*>& getAccessors();
    core::Vector<GenericResource>& getGenericResources();

    size_t getByteSize();

private:
    core::Vector<const Image*> m_images;
    core::Vector<const AudioSource*> m_audioSources;
    core::Vector<const Accessor*> m_accessors;
    core::Vector<GenericResource> m_genericResources;
};
}
