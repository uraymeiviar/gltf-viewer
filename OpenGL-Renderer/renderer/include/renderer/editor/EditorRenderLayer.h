// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Willy Becher 🏝

#pragma once

#include "math/VectorTypes.h"
#include "renderer/IRenderLayer.h"
#include "renderer/editor/AudioGizmoMapping.h"
#include "renderer/editor/CameraGizmoMapping.h"
#include "renderer/editor/EditorResourceManager.h"
#include "renderer/editor/LightGizmoMapping.h"
#include "renderer/editor/OrientationGizmoMapping.h"
#include "renderer/gltf/GltfRenderLayer.h"

#include <variant>
#include <vector>

namespace ux3d::renderer {
enum class RenderMode : uint32_t {
    None = 0u,
    Invalid,

    Wireframe,
    TexCoord_0,
    TexCoord_1,
    NormalTexture,
    ShadingNormal,
    NormalGeometry,
    Tangent,
    Bitangent,
    Alpha,

    Occlusion,
    Emissive,

    MetallicRoughness,
    Roughness,
    Metallic,
    BaseColor,

    Clearcoat,
    ClearcoatFactor,
    ClearcoatRoughness,
    ClearcoatNormal,

    Sheen,
    SheenColor,
    SheenRoughness,

    Specular,
    SpecularFactor,
    SpecularColor,

    TransmissionVolume,
    TransmissionFactor,
    VolumeThickness,

    Iridescence,
    IridescenceFactor,
    IridescenceThickness,

    // A2.1: inspect the decal prepass targets (shown only while the buffer is active).
    DecalPrepassDepth,
    DecalPrepassNormal,
    SSR,
    SSRRay,
    // A3.2: sun shadow cascades (index colours) and the sun visibility term.
    ShadowCascades,
    ShadowVisibility,

};

enum class GizmoSpace : uint32_t {
    Global = 0u,
    Local
};

class EditorRenderLayer : public renderer::IRenderLayer {
public:
    GltfRenderLayer& gltfLayer;
    RenderMode m_renderMode = RenderMode::None;
    math::vec4 m_wireframeColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    std::unordered_set<std::variant<const gltf::Node*, const gltf::Mesh*, const gltf::Light*, const gltf::Camera*, const gltf::Material*, const gltf::Primitive*>> m_selectedItems;
    // This only contains actually selected materials, not the materials of selected nodes
    // selected materials are both in this set and in m_selectedItems if they are selected
    // This set is used to quickly check if a material is selected
    std::unordered_set<const gltf::Material*> m_selectedMaterials;
    math::vec4 m_outlineColor = { 1.0f, 0.0f, 0.0f, 1.0f };
    math::vec3 m_boundingBoxColor = { 1, 1, 0 };
    math::vec3 m_skeletonColor = { 0, 1, 1 };
    math::vec4 m_gridColor = { 0.72f, 0.72f, 0.72f, 1.0f };
    uint32_t m_outlineThickness = 5;
    GizmoSpace m_gizmoSpace = GizmoSpace::Global;
    bool showGrid = true;
    bool showBoundingBoxes = false;
    bool showSkinsAndBones = false;

public:
    EditorRenderLayer(gl::GL* gl, GltfRenderLayer& gltfLayer, const filesystem::Path& editorTexturesPath, const filesystem::Path& editorGizmoModelsPath);

    void update(); // operates directly on the undelying gltfscene
    RendererScene applyLayer(RendererScene&& scene) override;
    ReverseMapping applyLayer(ReverseMapping&& reverseMapping);

    std::vector<std::tuple<Mesh*, Material*>> getSelectedTuples();

    void setGizmoedNode(const gltf::Node* node, const GizmoState* state);
    const gltf::Node* getGizmoedNode() const;

    bool hasActiveGizmo() const;
    EditorResourceManager& getEditorResourceManager() { return m_editorResourceManager; }

    void setGizmoScaling(float scaling) { m_gizmoScaling = scaling; }

    std::map<OrientationGizmoPiece, OrientationCubeInteractionMode> m_orientationGizmoPieces;

private:
    const gltf::Node* m_gizmoedNode = nullptr;
    const GizmoState* m_gizmoState = nullptr;

    float getGridTranslationOffset(float cameraPos, float scaling, float maxDistance);

    EditorResourceManager m_editorResourceManager;

    LightGizmoMapping m_lightGizmoMapping;
    CameraGizmoMapping m_cameraGizmoMapping;
    AudioGizmoMapping m_audioGizmoMapping;
    GizmoMapping m_gizmoMapping;
    OrientationGizmoMapping m_orientationGizmoMapping;

    float m_gizmoScaling;

    Material* m_wireframeMaterial = nullptr; // these clones are used to be able to set specific material parameters for wireframe material
    std::vector<Mesh*> m_selectedMeshClones; // these clones are used to be able to change mesh properties of selected nodes

    std::vector<std::unique_ptr<Mesh>> m_boundingBoxMeshes;
    std::vector<std::unique_ptr<Mesh>> m_skeletonMeshes;
    std::vector<std::pair<const gltf::Node*, const gltf::Node*>> m_bones;
    std::vector<const gltf::Node*> m_skins;

private:
    RendererScene unfoldMaterials(RendererScene&& scene);
};
}
