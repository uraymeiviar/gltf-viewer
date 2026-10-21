// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Gltf.h"
#include "core/Algorithms.h"
#include "gltf/DracoDecoder.h"
#include "gltf/Merger.h"

#include <future>

using namespace ux3d;
using namespace ux3d::gltf;

Extensions Gltf::getRootExtensions() const
{
    Extensions extensionsUsed = m_rootExtensions;
    if (hasMaterialVariants()) {
        extensionsUsed |= Extension::MaterialsVariants;
    }
    if (hasXMP()) {
        extensionsUsed |= Extension::XMP;
    }
    return extensionsUsed;
}

GltfResult Gltf::deleteNode(Node& _node)
{
    for (Skin& skin : m_skins) {
        if (core::contains(skin.getJoints(), &_node)) {
            core::logWarning("Node can't be deleted. It is a joint of a skin.");
            return GltfError::NodeIsPartOfSkin;
        }
    }
    for (Scene& scene : m_scenes) {
        scene.removeNode(_node);
    }
    for (Node& node : m_nodes) {
        node.removeChild(_node);
    }
    for (Animation& animation : m_animations) {
        for (AnimationChannel& channel : animation.getChannels()) {
            if (channel.getTarget().getNode() == &_node) {
                channel.getTarget().unsetNode();
            }
        }
    }
    return core::remove(m_nodes, _node) ? GltfResult::Success : GltfError::ObjectNotFound;
}

GltfResult Gltf::deleteScene(Scene& _Scene)
{
    bool success = core::remove(m_scenes, _Scene);

    if (&_Scene == m_pDefaultScene) {
        unsetDefaultScene();
    }
    return success ? GltfResult::Success : GltfError::ObjectNotFound;
}

GltfResult Gltf::deleteMesh(Mesh& _Mesh)
{
    for (Node& node : m_nodes) {
        if (node.getMesh() == &_Mesh) {
            node.unsetMesh();
        }
    }
    return core::remove(m_meshes, _Mesh) ? GltfResult::Success : GltfError::ObjectNotFound;
}

GltfResult Gltf::deleteCamera(Camera& _Camera)
{
    for (Node& node : m_nodes) {
        if (node.getCamera() == &_Camera) {
            node.unsetCamera();
        }
    }
    return core::remove(m_cameras, _Camera) ? GltfResult::Success : GltfError::ObjectNotFound;
}

GltfResult Gltf::deleteAudioSource(AudioSource& audioSource)
{
    for (AudioEmitter& emitter : m_audioEmitters) {
        if (emitter.getSource() == &audioSource) {
            emitter.unsetSource();
        }
    }

    GltfResult result = core::remove(m_audioSources, audioSource) ? GltfResult::Success : GltfError::ObjectNotFound;
    if (m_audioSources.empty() && m_audioEmitters.empty()) {
        // All extensions and extras are deleted if the last audio source/ emitter is removed, since the audio extensions itself is removed aswell
        m_audioArrayObject.reset();
    }
    return result;
}

GltfResult Gltf::deleteAudioEmitter(AudioEmitter& audioEmitter)
{
    for (Node& node : m_nodes) {
        if (node.getAudioEmitter() == &audioEmitter) {
            node.unsetAudioEmitter();
        }
    }
    for (Scene& scene : m_scenes) {
        if (scene.hasAudioEmitters()) {
            for (AudioEmitter* emitter : scene.getAudioEmitters()) {
                if (emitter == &audioEmitter) {
                    scene.removeAudioEmitter(emitter);
                    break;
                }
            }
        }
    }

    GltfResult result = core::remove(m_audioEmitters, audioEmitter) ? GltfResult::Success : GltfError::ObjectNotFound;
    if (m_audioSources.empty() && m_audioEmitters.empty()) {
        // All extensions and extras are deleted if the last audio source/ emitter is removed, since the audio extensions itself is removed aswell
        m_audioArrayObject.reset();
    }
    return result;
}

GltfResult Gltf::deleteSkin(Skin& _Skin)
{
    for (Node& node : m_nodes) {
        if (node.getSkin() == &_Skin) {
            node.unsetSkin();
        }
    }
    return core::remove(m_skins, _Skin) ? GltfResult::Success : GltfError::ObjectNotFound;
}

GltfResult Gltf::deleteMaterialVariant(MaterialVariant& _materialVariant)
{
    bool result = core::remove(m_materialVariants, _materialVariant);
    if (m_materialVariants.size() == 0) {
        destroyMaterialVariants();
    }
    return result ? GltfResult::Success : GltfError::ObjectNotFound;
}

