// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Node.h"
#include "gltf/AudioEmitter.h"
#include "gltf/Camera.h"
#include "gltf/Gltf.h"
#include "gltf/Light.h"
#include "gltf/Mesh.h"
#include "gltf/Scene.h"
#include "gltf/Skin.h"

#include "math/MatrixFunctions.h"
#include "math/VectorFunctions.h"

using namespace ux3d;
using namespace ux3d::gltf;

Node::Node(uint32_t _id)
    : ChildOfRoot(_id)
    , HelperVisible()
{
}

Node::Node(uint32_t _id, const Node& _source)
    : ChildOfRoot(_id, _source)
    , HelperVisible()
    , m_pMesh(_source.m_pMesh)
    , m_pCamera(_source.m_pCamera)
    , m_pSkin(_source.m_pSkin)
    , m_lightPointerObject(_source.m_lightPointerObject)
    , m_audioEmitterPointerObject(_source.m_audioEmitterPointerObject)
    , m_Scale(_source.m_Scale)
    , m_Rotation(_source.m_Rotation)
    , m_Translation(_source.m_Translation)
    , m_xmpPacket(_source.m_xmpPacket)
    , weights(_source.weights)
{
    // we can't copy the children because Nodes can only have one parent
}

GltfResult Node::getChildrenRecursively(ux3d::core::Vector<Node*>& _recursiveChildren)
{
    for (Node* pChild : m_Children) {
        if (!_recursiveChildren.push_back(pChild)) {
            return GltfError::AllocationFailure;
        }

        GltfResult result = pChild->getChildrenRecursively(_recursiveChildren);
        if (result != GltfResult::Success) {
            return result;
        }
    }

    return GltfResult::Success;
}

GltfResult Node::addChild(Node& _child, const Gltf& _root)
{
    core::string message;
    GltfResult result = canAddChild(_child, message, _root);

    if (core::Failed(result)) {
        core::logWarning(message.c_str());
        return result;
    }

    if (m_Children.emplace_back(&_child) == nullptr) {
        return GltfError::AllocationFailure;
    }

    return GltfResult::Success;
}

GltfResult Node::canAddChild(Node& _child, const Gltf& _root)
{
    core::string message;
    return canAddChild(_child, message, _root);
}

GltfResult Node::canAddChild(Node& _child, core::string& _message, const Gltf& _root)
{
    const core::string prefix = "Cannot add node as child ";

    if (&_child == this) {
        _message = prefix + "to itself";
        return GltfError::SelfReference;
    }
    if (core::contains(m_Children, &_child)) {
        _message = prefix + "because it is already a child of this node";
        return GltfError::ObjectAlreadyExists;
    }

    core::Vector<Node*> childsChildren;
    if (core::Failed(_child.getChildrenRecursively(childsChildren))) {
        return GltfError::AllocationFailure;
    }
    if (core::contains(childsChildren, this)) {
        _message = prefix + "because it is (directly or implicitly) a parent of this node";
        return GltfError::HierarchyRecursion;
    }

    if (_child.hasParent(_root)) {
        _message = prefix + "because it already has a parent (node or scene)";
        return GltfError::NodeHasParent;
    }

    return GltfResult::Success;
}

GltfResult Node::insertChild(Node& _Child, const size_t _Index, const Gltf& _root)
{
    GltfResult result = addChild(_Child, _root);
    if (result != GltfResult::Success) {
        return result;
    }

    return moveChild(m_Children.size() - 1, _Index);
}

GltfResult Node::moveChild(const size_t _FromIndex, const size_t _ToIndex)
{
    if (_FromIndex >= m_Children.size() || _ToIndex >= m_Children.size()) {
        return GltfError::ObjectNotFound;
    }

    bool increase = _FromIndex < _ToIndex;

    for (size_t i = _FromIndex; i != _ToIndex; i = increase ? i + 1 : i - 1) {
        size_t swapIndex = increase ? i + 1 : i - 1;
        Node* pTemp = m_Children[i];
        m_Children[i] = m_Children[swapIndex];
        m_Children[swapIndex] = pTemp;
    }

    return GltfResult::Success;
}

GltfResult Node::removeChild(Node& _Child)
{
    for (auto it = m_Children.begin(), end = m_Children.end(); it != end; ++it) {
        if (*it == &_Child) {
            m_Children.erase_keep_order(it);
            return GltfResult::Success;
        }
    }

    return GltfError::ObjectNotFound;
}

GltfResult Node::clearChildren()
{
    m_Children.clear();
    return GltfResult::Success;
}

bool Node::isAnimationTarget(const Gltf& _root, std::optional<AnimationPath> targetPath) const
{
    for (auto& animation : _root.getAnimations()) {
        for (auto& channel : animation.getChannels()) {
            std::string activeLocalPath = gltf::animationPathToString(channel.getTarget().getPath());
            const gltf::ChildOfRoot* activeChildOfRoot = channel.getTarget().getNode();
            if (channel.getTarget().m_animationPointer.has_value()) {
                // if animation pointer extension is set we use these values
                activeLocalPath = std::get<1>(channel.getTarget().m_animationPointer.value());
                activeChildOfRoot = std::get<0>(channel.getTarget().m_animationPointer.value());
            }

            if (activeChildOfRoot == this) {
                if (targetPath.has_value()) {
                    // if a target path is also given as parameter, we also check if the path is matching
                    if (gltf::animationPathToString(targetPath.value()) == activeLocalPath) {
                        return true;
                    }
                } else {
                    return true;
                }
            }
        }
    }
    return false;
}

GltfResult Node::setMesh(Mesh& _Mesh)
{
    m_pMesh = &_Mesh;
    return GltfResult::Success;
}

