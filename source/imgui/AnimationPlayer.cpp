// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/AnimationPlayer.h"
#include <imgui.h>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace ux3d::imgui {

void AnimationPlayer::reset()
{
    m_isPlaying = false;
    m_currentTime = 0.0f;
    m_maxDuration = 0.0f;
    m_activeAnimationIdx = 0;
}

void AnimationPlayer::setModel(ux3d::gltf::Gltf& gltfModel)
{
    reset();
    calculateMaxDuration(gltfModel);
}

void AnimationPlayer::calculateMaxDuration(ux3d::gltf::Gltf& gltfModel)
{
    m_maxDuration = 0.0f;
    auto& anims = gltfModel.getElements<ux3d::gltf::Animation>();
    if (anims.empty()) return;

    if (m_activeAnimationIdx < 0 || m_activeAnimationIdx >= static_cast<int>(anims.size())) {
        m_activeAnimationIdx = 0;
    }

    auto& anim = anims.at(m_activeAnimationIdx);

    for (const auto& ch : anim.getChannels()) {
        if (ch.getSampler() != nullptr && ch.getSampler()->getInputAccessor() != nullptr) {
            auto* acc = ch.getSampler()->getInputAccessor();
            if (!acc->getMax().empty()) {
                float t = static_cast<float>(acc->getMax()[0]);
                if (t > m_maxDuration) {
                    m_maxDuration = t;
                }
            }
        }
    }
}

void AnimationPlayer::update(float deltaTime, ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    if (m_isPlaying && m_maxDuration > 0.0f) {
        m_currentTime += deltaTime * m_playbackSpeed;
        if (m_currentTime > m_maxDuration) {
            if (m_isLooping) {
                m_currentTime = std::fmod(m_currentTime, m_maxDuration);
            } else {
                m_currentTime = m_maxDuration;
                m_isPlaying = false;
            }
        }
    }

    if (gltfLayer != nullptr) {
        gltfLayer->applyAnimationTime(m_currentTime);
    }
}

void AnimationPlayer::renderToolbar(ux3d::gltf::Gltf& gltfModel)
{
    auto& anims = gltfModel.getElements<ux3d::gltf::Animation>();
    if (anims.empty()) {
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.14f, 0.17f, 0.90f));
    ImGui::BeginChild("AnimationToolbar", ImVec2(0, 36), true, ImGuiWindowFlags_NoScrollbar);

    // Play / Pause button
    if (m_isPlaying) {
        if (ImGui::Button("Pause ||", ImVec2(70, 0))) {
            m_isPlaying = false;
        }
    } else {
        if (ImGui::Button("Play >", ImVec2(70, 0))) {
            m_isPlaying = true;
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Stop []", ImVec2(60, 0))) {
        m_isPlaying = false;
        m_currentTime = 0.0f;
    }

    ImGui::SameLine();
    // Loop toggle
    if (ImGui::Checkbox("Loop", &m_isLooping)) {}

    ImGui::SameLine();
    // Playback Speed
    ImGui::SetNextItemWidth(75);
    const char* speedOptions[] = { "0.25x", "0.5x", "1.0x", "1.5x", "2.0x" };
    static int speedIdx = 2;
    if (ImGui::Combo("##Speed", &speedIdx, speedOptions, 5)) {
        if (speedIdx == 0) m_playbackSpeed = 0.25f;
        else if (speedIdx == 1) m_playbackSpeed = 0.5f;
        else if (speedIdx == 2) m_playbackSpeed = 1.0f;
        else if (speedIdx == 3) m_playbackSpeed = 1.5f;
        else if (speedIdx == 4) m_playbackSpeed = 2.0f;
    }

    ImGui::SameLine();
    // Time formatted display
    char timeBuf[64];
    std::snprintf(timeBuf, sizeof(timeBuf), "%.2f / %.2fs", m_currentTime, m_maxDuration);
    ImGui::Text("%s", timeBuf);

    ImGui::SameLine();
    // Scrubber timeline slider
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - (anims.size() > 1 ? 160 : 0));
    if (m_maxDuration > 0.0f) {
        if (ImGui::SliderFloat("##Scrubber", &m_currentTime, 0.0f, m_maxDuration, "%.2fs")) {
            // User scrubbing
        }
    }

    // Active animation selector (if more than 1 animation)
    if (anims.size() > 1) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(150);
        std::string currentAnimName = "Clip " + std::to_string(m_activeAnimationIdx);
        if (m_activeAnimationIdx >= 0 && m_activeAnimationIdx < static_cast<int>(anims.size())) {
            auto& a = anims.at(m_activeAnimationIdx);
            if (a.hasName()) {
                currentAnimName = a.getName().c_str();
            }
        }

        if (ImGui::BeginCombo("##ActiveAnim", currentAnimName.c_str())) {
            int idx = 0;
            for (auto& a : anims) {
                bool isSel = (idx == m_activeAnimationIdx);
                std::string aName = a.hasName() ? a.getName().c_str() : ("Clip " + std::to_string(idx));
                if (ImGui::Selectable(aName.c_str(), isSel)) {
                    m_activeAnimationIdx = idx;
                    m_currentTime = 0.0f;
                    calculateMaxDuration(gltfModel);
                }
                ++idx;
            }
            ImGui::EndCombo();
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

} // namespace ux3d::imgui
