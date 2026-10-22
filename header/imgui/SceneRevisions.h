// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "imgui/SelfCheckResult.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace ux3d::imgui {

// Kinds of change that can invalidate renderer or viewport state (P4.1).
// Producers mark the kind they changed instead of setting one shared dirty
// boolean, so later tasks can narrow each consumer to the kinds it depends on.
// Geometry, morph weights and skin bindings currently have no direct UI
// producer: they change only through Topology, AnimationSet, AnimationTime and
// RenderSettings (skinning/morphing toggles), which still rebuild everything.
enum class SceneChange : uint8_t {
    Topology,         // model load / new document: every derived cache is stale
    Visibility,       // node or mesh visibility toggles (draw membership)
    Transform,        // node TRS edits
    Material,         // material properties, primitive material binding, active variant
    Light,            // KHR_lights_punctual property edits
    SceneCamera,      // glTF camera property edits
    Extension,        // unknown/vendor extension JSON edits (may affect rendering)
    Metadata,         // names and other non-rendered properties
    AnimationSet,     // active clip set changes
    AnimationTime,    // playback tick or seek
    RenderSettings,   // layer/command-generator globals: IBL, tone mapping, skybox, skinning, morphing
    ViewportSettings, // per-viewport debug mode, AA, culling, winding, wireframe
    ViewportLayout,   // viewport creation
    ViewCamera,       // per-viewport user camera: focus, attach/detach, projection, FoV
    Selection,        // inspector/OBB/gizmo selection only
    Count
};

constexpr size_t kSceneChangeCount = static_cast<size_t>(SceneChange::Count);
static_assert(kSceneChangeCount <= 32, "SceneChange mask must fit in 32 bits");

inline const char* sceneChangeName(SceneChange change)
{
    switch (change) {
    case SceneChange::Topology: return "Topology";
    case SceneChange::Visibility: return "Visibility";
    case SceneChange::Transform: return "Transform";
    case SceneChange::Material: return "Material";
    case SceneChange::Light: return "Light";
    case SceneChange::SceneCamera: return "SceneCamera";
    case SceneChange::Extension: return "Extension";
    case SceneChange::Metadata: return "Metadata";
    case SceneChange::AnimationSet: return "AnimationSet";
    case SceneChange::AnimationTime: return "AnimationTime";
    case SceneChange::RenderSettings: return "RenderSettings";
    case SceneChange::ViewportSettings: return "ViewportSettings";
    case SceneChange::ViewportLayout: return "ViewportLayout";
    case SceneChange::ViewCamera: return "ViewCamera";
    case SceneChange::Selection: return "Selection";
    case SceneChange::Count: break;
    }
    return "Unknown";
}

constexpr uint32_t sceneChangeBit(SceneChange change)
{
    return 1u << static_cast<uint32_t>(change);
}

constexpr uint32_t kAllSceneChanges = (1u << kSceneChangeCount) - 1u;

// Kinds that require the renderer layer to resynchronize from the glTF model
// (updateGltf/updateScene/applyAnimationTime). Excluded (P4.2):
// - Selection: the renderer never reads it; the OBB, gizmo, inspector and
//   focus read layer state that selection does not change.
// - ViewCamera: the viewport camera is chosen every frame (user camera, or the
//   attached glTF camera looked up in the load-time mapping) and its aspect and
//   projection are re-derived from stored parameters each frame. Edits to glTF
//   camera properties are SceneCamera and still sync.
constexpr uint32_t kRendererSyncChanges = kAllSceneChanges
    & ~sceneChangeBit(SceneChange::Selection)
    & ~sceneChangeBit(SceneChange::ViewCamera);

// Per-kind monotonically increasing revisions plus the set of kinds marked
// since the last consume. Revisions never reset, so a cache can key on the
// revision it was built from and detect any later change of that kind.
class SceneRevisions {
public:
    void mark(SceneChange change)
    {
        const size_t index = static_cast<size_t>(change);
        if (index >= kSceneChangeCount) return;
        ++m_revisions[index];
        ++m_markCount;
        m_pending |= sceneChangeBit(change);
    }

    bool hasPending() const { return m_pending != 0; }
    bool isPending(SceneChange change) const { return (m_pending & sceneChangeBit(change)) != 0; }
    uint32_t pendingMask() const { return m_pending; }

    // Returns the kinds marked since the previous consume and clears them.
    uint32_t consumePending()
    {
        const uint32_t pending = m_pending;
        m_pending = 0;
        return pending;
    }

    uint64_t revision(SceneChange change) const
    {
        const size_t index = static_cast<size_t>(change);
        return index < kSceneChangeCount ? m_revisions[index] : 0;
    }

    // Total number of mark() calls, including repeated marks of a pending kind.
    uint64_t markCount() const { return m_markCount; }

    static std::string maskToString(uint32_t mask)
    {
        std::string text;
        for (size_t i = 0; i < kSceneChangeCount; ++i) {
            if ((mask & (1u << i)) == 0) continue;
            if (!text.empty()) text += "|";
            text += sceneChangeName(static_cast<SceneChange>(i));
        }
        return text;
    }

    static SelfCheckResult runSelfCheck(bool forceFailure);

private:
    std::array<uint64_t, kSceneChangeCount> m_revisions{};
    uint64_t m_markCount = 0;
    uint32_t m_pending = 0;
};

} // namespace ux3d::imgui
