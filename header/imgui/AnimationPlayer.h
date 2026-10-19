// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "gltf/Gltf.h"
#include "gltf/Animation.h"
#include "renderer/gltf/GltfRenderLayer.h"
#include <string>
#include <vector>

namespace ux3d::imgui {

class AnimationPlayer {
public:
    AnimationPlayer() = default;

    void update(float deltaTime, ux3d::renderer::GltfRenderLayer* gltfLayer);
    void renderToolbar(ux3d::gltf::Gltf& gltfModel, ux3d::renderer::GltfRenderLayer* gltfLayer, bool& isSceneDirty);

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

    void calculateMaxDuration(ux3d::gltf::Gltf& gltfModel, ux3d::renderer::GltfRenderLayer* gltfLayer = nullptr);

private:
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