GltfResult Gltf::deleteMaterialVariant(uint32_t _variantId)
{
    core::LinkedListIterator it = m_materialVariants.erase(m_materialVariants.begin() + _variantId);
    if (m_materialVariants.size() == 0) {
        destroyMaterialVariants();
    }
    return it != nullptr ? GltfResult::Success : GltfError::ObjectNotFound;
}

GltfResult Gltf::deleteXMPPacket(Packet& _packet)
{
    if (hasXMP() == false) {
        return GltfResult::Success;
    }

    // remove packet reference from asset
    if (m_asset.hasXMPPacket()) {
        const Packet* assetXmpPacket = m_asset.getXMPPacket().value().m_packet;
        if (assetXmpPacket == &_packet) {
            m_asset.unsetXMPPacket();
        }
    }

    // remove xmp packet from scenes
    for (Scene& scene : m_scenes) {
        if (scene.hasXMPPacket()) {
            const Packet* sceneXmpPacket = scene.getXMPPacket().value().m_packet;
            if (sceneXmpPacket == &_packet) {
                scene.unsetXMPPacket();
            }
        }
    }

    // remove xmp packet from nodes
    for (Node& node : m_nodes) {
        if (node.hasXMPPacket()) {
            const Packet* nodeXmpPacket = node.getXMPPacket().value().m_packet;
            if (nodeXmpPacket == &_packet) {
                node.unsetXMPPacket();
            }
        }
    }

    // remove xmp packet from meshes
    for (Mesh& mesh : m_meshes) {
        if (mesh.hasXMPPacket()) {
            const Packet* meshXmpPacket = mesh.getXMPPacket().value().m_packet;
            if (meshXmpPacket == &_packet) {
                mesh.unsetXMPPacket();
            }
        }
    }

    // remove xmp packet from material
    for (Material& material : m_materials) {
        if (material.hasXMPPacket()) {
            const Packet* materialXmpPacket = material.getXMPPacket().value().m_packet;
            if (materialXmpPacket == &_packet) {
                material.unsetXMPPacket();
            }
        }
    }

    // remove xmp packet from images
    for (Image& image : m_images) {
        if (image.hasXMPPacket()) {
            const Packet* imageXmpPacket = image.getXMPPacket().value().m_packet;
            if (imageXmpPacket == &_packet) {
                image.unsetXMPPacket();
            }
        }
    }

    // remove xmp packet from animations
    for (Animation& animation : m_animations) {
        if (animation.hasXMPPacket()) {
            const Packet* animationXmpPacket = animation.getXMPPacket().value().m_packet;
            if (animationXmpPacket == &_packet) {
                animation.unsetXMPPacket();
            }
        }
    }

    bool deletionWorked = m_xmp.value().deletePacket(_packet);

    if (deletionWorked && m_xmp.value().getPackets().size() == 0) {
        destroyXMP();
    }

    return deletionWorked ? GltfResult::Success : GltfError::ObjectNotFound;
}

ux3d::core::Vector<const ux3d::core::string> Gltf::getMaterialVariantsNames() const
{
    ux3d::core::Vector<const ux3d::core::string> names;
    for (auto& variant : m_materialVariants) {
        names.emplace_back(variant.getName());
    }
    return names;
}

GltfResult Gltf::deleteLight(Light& _Light)
{
    for (Node& node : m_nodes) {
        if (node.getLight() == &_Light) {
            node.unsetLight();
        }
    }
    GltfResult result = core::remove(m_lights, _Light) ? GltfResult::Success : GltfError::ObjectNotFound;
    if (m_lights.empty()) {
        // All extensions and extras are deleted if the last light is removed, since the light extensions itself is removed aswell
        m_lightsArrayObject.reset();
    }
    return result;
}

GltfResult Gltf::deleteAnimation(Animation& _Animation)
{
    bool success = core::remove(m_animations, _Animation);
    return success ? GltfResult::Success : GltfError::ObjectNotFound;
}

