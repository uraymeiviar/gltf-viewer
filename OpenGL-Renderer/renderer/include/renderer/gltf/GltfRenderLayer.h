// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "animation/Animation.h"
#include "core/Algorithms.h"
#include "core/TupleHash.h"
#include "core/Vector.h"
#include "filesystem/Dir.h"
#include "gltf/ChildOfRoot.h"
#include "gltf/Gltf.h"
#include "renderer/Command.h"
#include "renderer/IRenderLayer.h"
#include "renderer/KHRAnimationPointerTarget.h"
#include "renderer/Material.h"
#include "renderer/Mesh.h"
#include "renderer/Skin.h"
#include "renderer/filtering/IBLSampler.h"
#include "renderer/gltf/GlTFModifierCallback.h"
#include "renderer/gltf/GltfAudioEmitterNodeMapping.h"
#include "renderer/gltf/GltfCameraMapping.h"
#include "renderer/gltf/GltfLightMapping.h"
#include "renderer/gltf/GltfResourceManager.h"

#include <functional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

template <>
struct std::hash<std::tuple<const ux3d::gltf::ChildOfRoot*, std::string>> {
    std::size_t operator()(std::tuple<const ux3d::gltf::ChildOfRoot*, std::string> const& tuple) const noexcept
    {
        std::size_t h1 = std::hash<const ux3d::gltf::ChildOfRoot*> {}(std::get<0>(tuple));
        std::size_t h2 = std::hash<std::string> {}(std::get<1>(tuple));
        return h1 ^ (h2 << 1);
    }
};

namespace ux3d::gltf {
class MaterialVariant;
}

namespace ux3d::renderer {

using ReverseMapping = std::unordered_map<Mesh*, const gltf::Node*>;

struct AnimationProfileTimes {
    double channelsMs = 0.0;
    double transformsMs = 0.0;
    double skinsMs = 0.0;
};

struct SceneProfileStats {
    uint64_t nodesVisited = 0;
    uint64_t primitivesVisited = 0;
    uint64_t drawables = 0;
};

class GltfRenderLayer : public renderer::IRenderLayer {
    // TODO: these should be removed when access to members is not needed any more
    friend class GltfLightMapping;
    friend class GltfCameraMapping;

public:
    GltfRenderLayer(gl::GL* gl, const filesystem::Path& shaderPath);
    virtual ~GltfRenderLayer() = default;

    void updateGltf(const gltf::Gltf& gltf);
    void updateScene(const gltf::Gltf& gltf, const gltf::Scene& scene);
    void applyAnimationTime(const std::optional<float>& animationTime);
    void updateCameras(const core::Vector<gltf::Node*>& cameraNodes);

    std::tuple<RendererScene, ReverseMapping> createScene(const gltf::Scene* scene = nullptr);

    const gltf::Scene* getGltfScene() const { return m_gltfScene; } // TODO: Cannot the renderer pass the same scene to the editor render layer?
    const auto& getTransformHierarchy() const { return m_transformHierarchyMapping; }

    void setCpuProfilingEnabled(bool enabled) { m_cpuProfilingEnabled = enabled; }
    const AnimationProfileTimes& getLastAnimationProfile() const { return m_lastAnimationProfile; }
    const SceneProfileStats& getLastSceneStats() const { return m_lastSceneStats; }

    // Change generations for caches derived from layer state (P4.4). The scene
    // generation advances on every updateScene (model resynchronised); the pose
    // generation also advances on every applyAnimationTime, i.e. whenever node
    // world transforms may have changed.
    uint64_t getSceneGeneration() const { return m_sceneGeneration; }
    // Advances on every updateGltf (materials, textures, samplers resynchronised; P4.5).
    uint64_t getGltfGeneration() const { return m_gltfGeneration; }
    uint64_t getPoseGeneration() const { return m_poseGeneration; }

    void setEnvironmentMap(const filesystem::Path& panoramaIrradiancePath);
    bool filterEnvironmentMap();
    EnvironmentMap* getActiveEnvironmentMap() { return m_activeEnvironmentMap; }
    void enableCubeMap(bool enable) { m_cubeMapEnabled = enable; };

    void setActiveGltfCameraByNode(const gltf::Node* cameraNode);
    const gltf::Node* getActiveGltfCameraNode() const;
    const gltf::Camera* getActiveGltfCamera() const;

    float getAnimationDuration() const;

