#include "gltf/ChildCollector.h"
#include "core/Algorithms.h"
#include "gltf/ChildOfRoot.h"
#include "gltf/Gltf.h"
#include "gltf/GltfObject.h"
#include "gltf/GltfType.h"

using namespace ux3d;
using namespace ux3d::gltf;

ChildCollector::ChildCollector(const Gltf& _root)
{
    m_dirtyState = true;
    m_childOfRootList.clear();
    m_gltfObjectList.clear();

    addInitialElements(_root.getScenes());
    addInitialElements(_root.getNodes());
    addInitialElements(_root.getCameras());
    addInitialElements(_root.getLights());
    addInitialElements(_root.getMeshes());
    addInitialElements(_root.getAccessors());
    addInitialElements(_root.getBuffers());
    addInitialElements(_root.getBufferViews());
    addInitialElements(_root.getImages());
    addInitialElements(_root.getMaterials());
    addInitialElements(_root.getTextures());
    addInitialElements(_root.getSamplers());
    addInitialElements(_root.getAnimations());
    addInitialElements(_root.getSkins());
    addInitialElements(_root.getAudioEmitters());
    addInitialElements(_root.getAudioSources());

    addRootGltfObjects(&_root);
}

template <typename ChildOfRootType>
void ChildCollector::addInitialElements(const core::LinkedList<ChildOfRootType>& _elements)
{
    for (auto& element : _elements) {
        m_childOfRootList.push_back(&element);
    }
}

ChildCollector::ChildCollector(const core::Vector<const ChildOfRoot*>& _initialList)
{
    m_dirtyState = true;
    m_gltfObjectList.clear();
    m_childOfRootList.assign(_initialList);
}

void ChildCollector::collect(core::Vector<const ChildOfRoot*>& _outCompleteList)
{
    if (m_dirtyState) {
        iterativeCollect();
        m_dirtyState = false;
    }
    _outCompleteList.assign(m_childOfRootList);
}

void ChildCollector::collectGltfObjects(core::Vector<const GltfObject*>& _outCompleteList)
{
    if (m_dirtyState) {
        iterativeCollect();
        m_dirtyState = false;
    }

    for (const GltfObject* element : m_gltfObjectList) {
        _outCompleteList.emplace_back(element);
    }

    for (const ChildOfRoot* element : m_childOfRootList) {
        _outCompleteList.emplace_back(static_cast<const GltfObject*>(element));
    }
}

void ChildCollector::iterativeCollect()
{

    for (size_t i = 0; i < m_childOfRootList.size(); ++i) {
        addChildrenToWorkList(m_childOfRootList[i]);
    }
}

void ChildCollector::addChildrenToWorkList(const ChildOfRoot* parent)
{
    switch (parent->getGltfType()) {
    case GltfType::Scene:
        addSceneChildren(static_cast<const Scene*>(parent));
        break;
    case GltfType::Node:
        addNodeChildren(static_cast<const Node*>(parent));
        break;
    case GltfType::Camera:
        addCameraChildren(static_cast<const Camera*>(parent));
        break;
    case GltfType::Mesh:
        addMeshChildren(static_cast<const Mesh*>(parent));
        break;
    case GltfType::Material:
        addMaterialChildren(static_cast<const Material*>(parent));
        break;
    case GltfType::Texture:
        addTextureChildren(static_cast<const Texture*>(parent));
        break;
    case GltfType::Image:
        addImageChildren(static_cast<const Image*>(parent));
        break;
    case GltfType::Sampler:
        // no children
        break;
    case GltfType::Accessor:
        addAccessorChildren(static_cast<const Accessor*>(parent));
        break;
    case GltfType::BufferView:
        addBufferViewChildren(static_cast<const BufferView*>(parent));
        break;
    case GltfType::Buffer:
        // no children
        break;
    case GltfType::Skin:
        addSkinChildren(static_cast<const Skin*>(parent));
        break;
    case GltfType::Animation:
        addAnimationChildren(static_cast<const Animation*>(parent));
        break;
    case GltfType::Light:
        addLightChildren(static_cast<const Light*>(parent));
        break;
    case GltfType::AudioEmitter:
        addAudioEmitterChildren(static_cast<const AudioEmitter*>(parent));
        break;
    case GltfType::AudioSource:
        addAudioSourceChildren(static_cast<const AudioSource*>(parent));
        break;
    case GltfType::Undefined:
    default:
        break;
    }
}