GltfResult Gltf::deleteAccessor(Accessor& _Accessor)
{
    for (Mesh& mesh : m_meshes) {
        for (Primitive& primitive : mesh.getPrimitives()) {
            if (primitive.getIndicesAccessor() == &_Accessor) {
                primitive.unsetIndicesAccessor();
            }
            for (Attribute& attribute : primitive.getAttributes()) {
                if (attribute.getAccessor() == &_Accessor) {
                    attribute.unsetAccessor();
                }
            }
            for (MorphTarget& morphTarget : primitive.getTargets()) {
                if (morphTarget.getNormalAccessor() == &_Accessor) {
                    morphTarget.unsetNormalAccessor();
                }
                if (morphTarget.getPositionAccessor() == &_Accessor) {
                    morphTarget.unsetPositionAccessor();
                }
                if (morphTarget.getTangentAccessor() == &_Accessor) {
                    morphTarget.unsetTangentAccessor();
                }
            }
        }
    }
    for (Skin& skin : m_skins) {
        if (skin.getInverseBindMatricesAccessor() == &_Accessor) {
            skin.unsetInverseBindMatricesAccessor();
        }
    }
    for (Animation& animation : m_animations) {
        for (AnimationSampler& sampler : animation.getSamplers()) {
            if (sampler.getInputAccessor() == &_Accessor) {
                sampler.unsetInputAccessor();
            }
            if (sampler.getOutputAccessor() == &_Accessor) {
                sampler.unsetOutputAccessor();
            }
        }
    }
    return core::remove(m_accessors, _Accessor) ? GltfResult::Success : GltfError::ObjectNotFound;
}

GltfResult Gltf::deleteMaterial(Material& _Material)
{
    for (Mesh& mesh : m_meshes) {
        for (Primitive& primitive : mesh.getPrimitives()) {
            if (primitive.getMaterial() == &_Material) {
                primitive.unsetMaterial();
            }
        }
    }
    return core::remove(m_materials, _Material) ? GltfResult::Success : GltfError::ObjectNotFound;
}

GltfResult Gltf::deleteBufferView(BufferView& _BufferView)
{
    for (Accessor& accessor : m_accessors) {
        if (accessor.getBufferView() == &_BufferView) {
            accessor.unsetBufferView();
        }
    }
    for (Image& image : m_images) {
        if (image.getBufferView() == &_BufferView) {
            image.unsetBufferView();
        }
    }
    return core::remove(m_bufferViews, _BufferView) ? GltfResult::Success : GltfError::ObjectNotFound;
}

GltfResult Gltf::deleteBuffer(Buffer& _Buffer)
{
    for (BufferView& bufferView : m_bufferViews) {
        if (bufferView.getBuffer() == &_Buffer) {
            bufferView.unsetBuffer();
        }
    }
    return core::remove(m_buffers, _Buffer) ? GltfResult::Success : GltfError::ObjectNotFound;
}

