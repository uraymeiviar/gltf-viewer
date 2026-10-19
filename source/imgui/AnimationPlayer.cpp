// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/AnimationPlayer.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "gltf/animation/AnimationPath.h"
#include <imgui.h>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>

namespace ux3d::imgui {

void AnimationPlayer::reset()
{
    m_isPlaying = false;
    m_currentTime = 0.0f;
    m_maxDuration = 0.0f;
    m_activeAnimationIdx = 0;
    m_currentLoop = 0;
}

void AnimationPlayer::setModel(ux3d::gltf::Gltf& gltfModel, ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    reset();
    selectAllPossibleAnimations(true, gltfModel, gltfLayer);
}

bool AnimationPlayer::checkConflict(const ux3d::gltf::Animation& animA, const ux3d::gltf::Animation& animB) const
{
    for (const auto& chA : animA.getChannels()) {
        const ux3d::gltf::ChildOfRoot* nodeA = chA.getTarget().getNode();
        std::string pathA = ux3d::gltf::animationPathToString(chA.getTarget().getPath()).c_str();
        if (chA.getTarget().m_animationPointer.has_value()) {
            nodeA = std::get<0>(chA.getTarget().m_animationPointer.value());
            pathA = std::get<1>(chA.getTarget().m_animationPointer.value());
        }

        for (const auto& chB : animB.getChannels()) {
            const ux3d::gltf::ChildOfRoot* nodeB = chB.getTarget().getNode();
            std::string pathB = ux3d::gltf::animationPathToString(chB.getTarget().getPath()).c_str();
            if (chB.getTarget().m_animationPointer.has_value()) {
                nodeB = std::get<0>(chB.getTarget().m_animationPointer.value());
                pathB = std::get<1>(chB.getTarget().m_animationPointer.value());
            }

            if (nodeA == nodeB && pathA == pathB && (nodeA != nullptr || !pathA.empty())) {
                return true;
            }
        }
    }
    return false;
}

bool AnimationPlayer::isAnimationConflictingWithActive(size_t index, const ux3d::gltf::Gltf& gltfModel) const
{
    const auto& animations = gltfModel.getElements<ux3d::gltf::Animation>();
    if (index >= animations.size()) return false;

    const auto& animToCheck = animations.at(index);
    for (size_t i = 0; i < animations.size(); ++i) {
        if (i == index) continue;
        const auto& activeAnim = animations.at(i);
        if (activeAnim.isPlaying()) {
            if (checkConflict(animToCheck, activeAnim)) {
                return true;
            }
        }
    }
    return false;
}

bool AnimationPlayer::areAllPossibleAnimationsSelected(const ux3d::gltf::Gltf& gltfModel) const
{
    const auto& animations = gltfModel.getElements<ux3d::gltf::Animation>();
    if (animations.empty()) return false;

    for (size_t i = 0; i < animations.size(); ++i) {
        const auto& anim = animations.at(i);
        if (!anim.isPlaying()) {
            if (!isAnimationConflictingWithActive(i, gltfModel)) {
                return false;
            }
        }
    }
    return true;
}

void AnimationPlayer::calculateMaxDuration(ux3d::gltf::Gltf& gltfModel, ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    m_maxDuration = 0.0f;
    if (gltfLayer != nullptr) {
        m_maxDuration = gltfLayer->getAnimationDuration();
    }
    if (m_maxDuration <= 0.0f) {
        auto& anims = gltfModel.getElements<ux3d::gltf::Animation>();
        for (const auto& anim : anims) {
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
    }
}

void AnimationPlayer::selectAllPossibleAnimations(bool select, ux3d::gltf::Gltf& gltfModel, ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    auto& animations = gltfModel.getElements<ux3d::gltf::Animation>();
    if (animations.empty()) return;

    if (select) {
        std::vector<const ux3d::gltf::Animation*> activeAnimations;
        for (auto& anim : animations) {
            bool canPlay = true;
            for (const auto* activeAnim : activeAnimations) {
                if (checkConflict(anim, *activeAnim)) {
                    canPlay = false;
                    break;
                }
            }
            anim.setPlaying(canPlay);
            if (canPlay) {
                activeAnimations.push_back(&anim);
            }
        }
    } else {
        for (auto& anim : animations) {
            anim.setPlaying(false);
        }
    }

    if (gltfLayer != nullptr) {
        gltfLayer->m_animationMapping.update(gltfModel.getAnimations());
        calculateMaxDuration(gltfModel, gltfLayer);
        gltfLayer->applyAnimationTime(m_currentTime);
    }
}

void AnimationPlayer::toggleAnimation(size_t index, bool playing, ux3d::gltf::Gltf& gltfModel, ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    auto& animations = gltfModel.getElements<ux3d::gltf::Animation>();
    if (index >= animations.size()) return;

    if (playing) {
        if (!isAnimationConflictingWithActive(index, gltfModel)) {
            animations.at(index).setPlaying(true);
        }
    } else {
        animations.at(index).setPlaying(false);
    }

    if (gltfLayer != nullptr) {
        gltfLayer->m_animationMapping.update(gltfModel.getAnimations());
        calculateMaxDuration(gltfModel, gltfLayer);
        gltfLayer->applyAnimationTime(m_currentTime);
    }
}

void AnimationPlayer::setActiveAnimationIndex(int idx, ux3d::gltf::Gltf& gltfModel, ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    auto& animations = gltfModel.getElements<ux3d::gltf::Animation>();
    if (idx < 0 || idx >= static_cast<int>(animations.size())) return;

    m_activeAnimationIdx = idx;
    for (size_t i = 0; i < animations.size(); ++i) {
        animations.at(i).setPlaying(static_cast<int>(i) == idx);
    }

    if (gltfLayer != nullptr) {
        gltfLayer->m_animationMapping.update(gltfModel.getAnimations());
        calculateMaxDuration(gltfModel, gltfLayer);
        gltfLayer->applyAnimationTime(m_currentTime);
    }
}

void AnimationPlayer::stop(ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    m_isPlaying = false;
    m_currentTime = 0.0f;
    m_currentLoop = 0;
    if (gltfLayer != nullptr) {
        gltfLayer->applyAnimationTime(0.0f);
    }
}

void AnimationPlayer::setCurrentTime(float t, ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    m_currentTime = t;
    if (gltfLayer != nullptr) {
        gltfLayer->applyAnimationTime(m_currentTime);
    }
}

void AnimationPlayer::update(float deltaTime, ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    if (m_isPlaying && m_maxDuration > 0.0f) {
        m_currentTime += deltaTime * m_playbackSpeed;
        if (m_currentTime > m_maxDuration) {
            if (m_isLooping) {
                if (m_loopCount > 0) {
                    m_currentLoop++;
                    if (m_currentLoop >= m_loopCount) {
                        m_currentTime = m_maxDuration;
                        m_isPlaying = false;
                        m_currentLoop = 0;
                    } else {
                        m_currentTime = std::fmod(m_currentTime, m_maxDuration);
                    }
                } else {
                    m_currentTime = std::fmod(m_currentTime, m_maxDuration);
                }
            } else {
                m_currentTime = m_maxDuration;
                m_isPlaying = false;
            }
        }
        if (gltfLayer != nullptr) {
            gltfLayer->applyAnimationTime(m_currentTime);
        }
    }
}

void AnimationPlayer::renderToolbar(ux3d::gltf::Gltf& gltfModel, ux3d::renderer::GltfRenderLayer* gltfLayer, bool& isSceneDirty)
{
    auto& anims = gltfModel.getElements<ux3d::gltf::Animation>();
    if (anims.empty()) {
        return;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.14f, 0.17f, 0.95f));
    ImGui::BeginChild("AnimationToolbar", ImVec2(0, 36), true, ImGuiWindowFlags_NoScrollbar);

    // 1. Animations Popup Menu Button (matching Gestaltor)
    size_t activeCount = 0;
    for (const auto& a : anims) {
        if (a.isPlaying()) ++activeCount;
    }
    char animBtnLabel[128];
    std::snprintf(animBtnLabel, sizeof(animBtnLabel), ICON_MDI_ANIMATION " Animations (%zu/%zu) ##Btn", activeCount, anims.size());
    if (ImGui::Button(animBtnLabel)) {
        ImGui::OpenPopup("AnimationsMenuPopup");
    }

    if (ImGui::BeginPopup("AnimationsMenuPopup")) {
        ImGui::TextDisabled("ANIMATIONS");
        ImGui::Separator();

        bool allSelected = areAllPossibleAnimationsSelected(gltfModel);
        if (ImGui::Checkbox("Select all non-conflicting", &allSelected)) {
            selectAllPossibleAnimations(allSelected, gltfModel, gltfLayer);
            isSceneDirty = true;
        }

        ImGui::Separator();

        float listHeight = std::min(260.0f, static_cast<float>(anims.size() * 26.0f));
        ImGui::BeginChild("AnimScrollList", ImVec2(300, listHeight), true);
        for (size_t i = 0; i < anims.size(); ++i) {
            auto& a = anims.at(i);
            bool isPlaying = a.isPlaying();
            bool conflict = !isPlaying && isAnimationConflictingWithActive(i, gltfModel);

            std::string name = a.hasName() ? a.getName().c_str() : ("Animation_" + std::to_string(i));

            if (conflict) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Checkbox((name + "##" + std::to_string(i)).c_str(), &isPlaying)) {
                toggleAnimation(i, isPlaying, gltfModel, gltfLayer);
                isSceneDirty = true;
            }
            if (conflict) {
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip("Conflicts with an already playing animation");
                }
            }
        }
        ImGui::EndChild();

