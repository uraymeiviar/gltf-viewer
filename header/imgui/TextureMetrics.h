// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// The Texture Distortion and Texture Density debug views: per-triangle UV measures of the current glTF scene, with the
// formulas, thresholds and colour scales of the FSS Blender heatmap (fss_blender_tools/uv_heatmap.py, msfs-blender-5-addons),
// so Glider and Blender show the same colours for the same faces. docs/TEXTURE_METRICS.md lists where glTF forces a
// difference. Built on the CPU in double precision when a viewport first selects one of the views, read by pbr.frag
// (UV_HEATMAP) through a table indexed by gl_PrimitiveID. Nothing is allocated or computed while neither view is selected.

#pragma once

#include <glad/glad.h>
#include <imgui.h>
#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ux3d::gltf {
class Gltf;
class Mesh;
class Node;
class Primitive;
}
namespace ux3d::renderer {
class GltfRenderLayer;
}

namespace ux3d::imgui {

class TextureMetrics {
public:
    static constexpr int kHistogramBins = 48; // CHART_BINS
    static constexpr double kFreedomLow = 0.9; // DEFAULT_FREEDOM: a face inside the range counts as 1.0
    static constexpr double kFreedomHigh = 1.1;
    static constexpr double kIslandLimit = 1.25; // fss_uv_autoseam_limit's default, for "islands over limit"

    struct Material {
        std::string name;
        uint64_t triangles = 0;
        double surfaceSquareMetres = 0.0;
        double referenceUvPerSquareMetre = 0.0; // area-weighted median of UV area / 3D area (1 without usable faces)
        std::string texture; // the slot that sets the pixels: baseColorTexture first; empty without a texture
        uint32_t uvSet = 0;
        uint32_t textureWidth = 0; // 0: unknown (no texture, or opened without textures)
        uint32_t textureHeight = 0;
        double pixelsPerMetre = 0.0; // sqrt(reference x width x height); 0 when the size is unknown
    };

    struct Stats {
        uint64_t triangles = 0;
        uint64_t degenerateTriangles = 0;
        uint64_t islands = 0;
        uint64_t islandsOverLimit = 0;
        double islandsOverLimitShare = 0.0;
        double surfaceSquareMetres = 0.0;
        double shareDegenerate = 0.0;
        double shareShearOver110 = 0.0;
        double shareShearOver125 = 0.0;
        double shareAreaOver110 = 0.0;
        double shareAreaOver125 = 0.0;
        double shareUnder = 0.0;
        double shareOver = 0.0;
        std::array<float, kHistogramBins> shear {};
        std::array<float, kHistogramBins> area {};
        std::array<float, kHistogramBins> density {};
        std::vector<Material> materials; // largest surface first
        std::vector<std::string> skippedNoUvs; // nodes with a primitive without the UV set, drawn in "no data" grey
        uint64_t nonTrianglePrimitives = 0;
        uint64_t truncatedTriangles = 0; // over the largest table the GPU can hold, drawn in "no data" grey
        uint64_t morphedPrimitives = 0; // measured on their base shape
        double buildMilliseconds = 0.0;
    };

    // Makes the table current for the layer's scene and returns its texture (0 when there is nothing to show), and
    // keeps u_UvHeatmapOffset set on the scene's renderer meshes. contentStamp: changes when the model, a node's
    // transform, a material or the clips change (not on a view setting, which also syncs the renderer: a sync only
    // sets the offsets again). A played animation does not rebuild it; re-select the view to measure the current pose.
    GLuint ensure(ux3d::renderer::GltfRenderLayer& layer, const ux3d::gltf::Gltf& model, uint64_t contentStamp);
    // The UV grid overlay in other views: keeps the UV uniforms set on the scene's renderer meshes, without any table.
    void ensureUvMaps(ux3d::renderer::GltfRenderLayer& layer, uint64_t contentStamp);
    // Frees the table; called when no viewport shows either view.
    void release();
    bool isBuilt() const { return m_built; }
    uint64_t builds() const { return m_builds; }

    const Stats& stats() const { return m_stats; }
    nlohmann::json stateJson() const;

    // Which sections of a viewport's legend are open; the chart always shows.
    struct LegendPanels {
        bool statistics = false;
        bool legend = false;
        bool settings = false;
    };
    // The legend in the viewport's lower left corner, as big as its open sections. distortionView: 0 both, 1 shear, 2 area
    // (when distortion is true). settings: draws the widgets of the Settings section. Call inside the viewport's window.
    void drawLegend(const ImVec2& imagePos, const ImVec2& imageSize, bool distortion, int distortionView, bool grid,
                    bool gridLabels, LegendPanels& panels, const std::function<void()>& settings) const;

private:
    void build(ux3d::renderer::GltfRenderLayer& layer, const ux3d::gltf::Gltf& model);

    GLuint m_texture = 0;
    bool m_built = false;
    uint64_t m_builds = 0;
    uint64_t m_sceneGeneration = 0;
    uint64_t m_contentStamp = 0;
    const void* m_scene = nullptr;
    struct MeshOffset {
        const ux3d::gltf::Node* node = nullptr;
        const ux3d::gltf::Mesh* mesh = nullptr;
        const ux3d::gltf::Primitive* primitive = nullptr;
        int offset = 0; // the first table entry + 1; 0: no data
        int uvSet = -1; // for the UV grid; -1: no grid
        std::array<float, 3> uvX { 1.0f, 0.0f, 0.0f }; // the UV's transforms as one affine map (pbr.frag)
        std::array<float, 3> uvY { 0.0f, 1.0f, 0.0f };
    };
    std::vector<MeshOffset> m_offsets;
    void applyOffsets(ux3d::renderer::GltfRenderLayer& layer) const;
    bool m_asoboUvOptions = false;
    uint64_t m_uvMapStamp = 0;
    uint64_t m_uvMapGeneration = 0;
    const void* m_uvMapLayer = nullptr;
    bool m_uvMapAsobo = false;
    bool m_uvMapsSet = false;
    Stats m_stats;
};

} // namespace ux3d::imgui
