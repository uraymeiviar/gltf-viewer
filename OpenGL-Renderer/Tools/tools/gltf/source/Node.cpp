// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#include "gltf/Node.h"
#include "gltf/AudioEmitter.h"
#include "gltf/Camera.h"
#include "gltf/Gltf.h"
#include "gltf/Light.h"
#include "gltf/Mesh.h"
#include "gltf/Scene.h"
#include "gltf/Skin.h"
#include "gltf/Accessor.h"
#include "gltf/BufferView.h"
#include "core/Logger.h"

#include "math/MatrixFunctions.h"
#include "math/VectorFunctions.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>

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
    , m_gpuInstancingAttributes(_source.m_gpuInstancingAttributes)
    , m_gpuInstances(_source.m_gpuInstances)
    , m_gpuInstancingStatus(_source.m_gpuInstancingStatus)
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

// ---------------------------------------------------------------------------------------------------------------------
// EXT_mesh_gpu_instancing

bool ux3d::gltf::gpu_instancing::disabledByTestHook()
{
    static const bool disabled = [] {
        const char* value = std::getenv("GLIDER_TEST_DISABLE_GPU_INSTANCING");
        return value != nullptr && value[0] != '\0' && std::string(value) != "0";
    }();
    return disabled;
}

namespace {

// One accessor as floats, `components` per element: float as stored, normalized integers as the glTF specification
// (and the renderer's attribute conversion) divides them, other integers (KHR_mesh_quantization) as their value.
// Returns an empty string on success, otherwise why the accessor cannot be read.
std::string readFloats(const Accessor& accessor, uint32_t components, std::vector<float>& out)
{
    if (Accessor::getComponentCountFrom(accessor.getType()) != components) {
        return "has " + std::to_string(Accessor::getComponentCountFrom(accessor.getType())) + " components, expected "
            + std::to_string(components);
    }
    const ComponentType type = accessor.getComponentType();
    const size_t componentSize = Accessor::getByteLengthFrom(type);
    if (componentSize == 0 || type == ComponentType::UnsignedInt) {
        return "has an unsupported component type";
    }
    const size_t elementSize = componentSize * components;
    size_t stride = elementSize;
    core::MemoryRange<const uint8_t> range;
    core::Slice<uint8_t> sparseData;
    if (accessor.getSparse().has_value()) {
        if (accessor.getBufferView() != nullptr && accessor.getBufferView()->getByteStride().has_value()
            && *accessor.getBufferView()->getByteStride() != elementSize) {
            return "is sparse with an interleaved bufferView";
        }
        if (core::Failed(accessor.getSparseDataAs(sparseData))) {
            return "sparse data cannot be read";
        }
        const auto memory = sparseData.getMemoryRange();
        range = core::makeRange(static_cast<const uint8_t*>(memory.data()), memory.size());
    } else {
        if (core::Failed(accessor.getData(range))) {
            return "data cannot be read";
        }
        if (accessor.getBufferView() != nullptr) {
            stride = accessor.getBufferView()->getByteStride().value_or(static_cast<uint32_t>(elementSize));
        }
    }
    const size_t count = accessor.getCount();
    if (count == 0 || stride < elementSize || (count - 1) * stride + elementSize > range.size()) {
        return "data is shorter than its count";
    }
    const bool normalized = accessor.isNormalized();
    out.resize(count * components);
    const uint8_t* data = range.data();
    for (size_t i = 0; i < count; ++i) {
        for (uint32_t c = 0; c < components; ++c) {
            const uint8_t* p = data + i * stride + c * componentSize;
            float value = 0.0f;
            switch (type) {
            case ComponentType::Float: {
                std::memcpy(&value, p, sizeof(float));
                break;
            }
            case ComponentType::Byte: {
                int8_t v;
                std::memcpy(&v, p, 1);
                value = normalized ? std::max(static_cast<float>(v) / 127.f, -1.f) : static_cast<float>(v);
                break;
            }
            case ComponentType::UnsignedByte: {
                uint8_t v;
                std::memcpy(&v, p, 1);
                value = normalized ? static_cast<float>(v) / 255.f : static_cast<float>(v);
                break;
            }
            case ComponentType::Short: {
                int16_t v;
                std::memcpy(&v, p, 2);
                value = normalized ? std::max(static_cast<float>(v) / 32767.f, -1.f) : static_cast<float>(v);
                break;
            }
            case ComponentType::UnsignedShort: {
                uint16_t v;
                std::memcpy(&v, p, 2);
                value = normalized ? static_cast<float>(v) / 65535.f : static_cast<float>(v);
                break;
            }
            default:
                return "has an unsupported component type";
            }
            out[i * components + c] = value;
        }
    }
    return {};
}

// The instances of one node, or why they are not applied (empty instances).
std::string decodeNode(const Node& node, std::vector<GpuInstance>& instances)
{
    const Accessor* translation = nullptr;
    const Accessor* rotation = nullptr;
    const Accessor* scale = nullptr;
    std::optional<uint32_t> count;
    for (const GpuInstancingAttribute& attribute : node.getGpuInstancingAttributes()) {
        if (attribute.accessor == nullptr) {
            return "attribute " + attribute.name + " has no valid accessor";
        }
        if (count.has_value() && *count != attribute.accessor->getCount()) {
            return "the attribute accessors have different counts";
        }
        count = attribute.accessor->getCount();
        if (attribute.name == "TRANSLATION") {
            translation = attribute.accessor;
        } else if (attribute.name == "ROTATION") {
            rotation = attribute.accessor;
        } else if (attribute.name == "SCALE") {
            scale = attribute.accessor;
        }
        // Custom attributes ("_NAME") are application data; other names are not defined by the extension. Both
        // are listed in the Inspector and do not change the drawing.
    }
    if (!count.has_value() || *count == 0) {
        return "no instance attribute with a count";
    }
    std::vector<float> t, r, s;
    if (translation != nullptr) {
        if (std::string why = readFloats(*translation, 3, t); !why.empty()) return "TRANSLATION " + why;
    }
    if (rotation != nullptr) {
        if (std::string why = readFloats(*rotation, 4, r); !why.empty()) return "ROTATION " + why;
    }
    if (scale != nullptr) {
        if (std::string why = readFloats(*scale, 3, s); !why.empty()) return "SCALE " + why;
    }
    instances.assign(*count, GpuInstance());
    for (size_t i = 0; i < instances.size(); ++i) {
        if (!t.empty()) instances[i].translation = math::vec3(t[i * 3], t[i * 3 + 1], t[i * 3 + 2]);
        // glTF stores x, y, z, w; glm's constructor takes w first.
        if (!r.empty()) instances[i].rotation = math::quat(r[i * 4 + 3], r[i * 4], r[i * 4 + 1], r[i * 4 + 2]);
        if (!s.empty()) instances[i].scale = math::vec3(s[i * 3], s[i * 3 + 1], s[i * 3 + 2]);
    }
    return {};
}

uint64_t meshTriangles(const Mesh& mesh)
{
    uint64_t triangles = 0;
    for (const Primitive& primitive : mesh.getPrimitives()) {
        if (primitive.getIndicesAccessor() != nullptr) {
            triangles += primitive.getIndicesAccessor()->getCount() / 3;
            continue;
        }
        for (const Attribute& attribute : primitive.getAttributes()) {
            if (attribute.getName() == "POSITION" && attribute.getAccessor() != nullptr) {
                triangles += attribute.getAccessor()->getCount() / 3;
                break;
            }
        }
    }
    return triangles;
}

std::string nodeLabel(const Node& node)
{
    return "node " + std::to_string(node.m_ID) + (node.hasName() ? " '" + node.getName() + "'" : std::string());
}

} // namespace

