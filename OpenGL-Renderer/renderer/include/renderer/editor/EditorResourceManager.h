// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Jim Eckerlein

#pragma once

#include "renderer/editor/GizmoMapping.h"
#include "renderer/editor/OrientationGizmoMapping.h"
#include "renderer/gltf/GltfResourceManager.h"

#include <memory>
#include <optional>
#include <vector>

namespace ux3d::filesystem {
class Path;
class File;
}

namespace ux3d::gltf {
class Gltf;
class Mesh;
}

namespace ux3d::renderer {
// forward delcs
class GizmoPass;
class OutlinePass;
class PostprocessingPass;
class Mesh;

class EditorResourceManager : public GltfResourceManager {
public:
    EditorResourceManager(gl::GL* gl, const filesystem::Path& editorGizmoModelsPath);
    virtual ~EditorResourceManager() = default;

    TransparentPass* getGizmoPass() const { return m_foregroundMaskPass; }
    Material* getSilhouetteMaterialCulled() const { return m_silhouetteMaterialCulled; }
    Material* getSilhouetteMaterial() const { return m_silhouetteMaterial; }
    Material* getOutlineMaterial() const { return m_outlineMaterial; }
    Mesh* getFullscreenTriangleMesh() const { return m_fullscreenTriangleMesh; }
    std::vector<std::tuple<Mesh*, Material*>> getAxes() const;
    std::tuple<Mesh*, Material*> getSmallGrid() const;
    std::tuple<Mesh*, Material*> getBigGrid() const;

    static constexpr int gridSize = 1000;
    static constexpr float axesThickness = 0.02f;
    static constexpr math::vec4 colorAxisX = math::vec4(0.73f, 0.08f, 0.08f, 1.0f);
    static constexpr math::vec4 colorAxisY = math::vec4(0.56f, 0.73f, 0.22f, 1.0f);
    static constexpr math::vec4 colorAxisZ = math::vec4(0.22f, 0.0f, 0.57f, 1.0f);

    const GizmoMeshMats& getGizmoMeshMats() const { return m_gizmoMeshMats; }
    const auto& getOrientationGizmoMeshMats() const { return m_orientationGizmoMeshMats; }

    std::optional<GizmoTransformMode> mapGizmoMeshToTransformMode(Mesh* mesh) const;
    std::optional<OrientationGizmoPiece> mapOrientationGizmoMeshToPiece(Mesh* mesh) const;

    void createFramebufferTexture(GLuint textureID);
    void setBlitFramebufferFunction(std::function<void()> blitFramebuffer, math::vec4 clearColor);

    std::unique_ptr<Mesh> makeBoundingBoxMesh();
    std::unique_ptr<Material> makeBoundingBoxMaterial();

    Material* makeWireframeMaterial();

    std::unique_ptr<Material> makeSkinHelperMaterial();
    std::unique_ptr<Material> makeBoneHelperMaterial();
    std::unique_ptr<Mesh> makeSkinHelperMesh();
    std::unique_ptr<Mesh> makeBoneHelperMesh();

private:
    void initGizmo(gltf::Gltf& outGltf, const filesystem::File& gltfFile, GizmoPiece (*meshNameToPiece)(const core::string& name));
    void initGizmo(gltf::Gltf& outGltf, const filesystem::File& gltfFile, OrientationGizmoPiece (*meshNameToPiece)(const core::string& name));
    void initGizmoMesh(const gltf::Mesh& gltfMesh, Mesh& mesh);
    Material* createGizmoPostMaterial(); // Will be masked by gizmo material
    Material* createGizmoMaterial();
    Material* createSilhouetteMaterial();
    Material* createOutlineMaterial();
    Mesh* createFullscreenTriangleMesh();

    std::unique_ptr<gltf::Gltf> m_translationGizmoGltf;
    std::unique_ptr<gltf::Gltf> m_scaleGizmoGltf;
    std::unique_ptr<gltf::Gltf> m_rotationGizmoGltf;
    std::unique_ptr<gltf::Gltf> m_cameraOrientationGizmoGltf;

    // Pass to render a mask for the m_gizmoMaskedPass
    // (anything drawn here will mask out the fragments of anything drawn in the m_gizmoMaskedPass)
    TransparentPass* m_foregroundMaskPass = nullptr;
    // Renderpass that is affected by m_gizmoMaskPass. This pass is also acting lij
    TransparentPass* m_foregroundMaskedPass = nullptr;
    // Renderpass for drawables that should always stay in the foreground (depth buffer cleared). This pass is not affected by the mask
    TransparentPass* m_foregroundPass = nullptr;
    // Renderpass that will be affected by depth testing of the other render layers (depth buffer not cleared)
    TransparentPass* m_depthTestedPass = nullptr;

    // Outline Rendering:
    PostprocessingPass* m_postprocessingPass = nullptr;
    OutlinePass* m_outlinePass = nullptr;
    GizmoMeshMats m_gizmoMeshMats;
    std::map<OrientationGizmoPiece, std::tuple<Mesh*, Material*>> m_orientationGizmoMeshMats;
    Material* m_silhouetteMaterial = nullptr;
    Material* m_silhouetteMaterialCulled = nullptr;
    Material* m_outlineMaterial = nullptr;

    // Grid:
    Material* m_meterGridMaterial = nullptr;
    Material* m_decimeterGridMaterial = nullptr;
    Material* m_originAxesXMaterial = nullptr;
    Material* m_originAxesZMaterial = nullptr;
    Mesh* m_originAxesXMesh = nullptr;
    Mesh* m_originAxesZMesh = nullptr;
    Mesh* m_meterGridMesh = nullptr;
    Mesh* m_decimeterGridMesh = nullptr;
    gl::Buffer* m_originAxisXVertexBuffer = nullptr;
    gl::Buffer* m_originAxisZVertexBuffer = nullptr;
    gl::Buffer* m_meterGridVertexBuffer = nullptr;
    gl::Buffer* m_decimeterGridVertexBuffer = nullptr;
    void initGrid();
    void initOriginAxes();

    Mesh* m_fullscreenTriangleMesh = nullptr;
    gl::Texture* m_framebufferTexture = nullptr;
    gl::SamplerState* m_framebufferSamplerState = nullptr;

    // Bounding boxes
    void initBoundingBoxBuffers();
    gl::Buffer* m_boundingBoxIndexBuffer;
    gl::Buffer* m_boundingBoxVertexBuffer;
    size_t m_boundingBoxIndexCount;
    size_t m_boundingBoxVertexCount;

    // Skin
    void initSkinHelperBuffers();
    void initBoneHelperBuffers();
    gl::Buffer* m_skinHelperBuffer;
    gl::Buffer* m_boneHelperBuffer;
    size_t m_skinHelperVertexCount;
    size_t m_boneHelperVertexCount;
};

}
