// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/SearchPathsPanel.h"
#include "imgui/AppSettings.h"
#include "imgui/CaptureSettings.h"
#include "imgui/GltfExtensionSettings.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "imgui/RenderQuality.h"
#include "imgui/UiProbe.h"

#include <imgui.h>
#include <portable-file-dialogs.h>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace ux3d::imgui {

namespace {
SettingsTab s_requestedTab = SettingsTab::None;
bool s_openRequested = false;
} // namespace

void requestSettingsWindow(SettingsTab tab)
{
    s_requestedTab = tab;
    s_openRequested = true;
}

bool takeSettingsWindowRequest()
{
    const bool requested = s_openRequested;
    s_openRequested = false;
    return requested;
}

static void renderSearchPathsContent(bool* pOpen, std::function<void()> onReloadModel)
{
    if (pOpen != nullptr && !*pOpen) {
        return;
    }

    {
        auto& settings = AppSettings::instance();
        std::string currentModelPath = settings.getCurrentModelPath();
        std::string currentModelDir = settings.getCurrentModelDir();
        std::vector<std::string> paths = settings.getSearchPaths();

        // 1. Informative description
        ImGui::TextWrapped("Configure directories where the viewer should search for external assets (such as textures and binary buffers) referenced by the glTF model.");
        ImGui::TextWrapped("Wildcards: ../texture* matches sibling texture folders; ../* searches immediate sibling folders; ../** searches them recursively. Matches are sorted; links/junctions are not followed. Reload to refresh folder matches.");
        ImGui::Spacing();

        // 2. Active Model Context Card
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
        ImGui::BeginChild("ModelContextCard", ImVec2(0, 78), true, ImGuiWindowFlags_None);
        {
            if (!currentModelPath.empty()) {
                fs::path mp = fs::u8path(currentModelPath);
                std::string filename = mp.filename().generic_string();
                ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), ICON_MDI_FILE_DOCUMENT_OUTLINE " Active Model: %s", filename.c_str());
                ImGui::TextDisabled("Folder: %s", currentModelDir.c_str());
            } else {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.75f, 1.0f), ICON_MDI_FILE_DOCUMENT_OUTLINE " Active Model: (No model open)");
                ImGui::TextDisabled("Search paths will be resolved relative to whichever glTF is opened.");
            }
            ImGui::TextColored(ImVec4(0.2f, 0.85f, 0.45f, 1.0f), "  " ICON_MDI_CHECK " . (glTF's own folder is always searched implicitly first)");
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.88f, 1.0f), ICON_MDI_FOLDER_OUTLINE " Configured Relative Search Paths (Priority Order):");

        // 3. Search Paths Table
        float tableHeight = ImGui::GetContentRegionAvail().y - 120.0f;
        if (tableHeight < 140.0f) tableHeight = 140.0f;

        if (ImGui::BeginTable("SearchPathsTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, tableHeight))) {
            ImGui::TableSetupColumn("Relative Path", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Order", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 40.0f);
            ImGui::TableHeadersRow();

            if (paths.empty()) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("No additional search paths configured. Only glTF folder will be searched.");
                ImGui::TableSetColumnIndex(1);
                ImGui::TableSetColumnIndex(2);
            }

            int itemToMoveUp = -1;
            int itemToMoveDown = -1;
            int itemToDelete = -1;

            for (size_t i = 0; i < paths.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                ImGui::TableNextRow();

                // Column 0: Path and status badge
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(ICON_MDI_FOLDER_OUTLINE " %s", paths[i].c_str());
                UiProbe::item("settings.search.path",static_cast<long long>(i));

                // If a model is loaded, check if the folder exists on disk
                if (!currentModelDir.empty()) {
                    std::error_code ec;
                    fs::path checkDir = fs::u8path(paths[i]);
                    if (checkDir.is_relative()) {
                        checkDir = fs::u8path(currentModelDir) / checkDir;
                    }

                    ImGui::SameLine();
                    if (paths[i].find_first_of("*?") != std::string::npos) {
                        ImGui::TextDisabled("[ Wildcard ]");
                    } else if (fs::is_directory(checkDir, ec)) {
                        ImGui::TextColored(ImVec4(0.2f, 0.85f, 0.45f, 1.0f), "[ " ICON_MDI_CHECK " Exists ]");
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Resolved path: %s", checkDir.generic_string().c_str());
                        }
                    } else {
                        ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.2f, 1.0f), "[ " ICON_MDI_ALERT_CIRCLE_OUTLINE " Folder not found ]");
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Target path not found on disk: %s", checkDir.generic_string().c_str());
                        }
                    }
                }

                // Column 1: Order buttons
                ImGui::TableSetColumnIndex(1);
                if (i == 0) {
                    ImGui::BeginDisabled();
                }
                if (ImGui::SmallButton(ICON_MDI_ARROW_UP "##Up")) {
                    itemToMoveUp = static_cast<int>(i);
                }
                if (i == 0) {
                    ImGui::EndDisabled();
                }

                ImGui::SameLine();
                if (i + 1 >= paths.size()) {
                    ImGui::BeginDisabled();
                }
                if (ImGui::SmallButton(ICON_MDI_ARROW_DOWN "##Down")) {
                    itemToMoveDown = static_cast<int>(i);
                }
                if (i + 1 >= paths.size()) {
                    ImGui::EndDisabled();
                }

                // Column 2: Remove button
                ImGui::TableSetColumnIndex(2);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                if (ImGui::SmallButton(ICON_MDI_DELETE_OUTLINE "##Del")) {
                    itemToDelete = static_cast<int>(i);
                }
                UiProbe::item("settings.search.remove",static_cast<long long>(i));
                ImGui::PopStyleColor();

                ImGui::PopID();
            }

            ImGui::EndTable();

            // Handle actions
            if (itemToMoveUp >= 0) {
                settings.moveSearchPathUp(static_cast<size_t>(itemToMoveUp));
                settings.saveSettings();
            } else if (itemToMoveDown >= 0) {
                settings.moveSearchPathDown(static_cast<size_t>(itemToMoveDown));
                settings.saveSettings();
            } else if (itemToDelete >= 0) {
                settings.removeSearchPath(static_cast<size_t>(itemToDelete));
                settings.saveSettings();
            }
        }

        // 4. Add New Search Path row
        ImGui::Spacing();
        static char s_newPathBuffer[512] = "";

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 180.0f);
        ImGui::InputTextWithHint("##NewPathInput", "e.g. ../texture*, ../*, ../**", s_newPathBuffer, sizeof(s_newPathBuffer));
        UiProbe::item("settings.search_path",0);
        ImGui::SameLine();

        if (ImGui::Button(ICON_MDI_PLUS " Add Path")) {
            std::string np = s_newPathBuffer;
            if (!np.empty()) {
                if (settings.addSearchPath(np)) {
                    settings.saveSettings();
                    s_newPathBuffer[0] = '\0';
                }
            }
        }
        UiProbe::item("settings.search.add",0);

        ImGui::SameLine();
        if (ImGui::Button(ICON_MDI_FOLDER_OPEN " Browse...")) {
            std::string startFolder = currentModelDir.empty() ? "" : currentModelDir;
            auto picked = pfd::select_folder("Select Asset Search Folder", startFolder).result();
            if (!picked.empty()) {
                if (!currentModelDir.empty()) {
                    std::error_code ec;
                    fs::path rel = fs::relative(fs::u8path(picked), fs::u8path(currentModelDir), ec);
                    if (!ec && !rel.empty()) {
                        std::string relStr = rel.generic_string();
                        std::snprintf(s_newPathBuffer, sizeof(s_newPathBuffer), "%s", relStr.c_str());
                    } else {
                        std::snprintf(s_newPathBuffer, sizeof(s_newPathBuffer), "%s", picked.c_str());
                    }
                } else {
                    std::snprintf(s_newPathBuffer, sizeof(s_newPathBuffer), "%s", picked.c_str());
                }
            }
        }

        // 5. Footer Bar
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (onReloadModel != nullptr && !currentModelPath.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.13f, 0.67f, 0.42f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.78f, 0.49f, 1.0f));
            if (ImGui::Button(ICON_MDI_REFRESH " Reload Active Model")) {
                onReloadModel();
            }
            ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Reload the currently open glTF model to re-resolve external textures with the updated search paths.");
            }
            ImGui::SameLine();
        }

        if (ImGui::Button("Close")) {
            if (pOpen != nullptr) {
                *pOpen = false;
            }
        }

        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 210.0f);
        ImGui::TextDisabled("Saved in %s", settings.getIniFilePath().c_str());
    }
}

