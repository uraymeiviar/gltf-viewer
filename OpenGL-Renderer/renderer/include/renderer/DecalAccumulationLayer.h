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
class DecalAccumulationPass;
namespace gl {
class GL;
class Texture;
struct SamplerState;
}

// A2.2: accumulates the decal draws (DrawOrderClass::Decal) into the shared decal buffer's
// A0-A5 targets. It removes those draws from the forward scene for the viewport, adds copies
// with the decal fragment shader and its own render pass, and leaves everything else alone.
// Decal factors come from the material's ASOBO extension and are refreshed every frame, so
// a material sync is visible without rebuilding the layer.
class DecalAccumulationLayer {
public:
    explicit DecalAccumulationLayer(gl::GL* gl);
    ~DecalAccumulationLayer();

    DecalAccumulationLayer(const DecalAccumulationLayer&) = delete;
    DecalAccumulationLayer& operator=(const DecalAccumulationLayer&) = delete;

    // Moves the applied decal-class scene drawables out of scene and appends the shader
    // copies. Drawables the renderer owns rather than the glTF scene (skybox, helpers) are
    // filtered out by isSceneMesh. Decals whose extension switch is off stay in the scene
    // and keep the A1.4 forward path.
    RendererScene applyLayer(RendererScene&& scene, const std::function<bool(const Mesh*)>& isSceneMesh);

    // The prepass result the decal shader needs for normalOverrideFactor.
    void setG0Target(GLuint texture);
    // A2.2: the prepass depth, sampled for the tolerant decal depth test. It is not an
    // attachment of this pass (the accumulation target has no depth buffer), so it can be
    // sampled without a feedback loop.
    void setDepthTarget(GLuint texture);
    // Which decal extensions this viewport applies (ASOBO_material_geometry_decal and
    // ASOBO_material_blend_gbuffer); decals of a switched-off extension stay forward.
    void setAppliedDecals(bool geometryDecal, bool blendGbuffer)
    {
        m_geometryDecalApplied = geometryDecal;
        m_blendGbufferApplied = blendGbuffer;
    }
    // Bind the accumulation target before the pass and restore the viewport target after it.
    void setPassCallbacks(std::function<void()> prePass, std::function<void()> postPass);

    size_t getDecalDrawableCount() const { return m_decalDrawableCount; }
    size_t getMaterialPoolSize() const { return m_materials.size(); }

private:
    bool appliesTo(const Material& material) const;

    gl::GL* m_gl = nullptr;

    std::unique_ptr<DecalAccumulationPass> m_pass;
    // Borrowed view of the shared prepass G0 texture; the handle is refreshed by setG0Target.
    std::unique_ptr<gl::Texture> m_g0Texture;
    std::unique_ptr<gl::SamplerState> m_g0SamplerState;
    // Borrowed view of the shared prepass depth texture; refreshed by setDepthTarget.
    std::unique_ptr<gl::Texture> m_depthTexture;
    std::unique_ptr<gl::SamplerState> m_depthSamplerState;

    // Stable copies of the decal materials, so command and instance caches keep their
    // pointers between frames. Contents are refreshed every applyLayer.
    std::unordered_map<const Material*, std::unique_ptr<Material>> m_materials;
    bool m_geometryDecalApplied = true;
    bool m_blendGbufferApplied = true;
    size_t m_decalDrawableCount = 0;
};
}
