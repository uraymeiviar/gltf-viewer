// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "Accessor.h"
#include "Animation.h"
#include "Asset.h"
#include "AudioEmitter.h"
#include "AudioSource.h"
#include "Buffer.h"
#include "BufferView.h"
#include "Camera.h"
#include "CameraOrthographic.h"
#include "CameraPerspective.h"
#include "GltfObject.h"
#include "GltfResult.h"
#include "Image.h"
#include "Light.h"
#include "Material.h"
#include "MaterialVariant.h"
#include "Mesh.h"
#include "Node.h"
#include "Sampler.h"
#include "Scene.h"
#include "Skin.h"
#include "Texture.h"
#include "XMP.h"
#include "core/LinkedList.h"
#include "filesystem/Dir.h"

#include <unordered_set>

namespace ux3d::gltf {
class Gltf : public GltfObject {
    Scene* m_pDefaultScene = nullptr;

    core::LinkedList<Scene> m_scenes;
    core::LinkedList<Node> m_nodes;
    core::LinkedList<Camera> m_cameras;
    core::LinkedList<Light> m_lights;
    core::LinkedList<Mesh> m_meshes;
    core::LinkedList<Accessor> m_accessors;
    core::LinkedList<Buffer> m_buffers;
    core::LinkedList<BufferView> m_bufferViews;
    core::LinkedList<Image> m_images;
    core::LinkedList<Material> m_materials;
    core::LinkedList<Texture> m_textures;
    core::LinkedList<Sampler> m_samplers;
    core::LinkedList<Animation> m_animations;
    core::LinkedList<Skin> m_skins;
    core::LinkedList<MaterialVariant> m_materialVariants;
    core::LinkedList<AudioSource> m_audioSources;
    core::LinkedList<AudioEmitter> m_audioEmitters;

    Asset m_asset;
    std::optional<XMP> m_xmp;

    Extensions m_rootExtensions;
    // This holds the extensions and extras for the lightArray
    std::optional<GltfObject> m_lightsArrayObject;
    // this holds if material variants are enabled or not
    std::optional<GltfObject> m_materialVariantsArrayObject;
    // This holds the extensions and extras for the audio extension
    std::optional<GltfObject> m_audioArrayObject;
    Extensions m_requiredKnownExtensions;
    std::unordered_set<core::string> m_requiredUnknownExtensions;
    // Every name in the loaded file's extensionsUsed, known or not. Vendor extensions
    // (ASOBO_*) are only honoured when declared here.
    std::unordered_set<core::string> m_declaredExtensions;

    // Monotonic revision for UI caches. It changes whenever the model is reset
    // or an element is created, so cached pointers and labels can be rejected
    // without walking the whole document.
    uint64_t m_revision = 0;

    struct GltfTypeIDs {
        uint32_t nextScenesId = 0;
        uint32_t nextNodesId = 0;
        uint32_t nextCamerasId = 0;
        uint32_t nextLightsId = 0;
        uint32_t nextMeshesId = 0;
        uint32_t nextAccessorsId = 0;
        uint32_t nextBuffersId = 0;
        uint32_t nextBufferViewsId = 0;
        uint32_t nextImagesId = 0;
        uint32_t nextMaterialsId = 0;
        uint32_t nextTexturesId = 0;
        uint32_t nextSamplersId = 0;
        uint32_t nextAnimationsId = 0;
        uint32_t nextSkinsId = 0;
        uint32_t nextMaterialVariantsId = 0;
        uint32_t nextAudioSourcesId = 0;
        uint32_t nextAudioEmittersId = 0;
    } m_elementIds;

public:
    Gltf() = default;
    Gltf(Gltf&& _other) = default;
    Gltf& operator=(Gltf&& _other) = default;
    ~Gltf() override = default;

    Gltf(const Gltf&) = delete;
    Gltf& operator=(const Gltf&) = delete;

    bool merge(const Gltf& _other);

    void reset();

    uint64_t getRevision() const { return m_revision; }

    Extensions getUsedExtensions() const override;

    template <typename TChildOfRoot, typename... TAdditionalArgs>
    TChildOfRoot* create(std::optional<uint32_t> _id, TAdditionalArgs&&... _additionalArgs);

    template <typename TChildOfRoot, typename... TAdditionalArgs>
    TChildOfRoot* create(TAdditionalArgs&&... _additionalArgs);

    template <typename TChildOfRoot>
    TChildOfRoot* import(const TChildOfRoot& _source);

    template <typename TChildOfRoot>
    TChildOfRoot* duplicate(const TChildOfRoot& _source);

    template <typename TChildOfRoot>
    core::LinkedList<TChildOfRoot>& getElements();

    template <typename TChildOfRoot>
    const core::LinkedList<TChildOfRoot>& getElements() const;

    void deleteElement(ChildOfRoot& _element);

    template <typename TChildOfRoot>
    void deleteElement(TChildOfRoot& _element);

    template <typename TChildOfRoot>
    TChildOfRoot* getByID(uint32_t _id);

    template <typename TChildOfRoot>
    const TChildOfRoot* getByID(uint32_t _id) const;