void ChildCollector::addSceneChildren(const Scene* scene)
{
    // nodes, ibl
    core::Vector<const ChildOfRoot*> nodes;
    nodes.assign(scene->getNodes());
    addToWorkList(nodes);

    if (scene->getAudioEmitterPointerObject().has_value()) {
        addToGltfObjectList(&scene->getAudioEmitterPointerObject().value());

        for (const ChildOfRoot* element : scene->getAudioEmitters()) {
            addToWorkList(element);
        }
    }
}

void ChildCollector::addNodeChildren(const Node* node)
{
    // node, skin, camera, mesh, light
    core::Vector<const ChildOfRoot*> nodes;
    nodes.assign(node->getChildren());
    addToWorkList(nodes);

    const ChildOfRoot* skin = node->getSkin();
    addToWorkList(skin);

    const ChildOfRoot* camera = node->getCamera();
    addToWorkList(camera);

    const ChildOfRoot* mesh = node->getMesh();
    addToWorkList(mesh);

    const ChildOfRoot* light = node->getLight();
    addToWorkList(light);

    if (node->getLightPointerObject().has_value()) {
        addToGltfObjectList(&node->getLightPointerObject().value());
    }

    if (node->hasAudio()) {
        addToGltfObjectList(&node->getAudioEmitterPointerObject().value());

        const AudioEmitter* audioEmitter = node->getAudioEmitter();
        addToWorkList(audioEmitter);
    }
}

void ChildCollector::addCameraChildren(const Camera* _camera)
{
    if (_camera->getCameraOrthographic().has_value()) {
        addToGltfObjectList(&_camera->getCameraOrthographic().value());
    }

    if (_camera->getCameraPerspective().has_value()) {
        addToGltfObjectList(&_camera->getCameraPerspective().value());
    }
}

void ChildCollector::addMeshChildren(const Mesh* mesh)
{
    // accessor, material
    const core::LinkedList<Primitive>& primitiveList = mesh->getPrimitives();
    for (Primitive& primitive : primitiveList) {
        addToGltfObjectList(&primitive);

        const ChildOfRoot* accessor = primitive.getIndicesAccessor();
        addToWorkList(accessor);

        const ChildOfRoot* material = primitive.getMaterial();
        addToWorkList(material);

        const core::LinkedList<Attribute>& attributeList = primitive.getAttributes();
        for (Attribute& attribute : attributeList) {
            const ChildOfRoot* attributeAccessor = attribute.getAccessor();
            addToWorkList(attributeAccessor);
        }

        const core::LinkedList<MorphTarget>& targetList = primitive.getTargets();
        for (MorphTarget& target : targetList) {
            addToWorkList(target.getNormalAccessor());
            addToWorkList(target.getPositionAccessor());
            addToWorkList(target.getTangentAccessor());
            addToWorkList(target.getColor0Accessor());
            addToWorkList(target.getTexCoord0Accessor());
            addToWorkList(target.getTexCoord1Accessor());
        }

        const auto& oDraco = primitive.getDracoMeshCompression();
        if (oDraco.has_value()) {
            addToGltfObjectList(&oDraco.value());
        }

        if (primitive.hasMaterialVariantsMapping()) {
            for (auto mapping : primitive.getMaterialVariantsMapping()->getMappings()) {
                const ChildOfRoot* material = mapping.material;
                addToWorkList(material);
            }
        }
    }
}

