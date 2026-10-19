// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/StatusBar.h"
#include "imgui/ValidationPanel.h"
#include "imgui/GltfValidator.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "imgui/Theme.h"
#include "imgui/AntiAliasing.h"
#include "renderer/gltf/GltfRenderLayer.h"
#include <imgui.h>
#include <cstdio>
#include <algorithm>

namespace ux3d::imgui {

void StatusBar::render(ux3d::gltf::Gltf& gltfModel,
                       ux3d::renderer::GltfRenderLayer* gltfLayer,
                       AnimationPlayer& animationPlayer,
                       bool& isSceneDirty,
                       bool& showLogger,
                       bool& showValidation,
                       const std::vector<LogEntry>& consoleLogs,
                       AntiAliasing* antiAliasing)
{
    const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    float statusBarHeight = ImGui::GetFrameHeight() + 8.0f;
    ImVec2 statusBarPos = ImVec2(mainViewport->WorkPos.x, mainViewport->WorkPos.y + mainViewport->WorkSize.y - statusBarHeight);
    ImVec2 statusBarSize = ImVec2(mainViewport->WorkSize.x, statusBarHeight);

    ImGui::SetNextWindowPos(statusBarPos);
    ImGui::SetNextWindowSize(statusBarSize);
    ImGui::SetNextWindowViewport(mainViewport->ID);

    ImGuiWindowFlags statusFlags = ImGuiWindowFlags_NoTitleBar |
                                   ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 2.0f));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.20f, 0.24f, 1.0f));

    if (ImGui::Begin("##AppStatusBar", nullptr, statusFlags)) {
        auto& anims = gltfModel.getElements<ux3d::gltf::Animation>();
        size_t totalAnims = anims.size();
        size_t activeCount = 0;
        for (const auto& a : anims) {
            if (a.isPlaying()) ++activeCount;
        }

        // -------------------------------------------------------------
        // Calculate Right Group Width (glTF Valid + Logger message count)
        // -------------------------------------------------------------
        size_t errCount = 0, warnCount = 0, infoCount = 0;
        for (const auto& e : consoleLogs) {
            if (e.level == ux3d::core::LogLevel::Error || e.level == ux3d::core::LogLevel::Fatal) errCount++;
            else if (e.level == ux3d::core::LogLevel::Warning) warnCount++;
            else infoCount++;
        }

        char errNumBuf[32], warnNumBuf[32], infoNumBuf[32];
        std::snprintf(errNumBuf, sizeof(errNumBuf), "%zu", errCount);
        std::snprintf(warnNumBuf, sizeof(warnNumBuf), "%zu", warnCount);
        std::snprintf(infoNumBuf, sizeof(infoNumBuf), "%zu", infoCount);

        float logPillWidth = ImGui::GetStyle().FramePadding.x * 2.0f + 8.0f
            + ImGui::CalcTextSize(ICON_MDI_ALERT_CIRCLE).x + 3.0f + ImGui::CalcTextSize(errNumBuf).x + 8.0f
            + ImGui::CalcTextSize(ICON_MDI_ALERT).x + 3.0f + ImGui::CalcTextSize(warnNumBuf).x + 8.0f
            + ImGui::CalcTextSize(ICON_MDI_INFORMATION).x + 3.0f + ImGui::CalcTextSize(infoNumBuf).x;

        const auto& valReport = GltfValidator::instance().getLastReport();
        const char* valText = ICON_MDI_CHECK_CIRCLE " glTF Valid";
        char valBuf[64];
        if (!valReport.validated) {
            valText = ICON_MDI_SHIELD_OUTLINE " Validate";
        } else if (valReport.errorCount > 0) {
            std::snprintf(valBuf, sizeof(valBuf), "%s %d %s", ICON_MDI_ALERT_OCTAGON, valReport.errorCount, valReport.errorCount == 1 ? "Error" : "Errors");
            valText = valBuf;
        } else if (valReport.warningCount > 0) {
            std::snprintf(valBuf, sizeof(valBuf), "%s %d %s", ICON_MDI_ALERT, valReport.warningCount, valReport.warningCount == 1 ? "Warning" : "Warnings");
            valText = valBuf;
        }
        float validBadgeWidth = ImGui::CalcTextSize(valText).x + ImGui::GetStyle().FramePadding.x * 2.0f + 6.0f;
        float rightGroupWidth = logPillWidth + ImGui::GetStyle().ItemSpacing.x + validBadgeWidth + 4.0f;

        // -------------------------------------------------------------
        // Left Group (A): Animation controls
        // -------------------------------------------------------------
        // 1. Animation selection popup button: [Icon] (X/Y)
        char animBtnLabel[64];
        std::snprintf(animBtnLabel, sizeof(animBtnLabel), "%s (%zu/%zu)##StatusBarAnimBtn",
                      ICON_MDI_PLAY_BOX_OUTLINE, activeCount, totalAnims);

        if (totalAnims == 0) ImGui::BeginDisabled();
        if (ImGui::Button(animBtnLabel)) {
            ImGui::OpenPopup("StatusBarAnimPopup");
        }
        if (totalAnims == 0) ImGui::EndDisabled();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Select active animations (%zu of %zu active)", activeCount, totalAnims);
        }

        if (ImGui::BeginPopup("StatusBarAnimPopup")) {
            ImGui::TextDisabled("ANIMATIONS");
            ImGui::Separator();

            bool allSelected = animationPlayer.areAllPossibleAnimationsSelected(gltfModel);
            if (ImGui::Checkbox("Select all non-conflicting", &allSelected)) {
                animationPlayer.selectAllPossibleAnimations(allSelected, gltfModel, gltfLayer);
                isSceneDirty = true;
            }

            ImGui::Separator();

            float listHeight = std::min(260.0f, static_cast<float>(anims.size() * 26.0f));
            ImGui::BeginChild("StatusBarAnimScrollList", ImVec2(300, listHeight), true);
            for (size_t i = 0; i < anims.size(); ++i) {
                auto& a = anims.at(i);
                bool isPlaying = a.isPlaying();
                bool conflict = !isPlaying && animationPlayer.isAnimationConflictingWithActive(i, gltfModel);

                std::string name = a.hasName() ? a.getName().c_str() : ("Animation " + std::to_string(i));

                if (conflict) ImGui::BeginDisabled();
                if (ImGui::Checkbox((name + "##" + std::to_string(i)).c_str(), &isPlaying)) {
                    animationPlayer.toggleAnimation(i, isPlaying, gltfModel, gltfLayer);
                    isSceneDirty = true;
                }
                if (conflict) {
                    ImGui::EndDisabled();
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                        ImGui::SetTooltip("Conflicts with an already active animation");
                    }
                }
            }
            ImGui::EndChild();

            ImGui::TextDisabled("Only non-conflicting animations can play together.");
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

        // 2. Playback buttons: |<<, Play/Pause, Stop, >>|
        if (totalAnims == 0) ImGui::BeginDisabled();

        // Rewind
        if (ImGui::Button(ICON_MDI_SKIP_BACKWARD "##Rewind", ImVec2(28, 0))) {
            animationPlayer.setCurrentTime(0.0f, gltfLayer);
            if (antiAliasing != nullptr) antiAliasing->resetHistory();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rewind (0.0s)");

        ImGui::SameLine();
        // Play / Pause
        if (animationPlayer.isPlaying()) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.45f, 0.15f, 1.0f));
            if (ImGui::Button(ICON_MDI_PAUSE "##Pause", ImVec2(28, 0))) {
                animationPlayer.pause();
            }
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Pause");
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.67f, 0.42f, 1.0f));
            if (ImGui::Button(ICON_MDI_PLAY "##Play", ImVec2(28, 0))) {
                animationPlayer.play();
            }
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Play");
        }

        ImGui::SameLine();
        // Stop
        if (ImGui::Button(ICON_MDI_STOP "##Stop", ImVec2(28, 0))) {
            animationPlayer.stop(gltfLayer);
            if (antiAliasing != nullptr) antiAliasing->resetHistory();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop & Reset (0.0s)");

        ImGui::SameLine();
        // Fast-Forward / End
        if (ImGui::Button(ICON_MDI_SKIP_FORWARD "##End", ImVec2(28, 0))) {
            animationPlayer.setCurrentTime(animationPlayer.getMaxDuration(), gltfLayer);
            if (antiAliasing != nullptr) antiAliasing->resetHistory();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Jump to end");

        ImGui::SameLine();

        // 3. Loop checkbox & Loop count
        bool looping = animationPlayer.isLooping();
        if (ImGui::Checkbox(ICON_MDI_REPEAT "##Loop", &looping)) {
            animationPlayer.setLooping(looping);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(looping ? "Looping enabled" : "Looping disabled");

        if (looping) {
            ImGui::SameLine();
            int loopCount = animationPlayer.getLoopCount();
            ImGui::SetNextItemWidth(36.0f);
            if (ImGui::DragInt("##LoopCount", &loopCount, 0.1f, 0, 99, loopCount == 0 ? "∞" : "%d")) {
                animationPlayer.setLoopCount(loopCount);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Loop Count (0 = Continuous ∞)");
        }

        ImGui::SameLine();
        // 4. Playback speed
        ImGui::SetNextItemWidth(56.0f);
        const char* speedOptions[] = { "0.25x", "0.5x", "1.0x", "1.5x", "2.0x" };
        static int speedIdx = 2;
        if (ImGui::Combo("##Speed", &speedIdx, speedOptions, 5)) {
            if (speedIdx == 0) animationPlayer.setPlaybackSpeed(0.25f);
            else if (speedIdx == 1) animationPlayer.setPlaybackSpeed(0.5f);
            else if (speedIdx == 2) animationPlayer.setPlaybackSpeed(1.0f);
            else if (speedIdx == 3) animationPlayer.setPlaybackSpeed(1.5f);
            else if (speedIdx == 4) animationPlayer.setPlaybackSpeed(2.0f);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Playback speed");

        ImGui::SameLine();
        // 5. Time text (e.g. 0.00 / 8.35s)
        char timeBuf[64];
        std::snprintf(timeBuf, sizeof(timeBuf), "%.2f / %.2fs", animationPlayer.getCurrentTime(), animationPlayer.getMaxDuration());
        ImGui::PushFont(getMonoFont());
        ImGui::Text("%s", timeBuf);
        ImGui::PopFont();

        if (totalAnims == 0) ImGui::EndDisabled();

        // -------------------------------------------------------------
        // Middle Area (B): Animation Slider (fills remaining width)
        // -------------------------------------------------------------
        ImGui::SameLine();
        float sliderStartX = ImGui::GetCursorPosX();
        float contentMaxX = ImGui::GetWindowContentRegionMax().x;
        float rightStart = contentMaxX - rightGroupWidth;
        float sliderWidth = rightStart - sliderStartX - ImGui::GetStyle().ItemSpacing.x;
        if (sliderWidth < 40.0f) {
            sliderWidth = 40.0f;
            rightStart = sliderStartX + sliderWidth + ImGui::GetStyle().ItemSpacing.x;
        }

        ImGui::SetNextItemWidth(sliderWidth);
        float maxDuration = animationPlayer.getMaxDuration();
        if (maxDuration > 0.0f && totalAnims > 0) {
            float t = animationPlayer.getCurrentTime();
            if (ImGui::SliderFloat("##StatusBarSlider", &t, 0.0f, maxDuration, "")) {
                animationPlayer.setCurrentTime(t, gltfLayer);
                if (antiAliasing != nullptr) antiAliasing->resetHistory();
                isSceneDirty = true;
            }
        } else {
            float dummy = 0.0f;
            ImGui::BeginDisabled();
            ImGui::SliderFloat("##StatusBarSlider", &dummy, 0.0f, 0.0f, "");
            ImGui::EndDisabled();
        }

        // -------------------------------------------------------------
        // Right Area: (C) Message Counts + (D) glTF Valid
        // -------------------------------------------------------------
        ImGui::SameLine(rightStart);

        // (C) Message counts custom pill with colored icons
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        float btnHeight = ImGui::GetFrameHeight();
        ImVec2 btnSize(logPillWidth, btnHeight);

        bool pillClicked = ImGui::InvisibleButton("##LogPillBtn", btnSize);
        bool pillHovered = ImGui::IsItemHovered();
        bool pillActive = ImGui::IsItemActive();

        if (pillClicked) {
            showLogger = !showLogger;
        }

        ImU32 bgCol;
        if (errCount > 0) {
            bgCol = pillActive ? IM_COL32(140, 40, 40, 220) : (pillHovered ? IM_COL32(115, 35, 35, 200) : IM_COL32(80, 25, 25, 180));
        } else if (warnCount > 0) {
            bgCol = pillActive ? IM_COL32(130, 95, 25, 220) : (pillHovered ? IM_COL32(110, 80, 20, 200) : IM_COL32(75, 58, 15, 180));
        } else {
            bgCol = pillActive ? IM_COL32(55, 60, 70, 220) : (pillHovered ? IM_COL32(45, 50, 60, 200) : IM_COL32(32, 35, 42, 180));
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        float rounding = ImGui::GetStyle().FrameRounding;
        dl->AddRectFilled(p0, ImVec2(p0.x + btnSize.x, p0.y + btnSize.y), bgCol, rounding);
        dl->AddRect(p0, ImVec2(p0.x + btnSize.x, p0.y + btnSize.y), IM_COL32(70, 75, 85, 255), rounding);

        float curX = p0.x + ImGui::GetStyle().FramePadding.x + 4.0f;
        float curY = p0.y + (btnHeight - ImGui::GetTextLineHeight()) * 0.5f;

        // Error icon (Red)
        dl->AddText(ImVec2(curX, curY), IM_COL32(240, 80, 80, 255), ICON_MDI_ALERT_CIRCLE);
        curX += ImGui::CalcTextSize(ICON_MDI_ALERT_CIRCLE).x + 3.0f;
        dl->AddText(ImVec2(curX, curY), IM_COL32(220, 220, 225, 255), errNumBuf);
        curX += ImGui::CalcTextSize(errNumBuf).x + 8.0f;

        // Warning icon (Amber)
        dl->AddText(ImVec2(curX, curY), IM_COL32(245, 180, 50, 255), ICON_MDI_ALERT);
        curX += ImGui::CalcTextSize(ICON_MDI_ALERT).x + 3.0f;
        dl->AddText(ImVec2(curX, curY), IM_COL32(220, 220, 225, 255), warnNumBuf);
        curX += ImGui::CalcTextSize(warnNumBuf).x + 8.0f;

        // Info icon (Sky Blue)
        dl->AddText(ImVec2(curX, curY), IM_COL32(85, 175, 245, 255), ICON_MDI_INFORMATION);
        curX += ImGui::CalcTextSize(ICON_MDI_INFORMATION).x + 3.0f;
        dl->AddText(ImVec2(curX, curY), IM_COL32(220, 220, 225, 255), infoNumBuf);

        if (pillHovered) {
            ImGui::SetTooltip("Logger Messages:\n  %s %zu Errors\n  %s %zu Warnings\n  %s %zu Info\nClick to %s Logger window.",
                              ICON_MDI_ALERT_CIRCLE, errCount,
                              ICON_MDI_ALERT, warnCount,
                              ICON_MDI_INFORMATION, infoCount,
                              showLogger ? "hide" : "show");
        }

        ImGui::SameLine();

        // (D) glTF Valid Status
        ValidationPanel::renderStatusBarIndicator(&showValidation);
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(4);
}

} // namespace ux3d::imgui
