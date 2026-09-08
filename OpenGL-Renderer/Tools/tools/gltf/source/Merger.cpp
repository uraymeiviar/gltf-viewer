// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Merger.h"
#include "gltf/Gltf.h"

#include "core/Algorithms.h"
#include "gltf/EmbeddedData.h"
#include <cstdlib>
#include <ctime>
#include <string>

using namespace ux3d;
using namespace ux3d::gltf;

Merger::Merger(const Gltf& _source, Gltf& _destination)
    : m_Destination(_destination)
    , m_Source(_source)
    , m_Indices(_source, _destination)
    , m_OriginalSceneCount(_destination.getScenes().size())
    , m_SceneMerger(m_Destination, m_Indices)
    , m_NodeMerger(m_Destination, m_Indices)
    , m_MeshMerger(m_Destination, m_Source, m_Indices)
    , m_MaterialMerger(m_Destination, m_Indices)
    , m_TextureMerger(m_Destination, m_Indices)
    , m_AccessorMerger(m_Destination, m_Indices)
    , m_BufferViewMerger(m_Destination, m_Indices)
    , m_AnimationMerger(m_Destination, m_Indices)
    , m_SkinMerger(m_Destination, m_Indices)
    , m_ImageMerger(m_Destination, m_Indices)
    , m_AudioEmitterMerger(m_Destination, m_Indices)
    , m_AudioSourceMerger(m_Destination, m_Indices)
{
}

bool Merger::fixURIs()
{

    std::unordered_set<std::string> usedURIs;

    for (auto& image : m_Destination.getImages()) {
        auto uri = image.getUri();
        if (uri.empty() || embedded_data::isDataUri(uri)) {
            continue;
        }
        if (usedURIs.count(uri) == 1) {
            uri = image.createUniqueURI(m_Destination);
            image.setUri(uri);
        }
        usedURIs.insert(uri);
    }

    for (auto& audioSource : m_Destination.getAudioSources()) {
        auto uri = audioSource.getUri();
        if (uri.empty() || embedded_data::isDataUri(uri)) {
            continue;
        }
        if (usedURIs.count(uri) == 1) {
            uri = audioSource.createUniqueURI(m_Destination);
            audioSource.setUri(uri);
        }
        usedURIs.insert(uri);
    }

    for (auto& buffer : m_Destination.getBuffers()) {
        auto uri = buffer.getUri();
        if (uri.empty() || embedded_data::isDataUri(uri)) {
            continue;
        }
        if (usedURIs.count(uri) == 1) {
            std::srand(static_cast<unsigned>(std::time(nullptr)));
            auto randomNumber = static_cast<uint64_t>(std::rand());
            uri = std::to_string(randomNumber) + ".bin";
            buffer.setUri(uri);
        }
        usedURIs.insert(uri);
    }
    return true;
}

bool Merger::merge()
{
    bool success = true;
    success &= createElements();
    success &= resolveDependencies();
    success &= fixURIs();
    return success;
}

bool Merger::createElements()
{
    bool success = true;
    success &= createCopies<Scene>();
    success &= createCopies<Node>();
    success &= createCopies<Camera>();
    success &= createCopies<Light>();
    success &= createCopies<Mesh>();
    success &= createCopies<Material>();
    success &= createCopies<Texture>();
    success &= createCopies<Sampler>();
    success &= createCopies<Image>();
    success &= createCopies<Accessor>();
    success &= createCopies<BufferView>();
    success &= createCopies<Buffer>();
    success &= createCopies<Animation>();
    success &= createCopies<Skin>();
    success &= createCopies<MaterialVariant>();
    success &= createCopies<AudioEmitter>();
    success &= createCopies<AudioSource>();
    return success;
}

bool Merger::resolveDependencies()
{
    bool success = true;
    resolveXMP();
    resolveDefaultScene();
    resolveExtensionsExtras();

    success &= resolve<Scene>(m_SceneMerger);
    success &= resolve<Node>(m_NodeMerger);
    success &= resolve<Mesh>(m_MeshMerger);
    success &= resolve<Material>(m_MaterialMerger);
    success &= resolve<Texture>(m_TextureMerger);
    success &= resolve<Accessor>(m_AccessorMerger);
    success &= resolve<BufferView>(m_BufferViewMerger);
    success &= resolve<Animation>(m_AnimationMerger);
    success &= resolve<Skin>(m_SkinMerger);
    success &= resolve<Image>(m_ImageMerger);
    success &= resolve<AudioSource>(m_AudioSourceMerger);
    success &= resolve<AudioEmitter>(m_AudioEmitterMerger);
    return success;
}