    GltfResult loadBinaryDataToMemory(const std::optional<filesystem::Dir>& gltfDir, const std::optional<filesystem::Dir>& resourceDir);

    GltfResult deleteNode(Node& _node);
    GltfResult deleteScene(Scene& _scene);
    GltfResult deleteMesh(Mesh& _mesh);
    GltfResult deleteCamera(Camera& _camera);
    GltfResult deleteLight(Light& _light);
    GltfResult deleteMaterial(Material& _material);
    GltfResult deleteTexture(Texture& _texture);
    GltfResult deleteSampler(Sampler& _sampler);
    GltfResult deleteImage(Image& _image);
    GltfResult deleteAccessor(Accessor& _accessor);
    GltfResult deleteBufferView(BufferView& _bufferView);
    GltfResult deleteBuffer(Buffer& _buffer);
    GltfResult deleteAnimation(Animation& _animation);
    GltfResult deleteSkin(Skin& _skin);
    GltfResult deleteMaterialVariant(MaterialVariant& _materialVariant);
    GltfResult deleteMaterialVariant(uint32_t _variantId);
    GltfResult deleteXMPPacket(Packet& _packet);
    GltfResult deleteAudioSource(AudioSource& _audioSource);
    GltfResult deleteAudioEmitter(AudioEmitter& _audioEmitter);

    GltfResult setDefaultScene(Scene& _scene);
    void unsetDefaultScene() { m_pDefaultScene = nullptr; }

    Scene* getDefaultScene() { return m_pDefaultScene; }
    const Scene* getDefaultScene() const { return m_pDefaultScene; }

    core::LinkedList<Scene>& getScenes() { return m_scenes; }
    const core::LinkedList<Scene>& getScenes() const { return m_scenes; }

    core::LinkedList<Node>& getNodes() { return m_nodes; }
    const core::LinkedList<Node>& getNodes() const { return m_nodes; }

    core::LinkedList<Camera>& getCameras() { return m_cameras; }
    const core::LinkedList<Camera>& getCameras() const { return m_cameras; }

    core::LinkedList<Light>& getLights() { return m_lights; }
    const core::LinkedList<Light>& getLights() const { return m_lights; }

    core::LinkedList<Mesh>& getMeshes() { return m_meshes; }
    const core::LinkedList<Mesh>& getMeshes() const { return m_meshes; }

    core::LinkedList<Accessor>& getAccessors() { return m_accessors; }
    const core::LinkedList<Accessor>& getAccessors() const { return m_accessors; }

    core::LinkedList<Buffer>& getBuffers() { return m_buffers; }
    const core::LinkedList<Buffer>& getBuffers() const { return m_buffers; }

    core::LinkedList<BufferView>& getBufferViews() { return m_bufferViews; }
    const core::LinkedList<BufferView>& getBufferViews() const { return m_bufferViews; }

    core::LinkedList<Image>& getImages() { return m_images; }
    const core::LinkedList<Image>& getImages() const { return m_images; }

    core::LinkedList<Material>& getMaterials() { return m_materials; }
    const core::LinkedList<Material>& getMaterials() const { return m_materials; }

    core::LinkedList<Texture>& getTextures() { return m_textures; }
    const core::LinkedList<Texture>& getTextures() const { return m_textures; }

    core::LinkedList<Sampler>& getSamplers() { return m_samplers; }
    const core::LinkedList<Sampler>& getSamplers() const { return m_samplers; }

    core::LinkedList<Animation>& getAnimations() { return m_animations; }
    const core::LinkedList<Animation>& getAnimations() const { return m_animations; }

    core::LinkedList<Skin>& getSkins() { return m_skins; }
    const core::LinkedList<Skin>& getSkins() const { return m_skins; }

    core::LinkedList<MaterialVariant>& getMaterialVariants() { return m_materialVariants; }
    const core::LinkedList<MaterialVariant>& getMaterialVariants() const { return m_materialVariants; }

    core::LinkedList<AudioSource>& getAudioSources() { return m_audioSources; }
    const core::LinkedList<AudioSource>& getAudioSources() const { return m_audioSources; }

    core::LinkedList<AudioEmitter>& getAudioEmitters() { return m_audioEmitters; }
    const core::LinkedList<AudioEmitter>& getAudioEmitters() const { return m_audioEmitters; }

    Asset& getAsset() { return m_asset; }
    const Asset& getAsset() const { return m_asset; }

    bool hasXMP() const { return m_xmp.has_value(); }
    std::optional<XMP>& getXMP() { return m_xmp; }
    const std::optional<XMP>& getXMP() const { return m_xmp; }
    XMP& createXMP() { return m_xmp.emplace(); }
    void destroyXMP() { m_xmp.reset(); }

    std::optional<GltfObject>& getLightsPunctual() { return m_lightsArrayObject; }
    const std::optional<GltfObject>& getLightsPunctual() const { return m_lightsArrayObject; }
    GltfObject& createLightsPunctual() { return m_lightsArrayObject.emplace(); }