void ChildCollector::addMaterialChildren(const Material* material)
{
    // textures

    if (material->getEmissiveTextureInfo().has_value()) {
        addToGltfObjectList(&material->getEmissiveTextureInfo().value());
        addToWorkList(material->getEmissiveTextureInfo()->getTexture());
    }

    if (material->getNormalTextureInfo().has_value()) {
        addToGltfObjectList(&material->getNormalTextureInfo().value());
        addToWorkList(material->getNormalTextureInfo()->getTexture());
    }

    if (material->getOcclusionTextureInfo().has_value()) {
        addToGltfObjectList(&material->getOcclusionTextureInfo().value());
        addToWorkList(material->getOcclusionTextureInfo()->getTexture());
    }

    if (material->getPbrMetallicRoughness().has_value()) {
        addToGltfObjectList(&material->getPbrMetallicRoughness().value());

        if (material->getPbrMetallicRoughness()->getBaseColorTextureInfo().has_value()) {
            addToGltfObjectList(&material->getPbrMetallicRoughness()->getBaseColorTextureInfo().value());

            if (material->getPbrMetallicRoughness()->getBaseColorTextureInfo()->getTextureTransform().has_value()) {
                addToGltfObjectList(&material->getPbrMetallicRoughness()->getBaseColorTextureInfo()->getTextureTransform().value());
            }

            addToWorkList(material->getPbrMetallicRoughness()->getBaseColorTextureInfo()->getTexture());
        }
        if (material->getPbrMetallicRoughness()->getMetallicRoughnessTextureInfo().has_value()) {
            addToGltfObjectList(&material->getPbrMetallicRoughness()->getMetallicRoughnessTextureInfo().value());
            addToWorkList(material->getPbrMetallicRoughness()->getMetallicRoughnessTextureInfo()->getTexture());
        }
    }

    if (material->getPbrSpecularGlossiness().has_value()) {
        addToGltfObjectList(&material->getPbrSpecularGlossiness().value());

        if (material->getPbrSpecularGlossiness()->getDiffuseTextureInfo().has_value()) {
            addToGltfObjectList(&material->getPbrSpecularGlossiness()->getDiffuseTextureInfo().value());
            addToWorkList(material->getPbrSpecularGlossiness()->getDiffuseTextureInfo()->getTexture());
        }
        if (material->getPbrSpecularGlossiness()->getSpecularGlossinessTextureInfo().has_value()) {
            addToGltfObjectList(&material->getPbrSpecularGlossiness()->getSpecularGlossinessTextureInfo().value());
            addToWorkList(material->getPbrSpecularGlossiness()->getSpecularGlossinessTextureInfo()->getTexture());
        }
    }

    if (material->getUnlit().has_value()) {
        addToGltfObjectList(&material->getUnlit().value());
    }

    if (material->getClearcoat().has_value()) {
        addToGltfObjectList(&material->getClearcoat().value()); // Extension -> GltfObject

        if (material->getClearcoat()->getClearcoatNormalTexture().has_value()) {
            addToGltfObjectList(&material->getClearcoat()->getClearcoatNormalTexture().value()); // TextureInfo -> GltfObject
            addToWorkList(material->getClearcoat()->getClearcoatNormalTexture()->getTexture()); // Texture -> ChildOfRoot
        }
        if (material->getClearcoat()->getClearcoatRoughnessTexture().has_value()) {
            addToGltfObjectList(&material->getClearcoat()->getClearcoatRoughnessTexture().value());
            addToWorkList(material->getClearcoat()->getClearcoatRoughnessTexture()->getTexture());
        }
        if (material->getClearcoat()->getClearcoatTexture().has_value()) {
            addToGltfObjectList(&material->getClearcoat()->getClearcoatTexture().value());
            addToWorkList(material->getClearcoat()->getClearcoatTexture()->getTexture());
        }
    }

    if (material->getSpecular().has_value()) {
        addToGltfObjectList(&material->getSpecular().value());

        if (material->getSpecular()->getSpecularTextureInfo().has_value()) {
            addToGltfObjectList(&material->getSpecular()->getSpecularTextureInfo().value());
            addToWorkList(material->getSpecular()->getSpecularTextureInfo()->getTexture());
        }
        if (material->getSpecular()->getSpecularColorTextureInfo().has_value()) {
            addToGltfObjectList(&material->getSpecular()->getSpecularColorTextureInfo().value());
            addToWorkList(material->getSpecular()->getSpecularColorTextureInfo()->getTexture());
        }
    }

    if (material->getSheen().has_value()) {
        addToGltfObjectList(&material->getSheen().value());

        if (material->getSheen()->getSheenColorTextureInfo().has_value()) {
            addToGltfObjectList(&material->getSheen()->getSheenColorTextureInfo().value());
            addToWorkList(material->getSheen()->getSheenColorTextureInfo()->getTexture());
        }
        if (material->getSheen()->getSheenRoughnessTextureInfo().has_value()) {
            addToGltfObjectList(&material->getSheen()->getSheenRoughnessTextureInfo().value());
            addToWorkList(material->getSheen()->getSheenRoughnessTextureInfo()->getTexture());
        }
    }

    if (material->getTransmission().has_value()) {
        addToGltfObjectList(&material->getTransmission().value());

        if (material->getTransmission()->getTransmissionTextureInfo().has_value()) {
            addToGltfObjectList(&material->getTransmission()->getTransmissionTextureInfo().value());
            addToWorkList(material->getTransmission()->getTransmissionTextureInfo()->getTexture());
        }
    }

    if (material->getVolume().has_value()) {
        addToGltfObjectList(&material->getVolume().value());

        if (material->getVolume()->getThicknessTextureInfo().has_value()) {
            addToGltfObjectList(&material->getVolume()->getThicknessTextureInfo().value());
            addToWorkList(material->getVolume()->getThicknessTextureInfo()->getTexture());
        }
    }

    if (material->getIridescence().has_value()) {
        addToGltfObjectList(&material->getIridescence().value());

        if (material->getIridescence()->getIridescenceTextureInfo().has_value()) {
            addToGltfObjectList(&material->getIridescence()->getIridescenceTextureInfo().value());
            addToWorkList(material->getIridescence()->getIridescenceTextureInfo()->getTexture());
        }

        if (material->getIridescence()->getIridescenceThicknessTextureInfo().has_value()) {
            addToGltfObjectList(&material->getIridescence()->getIridescenceThicknessTextureInfo().value());
            addToWorkList(material->getIridescence()->getIridescenceThicknessTextureInfo()->getTexture());
        }
    }

    if (material->getAnisotropy().has_value()) {
        addToGltfObjectList(&material->getAnisotropy().value());

        if (material->getAnisotropy()->getAnisotropyTextureInfo().has_value()) {
            addToGltfObjectList(&material->getAnisotropy()->getAnisotropyTextureInfo().value());
            addToWorkList(material->getAnisotropy()->getAnisotropyTextureInfo()->getTexture());
        }
    }

    if (material->getEmissiveStrength().has_value()) {
        addToGltfObjectList(&material->getEmissiveStrength().value());
    }
}

