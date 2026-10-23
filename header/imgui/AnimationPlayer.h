// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "gltf/Gltf.h"
#include "imgui/SceneRevisions.h"
#include "gltf/Animation.h"
#include "gltf/animation/AnimationChannelTarget.h"
#include "imgui/SelfCheckResult.h"
#include "renderer/gltf/GltfRenderLayer.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace ux3d::imgui {

class AnimationPlayer {
public:
    AnimationPlayer() = default;

    void update(float deltaTime, ux3d::renderer::GltfRenderLayer* gltfLayer);
    void renderToolbar(ux3d::gltf::Gltf& gltfModel, ux3d::renderer::GltfRenderLayer* gltfLayer, SceneRevisions& revisions);

    // Animation checklist used by the status-bar "active animations" popup and the
    // Animation > Active submenu. Includes the skinning/morphing settings.
    void renderActiveAnimationsContent(ux3d::gltf::Gltf& gltfModel,
                                       ux3d::renderer::GltfRenderLayer* gltfLayer,
                                       SceneRevisions& revisions);

    void reset();
    void setModel(ux3d::gltf::Gltf& gltfModel, ux3d::renderer::GltfRenderLayer* gltfLayer = nullptr);

    bool isPlaying() const { return m_isPlaying; }
    float getCurrentTime() const { return m_currentTime; }
    float getMaxDuration() const { return m_maxDuration; }
    int getActiveAnimationIndex() const { return m_activeAnimationIdx; }

    void play() { m_isPlaying = true; }
    void pause() { m_isPlaying = false; }
    void stop(ux3d::renderer::GltfRenderLayer* gltfLayer = nullptr);
    void setPlaying(bool play) { m_isPlaying = play; }
    void setCurrentTime(float t, ux3d::renderer::GltfRenderLayer* gltfLayer = nullptr);
    void setActiveAnimationIndex(int idx, ux3d::gltf::Gltf& gltfModel, ux3d::renderer::GltfRenderLayer* gltfLayer = nullptr);

    bool isLooping() const { return m_isLooping; }
    void setLooping(bool loop) { m_isLooping = loop; }
    int getLoopCount() const { return m_loopCount; }
    void setLoopCount(int count) { m_loopCount = count; m_currentLoop = 0; }
    float getPlaybackSpeed() const { return m_playbackSpeed; }
    void setPlaybackSpeed(float speed) { m_playbackSpeed = speed; }

    // Multi-animation non-conflicting selection (matching Gestaltor)
    void selectAllPossibleAnimations(bool select, ux3d::gltf::Gltf& gltfModel, ux3d::renderer::GltfRenderLayer* gltfLayer);
    void toggleAnimation(size_t index, bool playing, ux3d::gltf::Gltf& gltfModel, ux3d::renderer::GltfRenderLayer* gltfLayer);
    bool isAnimationConflictingWithActive(size_t index, const ux3d::gltf::Gltf& gltfModel) const;
    bool areAllPossibleAnimationsSelected(const ux3d::gltf::Gltf& gltfModel) const;
    bool checkConflict(const ux3d::gltf::Animation& animA, const ux3d::gltf::Animation& animB) const;

    // Diagnostic used by the reproducible P2.1 conflict-index check. It builds
    // synthetic clips, compares indexed answers with the legacy algorithm and
    // returns a structured result plus report text. No renderer state is required.
    SelfCheckResult runConflictSelfCheck(bool forceFailure = false);

    void calculateMaxDuration(ux3d::gltf::Gltf& gltfModel, ux3d::renderer::GltfRenderLayer* gltfLayer = nullptr);

private:
    struct ConflictTarget {
        const ux3d::gltf::ChildOfRoot* object = nullptr;
        std::string path;

        bool operator==(const ConflictTarget& _other) const
        {
            return object == _other.object && path == _other.path;
        }
    };

    struct ConflictTargetHash {
        size_t operator()(const ConflictTarget& _target) const noexcept
        {
            const size_t objectHash = std::hash<const void*>{}(_target.object);
            const size_t pathHash = std::hash<std::string>{}(_target.path);
            return objectHash ^ (pathHash + 0x9e3779b97f4a7c15ull + (objectHash << 6) + (objectHash >> 2));
        }
    };

    struct ConflictClip {
        std::vector<ConflictTarget> targets;
    };

    struct ConflictCache {
        const ux3d::gltf::Gltf* model = nullptr;
        uint64_t modelRevision = 0;
        uint64_t targetRevision = 0;
        uint64_t playingRevision = 0;
        size_t animationCount = 0;
        std::vector<ConflictClip> clips;
        std::unordered_map<ConflictTarget, uint32_t, ConflictTargetHash> activeTargetCounts;
        bool activeCountsValid = false;
        bool built = false;
    };

    void ensureConflictIndex(const ux3d::gltf::Gltf& gltfModel) const;
    void rebuildActiveConflictCounts(const ux3d::gltf::Gltf& gltfModel) const;
    void invalidateActiveConflictCounts() const;
    bool targetConflictsWithActive(const ConflictTarget& target, bool selfIsPlaying) const;

    mutable ConflictCache m_conflictCache;

    bool m_isPlaying = false;
    bool m_isLooping = true;
    int m_loopCount = 0; // 0 = infinite continuous loop
    int m_currentLoop = 0;
    float m_playbackSpeed = 1.0f;
    float m_currentTime = 0.0f;
    float m_maxDuration = 0.0f;
    int m_activeAnimationIdx = 0;
};

} // namespace ux3d::imgui
