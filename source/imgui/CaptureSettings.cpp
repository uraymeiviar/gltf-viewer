// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/CaptureSettings.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "imgui/SettingsFile.h"
#include "imgui/UiProbe.h"

#include <imgui.h>
#include <portable-file-dialogs.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>
#endif

namespace fs = std::filesystem;

namespace ux3d::imgui {

namespace {

constexpr const char* kSection = "Captures";
constexpr const char* kFolderKey = "folder";

std::string s_folder;
bool s_loaded = false;

std::string pathText(const fs::path& path)
{
    const auto text = path.u8string();
    return std::string(text.begin(), text.end());
}

bool sameFolder(const std::string& a, const std::string& b)
{
    const fs::path left = fs::u8path(a).lexically_normal();
    const fs::path right = fs::u8path(b).lexically_normal();
    auto lower = [](std::string text) {
        for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return text;
    };
    std::string leftText = lower(pathText(left));
    std::string rightText = lower(pathText(right));
    while (!leftText.empty() && (leftText.back() == '\\' || leftText.back() == '/')) leftText.pop_back();
    while (!rightText.empty() && (rightText.back() == '\\' || rightText.back() == '/')) rightText.pop_back();
    return leftText == rightText;
}

} // namespace

std::string CaptureSettings::defaultFolder()
{
    fs::path pictures;
#ifdef _WIN32
    PWSTR known = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, KF_FLAG_DEFAULT, nullptr, &known)) && known != nullptr) {
        pictures = fs::path(std::wstring(known));
    }
    if (known != nullptr) CoTaskMemFree(known);
    if (pictures.empty()) {
        if (const char* profile = std::getenv("USERPROFILE")) pictures = fs::u8path(profile) / "Pictures";
    }
#else
    if (const char* home = std::getenv("HOME")) pictures = fs::u8path(home) / "Pictures";
#endif
    if (pictures.empty()) {
        std::error_code ec;
        pictures = fs::temp_directory_path(ec);
    }
    return pathText(pictures / "Glider");
}

void CaptureSettings::ensureLoaded()
{
    if (s_loaded) return;
    s_loaded = true;
    SettingsFile& settings = SettingsFile::instance();
    if (!settings.persistenceEnabled()) return; // --isolated: never read.
    settings.load();
    s_folder = settings.get(kSection, kFolderKey);
}

std::string CaptureSettings::configuredFolder()
{
    ensureLoaded();
    return s_folder;
}

std::string CaptureSettings::effectiveFolder()
{
    ensureLoaded();
    return s_folder.empty() ? defaultFolder() : s_folder;
}

void CaptureSettings::setFolder(const std::string& folder)
{
    ensureLoaded();
    std::string value = folder;
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    if (!value.empty() && sameFolder(value, defaultFolder())) value.clear(); // Only non-default values are stored.
    s_folder = value;

    SettingsFile& settings = SettingsFile::instance();
    if (!settings.persistenceEnabled()) return; // --isolated: kept in memory for the session.
    settings.load();
    if (s_folder.empty()) {
        if (settings.hasKey(kSection, kFolderKey)) {
            settings.remove(kSection, kFolderKey);
            if (settings.section(kSection).empty()) settings.removeSection(kSection);
            settings.save();
        }
    } else {
        settings.set(kSection, kFolderKey, s_folder);
        settings.save();
    }
}

std::string CaptureSettings::proposedFileName(const std::string& modelPath, std::chrono::system_clock::time_point now)
{
    std::string stem;
    if (!modelPath.empty()) stem = pathText(fs::u8path(modelPath).stem());
    for (char& c : stem) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x20 || c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' ||
            c == '*') {
            c = '_';
        }
    }
    while (!stem.empty() && (stem.back() == ' ' || stem.back() == '.')) stem.pop_back();
    if (stem.empty()) stem = "untitled";

    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local);
    return "Glider_" + stem + "_" + stamp + ".png";
}

fs::path CaptureSettings::uniquePath(const fs::path& folder, const std::string& fileName)
{
    const fs::path name = fs::u8path(fileName);
    const fs::path stem = name.stem();
    const fs::path extension = name.extension();
    fs::path candidate = folder / name;
    std::error_code ec;
    for (int index = 2; fs::exists(candidate, ec) && index < 100000; ++index) {
        candidate = folder / fs::path(stem.native() + fs::u8path("_" + std::to_string(index)).native() + extension.native());
    }
    return candidate;
}

void CaptureSettings::drawSettings()
{
    ensureLoaded();
    ImGui::TextWrapped("Viewport -> Capture Viewport saves the viewport image as a PNG in this folder without asking. "
                       "Save Viewport View... starts its file dialog here.");
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.88f, 1.0f), ICON_MDI_FOLDER_OUTLINE " Capture folder");

    const std::string defaultPath = defaultFolder();
    static char s_buffer[1024] = "";
    static bool s_editing = false;
    if (!s_editing) std::snprintf(s_buffer, sizeof(s_buffer), "%s", configuredFolder().c_str());

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 250.0f);
    ImGui::InputTextWithHint("##CaptureFolder", defaultPath.c_str(), s_buffer, sizeof(s_buffer));
    UiProbe::item("settings.captures.folder", 0);
    s_editing = ImGui::IsItemActive();
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        setFolder(s_buffer);
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_FOLDER_OPEN " Browse...")) {
        auto picked = pfd::select_folder("Select Capture Folder", effectiveFolder()).result();
        if (!picked.empty()) {
            setFolder(picked);
            s_editing = false;
        }
    }
    UiProbe::item("settings.captures.browse", 0);

    ImGui::SameLine();
    const bool isSet = !configuredFolder().empty();
    if (!isSet) ImGui::BeginDisabled();
    if (ImGui::Button("Reset to default")) {
        setFolder("");
        s_editing = false;
    }
    if (!isSet) ImGui::EndDisabled();
    UiProbe::item("settings.captures.reset", 0);

    ImGui::Spacing();
    ImGui::TextDisabled("Default: %s", defaultPath.c_str());
    ImGui::TextDisabled("In use: %s", effectiveFolder().c_str());
    ImGui::TextDisabled("The folder is created when the first capture is saved. An empty field uses the default.");
}

} // namespace ux3d::imgui
