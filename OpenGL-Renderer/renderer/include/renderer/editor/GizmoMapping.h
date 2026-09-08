// Copyright © 2020 UX3D GmbH. All rights reserved.
// Authors: Jim Eckerlein, David Labode

#pragma once

#include "math/VectorTypes.h"
#include "renderer/Mapping.h"

#include <tuple>
#include <vector>

namespace ux3d::gltf {
class Node;
}

namespace ux3d::renderer {
class Material;
class Mesh;
class EditorRenderLayer;

/// Each gizmo piece corresponds to a single mesh-material instance.
enum class GizmoPiece {
    TranslationAxisX = 0u,
    TranslationAxisY,
    TranslationAxisZ,
    TranslationPlaneX,
    TranslationPlaneY,
    TranslationPlaneZ,
    TranslationHighlightPlaneX,
    TranslationHighlightPlaneY,
    TranslationHighlightPlaneZ,
    ScaleAxisX,
    ScaleAxisY,
    ScaleAxisZ,
    ScalePlaneX,
    ScalePlaneY,
    ScalePlaneZ,
    ScaleHighlightPlaneX,
    ScaleHighlightPlaneY,
    ScaleHighlightPlaneZ,
    ScaleCenter,
    RotationXCircle,
    RotationYCircle,
    RotationZCircle,
    RotationDepthStencil,
    Num,
    Invalid,
};

enum class GizmoTransformMode {
    DefaultTranslate,
    DefaultScale,
    DefaultRotate,
    TranslateX,
    TranslateY,
    TranslateZ,
    TranslatePlaneX,
    TranslatePlaneY,
    TranslatePlaneZ,
    ScaleX,
    ScaleY,
    ScaleZ,
    ScalePlaneX,
    ScalePlaneY,
    ScalePlaneZ,
    ScaleUniform,
    RotateX,
    RotateY,
    RotateZ
};

enum class GizmoInteractionMode {
    Hover = 0u,
    Drag
};

struct GizmoState {
    GizmoTransformMode transformMode;
    GizmoInteractionMode interactionMode;
};

using GizmoMeshMats = std::array<std::tuple<Mesh*, Material*>, static_cast<size_t>(GizmoPiece::Num)>;

using GltfGizmoTuple = std::tuple<const gltf::Node*, const GizmoState*>;

using RendererGizmoTuples = std::vector<std::tuple<Mesh*, Material*>>*;

class GizmoMapping : public Mapping<GltfGizmoTuple, RendererGizmoTuples> {
public:
    GizmoMapping(EditorRenderLayer& renderLayer);
    void setScale(float scale) { m_scale = scale; }
    void setNukeRotation(bool nukeRotation) { m_nukeRotation = nukeRotation; }

    static float getGizmoBaseScale() { return 0.175; }

protected:
    bool onUpdateMapping(GltfGizmoTuple gltfTuple, RendererGizmoTuples rendererTuples) final;
    RendererGizmoTuples onCreateMapping(GltfGizmoTuple gltfTuple) final;
    void onDestroyMapping(GltfGizmoTuple gltfTuple, RendererGizmoTuples rendereTuples) final;

protected:
    float m_scale;
    EditorRenderLayer& m_renderLayer;
    bool m_nukeRotation = true;

private:
    void getColors(const GizmoState& state, math::vec4* outColors[(size_t)GizmoPiece::Num]);

    static inline math::vec4 cX { 0.6f, 0.004f, 0.004f, 1.0f };
    static inline math::vec4 cY { 0.4f, 0.7056f, 0.05f, 1.0f };
    static inline math::vec4 cZ { 0.0f, 0.0f, 0.796f, 1.0f };
    static inline math::vec4 cUniform { 1.0f, 1.0f, 1.0f, 1.0f };
    static inline math::vec4 cAccent { 1.0f, 1.0f, 0.5f, 1.0f };
    static inline math::vec4 cAccentTranslucent { 1.0f, 1.0f, 0.6f, 0.5f };
    static inline math::vec4 cAttenuated { 1.0f, 1.0f, 1.0f, 0.3f };
    static inline math::vec4 cInvisible { 0.0f, 0.0f, 0.0f, 0.0f };
    static inline math::vec4 cMask { 1.0f, 1.0f, 1.0f, 0.0f };
};
}