GltfResult Gltf::deleteTexture(Texture& _Texture)
{
    for (Material& material : m_materials) {
        // Material Root:
        auto& normalTextureInfo = material.getNormalTextureInfo();
        if (normalTextureInfo.has_value()) {
            if (normalTextureInfo->getTexture() == &_Texture) {
                normalTextureInfo->unsetTexture();
            }
        }
        auto& occlusionTextureInfo = material.getOcclusionTextureInfo();
        if (occlusionTextureInfo.has_value()) {
            if (occlusionTextureInfo->getTexture() == &_Texture) {
                occlusionTextureInfo->unsetTexture();
            }
        }
        auto& emissiveTextureInfo = material.getEmissiveTextureInfo();
        if (emissiveTextureInfo.has_value()) {
            if (emissiveTextureInfo->getTexture() == &_Texture) {
                emissiveTextureInfo->unsetTexture();
            }
        }
        // Metallic Roughness
        auto& metallicRoughness = material.getPbrMetallicRoughness();
        if (metallicRoughness.has_value()) {
            auto& metallicRoughnessTextureInfo = metallicRoughness->getMetallicRoughnessTextureInfo();
            if (metallicRoughnessTextureInfo.has_value()) {
                if (metallicRoughnessTextureInfo->getTexture() == &_Texture) {
                    metallicRoughnessTextureInfo->unsetTexture();
                }
            }
            auto& baseColorTextureInfo = metallicRoughness->getBaseColorTextureInfo();
            if (baseColorTextureInfo.has_value()) {
                if (baseColorTextureInfo->getTexture() == &_Texture) {
                    baseColorTextureInfo->unsetTexture();
                }
            }
        }
        // Specular Glossiness
        auto& specularGlossiness = material.getPbrSpecularGlossiness();
        if (specularGlossiness.has_value()) {
            auto& diffuseTextureInfo = specularGlossiness->getDiffuseTextureInfo();
            if (diffuseTextureInfo.has_value()) {
                if (diffuseTextureInfo->getTexture() == &_Texture) {
                    diffuseTextureInfo->unsetTexture();
                }
            }
            auto& specularGlossinesTextureInfo = specularGlossiness->getSpecularGlossinessTextureInfo();
            if (specularGlossinesTextureInfo.has_value()) {
                if (specularGlossinesTextureInfo->getTexture() == &_Texture) {
                    specularGlossinesTextureInfo->unsetTexture();
                }
            }
        }
        // Clearcoat
        auto& clearcoat = material.getClearcoat();
        if (clearcoat.has_value()) {
            auto& clearcoatTextureInfo = clearcoat->getClearcoatTexture();
            if (clearcoatTextureInfo.has_value()) {
                if (clearcoatTextureInfo->getTexture() == &_Texture) {
                    clearcoatTextureInfo->unsetTexture();
                }
            }
            auto& clearcoatNormalTextureInfo = clearcoat->getClearcoatNormalTexture();
            if (clearcoatNormalTextureInfo.has_value()) {
                if (clearcoatNormalTextureInfo->getTexture() == &_Texture) {
                    clearcoatNormalTextureInfo->unsetTexture();
                }
            }
            auto& clearcoatRoughnessTextureInfo = clearcoat->getClearcoatRoughnessTexture();
            if (clearcoatRoughnessTextureInfo.has_value()) {
                if (clearcoatRoughnessTextureInfo->getTexture() == &_Texture) {
                    clearcoatRoughnessTextureInfo->unsetTexture();
                }
            }
        }
        // Transmission
        auto& transmission = material.getTransmission();
        if (transmission.has_value()) {
            auto& transmissionTextureInfo = transmission->getTransmissionTextureInfo();
            if (transmissionTextureInfo.has_value()) {
                if (transmissionTextureInfo->getTexture() == &_Texture) {
                    transmissionTextureInfo->unsetTexture();
                }
            }
        }
        // Sheen
        auto& sheen = material.getSheen();
        if (sheen.has_value()) {
            auto& sheenTextureInfo = sheen->getSheenColorTextureInfo();
            if (sheenTextureInfo.has_value()) {
                if (sheenTextureInfo->getTexture() == &_Texture) {
                    sheenTextureInfo->unsetTexture();
                }
            }
            auto& sheenRoughnessTextureInfo = sheen->getSheenRoughnessTextureInfo();
            if (sheenRoughnessTextureInfo.has_value()) {
                if (sheenRoughnessTextureInfo->getTexture() == &_Texture) {
                    sheenRoughnessTextureInfo->unsetTexture();
                }
            }
        }
        // Specular
        auto& specular = material.getSpecular();
        if (specular.has_value()) {
            auto& specularTextureInfo = specular->getSpecularTextureInfo();
            if (specularTextureInfo.has_value()) {
                if (specularTextureInfo->getTexture() == &_Texture) {
                    specularTextureInfo->unsetTexture();
                }
            }
            auto& specularColorTextureInfo = specular->getSpecularColorTextureInfo();
            if (specularColorTextureInfo.has_value()) {
                if (specularColorTextureInfo->getTexture() == &_Texture) {
                    specularColorTextureInfo->unsetTexture();
                }
            }
        }
        // Volume
        auto& volume = material.getVolume();
        if (volume.has_value()) {
            auto& volumeTextureInfo = volume->getThicknessTextureInfo();
            if (volumeTextureInfo.has_value()) {
                if (volumeTextureInfo->getTexture() == &_Texture) {
                    volumeTextureInfo->unsetTexture();
                }
            }
        }
        // Iridescence
        auto& iridescence = material.getIridescence();
        if (iridescence.has_value()) {
            auto& iridescenceTextureInfo = iridescence->getIridescenceTextureInfo();
            if (iridescenceTextureInfo.has_value()) {
                if (iridescenceTextureInfo->getTexture() == &_Texture) {
                    iridescenceTextureInfo->unsetTexture();
                }
            }
            auto& iridescenceThicknessTextureInfo = iridescence->getIridescenceThicknessTextureInfo();
            if (iridescenceThicknessTextureInfo.has_value()) {
                if (iridescenceThicknessTextureInfo->getTexture() == &_Texture) {
                    iridescenceThicknessTextureInfo->unsetTexture();
                }
            }
        }
    }
    return core::remove(m_textures, _Texture) ? GltfResult::Success : GltfError::ObjectNotFound;
}

GltfResult Gltf::deleteImage(Image& _Image)
{
    for (Texture& texture : m_textures) {
        if (texture.getSource() == &_Image) {
            texture.unsetSource();
        }
        if (texture.getBasisU().has_value() && texture.getBasisU().value().getSource() == &_Image) {
            texture.destroyBasisU();
        }
        if (texture.getWebP().has_value() && texture.getWebP().value().getSource() == &_Image) {
            texture.destroyWebP();
        }
    }
    return core::remove(m_images, _Image) ? GltfResult::Success : GltfError::ObjectNotFound;
}

GltfResult Gltf::deleteSampler(Sampler& _Sampler)
{
    for (Texture& texture : m_textures) {
        if (texture.getSampler() == &_Sampler) {
            texture.unsetSample();
        }
    }
    return core::remove(m_samplers, _Sampler) ? GltfResult::Success : GltfError::ObjectNotFound;
}

