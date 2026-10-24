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

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
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

// Which declared ASOBO (MSFS) extensions a viewport applies (docs/ASOBO_EXTENSIONS_PHASE_0.md, A0.4).
// Defaults apply all of them; with everything off, rendering matches a build without ASOBO support.
struct AsoboOptions {
    bool enabled = true; // master switch
    std::vector<std::string> disabled; // extensions switched off individually, sorted

    bool applies(std::string_view extension) const
    {
        return enabled && std::find(disabled.begin(), disabled.end(), extension) == disabled.end();
    }
    void setApplied(const std::string& extension, bool applied)
    {
        const auto it = std::lower_bound(disabled.begin(), disabled.end(), extension);
        const bool present = it != disabled.end() && *it == extension;
        if (applied && present) {
            disabled.erase(it);
        } else if (!applied && !present) {
            disabled.insert(it, extension);
        }
    }
    bool operator==(const AsoboOptions&) const = default;
};

struct SceneProfileStats {
    uint64_t nodesVisited = 0;
    uint64_t primitivesVisited = 0;
    uint64_t drawables = 0;
    uint64_t culled = 0;      // primitives outside the active camera's frustum (P5.5)
    uint64_t uncullable = 0;  // primitives kept because their bounds are not conservative
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

    // Loads the panorama with iblParameters and iblSampling; filterEnvironmentMap runs the filters.
    void setEnvironmentMap(const filesystem::Path& panoramaIrradiancePath);
    bool filterEnvironmentMap();
    const filesystem::Path& getEnvironmentMapPath() const { return m_environmentMapPath; }
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

    // The document updateGltf is synchronising; the material mapping reads its extensionsUsed.
    const gltf::Gltf* m_syncingGltf = nullptr;

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

    // IBL filter quality, applied by the next setEnvironmentMap.
    InitizalizationParameters iblParameters;
    SamplingParameters iblSampling;

    bool recalculateBones = true;

    RenderSettings renderSettings;

    // P5.5: when set, createScene leaves out static primitives whose POSITION bounds lie
    // entirely outside the active camera's frustum. Skinned, morphed, non-float and
    // bound-less primitives are always kept.
    bool frustumCulling = false;

    // The scene traversal (visible primitives with their meshes, materials, nodes and
    // static bounds, plus lights) does not depend on the camera, so between
    // beginSceneFrame and endSceneFrame createScene computes it once and every viewport
    // reuses it; only culling and the per-drawable state are redone. Outside a frame
    // createScene always traverses.
    void beginSceneFrame() { m_sceneFrameToken = ++m_nextSceneFrameToken; }
    void endSceneFrame() { m_sceneFrameToken = 0; }
    uint64_t getTraversalReuses() const { return m_traversalReuses; }

    // Which ASOBO (MSFS) extensions createScene applies; set per viewport (A0.4). With
    // ASOBO_material_invisible not applied, materials it marks invisible are drawn.
    AsoboOptions asoboOptions;

    // Isolation (local view): when root is not null, only that node is drawn, plus its
    // descendants when includeChildren is set. Nodes already hidden by their own visible
    // flag stay hidden. This is a view filter: it does not touch the glTF visibility flags,
    // so exiting isolation restores the scene without side effects. Materials, lights and
    // picking follow the same filter.
    void setIsolation(const gltf::Node* root, bool includeChildren)
    {
        m_isolationRoot = root;
        m_isolationIncludeChildren = includeChildren;
    }
    const gltf::Node* getIsolationRoot() const { return m_isolationRoot; }
    bool getIsolationIncludeChildren() const { return m_isolationIncludeChildren; }

    // P8.5: removes from drawables the static primitives (found through reverseMapping)
    // whose POSITION bounds lie entirely outside clipFromWorld's view volume, with the
    // test frustumCulling uses. Primitives it cannot bound and drawables the mapping does
    // not know (cube map, transmission copies) are kept. Returns the number removed.
    size_t cullToClip(core::LinkedList<std::tuple<Mesh*, Material*>>& drawables, const ReverseMapping& reverseMapping,
                      const math::mat4x4& clipFromWorld) const;

private:
    struct SceneTraversal {
        struct Primitive {
            Mesh* mesh = nullptr;
            Material* material = nullptr;
            const gltf::Node* node = nullptr;
            bool bounded = false; // lo/hi bound the vertex shader output (see getStaticPositionBounds)
            math::vec3 lo = math::vec3(0.0f);
            math::vec3 hi = math::vec3(0.0f);
        };
        std::vector<Primitive> primitives;
        std::vector<Light*> lights;
        uint64_t nodesVisited = 0;
        uint64_t primitivesVisited = 0;
        bool containsTransmissiveMaterial = false;
        const gltf::Scene* scene = nullptr;
        AsoboOptions asoboOptions;
        std::optional<uint32_t> materialVariantID;
        const gltf::Node* isolationRoot = nullptr;
        bool isolationIncludeChildren = false;
        uint64_t frameToken = 0;
    };
    const SceneTraversal& traverseScene(const gltf::Scene* gltfScene);
    SceneTraversal m_traversal;
    uint64_t m_sceneFrameToken = 0;
    uint64_t m_nextSceneFrameToken = 0;
    uint64_t m_traversalReuses = 0;
    const gltf::Node* m_isolationRoot = nullptr;
    bool m_isolationIncludeChildren = false;

    IBLSampler m_iblSampler;

    Camera* m_activeCamera = nullptr;
    std::optional<uint32_t> m_materialVariantID = std::nullopt;
    EnvironmentMap* m_activeEnvironmentMap = nullptr;
    filesystem::Path m_environmentMapPath;
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
    core::LinkedList<std::tuple<Mesh*, Material*>> m_postTransmissionDrawables; // rebuilt by every createScene
    // Transmission duplicates are pooled and refreshed by assignment instead of
    // destroyed and recreated per createScene, so their addresses stay stable
    // for the command cache (P4.6c).
    std::vector<std::unique_ptr<Material>> m_transmissionMaterialPool;
    std::vector<std::unique_ptr<RenderPass>> m_transmissionPassPool;

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