void renderSearchPathsWindow(bool* pOpen, std::function<void()> onReloadModel, SceneRevisions* revisions,
                             std::function<void()> drawAgentControl, ux3d::renderer::GltfRenderLayer* layer)
{
    if (pOpen && !*pOpen) return;
    ImGui::SetNextWindowSize(ImVec2(720,840),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(560,420),ImVec2(2000,2000));
    // A tab asked for by another panel is selected once, on the frame the window draws.
    const SettingsTab requested = std::exchange(s_requestedTab, SettingsTab::None);
    const auto flagsFor = [&](SettingsTab tab) { return requested == tab ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None; };
    if (ImGui::Begin(ICON_MDI_COG " Settings###SearchPathsWindow",pOpen,ImGuiWindowFlags_NoCollapse)) {
        if (ImGui::BeginTabBar("SettingsTabs")) {
            SceneRevisions dummy;
            SceneRevisions& sceneRevisions = revisions ? *revisions : dummy;
            const bool search = ImGui::BeginTabItem("Search Paths",nullptr,flagsFor(SettingsTab::SearchPaths));
            UiProbe::item("settings.tab.search",0);
            if (search) {
                renderSearchPathsContent(pOpen,onReloadModel);
                ImGui::EndTabItem();
            }
            // Quality: the preset, what is configured and what is in effect, the current settings and the presets'
            // definitions. Everything that costs time or memory lives here (it applies to every viewport).
            const bool quality = ImGui::BeginTabItem("Quality",nullptr,flagsFor(SettingsTab::Quality));
            UiProbe::item("settings.tab.quality",0);
            if (quality) {
                ImGui::BeginChild("QualityPage",ImVec2(0,-40),false);
                RenderQuality::drawSettingsTab(layer,sceneRevisions);
                ImGui::EndChild();
                if (ImGui::Button("Close")) { if (pOpen) *pOpen = false; }
                UiProbe::item("settings.close",0);
                ImGui::EndTabItem();
            }
            // glTF Extensions: which extensions (ASOBO now, other vendors later) Glider applies; an app setting.
            const bool extensions = ImGui::BeginTabItem("glTF Extensions",nullptr,flagsFor(SettingsTab::GltfExtensions));
            UiProbe::item("settings.tab.extensions",0);
            if (extensions) {
                ImGui::BeginChild("ExtensionsPage",ImVec2(0,-40),false);
                GltfExtensionSettings::instance().drawSettings(sceneRevisions);
                ImGui::EndChild();
                if (ImGui::Button("Close")) { if (pOpen) *pOpen = false; }
                UiProbe::item("settings.ext.close",0);
                ImGui::EndTabItem();
            }
            const bool captures = ImGui::BeginTabItem("Captures",nullptr,flagsFor(SettingsTab::Captures));
            UiProbe::item("settings.tab.captures",0);
            if (captures) {
                CaptureSettings::drawSettings();
                ImGui::EndTabItem();
            }
            if (drawAgentControl) {
                const bool agent = ImGui::BeginTabItem("Agent Control",nullptr,flagsFor(SettingsTab::AgentControl));
                UiProbe::item("settings.tab.agent",0);
                if (agent) {
                    drawAgentControl();
                    ImGui::EndTabItem();
                }
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}

} // namespace ux3d::imgui