GltfResult Gltf::setDefaultScene(Scene& _Scene)
{
    m_pDefaultScene = &_Scene;
    return GltfResult::Success;
}

bool Gltf::merge(const Gltf& _other)
{
    Merger merger(_other, *this);
    return merger.merge();
}

void Gltf::reset()
{
    ++m_revision;
    m_scenes.clear();
    m_nodes.clear();
    m_cameras.clear();
    m_lights.clear();
    m_meshes.clear();
    m_accessors.clear();
    m_buffers.clear();
    m_bufferViews.clear();
    m_images.clear();
    m_materials.clear();
    m_textures.clear();
    m_samplers.clear();
    m_animations.clear();
    m_skins.clear();
    m_materialVariants.clear();
    m_xmp.reset();
    m_audioEmitters.clear();
    m_audioSources.clear();
    m_elementIds = {};

    m_pDefaultScene = nullptr;
    m_materialVariantsArrayObject.reset();
}

Extensions Gltf::getUsedExtensions() const
{
    Extensions usedExtensions = m_rootExtensions;
    // MaterialsVariants used flag is added by mesh or if we have MaterialsVariants elements in our root glTF
    usedExtensions &= static_cast<Extension>(~(static_cast<uint32_t>(Extension::MaterialsVariants)));

    for (const Scene& scene : m_scenes) {
        usedExtensions |= scene.getUsedExtensions();
    }
    for (const Node& node : m_nodes) {
        usedExtensions |= node.getUsedExtensions();
    }
    for (const Camera& camera : m_cameras) {
        usedExtensions |= camera.getUsedExtensions();
    }
    for (const Light& light : m_lights) {
        usedExtensions |= light.getUsedExtensions();
    }
    for (const AudioEmitter& audioEmitter : m_audioEmitters) {
        usedExtensions |= audioEmitter.getUsedExtensions();
    }
    for (const AudioSource& audioSource : m_audioSources) {
        usedExtensions |= audioSource.getUsedExtensions();
    }
    for (const Mesh& mesh : m_meshes) {
        usedExtensions |= mesh.getUsedExtensions();
    }
    for (const Accessor& accessor : m_accessors) {
        usedExtensions |= accessor.getUsedExtensions();
    }
    for (const Buffer& buffer : m_buffers) {
        usedExtensions |= buffer.getUsedExtensions();
    }
    for (const BufferView& bufferView : m_bufferViews) {
        usedExtensions |= bufferView.getUsedExtensions();
    }
    for (const Image& image : m_images) {
        usedExtensions |= image.getUsedExtensions();
    }
    for (const Material& material : m_materials) {
        usedExtensions |= material.getUsedExtensions();
    }
    for (const Texture& texture : m_textures) {
        usedExtensions |= texture.getUsedExtensions();
    }
    for (const Sampler& sampler : m_samplers) {
        usedExtensions |= sampler.getUsedExtensions();
    }
    for (const Animation& animation : m_animations) {
        usedExtensions |= animation.getUsedExtensions();
    }
    for (const Skin& skin : m_skins) {
        usedExtensions |= skin.getUsedExtensions();
    }
    if (m_materialVariants.size() > 0) {
        usedExtensions |= Extension::MaterialsVariants;
    }
    if (m_xmp.has_value()) {
        usedExtensions |= Extension::XMP;
        usedExtensions |= m_xmp.value().getUsedExtensions();
    }

    return usedExtensions;
}

void Gltf::addRequiredUnknownExtension(const core::string& _extensionName)
{
    m_requiredUnknownExtensions.insert(_extensionName);
}