GltfResult Node::setCamera(Camera& _Camera)
{
    m_pCamera = &_Camera;
    return GltfResult::Success;
}

Light* Node::getLight()
{
    if (m_lightPointerObject.has_value()) {
        return m_lightPointerObject->m_pLight;
    }
    return nullptr;
}

const Light* Node::getLight() const
{
    if (m_lightPointerObject.has_value()) {
        return m_lightPointerObject->m_pLight;
    }
    return nullptr;
}

GltfResult Node::setLight(Light& _Light)
{
    if (!m_lightPointerObject.has_value()) {
        m_lightPointerObject.emplace();
    }
    m_lightPointerObject->m_pLight = &_Light;
    return GltfResult::Success;
}

AudioEmitter* Node::getAudioEmitter()
{
    if (m_audioEmitterPointerObject.has_value()) {
        return m_audioEmitterPointerObject->m_pEmitter;
    }
    return nullptr;
}

const AudioEmitter* Node::getAudioEmitter() const
{
    if (m_audioEmitterPointerObject.has_value()) {
        return m_audioEmitterPointerObject->m_pEmitter;
    }
    return nullptr;
}

void Node::setAudioEmitter(AudioEmitter& emitter)
{
    if (!m_audioEmitterPointerObject.has_value()) {
        m_audioEmitterPointerObject.emplace();
    }
    m_audioEmitterPointerObject->m_pEmitter = &emitter;
}

GltfResult Node::setSkin(Skin& _Skin)
{
    m_pSkin = &_Skin;
    return GltfResult::Success;
}

ux3d::math::dvec3 Node::getScaleOrDefault() const
{
    return m_Scale.value_or(math::dvec3(1.0, 1.0, 1.0));
}

GltfResult Node::setScale(const math::dvec3& _Scale)
{
    m_Scale.emplace(_Scale);
    return GltfResult::Success;
}

GltfResult Node::setTranslation(const math::dvec3& _Translation)
{
    m_Translation.emplace(_Translation);
    return GltfResult::Success;
}

ux3d::math::dvec3 Node::getTranslationOrDefault() const
{
    return m_Translation.value_or(math::dvec3(0.0, 0.0, 0.0));
}

GltfResult Node::setRotation(const math::dquat& _Rotation)
{
    GltfResult result = GltfResult::Success;

    if (math::isNormalized(_Rotation) == false) {
        result = GltfError::InvalidInput;
    }

    // emplace rotation anyway
    m_Rotation.emplace(_Rotation);
    return result;
}

ux3d::math::dquat Node::getRotationOrDefault() const
{
    return m_Rotation.value_or(math::dquat(1, 0, 0, 0));
}

GltfResult Node::decomposeTransformMatrix(const math::dmat4x4& _TransformMatrix)
{
    math::dvec3 translation;
    math::dvec3 scale;
    math::dquat rotation;
    math::dvec4 perspective;
    math::dvec3 skew;
    if (math::decompose(_TransformMatrix, scale, rotation, translation, skew, perspective)) {
        setTranslation(translation);
        setScale(scale);
        setRotation(rotation);
        return GltfResult::Success;
    } else {
        core::logWarning("Transformation matrix is not decomposable. Using default values instead");
        return GltfError::InvalidInput;
    }
}

ux3d::math::dmat4x4 Node::getTransformMatrix() const
{
    if (hasRotation() || hasScale() || hasTranslation()) {
        // TODO: write a simple getter for this
        constexpr math::dmat4 identity = math::identity<math::dmat4>();

        math::dmat4 scale = math::scale(identity, getScaleOrDefault());
        math::dmat4 rotation = math::mat4_cast(getRotationOrDefault());
        math::dmat4 translation = math::translate(identity, getTranslationOrDefault());

        return translation * rotation * scale;
    }

    return math::identity<math::dmat4>();
}

bool Node::isJoint(const Gltf& gltf) const
{
    for (auto& skin : gltf.getSkins()) {
        for (auto node : skin.getJoints()) {
            if (node == this) {
                return true;
            }
        }
    }
    return false;
}

void Node::storeAsMatrix(bool _asMatrix, const Gltf* gltfForJointResolve)
{
    if (gltfForJointResolve != nullptr && _asMatrix && isJoint(*gltfForJointResolve)) {
        core::logWarning("Using matrix transform for a joint might cause issues with some renderers and causes an invalid glTF if used in an animtion. To resolve all joints to TRS use the menu action in the Automation menu.");
    }
    m_storeAsMatrix = _asMatrix;
}

Extensions Node::getUsedExtensions() const
{
    if (m_lightPointerObject.has_value()) {
        return Extension::PunctualLights;
    }

    return Extension::None;
}

bool Node::hasParent(const Gltf& _root) const
{
    if (isParentAScene(_root)) {
        return true;
    }

    for (const Node& node : _root.getNodes()) {
        if (core::contains(node.getChildren(), this)) {
            return true;
        }
    }

    return false;
}

bool Node::isParentAScene(const Gltf& _root) const
{
    for (const Scene& scene : _root.getScenes()) {
        if (core::contains(scene.getNodes(), this)) {
            return true;
        }
    }

    return false;
}

GltfResult Node::setXMPPacket(Packet* _xmpPacket)
{
    if (m_xmpPacket.has_value() == false) {
        m_xmpPacket.emplace(PacketContainer());
    }
    m_xmpPacket.value().m_packet = _xmpPacket;
    return GltfResult::Success;
}

GltfResult Node::unsetXMPPacket()
{
    m_xmpPacket.reset();
    return GltfResult::Success;
}

bool Node::isValid() const
{
    // nothing is required of a node
    return true;
}

GltfResult Node::getInvalidProperties(core::Vector<core::string>&) const
{
    return GltfResult::Success;
}