void ux3d::gltf::gpu_instancing::decodeNodes(Gltf& gltf)
{
    const bool declared = gltf.isExtensionDeclared(ExtensionName);
    uint32_t applied = 0;
    uint64_t total = 0;
    for (Node& node : gltf.getNodes()) {
        if (!node.hasGpuInstancing()) {
            continue;
        }
        std::vector<GpuInstance> instances;
        std::string status;
        if (!declared) {
            status = "not applied: the file does not declare EXT_mesh_gpu_instancing in extensionsUsed";
        } else if (disabledByTestHook()) {
            status = "not applied: switched off (GLIDER_TEST_DISABLE_GPU_INSTANCING)";
        } else if (node.getMesh() == nullptr) {
            status = "not applied: the node has no mesh";
        } else if (node.getSkin() != nullptr) {
            status = "not applied: skinned node, the mesh is drawn once";
            core::logWarning("[Loading] EXT_mesh_gpu_instancing: %s is skinned; instancing is not applied, the mesh "
                             "is drawn once", nodeLabel(node).c_str());
        } else if (std::string why = decodeNode(node, instances); !why.empty()) {
            instances.clear();
            status = "not applied: " + why;
            core::logWarning("[Loading] EXT_mesh_gpu_instancing: %s cannot be instanced: %s; the mesh is drawn once",
                             nodeLabel(node).c_str(), why.c_str());
        } else {
            status = "applied";
            ++applied;
            total += instances.size();
        }
        node.setGpuInstances(std::move(instances), status);
    }
    if (applied != 0) {
        core::logInfo("[Loading] EXT_mesh_gpu_instancing: %u node(s), %llu instance(s)", applied,
                      static_cast<unsigned long long>(total));
    }
}

math::dmat4 ux3d::gltf::gpu_instancing::localMatrix(const GpuInstance& instance)
{
    constexpr math::dmat4 identity = math::identity<math::dmat4>();
    const math::dmat4 scale = math::scale(identity, math::dvec3(instance.scale));
    const math::dmat4 rotation = math::mat4_cast(math::dquat(instance.rotation));
    const math::dmat4 translation = math::translate(identity, math::dvec3(instance.translation));
    return translation * rotation * scale;
}

ux3d::gltf::gpu_instancing::Summary ux3d::gltf::gpu_instancing::summarize(const Gltf& gltf)
{
    Summary summary;
    for (const Node& node : gltf.getNodes()) {
        if (!node.hasGpuInstancing()) {
            continue;
        }
        if (node.getGpuInstances().empty() || node.getMesh() == nullptr) {
            ++summary.notApplied;
            continue;
        }
        ++summary.nodes;
        summary.instances += node.getGpuInstances().size();
        summary.triangles += meshTriangles(*node.getMesh()) * node.getGpuInstances().size();
    }
    return summary;
}