GltfResult Gltf::loadBinaryDataToMemory(const std::optional<filesystem::Dir>& gltfDir, const std::optional<filesystem::Dir>& resourceDir)
{
    GltfResult result;

    {
        core::Vector<std::future<GltfResult>> results;

        // buffers need to be resolved first because images may get their data from buffers
        if (!results.reserve(m_buffers.size())) {
            return GltfError::AllocationFailure;
        }
        for (auto& buffer : m_buffers) {
            results.emplace_back(std::async(std::launch::async, [&buffer, &gltfDir]() { return buffer.loadDataToMemory(gltfDir); }));
        }
        for (auto& res : results) {
            result = core::Max(res.get(), result);
        }

        results.clear();
        if (!results.reserve(m_images.size())) {
            return GltfError::AllocationFailure;
        }
        for (auto& image : m_images) {
            std::optional<filesystem::Dir> searchDir = gltfDir;
            if (resourceDir.has_value()) {
                searchDir = resourceDir;
            }
            results.emplace_back(std::async(std::launch::async, [&image, searchDir]() { return image.loadDataToMemory(searchDir); }));
        }
        for (auto& audioSource : m_audioSources) {
            std::optional<filesystem::Dir> searchDir = gltfDir;
            if (resourceDir.has_value()) {
                searchDir = resourceDir;
            }
            results.emplace_back(std::async(std::launch::async, [&audioSource, searchDir]() { return audioSource.loadDataToMemory(searchDir); }));
        }
        for (auto& res : results) {
            result = core::Max(res.get(), result);
        }
    }

    // draco decoding
    {
        DracoDecoder dracoDecoder(*this);

        core::LinkedList<GltfResult> results; // dont want to count number of primitives with draco, use list to avoid resize
        for (auto& mesh : m_meshes) {
            for (auto& primitive : mesh.getPrimitives()) {
                if (primitive.getDracoMeshCompression().has_value()) {
                    results.emplace_back(dracoDecoder.decodePrimitive(primitive));
                }
            }
        }

        for (auto& res : results) {
            result = core::Max(res, result);
        }
    }

    return result;
}

void Gltf::deleteElement(ChildOfRoot& _element)
{
    bumpRevision();
    switch (_element.getGltfType()) {
    case GltfType::Scene: {
        deleteScene(static_cast<Scene&>(_element));
    } break;
    case GltfType::Node: {
        deleteNode(static_cast<Node&>(_element));
    } break;
    case GltfType::Camera: {
        deleteCamera(static_cast<Camera&>(_element));
    } break;
    case GltfType::Light: {
        deleteLight(static_cast<Light&>(_element));
    } break;
    case GltfType::Mesh: {
        deleteMesh(static_cast<Mesh&>(_element));
    } break;
    case GltfType::Accessor: {
        deleteAccessor(static_cast<Accessor&>(_element));
    } break;
    case GltfType::Buffer: {
        deleteBuffer(static_cast<Buffer&>(_element));
    } break;
    case GltfType::BufferView: {
        deleteBufferView(static_cast<BufferView&>(_element));
    } break;
    case GltfType::Image: {
        deleteImage(static_cast<Image&>(_element));
    } break;
    case GltfType::Material: {
        deleteMaterial(static_cast<Material&>(_element));
    } break;
    case GltfType::Texture: {
        deleteTexture(static_cast<Texture&>(_element));
    } break;
    case GltfType::Sampler: {
        deleteSampler(static_cast<Sampler&>(_element));
    } break;
    case GltfType::Animation: {
        deleteAnimation(static_cast<Animation&>(_element));
    } break;
    case GltfType::Skin: {
        deleteSkin(static_cast<Skin&>(_element));
    } break;
    case GltfType::MaterialVariant: {
        deleteMaterialVariant(static_cast<MaterialVariant&>(_element));
    } break;
    case GltfType::AudioSource: {
        deleteAudioSource(static_cast<AudioSource&>(_element));
    } break;
    case GltfType::AudioEmitter: {
        deleteAudioEmitter(static_cast<AudioEmitter&>(_element));
    } break;
    default:
        break;
    }
}

template <>
const ux3d::core::LinkedList<Scene>& Gltf::getElements() const { return m_scenes; }
template <>
const ux3d::core::LinkedList<Node>& Gltf::getElements() const { return m_nodes; }
template <>
const ux3d::core::LinkedList<Camera>& Gltf::getElements() const { return m_cameras; }
template <>
const ux3d::core::LinkedList<Light>& Gltf::getElements() const { return m_lights; }
template <>
const ux3d::core::LinkedList<Mesh>& Gltf::getElements() const { return m_meshes; }
template <>
const ux3d::core::LinkedList<Accessor>& Gltf::getElements() const { return m_accessors; }
template <>
const ux3d::core::LinkedList<Buffer>& Gltf::getElements() const { return m_buffers; }
template <>
const ux3d::core::LinkedList<BufferView>& Gltf::getElements() const { return m_bufferViews; }
template <>
const ux3d::core::LinkedList<Image>& Gltf::getElements() const { return m_images; }
template <>
const ux3d::core::LinkedList<Material>& Gltf::getElements() const { return m_materials; }
template <>
const ux3d::core::LinkedList<Texture>& Gltf::getElements() const { return m_textures; }
template <>
const ux3d::core::LinkedList<Sampler>& Gltf::getElements() const { return m_samplers; }
template <>
const ux3d::core::LinkedList<Animation>& Gltf::getElements() const { return m_animations; }
template <>
const ux3d::core::LinkedList<Skin>& Gltf::getElements() const { return m_skins; }
template <>
const ux3d::core::LinkedList<MaterialVariant>& Gltf::getElements() const { return m_materialVariants; }
template <>
const ux3d::core::LinkedList<AudioEmitter>& Gltf::getElements() const { return m_audioEmitters; }
template <>
const ux3d::core::LinkedList<AudioSource>& Gltf::getElements() const { return m_audioSources; }