    bool hasAudio() const { return m_audioArrayObject.has_value(); }
    std::optional<GltfObject>& getAudio() { return m_audioArrayObject; }
    const std::optional<GltfObject>& getAudio() const { return m_audioArrayObject; }
    GltfObject& createAudio() { return m_audioArrayObject.emplace(); }

    bool hasMaterialVariants() const { return m_materialVariantsArrayObject.has_value(); }
    GltfObject& createMaterialVariants() { return m_materialVariantsArrayObject.emplace(); }
    std::optional<GltfObject>& getMaterialVariantsOptional() { return m_materialVariantsArrayObject; }
    const std::optional<GltfObject>& getMaterialVariantsOptional() const { return m_materialVariantsArrayObject; }
    void destroyMaterialVariants() { m_materialVariantsArrayObject.reset(); }
    core::Vector<const core::string> getMaterialVariantsNames() const;

    void addRootExtension(const Extension _extension) { m_rootExtensions |= _extension; }
    void removeRootExtension(Extension _extension) { m_rootExtensions &= static_cast<Extension>(~(static_cast<uint32_t>(_extension))); }
    Extensions getRootExtensions() const;

    void addRequiredKnownExtension(const Extension _extension) { m_requiredKnownExtensions |= _extension; }
    Extensions getRequiredKnownExtensions() const { return m_requiredKnownExtensions; }

    void addRequiredUnknownExtension(const core::string& _extensionName);
    std::unordered_set<core::string>& getRequiredUnknownExtensions() { return m_requiredUnknownExtensions; }
    const std::unordered_set<core::string>& getRequiredUnknownExtensions() const { return m_requiredUnknownExtensions; }
    bool containsUnknownRequiredExtensions() const { return m_requiredUnknownExtensions.size() > 0; }

    void addDeclaredExtension(const core::string& _extensionName) { m_declaredExtensions.insert(_extensionName); }
    bool isExtensionDeclared(const core::string& _extensionName) const { return m_declaredExtensions.count(_extensionName) != 0; }
    const std::unordered_set<core::string>& getDeclaredExtensions() const { return m_declaredExtensions; }

    core::Vector<TextureInfo*> getTextureInfos();

private:
    void bumpRevision() { ++m_revision; }

    template <typename TChildOfRoot>
    uint32_t getNextGltfTypeID()
    {
        bumpRevision();
        return id<TChildOfRoot>()++;
    }

    template <typename TChildOfRoot>
    bool isFreeGltfTypeID(uint32_t _id) { return id<TChildOfRoot>() <= _id; }

    template <typename TChildOfRoot>
    void reserveGltfTypeID(uint32_t _id) { id<TChildOfRoot>() = _id + 1; }

    template <typename TChildOfRoot>
    uint32_t& id();

    void duplicateChildren(const Node& _sourceNode, Node& _node);
};

template <class TChildOfRoot, class... Args>
TChildOfRoot* Gltf::create(Args&&... _additionalArgs)
{
    return getElements<TChildOfRoot>()
        .emplace_back(getNextGltfTypeID<TChildOfRoot>(), std::forward<Args>(_additionalArgs)...);
}

template <class TChildOfRoot, class... Args>
TChildOfRoot* Gltf::create(std::optional<uint32_t> _id, Args&&... _additionalArgs)
{
    {
        if (_id.has_value() && isFreeGltfTypeID<TChildOfRoot>(_id.value())) {
            reserveGltfTypeID<TChildOfRoot>(_id.value());
            bumpRevision();

            return getElements<TChildOfRoot>()
                .emplace_back(_id.value(), std::forward<Args>(_additionalArgs)...);
        }
    }

    return create<TChildOfRoot>(std::forward<Args>(_additionalArgs)...);
}

template <class TChildOfRoot>
TChildOfRoot* Gltf::import(const TChildOfRoot& _source)
{
    return getElements<TChildOfRoot>()
        .emplace_back(getNextGltfTypeID<TChildOfRoot>(), _source);
}

template <>
Node* Gltf::duplicate(const Node& _source);

template <>
Scene* Gltf::duplicate(const Scene& _source);

template <class TChildOfRoot>
TChildOfRoot* Gltf::duplicate(const TChildOfRoot& _source)
{
    return getElements<TChildOfRoot>()
        .emplace_back(getNextGltfTypeID<TChildOfRoot>(), _source);
}

template <class TChildOfRoot>
core::LinkedList<TChildOfRoot>& Gltf::getElements()
{
    return const_cast<core::LinkedList<TChildOfRoot>&>(
        const_cast<const Gltf*>(this)->getElements<TChildOfRoot>());
}

template <typename TChildOfRoot>
TChildOfRoot* Gltf::getByID(uint32_t _id)
{
    return const_cast<TChildOfRoot*>(const_cast<const Gltf*>(this)->getByID<TChildOfRoot>(_id));
}

template <typename TChildOfRoot>
const TChildOfRoot* Gltf::getByID(uint32_t _id) const
{
    for (const TChildOfRoot& element : getElements<TChildOfRoot>()) {
        if (element.m_ID == _id) {
            return &element;
        }
    }

    return nullptr;
}
}
