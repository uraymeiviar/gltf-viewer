// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "Application.h"
#include "imgui/SettingsFile.h"
#include "imgui/SettingsStorage.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace ux3d::imgui {

void logSettingsMessage(const std::string& msg, bool isWarning)
{
    const std::string formatted = "[Settings] " + msg;
    std::fprintf(stderr, "%s\n", formatted.c_str());
    std::fflush(stderr);
    ux3d::core::log(isWarning ? ux3d::core::LogLevel::Warning : ux3d::core::LogLevel::Info, "%s", formatted.c_str());
}

} // namespace ux3d::imgui

namespace ux3d::imgui::application {

namespace {

bool parseInt(const std::string& text, int& value)
{
    char* end = nullptr;
    const long parsed = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || (end != nullptr && *end != '\0')) {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

bool parseBool(const std::string& text, bool& value)
{
    if (text == "1" || text == "true" || text == "True" || text == "yes") {
        value = true;
        return true;
    }
    if (text == "0" || text == "false" || text == "False" || text == "no") {
        value = false;
        return true;
    }
    return false;
}

std::string intText(int value)
{
    return std::to_string(value);
}

bool startsWithFilePrefix(const std::string& key)
{
    return key.size() > 4 && key.compare(0, 4, "file") == 0;
}

} // namespace

std::vector<std::string> Application::loadRecentFiles()
{
    std::vector<std::string> files;
    if (s_isolatedState) {
        return files;
    }
    SettingsFile& settings = SettingsFile::instance();
    settings.load();
    for (const SettingsEntry& entry : settings.section("RecentFiles")) {
        if (startsWithFilePrefix(entry.first) && !entry.second.empty()) {
            files.push_back(entry.second);
        }
    }
    if (files.size() > 10) {
        files.resize(10);
    }
    return files;
}

void Application::saveRecentFiles(const std::vector<std::string>& files)
{
    if (s_isolatedState) {
        return;
    }
    SettingsFile& settings = SettingsFile::instance();
    settings.load();
    std::vector<SettingsEntry> entries;
    for (size_t i = 0; i < files.size() && i < 10; ++i) {
        if (!files[i].empty()) {
            entries.emplace_back("file" + std::to_string(entries.size()), files[i]);
        }
    }
    settings.replaceSection("RecentFiles", std::move(entries));
    settings.save();
}

void Application::addRecentFile(std::vector<std::string>& files, const std::string& path)
{
    if (path.empty()) return;
    if (s_isolatedState) return; // Do not touch user-persisted state in scenario runs.
    if (agentNoPersistScope) return; // Pipe commands do not save settings without set_persist(true).
    std::string normPath = path;
    for (char& c : normPath) {
        if (c == '\\') c = '/';
    }
    files.erase(std::remove_if(files.begin(), files.end(), [&](const std::string& f) { return f == normPath; }),
                files.end());

    files.insert(files.begin(), normPath);
    if (files.size() > 10) {
        files.resize(10);
    }
    saveRecentFiles(files);
}

WindowState Application::loadWindowState()
{
    WindowState state;
    if (s_isolatedState) {
        return state;
    }
    SettingsFile& settings = SettingsFile::instance();
    settings.load();

    int value = 0;
    if (parseInt(settings.get("Window", "width"), value) && value >= 640) {
        state.width = value;
    }
    if (parseInt(settings.get("Window", "height"), value) && value >= 480) {
        state.height = value;
    }
    if (parseInt(settings.get("Window", "posX"), value)) {
        state.posX = value;
    }
    if (parseInt(settings.get("Window", "posY"), value)) {
        state.posY = value;
    }
    bool maximized = false;
    if (parseBool(settings.get("Window", "maximized"), maximized)) {
        state.maximized = maximized;
    }
    return state;
}

void Application::saveWindowState(GLFWwindow* window)
{
    if (window == nullptr || s_isolatedState) return;
    WindowState state;
    state.maximized = (glfwGetWindowAttrib(window, GLFW_MAXIMIZED) != 0);
    if (!state.maximized) {
        glfwGetWindowSize(window, &state.width, &state.height);
        glfwGetWindowPos(window, &state.posX, &state.posY);
    } else {
        WindowState prev = loadWindowState();
        state.width = prev.width;
        state.height = prev.height;
        state.posX = prev.posX;
        state.posY = prev.posY;
    }

    SettingsFile& settings = SettingsFile::instance();
    settings.load();
    std::vector<SettingsEntry> entries;
    const WindowState defaults;
    if (state.width != defaults.width) entries.emplace_back("width", intText(state.width));
    if (state.height != defaults.height) entries.emplace_back("height", intText(state.height));
    if (state.posX != defaults.posX) entries.emplace_back("posX", intText(state.posX));
    if (state.posY != defaults.posY) entries.emplace_back("posY", intText(state.posY));
    if (state.maximized) entries.emplace_back("maximized", "1");
    settings.replaceSection("Window", std::move(entries));
    settings.save();
}

void Application::loadImguiLayout()
{
    if (s_isolatedState) {
        return;
    }
    SettingsFile& settings = SettingsFile::instance();
    settings.load();
    const std::vector<SettingsEntry> lines = settings.section("Layout");
    if (lines.empty()) {
        layoutPersisted = false;
        return;
    }

    std::string text;
    for (const SettingsEntry& line : lines) {
        text += line.second;
        text += '\n';
    }
    if (text.find('[') == std::string::npos) {
        static bool s_corruptLogged = false;
        if (!s_corruptLogged) {
            logSettingsMessage("Corrupt or invalid layout in settings.ini; using the default layout.", true);
            s_corruptLogged = true;
        }
        layoutPersisted = false;
        return;
    }
    ImGui::LoadIniSettingsFromMemory(text.c_str(), text.size());
    layoutPersisted = true;
    layoutBaselineCountdown = 3;
}

void Application::captureLayoutBaseline()
{
    size_t size = 0;
    const char* data = ImGui::SaveIniSettingsToMemory(&size);
    layoutBaseline.assign(data != nullptr ? data : "", data != nullptr ? size : 0);
}

void Application::resetImguiLayoutPersistence()
{
    if (s_isolatedState) {
        return;
    }
    layoutBaseline.clear();
    layoutBaselineCountdown = 3;
    layoutPersisted = false;
    SettingsFile& settings = SettingsFile::instance();
    settings.load();
    if (settings.hasSection("Layout")) {
        settings.removeSection("Layout");
        settings.save();
    }
}

void Application::saveImguiLayout()
{
    if (s_isolatedState || layoutBaselineCountdown > 0) {
        return;
    }
    size_t size = 0;
    const char* data = ImGui::SaveIniSettingsToMemory(&size);
    if (data == nullptr || size == 0) {
        return;
    }
    std::string current(data, size);

    SettingsFile& settings = SettingsFile::instance();
    settings.load();
    if (current == layoutBaseline) {
        // The layout still equals the loaded or freshly generated one. A pristine run
        // stores nothing; a reset removed the section when it happened.
        if (!layoutPersisted && settings.hasSection("Layout")) {
            settings.removeSection("Layout");
            settings.save();
        }
        return;
    }

    std::vector<SettingsEntry> lines;
    std::istringstream input(current);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.emplace_back("line" + std::to_string(lines.size()), line);
    }
    settings.replaceSection("Layout", std::move(lines));
    settings.save();
    layoutBaseline = current;
    layoutPersisted = true;
}

void Application::updateWindowTitle(GLFWwindow* window, const std::string& currentFilePath)
{
    if (window == nullptr) return;
    std::string title = "Glider - ";
    if (currentFilePath.empty()) {
        title += "untitled.glb";
    } else {
        std::filesystem::path p(currentFilePath);
        title += p.filename().string();
    }
    glfwSetWindowTitle(window, title.c_str());
}

} // namespace ux3d::imgui::application