template <>
void Gltf::deleteElement(Scene& _element) { deleteScene(_element); }
template <>
void Gltf::deleteElement(Node& _element) { deleteNode(_element); }
template <>
void Gltf::deleteElement(Camera& _element) { deleteCamera(_element); }
template <>
void Gltf::deleteElement(Light& _element) { deleteLight(_element); }
template <>
void Gltf::deleteElement(Mesh& _element) { deleteMesh(_element); }
template <>
void Gltf::deleteElement(Accessor& _element) { deleteAccessor(_element); }
template <>
void Gltf::deleteElement(Buffer& _element) { deleteBuffer(_element); }
template <>
void Gltf::deleteElement(BufferView& _element) { deleteBufferView(_element); }
template <>
void Gltf::deleteElement(Image& _element) { deleteImage(_element); }
template <>
void Gltf::deleteElement(Material& _element) { deleteMaterial(_element); }
template <>
void Gltf::deleteElement(Texture& _element) { deleteTexture(_element); }
template <>
void Gltf::deleteElement(Sampler& _element) { deleteSampler(_element); }
template <>
void Gltf::deleteElement(Animation& _element) { deleteAnimation(_element); }
template <>
void Gltf::deleteElement(Skin& _element) { deleteSkin(_element); }
template <>
void Gltf::deleteElement(MaterialVariant& _element) { deleteMaterialVariant(_element); }
template <>
void Gltf::deleteElement(AudioSource& _element) { deleteAudioSource(_element); }
template <>
void Gltf::deleteElement(AudioEmitter& _element) { deleteAudioEmitter(_element); }

template <>
uint32_t& Gltf::id<Scene>() { return m_elementIds.nextScenesId; }
template <>
uint32_t& Gltf::id<Node>() { return m_elementIds.nextNodesId; }
template <>
uint32_t& Gltf::id<Camera>() { return m_elementIds.nextCamerasId; }
template <>
uint32_t& Gltf::id<Light>() { return m_elementIds.nextLightsId; }
template <>
uint32_t& Gltf::id<Mesh>() { return m_elementIds.nextMeshesId; }
template <>
uint32_t& Gltf::id<Accessor>() { return m_elementIds.nextAccessorsId; }
template <>
uint32_t& Gltf::id<Buffer>() { return m_elementIds.nextBuffersId; }
template <>
uint32_t& Gltf::id<BufferView>() { return m_elementIds.nextBufferViewsId; }
template <>
uint32_t& Gltf::id<Image>() { return m_elementIds.nextImagesId; }
template <>
uint32_t& Gltf::id<Material>() { return m_elementIds.nextMaterialsId; }
template <>
uint32_t& Gltf::id<Texture>() { return m_elementIds.nextTexturesId; }
template <>
uint32_t& Gltf::id<Sampler>() { return m_elementIds.nextSamplersId; }
template <>
uint32_t& Gltf::id<Animation>() { return m_elementIds.nextAnimationsId; }
template <>
uint32_t& Gltf::id<Skin>() { return m_elementIds.nextSkinsId; }
template <>
uint32_t& Gltf::id<MaterialVariant>() { return m_elementIds.nextMaterialVariantsId; }
template <>
uint32_t& Gltf::id<AudioSource>() { return m_elementIds.nextAudioSourcesId; }
template <>
uint32_t& Gltf::id<AudioEmitter>() { return m_elementIds.nextAudioEmittersId; }

template <>
Node* Gltf::duplicate(const Node& _source)
{
    Node* pResult = m_nodes.emplace_back(getNextGltfTypeID<Node>(), _source);
    if (pResult == nullptr) {
        return nullptr;
    }

    duplicateChildren(_source, *pResult);

    return pResult;
}

template <>
Scene* Gltf::duplicate(const Scene& _source)
{
    Scene* pScene = m_scenes.emplace_back(getNextGltfTypeID<Scene>(), _source);
    if (pScene == nullptr) {
        return nullptr;
    }

    for (const Node* pSourceNode : _source.getNodes()) {
        Node* pNode = m_nodes.emplace_back(getNextGltfTypeID<Node>(), *pSourceNode);
        if (pNode == nullptr) {
            continue;
        }
        if (core::Failed(pScene->addNode(*pNode, *this))) {
            continue;
        }

        duplicateChildren(*pSourceNode, *pNode);
    }

    return pScene;
}

