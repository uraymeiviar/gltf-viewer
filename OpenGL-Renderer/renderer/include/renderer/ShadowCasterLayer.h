// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "core/LinkedList.h"
#include "renderer/Material.h"
#include "renderer/gl/OpenGL.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace ux3d::renderer {
class OpaquePass;
class Mesh;
namespace gl {
class GL;
}

// A3.2: the sun shadow caster pass (docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.2). Like DecalPrepassLayer it
// reuses the scene's drawables, so skinning, morph targets, instancing and Mesh::effectiveFrontFace() are
// those of the forward pass; only the fragment shader (shadow_caster.frag: depth only, MASK cutoff and
// A1.7 dither discard) and the pass are replaced.
//
// Casters: OPAQUE, MASK and dithered materials of the forward Opaque pass. Not casting: BLEND (Transparent
// passes) and KHR transmission (Transmissive pass), ASOBO decals (geometry decal, blend gbuffer), glass and
// windshield materials (ASOBO glass/windshield types and the extras material code), invisible materials
// and environment occluders (also while the viewport shows them), and materials whose
// ASOBO_material_shadow_options sets noCastShadow (while the viewport applies that extension).
// Casters draw without face culling, so open or single-sided cockpit geometry still blocks the sun.
class ShadowCasterLayer {
public:
    struct Options {
        bool asoboEnabled = true;          // the viewport's ASOBO master switch
        bool applyShadowOptions = true;    // ASOBO_material_shadow_options (noCastShadow) applied
        bool applyMaterialCode = true;     // ASOBO_material_code (extras glass/windshield) applied
        float slopeBias = 2.0f;            // glPolygonOffset factor
        float constantBias = 1.0f;         // glPolygonOffset units
        // Changes whenever the source materials may have changed (model, material edits, render settings):
        // the caster copies are refreshed from their sources only then (no per-frame material copies).
        uint64_t sourceRevision = 0;
        // Changes when the model is replaced: the pool is emptied (its keys are the old model's materials).
        uint64_t modelGeneration = 0;
    };

    struct Stats {
        size_t casters = 0;
        size_t notOpaquePass = 0;   // BLEND, transmission, helpers
        size_t decals = 0;
        size_t glass = 0;
        size_t invisible = 0;       // invisible materials and environment occluders
        size_t noCastShadow = 0;
    };

    explicit ShadowCasterLayer(gl::GL* gl);
    ~ShadowCasterLayer();

    ShadowCasterLayer(const ShadowCasterLayer&) = delete;
    ShadowCasterLayer& operator=(const ShadowCasterLayer&) = delete;

    // Replaces casters with the (mesh, caster material) pairs of the drawables that cast. Drawables the
    // renderer owns rather than the glTF scene (skybox, transmission helpers) are filtered out by isSceneMesh.
    void collect(const core::LinkedList<std::tuple<Mesh*, Material*>>& drawables,
                 const std::function<bool(const Mesh*)>& isSceneMesh, const Options& options,
                 core::LinkedList<std::tuple<Mesh*, Material*>>& casters);

    // Whether a source material casts under these options (the rules above; the pass is not checked).
    static bool materialCasts(const Material& material, const Options& options, Stats* stats = nullptr);

    // Bind the shadow-map layer before the pass and restore the viewport target after it.
    std::function<void()> prePass;
    std::function<void()> postPass;

    const Stats& getStats() const { return m_stats; }
    size_t getMaterialPoolSize() const { return m_materials.size(); }
    uint64_t getMaterialCopies() const { return m_materialCopies; }
    void clear() { m_materials.clear(); }

private:
    struct Entry {
        std::unique_ptr<Material> material;
        uint64_t sourceRevision = 0;
        float slopeBias = 0.0f;
        float constantBias = 0.0f;
    };

    gl::GL* m_gl = nullptr;
    std::unique_ptr<OpaquePass> m_pass;
    std::unordered_map<const Material*, Entry> m_materials;
    Stats m_stats;
    uint64_t m_materialCopies = 0;
    uint64_t m_modelGeneration = 0;
};
}
