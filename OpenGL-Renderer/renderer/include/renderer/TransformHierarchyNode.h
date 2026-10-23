// Copyright (c) 2019 UX3D GmbH. All rights reserved.

// Authors: Willy Becher 🌴

#pragma once

#include <cstdint>
#include <set>
#include <vector>

#include "animation/IAnimationTarget.h"
#include "math/MatrixFunctions.h"
#include "math/VectorFunctions.h"
#include "renderer/TransformAnimationTarget.h"

namespace ux3d::renderer {

struct Light;
struct Mesh;
struct Camera;
struct AudioEmitter;

// Single transform in the hierarchy of transforms. Also responsible for updating all registered target objects
class TransformHierarchyNode {
public:
    TransformHierarchyNode(const math::vec3& translation, const math::quat& rotation, const math::vec3& scale, const std::vector<float>& weights);

    void addMesh(Mesh* mesh);
    void removeMesh(Mesh* mesh);
    void addCamera(Camera* camera);
    void removeCamera(Camera* camera);
    void addLight(Light* light);
    void removeLight(Light* light);
    void addAudioEmitter(AudioEmitter* audioEmitter);
    void removeAudioEmitter(AudioEmitter* audioEmitter);

    void addChild(TransformHierarchyNode* child);
    // parentChanged: the parent's world transform differs from the one this
    // node's cached world transform was computed from (P4.7). A node recomputes
    // and pushes its world transform only when the parent changed, its own TRS
    // targets are dirty, or it was invalidated.
    math::mat4 traverse(const math::mat4x4& transformMatrix, const math::mat3x3& rotationMatrix, bool parentChanged = true);
    void clearChildrenRecursive();
    void invalidate() { m_worldValid = false; }

    // Cumulative traversal statistics (P4.7 measurements).
    struct TraverseStats {
        uint64_t visited = 0;
        uint64_t recomputed = 0;
        uint64_t weightPushes = 0;
    };
    static TraverseStats& stats();
    static void setReuseEnabled(bool enabled);

    const math::vec3& getTranslation();
    const math::mat4x4& getRotation();
    const math::vec3& getScale();
    math::vec3 getWorldTranslation() const;
    math::mat3x3 getWorldRotation() const;
    const math::mat4& getWorldTransform() const;

    TransformAnimationTarget<math::vec3> translationTarget;
    TransformAnimationTarget<math::quat> rotationTarget;
    TransformAnimationTarget<math::vec3> scaleTarget;
    TransformAnimationTarget<std::vector<float>> weightTarget;

private:
    bool isDirty();

    std::set<TransformHierarchyNode*> m_children;
    // TODO: remove the references to renderer objects
    std::set<Mesh*> m_meshes;
    std::set<Camera*> m_cameras;
    std::set<Light*> m_lights;
    std::set<AudioEmitter*> m_audioEmitters;

    math::vec3 m_localTranslateCache = math::vec3(0);
    math::mat4x4 m_localRotateCache = math::mat4x4(1);
    math::vec3 m_localScaleCache = math::vec3(1);

    math::mat4 m_worldTransformCache = math::mat4x4(1);
    math::mat3 m_worldRotateCache = math::mat4x4(1);
    bool m_worldValid = false;
};

}