void Gltf::duplicateChildren(const Node& _sourceNode, Node& _node)
{
    core::Vector<Node*> nodes = { &_node };
    core::Vector<const Node*> sourceNodes = { &_sourceNode };

    for (size_t index = 0; index < nodes.size() && index < sourceNodes.size(); index++) {
        Node* pParent = nodes[index];
        const Node* pSourceParent = sourceNodes[index];

        for (const Node* pSourceChild : pSourceParent->getChildren()) {
            Node* pChild = m_nodes.emplace_back(getNextGltfTypeID<Node>(), *pSourceChild);
            if (pChild == nullptr) {
                continue;
            }
            if (core::Failed(pParent->addChild(*pChild, *this))) {
                continue;
            }
            if (nodes.emplace_back(pChild) == nullptr) {
                continue;
            }
            if (sourceNodes.emplace_back(pSourceChild) == nullptr) {
                nodes.erase(nodes.back_ref());
            }
        }
    }
}

core::Vector<TextureInfo*> Gltf::getTextureInfos()
{
    core::Vector<TextureInfo*> result;
    for (Material& mat : getMaterials()) {
        if (auto& textureInfo = mat.getNormalTextureInfo();
            textureInfo.has_value()) {
            result.emplace_back(&textureInfo.value());
        }

        if (auto& textureInfo = mat.getOcclusionTextureInfo();
            textureInfo.has_value()) {
            result.emplace_back(&textureInfo.value());
        }

        if (auto& textureInfo = mat.getEmissiveTextureInfo();
            textureInfo.has_value()) {
            result.emplace_back(&textureInfo.value());
        }

        if (auto& clearcoat = mat.getClearcoat();
            clearcoat.has_value()) {
            if (auto& textureInfo = clearcoat.value().getClearcoatNormalTexture();
                textureInfo.has_value()) {
                result.emplace_back(&textureInfo.value());
            }

            if (auto& textureInfo = clearcoat.value().getClearcoatRoughnessTexture();
                textureInfo.has_value()) {
                result.emplace_back(&textureInfo.value());
            }

            if (auto& textureInfo = clearcoat.value().getClearcoatTexture();
                textureInfo.has_value()) {
                result.emplace_back(&textureInfo.value());
            }
        }

        if (auto& metallicRoughness = mat.getPbrMetallicRoughness();
            metallicRoughness.has_value()) {
            if (auto& textureInfo = metallicRoughness.value().getBaseColorTextureInfo();
                textureInfo.has_value()) {
                result.emplace_back(&textureInfo.value());
            }

            if (auto& textureInfo = metallicRoughness.value().getMetallicRoughnessTextureInfo();
                textureInfo.has_value()) {
                result.emplace_back(&textureInfo.value());
            }
        }

        if (auto& specularGlossiness = mat.getPbrSpecularGlossiness();
            specularGlossiness.has_value()) {
            if (auto& textureInfo = specularGlossiness.value().getDiffuseTextureInfo();
                textureInfo.has_value()) {
                result.emplace_back(&textureInfo.value());
            }

            if (auto& textureInfo = specularGlossiness.value().getSpecularGlossinessTextureInfo();
                textureInfo.has_value()) {
                result.emplace_back(&textureInfo.value());
            }
        }

        if (auto& sheen = mat.getSheen();
            sheen.has_value()) {
            if (auto& textureInfo = sheen.value().getSheenColorTextureInfo();
                textureInfo.has_value()) {
                result.emplace_back(&textureInfo.value());
            }

            if (auto& textureInfo = sheen.value().getSheenRoughnessTextureInfo();
                textureInfo.has_value()) {
                result.emplace_back(&textureInfo.value());
            }
        }

        if (auto& specular = mat.getSpecular();
            specular.has_value()) {
            if (auto& textureInfo = specular.value().getSpecularColorTextureInfo();
                textureInfo.has_value()) {
                result.emplace_back(&textureInfo.value());
            }

            if (auto& textureInfo = specular.value().getSpecularTextureInfo();
                textureInfo.has_value()) {
                result.emplace_back(&textureInfo.value());
            }
        }

        if (auto& transmission = mat.getTransmission();
            transmission.has_value()) {
            if (auto& textureInfo = transmission.value().getTransmissionTextureInfo();
                textureInfo.has_value()) {
                result.emplace_back(&textureInfo.value());
            }
        }

        if (auto& volume = mat.getVolume();
            volume.has_value()) {
            if (auto& textureInfo = volume.value().getThicknessTextureInfo();
                textureInfo.has_value()) {
                result.emplace_back(&textureInfo.value());
            }
        }
    }
    return result;
}