template <class ChildOfRoot>
bool Merger::createCopies()
{
    for (const ChildOfRoot& sourceElement : m_Source.getElements<ChildOfRoot>()) {
        if (!m_Destination.import(sourceElement)) {
            return false;
        }
    }
    return true;
}

template <class ChildOfRoot, class Resolver>
bool Merger::resolve(Resolver& _resolver)
{
    auto& elements = m_Destination.getElements<ChildOfRoot>();
    const size_t count = elements.size();

    const auto& sourceElements = m_Source.getElements<ChildOfRoot>();
    const size_t sourceCount = sourceElements.size();

    auto elementIter = elements.begin();
    elementIter = elementIter + (count - sourceCount);
    for (const ChildOfRoot& sourceElement : sourceElements) {
        ChildOfRoot& element = *elementIter;
        if (!_resolver.resolveDependencies(sourceElement, element)) {
            return false;
        }
        elementIter++;
    }

    return true;
}

void Merger::resolveDefaultScene()
{
    if (m_Destination.getDefaultScene() == nullptr) {
        if (m_Source.getDefaultScene() != nullptr) {
            const size_t index = m_Indices[m_Source.getDefaultScene()];
            Scene& defaultScene = m_Destination.getScenes().at(index);
            m_Destination.setDefaultScene(defaultScene);
        }
    }
}

void Merger::resolveXMP()
{
    if (m_Source.hasXMP()) {
        XMP* xmpDestination;
        if (m_Destination.hasXMP()) {
            xmpDestination = &(m_Destination.getXMP().value());
        } else {
            xmpDestination = &(m_Destination.createXMP());
        }

        const XMP& xmpSource = m_Source.getXMP().value();
        const core::LinkedList<Packet>& sourcePackets = xmpSource.getPackets();

        for (const Packet& sourcePacket : sourcePackets) {
            xmpDestination->addPacket(sourcePacket);
        }
    }
}

void Merger::resolveExtensionsExtras()
{
    m_Destination.addExtras(m_Source.getExtras());
    for (const auto& unknownExtension : m_Source.getUnknownExtensions()) {
        m_Destination.addUnknownExtension(unknownExtension.first, unknownExtension.second);
    }

    m_Destination.getAsset().setExtras(m_Source.getAsset().getExtras());
    for (const auto& unknownExtension : m_Source.getAsset().getUnknownExtensions()) {
        m_Destination.getAsset().addUnknownExtension(unknownExtension.first, unknownExtension.second);
    }

    if (m_Source.getLightsPunctual().has_value()) {
        if (!m_Destination.getLightsPunctual().has_value()) {
            m_Destination.createLightsPunctual();
        }

        auto& lightsArrayObject = m_Destination.getLightsPunctual().value();
        const auto& sourceLightsArrayObject = m_Source.getLightsPunctual().value();
        lightsArrayObject.addExtras(sourceLightsArrayObject.getExtras());
        for (const auto& unknownExtension : sourceLightsArrayObject.getUnknownExtensions()) {
            lightsArrayObject.addUnknownExtension(unknownExtension.first, unknownExtension.second);
        }
    }

    if (m_Source.getAudio().has_value()) {
        if (!m_Destination.getAudio().has_value()) {
            m_Destination.createAudio();
        }

        auto& audioArrayObject = m_Destination.getAudio().value();
        const auto& sourceAudioeArrayObject = m_Source.getAudio().value();
        audioArrayObject.addExtras(sourceAudioeArrayObject.getExtras());
        for (const auto& unknownExtension : sourceAudioeArrayObject.getUnknownExtensions()) {
            audioArrayObject.addUnknownExtension(unknownExtension.first, unknownExtension.second);
        }
    }
    if (m_Source.hasMaterialVariants()) {
        if (!m_Destination.hasMaterialVariants()) {
            m_Destination.createMaterialVariants();
        }

        auto& materialVariantsArrayObject = m_Destination.getMaterialVariantsOptional().value();
        const auto& sourcematerialVariantsArrayObject = m_Source.getMaterialVariantsOptional().value();
        materialVariantsArrayObject.addExtras(sourcematerialVariantsArrayObject.getExtras());
        for (const auto& unknownExtension : sourcematerialVariantsArrayObject.getUnknownExtensions()) {
            materialVariantsArrayObject.addUnknownExtension(unknownExtension.first, unknownExtension.second);
        }
    }

    const auto& requiredUnknownExtensions = m_Source.getRequiredUnknownExtensions();
    m_Destination.getRequiredUnknownExtensions().insert(requiredUnknownExtensions.begin(), requiredUnknownExtensions.end());
    m_Destination.addRootExtension(m_Source.getRootExtensions());
    m_Destination.addRequiredKnownExtension(m_Source.getRequiredKnownExtensions());
}