void ChildCollector::addTextureChildren(const Texture* texture)
{
    // image, sampler
    addToWorkList(texture->getSource());
    addToWorkList(texture->getSampler());
    if (texture->getBasisU().has_value()) {
        addToWorkList(texture->getBasisU()->getSource());
    }
    if (texture->getWebP().has_value()) {
        addToWorkList(texture->getWebP()->getSource());
    }
    if (texture->getTextureDds().has_value()) {
        addToWorkList(texture->getTextureDds()->getSource());
    }
}

void ChildCollector::addImageChildren(const Image* image)
{
    addToWorkList(image->getBufferView());
}

void ChildCollector::addAccessorChildren(const Accessor* accessor)
{
    // bufferView, sparse bufferViews
    addToWorkList(accessor->getBufferView());

    const auto& sparse = accessor->getSparse();

    if (sparse.has_value()) {
        addToGltfObjectList(&sparse.value());

        addToGltfObjectList(&sparse->getIndices());
        addToWorkList(sparse->getIndices().getBufferView());

        addToGltfObjectList(&sparse->getValues());
        addToWorkList(sparse->getValues().getBufferView());
    }
}

void ChildCollector::addBufferViewChildren(const BufferView* bufferView)
{
    // buffer
    addToWorkList(bufferView->getBuffer());
}