    math::mat4 getWorldTransform(const gltf::Node* searchNode) const;
    math::mat3x3 getWorldRotation(const gltf::Node* searchNode) const;
    math::quat getWorldRotationQuat(const gltf::Node* searchNode) const;
    math::mat4 getParentWorldTransform(const gltf::Node* searchNode) const;
    math::mat3x3 getParentWorldRotation(const gltf::Node* searchNode) const;
    math::quat getParentWorldRotationQuat(const gltf::Node* searchNode) const;

    GltfResourceManager& getResourceManager() { return m_resourceManager; }

    // TODO: Deprecated: Use Editor render layer for this
    Camera* addCamera();
    void setActiveCamera(Camera* camera) { m_activeCamera = camera; }
    Camera* getActiveCamera() const { return m_activeCamera; }

    void setMaterialVariant(std::optional<uint32_t> materialVariantID) { m_materialVariantID = materialVariantID; }
    const std::optional<uint32_t> getMaterialVariant() const { return m_materialVariantID; }

    float viewportAspectRatio = 1;

private:
    GltfResourceManager m_resourceManager;

    void updateMaterials(const core::LinkedList<gltf::Material>& gltfMaterials);
    void updateMeshes(const core::Vector<gltf::Node*>& rootNodes);
    void updateSkins(const core::Vector<gltf::Node*>& rootNodes);
    void updateLights(const core::Vector<gltf::Node*>& lightNodes);
    void updateAudioEmitters(const core::Vector<gltf::Node*>& audioNodes);
    void updateTransformHierarchy(const core::LinkedList<gltf::Node>& nodes, const core::Vector<gltf::Node*>& rootNodes);
    void updateAnimationPointer(const core::LinkedList<gltf::Animation>& animations);

public:
    GltfToRendererMapping<std::tuple<const gltf::Image*, bool>, renderer::gl::Texture*> m_textureMapping;
    GltfToRendererMapping<const gltf::Accessor*, renderer::gl::Buffer*> m_bufferMapping;
    GltfToRendererMapping<const gltf::Sampler*, renderer::gl::SamplerState*> m_samplerStateMapping;
    GltfToRendererMapping<const gltf::Material*, renderer::Material*> m_materialMapping;
    GltfToRendererMapping<std::tuple<const gltf::Node*, const gltf::Mesh*, const gltf::Primitive*>, renderer::Mesh*> m_meshMapping;
    GltfToRendererMapping<const gltf::Node*, renderer::TransformHierarchyNode*> m_transformHierarchyMapping;
    GltfCameraMapping m_cameraMapping;
    GltfLightMapping m_lightMapping;
    GltfAudioEmitterNodeMapping m_audioMapping;
    GltfToRendererMapping<const gltf::Animation*, animation::Animation*> m_animationMapping;
    GltfToRendererMapping<std::tuple<const gltf::Node*, const gltf::Skin*>, renderer::Skin*> m_skinMapping;

    uint32_t iblMaxResolution = 1024u;

    bool recalculateBones = true;

    RenderSettings renderSettings;

private:
    IBLSampler m_iblSampler;

    Camera* m_activeCamera = nullptr;
    std::optional<uint32_t> m_materialVariantID = std::nullopt;
    EnvironmentMap* m_activeEnvironmentMap = nullptr;
    bool m_cubeMapEnabled = true;
    const gltf::Scene* m_gltfScene;
    size_t m_max2DTextureSize = 0;
    size_t m_maxTextureArrayLength = 0;
    std::list<renderer::Skin> m_skins;
    std::list<animation::Animation> m_animations;

    bool m_cpuProfilingEnabled = false;
    AnimationProfileTimes m_lastAnimationProfile;
    SceneProfileStats m_lastSceneStats;
    uint64_t m_sceneGeneration = 0;
    uint64_t m_gltfGeneration = 0;
    uint64_t m_poseGeneration = 0;

    // list for all materials needed for transmission draw call.
    core::LinkedList<std::tuple<Mesh*, Material*>> m_postTransmissionDrawables; // this list has to persist between frames so that the old elements can be cleared each frame.

    std::unordered_map<std::tuple<const gltf::ChildOfRoot*, std::string>, KHRAnimationPointerTarget> m_animationPointerTargets;

    template <typename T, typename RenderObject>
    void setUniformAsAnimationPointerTarget(
        const gltf::ChildOfRoot* gltfObject,
        std::string property,
        RenderObject* renderObject,
        const char* uniformName)
    {
        m_animationPointerTargets[{ gltfObject, property }].callback = KHRAnimationPointerTarget::Callback<T> {
            [renderObject, uniformName](const T& value) -> void {
                renderObject->template setUniform<T>(uniformName, value);
            }
        };
    }
};

}