        ImGui::TextDisabled("You can only select non-conflicting animations.");
        ImGui::Separator();

        ImGui::TextDisabled("SETTINGS");
        if (gltfLayer != nullptr) {
            if (ImGui::Checkbox("Skinning", &gltfLayer->renderSettings.enableSkinning)) {
                isSceneDirty = true;
            }
            if (ImGui::Checkbox("Morphing", &gltfLayer->renderSettings.enableMorphing)) {
                isSceneDirty = true;
            }
        }

        ImGui::EndPopup();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // 2. Playback control buttons: |<, Play/Pause, Stop, >|
    if (ImGui::Button(ICON_MDI_SKIP_BACKWARD "##Rewind", ImVec2(34, 0))) {
        setCurrentTime(0.0f, gltfLayer);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rewind to start (0.0s)");

    ImGui::SameLine();
    if (m_isPlaying) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.45f, 0.15f, 1.0f));
        if (ImGui::Button(ICON_MDI_PAUSE " Pause", ImVec2(76, 0))) {
            pause();
        }
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.67f, 0.42f, 1.0f));
        if (ImGui::Button(ICON_MDI_PLAY " Play", ImVec2(76, 0))) {
            play();
        }
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_STOP " Stop", ImVec2(68, 0))) {
        stop(gltfLayer);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop & Reset (0.0s)");

    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_SKIP_FORWARD "##End", ImVec2(34, 0))) {
        setCurrentTime(m_maxDuration, gltfLayer);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Jump to end");

    ImGui::SameLine();
    // 3. Loop checkbox
    ImGui::Checkbox("Loop", &m_isLooping);

    ImGui::SameLine();
    // 4. Playback speed
    ImGui::SetNextItemWidth(72);
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
    // 5. Time formatted display
    char timeBuf[64];
    std::snprintf(timeBuf, sizeof(timeBuf), "%.2f / %.2fs", m_currentTime, m_maxDuration);
    ImGui::Text("%s", timeBuf);

    ImGui::SameLine();
    // 6. Timeline Scrubber slider
    float availW = ImGui::GetContentRegionAvail().x;
    if (availW > 40.0f) {
        ImGui::SetNextItemWidth(availW);
        if (m_maxDuration > 0.0f) {
            float t = m_currentTime;
            if (ImGui::SliderFloat("##Scrubber", &t, 0.0f, m_maxDuration, "%.2fs")) {
                m_currentTime = t;
                if (gltfLayer != nullptr) {
                    gltfLayer->applyAnimationTime(m_currentTime);
                }
            }
        } else {
            float dummy = 0.0f;
            ImGui::SliderFloat("##Scrubber", &dummy, 0.0f, 0.0f, "0.00s");
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

} // namespace ux3d::imgui
