// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "Application.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace ux3d::imgui::application {

std::string Application::getRecentFilesConfigPath()
{
    const char* appData = std::getenv("APPDATA");
    if (appData != nullptr && std::strlen(appData) > 0) {
        std::filesystem::path dir = std::filesystem::path(appData) / "Glider";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        std::filesystem::path newPath = dir / "recent_files.txt";
        if (!std::filesystem::exists(newPath)) {
            std::filesystem::path oldPath = std::filesystem::path(appData) / "Gestaltor" / "recent_files.txt";
            if (std::filesystem::exists(oldPath)) {
                std::filesystem::copy_file(oldPath, newPath, ec);
            }
        }
        return newPath.string();
    }
    return "recent_files.txt";
}

std::vector<std::string> Application::loadRecentFiles()
{
    std::vector<std::string> files;
    std::ifstream in(getRecentFilesConfigPath());
    if (!in.is_open()) return files;
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' '))
            line.pop_back();
        while (!line.empty() && (line.front() == ' '))
            line.erase(line.begin());
        if (!line.empty()) {
            files.push_back(line);
        }
    }
    return files;
}

void Application::saveRecentFiles(const std::vector<std::string>& files)
{
    std::ofstream out(getRecentFilesConfigPath());
    if (!out.is_open()) return;
    for (const auto& path : files) {
        out << path << "\n";
    }
}

void Application::addRecentFile(std::vector<std::string>& files, const std::string& path)
{
    if (path.empty()) return;
    if (s_isolatedState) return; // Do not touch user-persisted state in scenario runs.
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

std::string Application::getSettingsConfigPath()
{
    const char* appData = std::getenv("APPDATA");
    if (appData == nullptr || std::strlen(appData) == 0) {
        return "settings.ini";
    }
    std::filesystem::path dir = std::filesystem::path(appData) / "Glider";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::filesystem::path path = dir / "settings.ini";
    if (!std::filesystem::exists(path, ec)) {
        const auto exeDir = ux3d::imgui::StdFileSystem::instance().getExecutableDirPath();
        const std::filesystem::path legacy = std::filesystem::u8path(exeDir.toString().c_str()) / "settings.ini";
        if (std::filesystem::exists(legacy, ec)) {
            std::filesystem::copy_file(legacy, path, ec);
        }
    }
    return path.string();
}

std::string Application::getWindowStateConfigPath()
{
    const char* appData = std::getenv("APPDATA");
    if (appData != nullptr && std::strlen(appData) > 0) {
        std::filesystem::path dir = std::filesystem::path(appData) / "Glider";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        std::filesystem::path newPath = dir / "window_state.txt";
        if (!std::filesystem::exists(newPath)) {
            std::filesystem::path oldPath = std::filesystem::path(appData) / "Gestaltor" / "window_state.txt";
            if (std::filesystem::exists(oldPath)) {
                std::filesystem::copy_file(oldPath, newPath, ec);
            }
        }
        return newPath.string();
    }
    return "window_state.txt";
}

WindowState Application::loadWindowState()
{
    WindowState state;
    std::ifstream in(getWindowStateConfigPath());
    if (in.is_open()) {
        std::string key;
        int val = 0;
        while (in >> key >> val) {
            if (key == "width" && val >= 640)
                state.width = val;
            else if (key == "height" && val >= 480)
                state.height = val;
            else if (key == "posX")
                state.posX = val;
            else if (key == "posY")
                state.posY = val;
            else if (key == "maximized")
                state.maximized = (val != 0);
        }
    }
    return state;
}

void Application::saveWindowState(GLFWwindow* window)
{
    if (window == nullptr) return;
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

    std::ofstream out(getWindowStateConfigPath());
    if (out.is_open()) {
        out << "width " << state.width << "\n";
        out << "height " << state.height << "\n";
        out << "posX " << state.posX << "\n";
        out << "posY " << state.posY << "\n";
        out << "maximized " << (state.maximized ? 1 : 0) << "\n";
    }
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
