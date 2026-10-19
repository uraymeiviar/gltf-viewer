// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/ValidationPanel.h"
#include "imgui/GltfValidator.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "imgui/Theme.h"
#include "renderer/gltf/GltfRenderLayer.h"

#include <imgui.h>
#include <algorithm>
#include <string>

namespace ux3d::imgui {

static bool s_filterErrors = true;
static bool s_filterWarnings = true;
static bool s_filterInfos = true;
static char s_searchBuffer[256] = "";
static int s_selectedIssueIndex = -1;
static bool s_focusRequested = false;
static int s_focusFrameCountdown = 0;

void ValidationPanel::requestFocus()
{
    s_focusRequested = true;
    s_focusFrameCountdown = 4;
    std::string windowTitle = std::string(ICON_MDI_SHIELD_CHECK_OUTLINE) + " Validation###ValidationPanel";
    ImGui::SetWindowFocus(windowTitle.c_str());
}

void ValidationPanel::render(bool* pOpen,
                             const std::string& currentFilePath,
                             ux3d::gltf::Gltf& gltfModel,
                             ux3d::imgui::SelectionState& selection,
                             ux3d::editor::CameraActor& cameraActor,
                             ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    if (pOpen != nullptr && !*pOpen) return;

    ImGui::SetNextWindowSize(ImVec2(750, 420), ImGuiCond_FirstUseEver);

    if (s_focusRequested || s_focusFrameCountdown > 0) {
        ImGui::SetNextWindowFocus();
        if (s_focusFrameCountdown > 0) s_focusFrameCountdown--;
        s_focusRequested = false;
    }

    std::string windowTitle = std::string(ICON_MDI_SHIELD_CHECK_OUTLINE) + " Validation###ValidationPanel";
    if (!ImGui::Begin(windowTitle.c_str(), pOpen)) {
        ImGui::End();
        return;
    }

    auto& validator = GltfValidator::instance();
    const auto& report = validator.getLastReport();

    // -----------------------------------------------------------------
    // Header & Summary Status Bar
    // -----------------------------------------------------------------
    if (!report.validated) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s Validation has not run on current model.", ICON_MDI_INFORMATION_OUTLINE);
        ImGui::SameLine();
        if (ImGui::Button(ICON_MDI_REFRESH " Run Validation")) {
            validator.validate(currentFilePath, gltfModel);
        }
    } else {
        if (report.errorCount > 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
                               "%s Validation Failed: %d %s, %d %s (Scan: %.1f ms)",
                               ICON_MDI_ALERT_OCTAGON,
                               report.errorCount, report.errorCount == 1 ? "Error" : "Errors",
                               report.warningCount, report.warningCount == 1 ? "Warning" : "Warnings",
                               report.durationMs);
        } else if (report.warningCount > 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f),
                               "%s Valid glTF with %d %s (Scan: %.1f ms)",
                               ICON_MDI_ALERT,
                               report.warningCount, report.warningCount == 1 ? "Warning" : "Warnings",
                               report.durationMs);
        } else {
            ImGui::TextColored(ImVec4(0.2f, 0.95f, 0.5f, 1.0f),
                               "%s Valid glTF 2.0 Asset (0 Errors, 0 Warnings, Scan: %.1f ms)",
                               ICON_MDI_CHECK_CIRCLE,
                               report.durationMs);
        }

        ImGui::SameLine(ImGui::GetWindowWidth() - 250);
        if (ImGui::Button(ICON_MDI_REFRESH " Re-validate")) {
            validator.validate(currentFilePath, gltfModel);
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_MDI_CONTENT_COPY " Copy Report")) {
            ImGui::SetClipboardText(report.toMarkdown().c_str());
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Copy full validation report as Markdown to clipboard.");
        }
    }

    ImGui::Separator();

    // -----------------------------------------------------------------
    // Filter & Search Controls
    // -----------------------------------------------------------------
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Filter:");
    ImGui::SameLine();

    // Errors filter toggle
    char errLabel[64];
    std::snprintf(errLabel, sizeof(errLabel), "%s Errors (%d)###FiltErr", ICON_MDI_ALERT_OCTAGON, report.errorCount);
    if (s_filterErrors) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.15f, 0.15f, 1.0f));
    }
    if (ImGui::Button(errLabel)) {
        s_filterErrors = !s_filterErrors;
    }
    if (s_filterErrors) {
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();

    // Warnings filter toggle
    char warnLabel[64];
    std::snprintf(warnLabel, sizeof(warnLabel), "%s Warnings (%d)###FiltWarn", ICON_MDI_ALERT, report.warningCount);
    if (s_filterWarnings) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.45f, 0.1f, 1.0f));
    }
    if (ImGui::Button(warnLabel)) {
        s_filterWarnings = !s_filterWarnings;
    }
    if (s_filterWarnings) {
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();

    // Infos filter toggle
    char infoLabel[64];
    std::snprintf(infoLabel, sizeof(infoLabel), "%s Infos (%d)###FiltInfo", ICON_MDI_INFORMATION, report.infoCount);
    if (s_filterInfos) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.35f, 0.6f, 1.0f));
    }
    if (ImGui::Button(infoLabel)) {
        s_filterInfos = !s_filterInfos;
    }
    if (s_filterInfos) {
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(180);
    ImGui::InputTextWithHint("##IssueSearch", "Search issues...", s_searchBuffer, sizeof(s_searchBuffer));
    if (s_searchBuffer[0] != '\0') {
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_MDI_CLOSE)) {
            s_searchBuffer[0] = '\0';
        }
    }

    ImGui::Spacing();

    // -----------------------------------------------------------------
    // Issues Table
    // -----------------------------------------------------------------
    static ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable |
                                       ImGuiTableFlags_Reorderable |
                                       ImGuiTableFlags_RowBg |
                                       ImGuiTableFlags_Borders |
                                       ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("IssuesTable", 4, tableFlags, ImVec2(0, 0))) {
        ImGui::TableSetupColumn("Severity", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Code", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableSetupColumn("Pointer / Target", ImGuiTableColumnFlags_WidthFixed, 160.0f);
        ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        std::string searchLower = s_searchBuffer;
        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

        int rowIdx = 0;
        for (const auto& issue : report.issues) {
            // Apply severity filter
            if (issue.severity == ValidationSeverity::Error && !s_filterErrors) {
                rowIdx++;
                continue;
            }
            if (issue.severity == ValidationSeverity::Warning && !s_filterWarnings) {
                rowIdx++;
                continue;
            }
            if (issue.severity == ValidationSeverity::Info && !s_filterInfos) {
                rowIdx++;
                continue;
            }

            // Apply text search filter
            if (!searchLower.empty()) {
                std::string codeLower = issue.code;
                std::transform(codeLower.begin(), codeLower.end(), codeLower.begin(), ::tolower);
                std::string msgLower = issue.message;
                std::transform(msgLower.begin(), msgLower.end(), msgLower.begin(), ::tolower);
                std::string ptrLower = issue.jsonPointer;
                std::transform(ptrLower.begin(), ptrLower.end(), ptrLower.begin(), ::tolower);

                if (codeLower.find(searchLower) == std::string::npos &&
                    msgLower.find(searchLower) == std::string::npos &&
                    ptrLower.find(searchLower) == std::string::npos) {
                    rowIdx++;
                    continue;
                }
            }

            ImGui::TableNextRow();

            // Column 1: Severity
            ImGui::TableSetColumnIndex(0);
            ImVec4 sevColor = (issue.severity == ValidationSeverity::Error) ? ImVec4(1.0f, 0.35f, 0.35f, 1.0f) :
                              (issue.severity == ValidationSeverity::Warning) ? ImVec4(1.0f, 0.85f, 0.25f, 1.0f) :
                              ImVec4(0.4f, 0.75f, 1.0f, 1.0f);
            const char* sevIcon = (issue.severity == ValidationSeverity::Error) ? ICON_MDI_ALERT_OCTAGON :
                                  (issue.severity == ValidationSeverity::Warning) ? ICON_MDI_ALERT :
                                  ICON_MDI_INFORMATION;

            bool isSelected = (s_selectedIssueIndex == rowIdx);
            char rowSelectableId[64];
            std::snprintf(rowSelectableId, sizeof(rowSelectableId), "##IssueRow_%d", rowIdx);

            if (ImGui::Selectable(rowSelectableId, isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                s_selectedIssueIndex = rowIdx;

                // Select target object in application inspector
                if (issue.targetObject != nullptr) {
                    switch (issue.targetType) {
                        case SelectionType::Node:
                            selection.selectNode(static_cast<ux3d::gltf::Node*>(issue.targetObject));
                            break;
                        case SelectionType::Material:
                            selection.selectMaterial(static_cast<ux3d::gltf::Material*>(issue.targetObject));
                            break;
                        case SelectionType::Mesh:
                            selection.selectMesh(static_cast<ux3d::gltf::Mesh*>(issue.targetObject));
                            break;
                        case SelectionType::Camera:
                            selection.selectCamera(static_cast<ux3d::gltf::Camera*>(issue.targetObject));
                            break;
                        case SelectionType::Skin:
                            selection.selectSkin(static_cast<ux3d::gltf::Skin*>(issue.targetObject));
                            break;
                        case SelectionType::Animation:
                            selection.selectAnimation(static_cast<ux3d::gltf::Animation*>(issue.targetObject));
                            break;
                        case SelectionType::Texture:
                            selection.selectTexture(static_cast<ux3d::gltf::Texture*>(issue.targetObject));
                            break;
                        case SelectionType::Image:
                            selection.selectImage(static_cast<ux3d::gltf::Image*>(issue.targetObject));
                            break;
                        default: break;
                    }
                }

                // If double-clicked on a row with a node, focus camera on it
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    if (issue.targetType == SelectionType::Node && selection.selectedNode != nullptr) {
                        cameraActor.presentFocusNode(gltfLayer, selection.selectedNode, &gltfModel);
                    }
                }
            }

            ImGui::SameLine();
            ImGui::TextColored(sevColor, "%s %s", sevIcon, issue.getSeverityString());

            // Column 2: Code
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(issue.code.c_str());

            // Column 3: Location / JSON Pointer
            ImGui::TableSetColumnIndex(2);
            if (!issue.jsonPointer.empty()) {
                ImGui::TextUnformatted(issue.jsonPointer.c_str());
            } else if (issue.targetIndex >= 0) {
                ImGui::Text("[%d]", issue.targetIndex);
            } else {
                ImGui::TextDisabled("-");
            }

            // Column 4: Message
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(issue.message.c_str());

            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::TextColored(sevColor, "%s %s: %s", sevIcon, issue.getSeverityString(), issue.code.c_str());
                if (!issue.jsonPointer.empty()) {
                    ImGui::TextDisabled("Location: %s", issue.jsonPointer.c_str());
                }
                ImGui::Separator();
                ImGui::TextWrapped("%s", issue.message.c_str());
                if (issue.targetObject != nullptr) {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Double-click to focus and inspect object.");
                }
                ImGui::EndTooltip();
            }

            rowIdx++;
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

void ValidationPanel::renderStatusBarIndicator(bool* pOpenValidationPanel)
{
    const auto& report = GltfValidator::instance().getLastReport();

    if (!report.validated) {
        if (ImGui::SmallButton(ICON_MDI_SHIELD_OUTLINE " Validate")) {
            if (pOpenValidationPanel) *pOpenValidationPanel = true;
            requestFocus();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Run glTF 2.0 specification validation.");
        }
        return;
    }

    if (report.errorCount > 0) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s %d %s", ICON_MDI_ALERT_OCTAGON, report.errorCount, report.errorCount == 1 ? "Error" : "Errors");
        if (ImGui::SmallButton(buf)) {
            if (pOpenValidationPanel) *pOpenValidationPanel = true;
            requestFocus();
        }
        ImGui::PopStyleColor(2);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Asset has %d validation %s and %d %s.\nClick to inspect in Validation panel.",
                              report.errorCount, report.errorCount == 1 ? "error" : "errors",
                              report.warningCount, report.warningCount == 1 ? "warning" : "warnings");
        }
    } else if (report.warningCount > 0) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.45f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.58f, 0.15f, 1.0f));
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s %d %s", ICON_MDI_ALERT, report.warningCount, report.warningCount == 1 ? "Warning" : "Warnings");
        if (ImGui::SmallButton(buf)) {
            if (pOpenValidationPanel) *pOpenValidationPanel = true;
            requestFocus();
        }
        ImGui::PopStyleColor(2);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Asset is valid with %d %s.\nClick to view in Validation panel.",
                              report.warningCount, report.warningCount == 1 ? "warning" : "warnings");
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.45f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.6f, 0.36f, 1.0f));
        if (ImGui::SmallButton(ICON_MDI_CHECK_CIRCLE " glTF Valid")) {
            if (pOpenValidationPanel) *pOpenValidationPanel = true;
            requestFocus();
        }
        ImGui::PopStyleColor(2);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Asset strictly complies with glTF 2.0 specification.\nClick to view report.");
        }
    }
}

} // namespace ux3d::imgui
