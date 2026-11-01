// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRoot.h"
#include "HelperVisible.h"
#include "XMP.h"
#include "gltf/animation/AnimationPath.h"
#include "math/VectorTypes.h"

#include <optional>
#include <string>
#include <vector>

// See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#node

namespace ux3d::gltf {
class Gltf;
class Mesh;
class Camera;
class Light;
class AudioEmitter;
class Skin;
class Accessor;

// EXT_mesh_gpu_instancing (https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Vendor/EXT_mesh_gpu_instancing):
// one instance of a node's mesh. It is drawn with the node's world matrix x T * R * S of the instance; the values are
// the dequantized accessor values (normalized integers as the specification divides them, not renormalized).
struct GpuInstance {
    math::vec3 translation = math::vec3(0.0f);
    math::quat rotation = math::quat(1.0f, 0.0f, 0.0f, 0.0f);
    math::vec3 scale = math::vec3(1.0f);
};

// One entry of the node's EXT_mesh_gpu_instancing.attributes (TRANSLATION, ROTATION, SCALE or a custom "_NAME").
struct GpuInstancingAttribute {
    std::string name;
    Accessor* accessor = nullptr;
};

namespace gpu_instancing {
inline constexpr const char* ExtensionName = "EXT_mesh_gpu_instancing";
// Test hook GLIDER_TEST_DISABLE_GPU_INSTANCING=1: read files as a loader without the extension (every node drawn once).
bool disabledByTestHook();
// After the buffers are in memory (Gltf::loadBinaryDataToMemory): decodes every node's instance attributes into
// Node::getGpuInstances(). Nodes stay empty (drawn once, as without the extension) when the file does not declare
// the extension, the test hook switches it off, the node is skinned (logged) or the attributes are invalid (logged).
void decodeNodes(Gltf& gltf);
// T * R * S of one instance in double precision, composed like Node::getTransformMatrix().
math::dmat4 localMatrix(const GpuInstance& instance);
// Statistics / profile report: nodes with applied instances, their instance total and the triangles they draw
// (triangles of the node's mesh x instances, counted like Statistics' Total Triangles), and nodes whose extension is
// present but not applied.
struct Summary {
    uint32_t nodes = 0;
    uint64_t instances = 0;
    uint64_t triangles = 0;
    uint32_t notApplied = 0;
};
Summary summarize(const Gltf& gltf);
}

// KHR_node_visibility (https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_node_visibility):
// node.extensions.KHR_node_visibility.visible (boolean, default true). A node that is not visible is not rendered and
// neither are its descendants, whatever their own value; the value is animatable through KHR_animation_pointer
// (PointerPath below, relative to /nodes/{i}/). It is read only when the file declares the extension in
// extensionsUsed. The file's value is separate from the viewer's Hide (HelperVisible), which it never changes.
namespace node_visibility {
inline constexpr const char* ExtensionName = "KHR_node_visibility";
inline constexpr const char* PointerPath = "extensions/KHR_node_visibility/visible";
// Statistics / profile report: nodes carrying the extension and how many of them the file hides (authored value).
struct Summary {
    uint32_t nodes = 0;
    uint32_t hidden = 0;
};
Summary summarize(const Gltf& gltf);
}

// KHR_node_selectability / KHR_node_hoverability (https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos):
// node.extensions.KHR_node_selectability.selectable and node.extensions.KHR_node_hoverability.hoverable (booleans,
// default true). Inherited like KHR_node_visibility: a false ancestor makes the whole subtree non-selectable /
// non-hoverable. Both are animatable through KHR_animation_pointer (the pointer paths below, relative to
// /nodes/{i}/) and read only when the file declares the extension. Glider: a node that is not selectable still
// renders and can be selected in the Scene Hierarchy, but a viewport click passes through it; Glider has no viewport
// hover highlight, so hoverable is read and shown only.
namespace node_interaction {
inline constexpr const char* SelectabilityName = "KHR_node_selectability";
inline constexpr const char* SelectablePointer = "extensions/KHR_node_selectability/selectable";
inline constexpr const char* HoverabilityName = "KHR_node_hoverability";
inline constexpr const char* HoverablePointer = "extensions/KHR_node_hoverability/hoverable";
// Statistics: nodes carrying each extension and how many of them the file sets to false (authored values).
struct Summary {
    uint32_t selectabilityNodes = 0;
    uint32_t unselectable = 0;
    uint32_t hoverabilityNodes = 0;
    uint32_t unhoverable = 0;
};
Summary summarize(const Gltf& gltf);
}

class Node : public ChildOfRoot, public HelperVisible {
    class LightPointerObject : public GltfObject {
    public:
        LightPointerObject() { } // Benni: for some reason 'm_lightPointerObject.emplace()' won't compile without this
        Light* m_pLight = nullptr;
    };

    class AudioEmitterPointerObject : public GltfObject {
    public:
        AudioEmitterPointerObject() { }
        AudioEmitter* m_pEmitter = nullptr;
    };

private:
    core::Vector<Node*> m_Children;
    Mesh* m_pMesh = nullptr;
    Camera* m_pCamera = nullptr;
    Skin* m_pSkin = nullptr;

