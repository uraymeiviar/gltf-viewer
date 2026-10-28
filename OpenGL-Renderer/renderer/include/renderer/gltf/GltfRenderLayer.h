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
class Primitive;
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
    // A2.2: refreshes only renderer material state, uniforms and sampler bindings from the
    // model. It never touches geometry, buffer mappings or generated tangents, so an
    // Inspector material edit (for example dragging an emissive colour) is cheap. Returns
    // true when a material's normal-map UV set changed and the meshes must be rebuilt to
    // pick up the tangent buffer generated for that UV set.
    bool updateMaterialStates(const gltf::Gltf& gltf);
    // A2.2: drops the cached MikkTSpace tangent buffers. Call when the model is replaced;
    // the cache is otherwise kept for the lifetime of the loaded model.
    void clearGeneratedTangents();
    void releaseModelTextures();
    // CPU-only import preparation. Call while no renderer/model operations run.
    void prepareGeneratedTangents(const gltf::Gltf& gltf);
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
    // A3.1: remove the panorama's own sun from the next setEnvironmentMap (IBL and background), and the sun
    // found in the current panorama (cube-map space; see EnvironmentSun).
    void setEnvironmentSunRemoval(bool remove) { m_iblSampler.setSunRemoval(remove); }
    bool getEnvironmentSunRemoval() const { return m_iblSampler.getSunRemoval(); }
    const EnvironmentSun& getEnvironmentSun() const { return m_iblSampler.getEnvironmentSun(); }
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

    // The image a texture actually renders with: MSFT_texture_dds first, then
    // KHR_texture_basisu, EXT_texture_webp, and finally the core source. Panels use
    // this same choice so their labels and previews match the renderer.
    static gltf::Image* pickImageForTexture(const gltf::Texture* gltfTexture, bool ddsDeclared);

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

    // Set by updateGltf from the asset's declared ASOBO_normal_map_convention (A1.2).
    bool m_assetNormalMapConventionDirectX = false;
    bool m_assetHasAsoboExtensions = false; // A1.6: extensionsUsed contains ASOBO_*
    // Set by updateGltf when extensionsUsed declares ASOBO_material_draw_order (A1.4).
    bool m_assetDrawOrderDeclared = false;
    // A2.1: set by updateGltf when at least one material carries
    // ASOBO_material_geometry_decal or ASOBO_material_blend_gbuffer. Files without them
    // never allocate the shared decal buffer.
    bool m_assetHasDecalMaterials = false;
    // A2.2: which decal extension the file actually uses, so the viewport switches can
    // gate the buffer and the accumulation pass independently.
    bool m_assetHasGeometryDecalMaterials = false;
    bool m_assetHasBlendGbufferMaterials = false;
    // Set by updateGltf when extensionsUsed declares ASOBO_asset_optimized (A1.9). The package
    // tool stores TEXCOORD_n/COLOR_0 as half-floats in SHORT/USHORT and NORMAL/TANGENT as
    // signed bytes; the vertex attributes are fed to GL with the matching type.
    bool m_assetOptimized = false;
    bool m_modelImagesOmitted = false;

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
    MaterialExtensionOptions materialExtensionOptions;

    // A1.2: the asset's ASOBO_normal_map_convention is "DirectX". Whether the viewport
    // applies it is a separate AsoboOptions switch, so the shader flag combines both.
    bool usesDirectXNormalMaps() const { return m_assetNormalMapConventionDirectX; }
    bool declaresAsoboExtensions() const { return m_assetHasAsoboExtensions; }

    // A2.1: the loaded file contains at least one decal material.
    bool hasDecalMaterials() const { return m_assetHasDecalMaterials; }
    // A2.2: the file's decal materials by extension, for the per-viewport switches.
    bool hasGeometryDecalMaterials() const { return m_assetHasGeometryDecalMaterials; }
    bool hasBlendGbufferMaterials() const { return m_assetHasBlendGbufferMaterials; }
    // A3.7: the scene's nodes with ASOBO_sky_portal (parsed on updateScene and on extension edits).
    struct SkyPortalSource {
        float radiusMetres = 0.0f; // source_radius / 100, before the node scale
        float innerRadiusMetres = 0.0f; // the exporter preview's inner circle (gizmo only)
        float cosInner = 0.0f;
        float cosOuter = 0.0f;
    };
    bool hasSkyPortals() const { return !m_skyPortalSources.empty(); }
    const std::unordered_map<const gltf::Node*, SkyPortalSource>& getSkyPortalSources() const { return m_skyPortalSources; }

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
    // A3.2: the world-space AABB (lo, hi) of the drawables' static POSITION bounds, with the same per-primitive
    // bounds cullToClip uses. Returns the number of scene drawables it could not bound (skinned, morphed,
    // quantized); bounded is false when none could be bounded.
    size_t boundDrawables(const core::LinkedList<std::tuple<Mesh*, Material*>>& drawables, const ReverseMapping& reverseMapping,
                          math::vec3& lo, math::vec3& hi, bool& bounded) const;

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
        bool hasInteriorVolume = false;
        math::vec3 interiorMin = math::vec3(0.0f);
        math::vec3 interiorMax = math::vec3(0.0f);
        bool containsTransmissiveMaterial = false;
        std::vector<const gltf::Node*> skyPortals; // A3.7: visible portal nodes (isolation does not remove them)
        const gltf::Scene* scene = nullptr;
        AsoboOptions asoboOptions;
        MaterialExtensionOptions materialExtensionOptions;
        std::optional<uint32_t> materialVariantID;
        const gltf::Node* isolationRoot = nullptr;
        bool isolationIncludeChildren = false;
        uint64_t frameToken = 0;
    };
    const SceneTraversal& traverseScene(const gltf::Scene* gltfScene);
    void updateSkyPortals(const gltf::Gltf& gltf);
    std::unordered_map<const gltf::Node*, SkyPortalSource> m_skyPortalSources;
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
    const gltf::Gltf* m_buffersUploadedForScene = nullptr;
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

    // A1.11: generated MikkTSpace tangents for primitives without TANGENT whose material
    // uses a normal map. A2.2: cached once per loaded model and keyed by the primitive and
    // the normal map's UV set, so a material edit does not regenerate vertex data.
    struct GeneratedTangentKey {
        const gltf::Primitive* primitive = nullptr;
        uint32_t uvSet = 0;
        bool operator==(const GeneratedTangentKey&) const = default;
    };
    struct GeneratedTangentKeyHash {
        size_t operator()(const GeneratedTangentKey& key) const noexcept
        {
            const size_t primitive = std::hash<const void*> {}(key.primitive);
            return primitive ^ (static_cast<size_t>(key.uvSet) * 0x9E3779B97F4A7C15ULL);
        }
    };
    std::unordered_map<GeneratedTangentKey, std::unique_ptr<gl::Buffer>, GeneratedTangentKeyHash>
        m_generatedTangentBuffers;
    std::unordered_map<GeneratedTangentKey, std::vector<glm::vec4>, GeneratedTangentKeyHash>
        m_preparedTangents;
    // A2.2: the normal-map UV set the tangents were last synced for, per glTF material. A
    // material-only sync compares it to decide whether the meshes need a rebuild.
    std::unordered_map<const gltf::Material*, uint32_t> m_materialNormalUvSets;

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
