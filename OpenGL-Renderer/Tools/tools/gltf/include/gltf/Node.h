// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRoot.h"
#include "HelperVisible.h"
#include "XMP.h"
#include "gltf/animation/AnimationPath.h"
#include "math/VectorTypes.h"

#include <optional>
#include <vector>

// See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#node

namespace ux3d::gltf {
class Gltf;
class Mesh;
class Camera;
class Light;
class AudioEmitter;
class Skin;

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