    std::optional<LightPointerObject> m_lightPointerObject;
    std::optional<AudioEmitterPointerObject> m_audioEmitterPointerObject;

    // Transform given via Triple
    std::optional<math::dvec3> m_Scale;
    std::optional<math::dquat> m_Rotation;
    std::optional<math::dvec3> m_Translation;

    bool m_storeAsMatrix = false;

    // extensions
    std::optional<PacketContainer> m_xmpPacket = std::nullopt;

    // EXT_mesh_gpu_instancing: the attributes as read (the extension JSON itself stays an unknown extension) and the
    // decoded instances (empty: the node's mesh is drawn once).
    std::vector<GpuInstancingAttribute> m_gpuInstancingAttributes;
    std::vector<GpuInstance> m_gpuInstances;
    std::string m_gpuInstancingStatus;

    // KHR_node_visibility: the authored value (nullopt: the node does not carry the extension, it is visible) and the
    // current value, which KHR_animation_pointer channels set every animation apply (mutable: the renderer holds
    // const nodes; it is a per-frame flag like an animated transform, not a model edit).
    std::optional<bool> m_fileVisible;
    mutable bool m_fileVisibleNow = true;
    // KHR_node_selectability / KHR_node_hoverability: authored values (nullopt: not carried) and current values
    // (set by KHR_animation_pointer channels; mutable for the same reason as m_fileVisibleNow).
    std::optional<bool> m_fileSelectable;
    mutable bool m_fileSelectableNow = true;
    std::optional<bool> m_fileHoverable;
    mutable bool m_fileHoverableNow = true;

public:
    Node(uint32_t _id); // regular constructor
    Node(uint32_t _id, const Node& _source); // duplicate in same glTF

    GltfType getGltfType() const override { return GltfType::Node; }

    Mesh* getMesh() { return m_pMesh; }
    const Mesh* getMesh() const { return m_pMesh; }
    GltfResult setMesh(Mesh& _Mesh);
    void unsetMesh() { m_pMesh = nullptr; }

    Camera* getCamera() { return m_pCamera; }
    const Camera* getCamera() const { return m_pCamera; }
    GltfResult setCamera(Camera& _Camera);
    void unsetCamera() { m_pCamera = nullptr; }

    Light* getLight();
    const Light* getLight() const;
    GltfResult setLight(Light& _Light);
    void unsetLight() { m_lightPointerObject.reset(); }
    std::optional<LightPointerObject>& getLightPointerObject() { return m_lightPointerObject; }
    const std::optional<LightPointerObject>& getLightPointerObject() const { return m_lightPointerObject; }
    LightPointerObject& createLightPointerObject() { return m_lightPointerObject.emplace(); }

    AudioEmitter* getAudioEmitter();
    const AudioEmitter* getAudioEmitter() const;
    void setAudioEmitter(AudioEmitter& emitter);
    void unsetAudioEmitter() { m_audioEmitterPointerObject.reset(); }
    bool hasAudio() const { return m_audioEmitterPointerObject.has_value(); }
    bool hasAudioEmitter() const { return m_audioEmitterPointerObject.has_value() && getAudioEmitter() != nullptr; }
    std::optional<AudioEmitterPointerObject>& getAudioEmitterPointerObject() { return m_audioEmitterPointerObject; }
    const std::optional<AudioEmitterPointerObject>& getAudioEmitterPointerObject() const { return m_audioEmitterPointerObject; }
    AudioEmitterPointerObject& createAudioEmitterPointerObject() { return m_audioEmitterPointerObject.emplace(); }

    Skin* getSkin() { return m_pSkin; }
    const Skin* getSkin() const { return m_pSkin; }
    GltfResult setSkin(Skin& _Skin);
    void unsetSkin() { m_pSkin = nullptr; }

    core::Vector<Node*>& getChildren() { return m_Children; }
    const core::Vector<Node*>& getChildren() const { return m_Children; }

    bool hasXMPPacket() const { return m_xmpPacket.has_value(); }
    std::optional<PacketContainer>& getXMPPacket() { return m_xmpPacket; }
    const std::optional<PacketContainer>& getXMPPacket() const { return m_xmpPacket; }
    GltfResult setXMPPacket(Packet* _xmpPacket);
    GltfResult unsetXMPPacket();

    // EXT_mesh_gpu_instancing (see GpuInstance). hasGpuInstancing(): the node carries the extension's attributes;
    // getGpuInstances() is empty unless they were decoded and applied; getGpuInstancingStatus() says why not.
    bool hasGpuInstancing() const { return !m_gpuInstancingAttributes.empty(); }
    const std::vector<GpuInstancingAttribute>& getGpuInstancingAttributes() const { return m_gpuInstancingAttributes; }
    void addGpuInstancingAttribute(const std::string& _name, Accessor* _accessor) { m_gpuInstancingAttributes.push_back({ _name, _accessor }); }
    const std::vector<GpuInstance>& getGpuInstances() const { return m_gpuInstances; }
    void setGpuInstances(std::vector<GpuInstance> _instances, const std::string& _status)
    {
        m_gpuInstances = std::move(_instances);
        m_gpuInstancingStatus = _status;
    }
    const std::string& getGpuInstancingStatus() const { return m_gpuInstancingStatus; }

