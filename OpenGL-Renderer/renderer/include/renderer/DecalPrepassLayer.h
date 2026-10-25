// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "renderer/IRenderLayer.h"
#include "renderer/Material.h"
#include "renderer/gl/OpenGL.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

namespace ux3d::renderer {
class OpaquePass;
class RenderPass;
namespace gl {
class GL;
class Texture;
struct SamplerState;
}

// A2.1: draws the decal prepass geometry (all OPAQUE and MASK drawables except decals)
// into the shared decal buffer before the viewport's forward opaque pass. It is modelled
// on PickingRenderLayer: it shares the scene traversal and draw lists and adds its own
// fragment shader and render pass. The prepass copies keep the source mesh, so skinning,
// morphing, instancing, culling and Mesh::effectiveFrontFace() are the same as the
// forward pass; only the fragment shader is replaced.
class DecalPrepassLayer {
public:
    enum class DebugView : uint8_t {
        None,
        Depth,
        Normal,
    };

    explicit DecalPrepassLayer(gl::GL* gl);
    ~DecalPrepassLayer();

    DecalPrepassLayer(const DecalPrepassLayer&) = delete;
    DecalPrepassLayer& operator=(const DecalPrepassLayer&) = delete;

    // Appends the prepass copies of the scene's opaque drawables to the scene. Drawables
    // the renderer owns rather than the glTF scene (skybox, transmission helpers) are
    // filtered out by isSceneMesh, because their meshes do not use primitive.vert.
    RendererScene applyLayer(RendererScene&& scene, const std::function<bool(const Mesh*)>& isSceneMesh);

    // The shared buffer's depth and G0 textures, for the decal debug Render Modes. A zero
    // handle (inactive buffer) suppresses the debug drawable.
    void setDebugTargets(GLuint depthTexture, GLuint normalTexture, int bufferWidth, int bufferHeight);
    // The sub-rectangle of the shared buffer this viewport rendered; the preview shader
    // samples only that part when the buffer is larger than the viewport.
    void setDebugViewport(int width, int height);
    void setDebugView(DebugView view) { m_debugView = view; }

    // Bind the prepass target before the pass and restore the viewport target after it.
    // Set by the application, like PickingRenderLayer::prePass/postPass.
    std::function<void()> prePass;
    std::function<void()> postPass;

    size_t getPrepassDrawableCount() const { return m_prepassDrawableCount; }
    size_t getMaterialPoolSize() const { return m_materials.size(); }
    uint64_t getMaterialsCreated() const { return m_materialsCreated; }
    uint64_t getMaterialsReused() const { return m_materialsReused; }

private:
    gl::GL* m_gl = nullptr;

    std::unique_ptr<OpaquePass> m_pass;
    // The fullscreen drawable that shows the prepass depth or G0 in a debug Render Mode.
    std::unique_ptr<RenderPass> m_debugPass;
    std::unique_ptr<Mesh> m_debugMesh;
    std::unique_ptr<Material> m_debugMaterial;
    std::unique_ptr<gl::Texture> m_depthTexture;
    std::unique_ptr<gl::Texture> m_normalTexture;
    std::unique_ptr<gl::SamplerState> m_debugSamplerState;

    // Stable copies of the source materials, so the command and instance caches keep their
    // material pointers between frames. Contents are refreshed every applyLayer.
    std::unordered_map<const Material*, std::unique_ptr<Material>> m_materials;
    DebugView m_debugView = DebugView::None;
    int m_bufferWidth = 0;
    int m_bufferHeight = 0;
    int m_viewportWidth = 0;
    int m_viewportHeight = 0;
    void updateDebugUVScale();
    size_t m_prepassDrawableCount = 0;
    uint64_t m_materialsCreated = 0;
    uint64_t m_materialsReused = 0;
};
}
