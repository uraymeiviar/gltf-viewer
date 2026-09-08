// Copyright (c) 2019 UX3D GmbH. All rights reserved.

// Authors: Willy Becher 🌴

#include "renderer/TransformHierarchyNode.h"
#include "renderer/Camera.h"
#include "renderer/Lights.h"
#include "renderer/Mesh.h"

using namespace ux3d;
using namespace ux3d::renderer;

TransformHierarchyNode::TransformHierarchyNode(const math::vec3& translation, const math::quat& rotation, const math::vec3& scale, const std::vector<float>& weights)
{
    translationTarget.update(translation);
    rotationTarget.update(rotation);
    scaleTarget.update(scale);
    weightTarget.update(weights);
    getTranslation();
    getScale();
    getRotation();
}

void TransformHierarchyNode::addChild(TransformHierarchyNode* child)
{
    m_children.insert(child);
}

void TransformHierarchyNode::addMesh(Mesh* mesh)
{
    m_meshes.insert(mesh);
}

void TransformHierarchyNode::removeMesh(Mesh* mesh)
{
    auto it = std::find(m_meshes.begin(), m_meshes.end(), mesh);
    if (it != m_meshes.end()) {
        m_meshes.erase(it);
    }
}

void TransformHierarchyNode::addCamera(Camera* camera)
{
    m_cameras.insert(camera);
}

void TransformHierarchyNode::removeCamera(Camera* camera)
{
    auto it = std::find(m_cameras.begin(), m_cameras.end(), camera);
    if (it != m_cameras.end()) {
        m_cameras.erase(it);
    }
}

void TransformHierarchyNode::addLight(Light* light)
{
    m_lights.insert(light);
}

void TransformHierarchyNode::removeLight(Light* light)
{
    auto it = std::find(m_lights.begin(), m_lights.end(), light);
    if (it != m_lights.end()) {
        m_lights.erase(it);
    }
}

bool TransformHierarchyNode::isDirty()
{
    return translationTarget.isDirty() || rotationTarget.isDirty() || scaleTarget.isDirty();
}

const math::vec3& TransformHierarchyNode::getTranslation()
{
    if (isDirty()) {
        m_localTranslateCache = translationTarget.getValue();
        translationTarget.clean();
    }

    return m_localTranslateCache;
}

const math::mat4x4& TransformHierarchyNode::getRotation()
{
    if (isDirty()) {
        m_localRotateCache = math::toMat4(rotationTarget.getValue());
        rotationTarget.clean();
    }

    return m_localRotateCache;
}

const math::vec3& TransformHierarchyNode::getScale()
{
    if (isDirty()) {
        m_localScaleCache = scaleTarget.getValue();
        scaleTarget.clean();
    }

    return m_localScaleCache;
}

math::vec3 TransformHierarchyNode::getWorldTranslation() const
{
    return math::vec3(m_worldTransformCache[3].x, m_worldTransformCache[3].y, m_worldTransformCache[3].z) / m_worldTransformCache[3].w;
}

math::mat3x3 TransformHierarchyNode::getWorldRotation() const
{
    return m_worldRotateCache;
}

const math::mat4& TransformHierarchyNode::getWorldTransform() const
{
    return m_worldTransformCache;
}

void TransformHierarchyNode::clearChildrenRecursive()
{
    for (auto child : m_children) {
        child->clearChildrenRecursive();
    }
    m_children.clear();
}

math::mat4 TransformHierarchyNode::traverse(const math::mat4x4& transformMatrix, const math::mat3x3& rotationMatrix)
{
    math::mat4x4 translateMatrix = math::translate(transformMatrix, getTranslation());
    math::mat4x4 rotateMatrix = translateMatrix * getRotation();
    m_worldTransformCache = math::scale(rotateMatrix, getScale());
    m_worldRotateCache = rotationMatrix * math::mat3(getRotation());

    for (auto mesh : m_meshes) {
        if (mesh->nukeScale && mesh->nukeRotation) {
            math::mat4x4 onlyPosition(1.0f);
            onlyPosition[3] = translateMatrix[3];
            mesh->updateTransform(onlyPosition);
        } else if (mesh->nukeScale) {
            // create a fixed size matrix that doesn't apply scale to the mesh
            auto fixedSizeMatrix = m_worldTransformCache;
            fixedSizeMatrix[0] = math::normalize(fixedSizeMatrix[0]);
            fixedSizeMatrix[1] = math::normalize(fixedSizeMatrix[1]);
            fixedSizeMatrix[2] = math::normalize(fixedSizeMatrix[2]);
            mesh->updateTransform(fixedSizeMatrix);
        }
        // We don't handle only nuke rotation currently
        else {
            mesh->updateTransform(m_worldTransformCache);
        }

        mesh->updateWeights(weightTarget.getValue());
        mesh->resetWeightsUpdated(); // this is called after the last potential update of mesh weights for this frame to enable weight updates in the next frame
    }

    for (auto camera : m_cameras) {
        camera->updateTransform(m_worldTransformCache);
    }

    for (auto child : m_children) {
        child->traverse(m_worldTransformCache, m_worldRotateCache);
    }

    for (Light* light : m_lights) {
        light->updateTransform(m_worldTransformCache);
    }

    return m_worldTransformCache;
}

void TransformHierarchyNode::addAudioEmitter(AudioEmitter* audioEmitter)
{
    m_audioEmitters.insert(audioEmitter);
}

void TransformHierarchyNode::removeAudioEmitter(AudioEmitter* audioEmitter)
{
    auto it = std::find(m_audioEmitters.begin(), m_audioEmitters.end(), audioEmitter);
    if (it != m_audioEmitters.end()) {
        m_audioEmitters.erase(it);
    }
}