    // KHR_node_visibility (see node_visibility). hasNodeVisibility(): the node carries the extension (read);
    // getAuthoredFileVisible(): the file's value; isFileVisible(): the current value (authored, or as an animation set
    // it). Only the node's own value: the renderer and the hierarchy inherit it down the subtree.
    bool hasNodeVisibility() const { return m_fileVisible.has_value(); }
    bool getAuthoredFileVisible() const { return m_fileVisible.value_or(true); }
    void setNodeVisibility(std::optional<bool> _visible)
    {
        m_fileVisible = _visible;
        m_fileVisibleNow = _visible.value_or(true);
    }
    bool isFileVisible() const { return m_fileVisibleNow; }
    void setAnimatedFileVisible(bool _visible) const { m_fileVisibleNow = _visible; }
    void resetAnimatedFileVisible() const { m_fileVisibleNow = m_fileVisible.value_or(true); }

    // KHR_node_selectability / KHR_node_hoverability (see node_interaction): the node's own values; the renderer and
    // the hierarchy inherit them down the subtree.
    bool hasNodeSelectability() const { return m_fileSelectable.has_value(); }
    bool getAuthoredSelectable() const { return m_fileSelectable.value_or(true); }
    void setNodeSelectability(std::optional<bool> _selectable)
    {
        m_fileSelectable = _selectable;
        m_fileSelectableNow = _selectable.value_or(true);
    }
    bool isFileSelectable() const { return m_fileSelectableNow; }
    void setAnimatedSelectable(bool _selectable) const { m_fileSelectableNow = _selectable; }
    void resetAnimatedSelectable() const { m_fileSelectableNow = m_fileSelectable.value_or(true); }

    bool hasNodeHoverability() const { return m_fileHoverable.has_value(); }
    bool getAuthoredHoverable() const { return m_fileHoverable.value_or(true); }
    void setNodeHoverability(std::optional<bool> _hoverable)
    {
        m_fileHoverable = _hoverable;
        m_fileHoverableNow = _hoverable.value_or(true);
    }
    bool isFileHoverable() const { return m_fileHoverableNow; }
    void setAnimatedHoverable(bool _hoverable) const { m_fileHoverableNow = _hoverable; }
    void resetAnimatedHoverable() const { m_fileHoverableNow = m_fileHoverable.value_or(true); }

    GltfResult getChildrenRecursively(core::Vector<Node*>& _recursiveChildren);
    GltfResult addChild(Node& _child, const Gltf& _root);
    GltfResult canAddChild(Node& _child, const Gltf& _root);
    GltfResult canAddChild(Node& _child, core::string& _message, const Gltf& _root);
    GltfResult insertChild(Node& _Child, size_t _Index, const Gltf& _root);
    GltfResult moveChild(size_t _FromIndex, size_t _ToIndex);
    GltfResult removeChild(Node& _Child);
    GltfResult clearChildren();

    bool isAnimationTarget(const Gltf& _root, std::optional<AnimationPath> targetPath = std::nullopt) const;

    bool hasScale() const { return m_Scale.has_value(); }
    math::dvec3 getScaleOrDefault() const;
    GltfResult setScale(const math::dvec3& _Scale);
    void unsetScale() { m_Scale.reset(); }

    bool hasRotation() const { return m_Rotation.has_value(); }
    math::dquat getRotationOrDefault() const;
    GltfResult setRotation(const math::dquat& _Rotation);
    void unsetRotation() { m_Rotation.reset(); }

    bool hasTranslation() const { return m_Translation.has_value(); }
    math::dvec3 getTranslationOrDefault() const;
    GltfResult setTranslation(const math::dvec3& _Translation);
    void unsetTranslation() { m_Translation.reset(); }

    bool isJoint(const Gltf& gltf) const;

    void storeAsMatrix(bool _asMatrix, const Gltf* gltfForJointResolve = nullptr);
    bool getStoreAsMatrix() const { return m_storeAsMatrix; }
    GltfResult decomposeTransformMatrix(const math::dmat4x4& _TransformMatrix);
    math::dmat4x4 getTransformMatrix() const;

    Extensions getUsedExtensions() const override;

    bool hasParent(const Gltf& _root) const;
    // returns true if the parent is a scene (returns false if no parent or parent is not a scene)
    bool isParentAScene(const Gltf& _root) const;

    bool isValid() const override;
    GltfResult getInvalidProperties(core::Vector<core::string>& _InvalidProperties) const override;

    Node(const Node&) = delete;
    Node(Node&&) = delete;
    void operator=(const Node&) = delete;
    void operator=(Node&&) = delete;

    std::optional<std::vector<double>> weights;
};
}