void ChildCollector::addSkinChildren(const Skin* skin)
{
    // accessor, node
    addToWorkList(skin->getInverseBindMatricesAccessor());
    addToWorkList(skin->getSkeletonRoot());
    core::Vector<const ChildOfRoot*> joints;
    joints.assign(skin->getJoints());
    addToWorkList(joints);
}

void ChildCollector::addAnimationChildren(const Animation* animation)
{
    // accessors, nodes
    const core::LinkedList<AnimationChannel>& channels = animation->getChannels();
    for (AnimationChannel& channel : channels) {
        addToGltfObjectList(&channel);

        if (channel.getSampler() != nullptr) {
            addToGltfObjectList(channel.getSampler());
            addToWorkList(channel.getSampler()->getInputAccessor());
            addToWorkList(channel.getSampler()->getOutputAccessor());
        }

        addToGltfObjectList(&channel.getTarget());
        addToWorkList(channel.getTarget().getNode());
        if (channel.getTarget().m_animationPointer.has_value()) {
            auto [gltfObject, localPath] = channel.getTarget().m_animationPointer.value();
            addToWorkList(gltfObject);
        }
    }

    const core::LinkedList<AnimationSampler>& samplers = animation->getSamplers();
    for (AnimationSampler& sampler : samplers) {
        addToGltfObjectList(&sampler);
        addToWorkList(sampler.getInputAccessor());
        addToWorkList(sampler.getOutputAccessor());
    }
}

void ux3d::gltf::ChildCollector::addLightChildren(const Light* _light)
{
    if (_light->getSpot().has_value()) {
        addToGltfObjectList(&_light->getSpot().value());
    }
}

void ux3d::gltf::ChildCollector::addAudioEmitterChildren(const AudioEmitter* _emitter)
{
    if (_emitter->getSource() != nullptr) {
        addToWorkList(_emitter->getSource());
    }
}

void ux3d::gltf::ChildCollector::addAudioSourceChildren(const AudioSource* _source)
{
    if (_source->hasBufferView()) {
        addToWorkList(_source->getBufferView());
    }
}

void ux3d::gltf::ChildCollector::addRootGltfObjects(const Gltf* _gltf)
{
    m_gltfObjectList.emplace_back(_gltf);

    if (_gltf->getLightsPunctual().has_value()) {
        addToGltfObjectList(&_gltf->getLightsPunctual().value());
    }
    if (_gltf->hasMaterialVariants()) {
        addToGltfObjectList(&_gltf->getMaterialVariantsOptional().value());
    }
    if (_gltf->hasAudio()) {
        addToGltfObjectList(&_gltf->getAudio().value());
    }

    addToGltfObjectList(&_gltf->getAsset());
}

void ChildCollector::addToWorkList(const core::Vector<const ChildOfRoot*>& _elementList)
{
    for (const ChildOfRoot* element : _elementList) {
        addToWorkList(element);
    }
}

void ChildCollector::addToWorkList(const ChildOfRoot* _element)
{
    if (_element == nullptr) {
        return;
    }

    if (core::contains(m_childOfRootList, _element)) {
        return;
    }

    m_childOfRootList.push_back(_element);
}

void ux3d::gltf::ChildCollector::addToGltfObjectList(const GltfObject* _element)
{
    if (_element == nullptr) {
        return;
    }

    if (core::contains(m_gltfObjectList, _element)) {
        return;
    }

    m_gltfObjectList.push_back(_element);
}
