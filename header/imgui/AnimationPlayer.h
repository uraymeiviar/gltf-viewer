// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "gltf/Gltf.h"
#include "gltf/Animation.h"
#include "renderer/gltf/GltfRenderLayer.h"

namespace ux3d::imgui {

class AnimationPlayer {
public:
    AnimationPlayer() = default;

    void update(float deltaTime, ux3d::renderer::GltfRenderLayer* gltfLayer);
    void renderToolbar(ux3d::gltf::Gltf& gltfModel);

    void reset();
    void setModel(ux3d::gltf::Gltf& gltfModel);

    bool isPlaying() const { return m_isPlaying; }
    float getCurrentTime() const { return m_currentTime; }
    float getMaxDuration() const { return m_maxDuration; }
    int getActiveAnimationIndex() const { return m_activeAnimationIdx; }

    void setActiveAnimationIndex(int idx, ux3d::gltf::Gltf& gltfModel) {
        m_activeAnimationIdx = idx;
        m_currentTime = 0.0f;
        calculateMaxDuration(gltfModel);
    }
    void play() { m_isPlaying = true; }
    void pause() { m_isPlaying = false; }
    void setPlaying(bool play) { m_isPlaying = play; }
    void setCurrentTime(float t) { m_currentTime = t; }

private:
    void calculateMaxDuration(ux3d::gltf::Gltf& gltfModel);

    bool m_isPlaying = false;
    bool m_isLooping = true;
    float m_playbackSpeed = 1.0f;
    float m_currentTime = 0.0f;
    float m_maxDuration = 0.0f;
    int m_activeAnimationIdx = 0;
};

} // namespace ux3d::imgui
