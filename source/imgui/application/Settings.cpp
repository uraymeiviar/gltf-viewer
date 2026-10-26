// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "Application.h"
#include "imgui/SettingsStorage.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ux3d::imgui {

void logSettingsMessage(const std::string& msg, bool isWarning)
{
    const std::string formatted = "[Settings] " + msg;
    std::fprintf(stderr, "%s\n", formatted.c_str());
    std::fflush(stderr);
    ux3d::core::log(isWarning ? ux3d::core::LogLevel::Warning : ux3d::core::LogLevel::Info, "%s", formatted.c_str());
}

static std::filesystem::path getExecutableDirectory()
{
#ifdef _WIN32
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    while (len == buffer.size()) {
        buffer.resize(buffer.size() * 2);
        len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    }
    if (len > 0) {
        buffer.resize(len);
        std::filesystem::path exePath(buffer);
        return exePath.parent_path();
    }
#endif
    std::error_code ec;
    return std::filesystem::current_path(ec);
}

static bool isPathWritable(const std::filesystem::path& dir)
{
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) {
        if (!std::filesystem::create_directories(dir, ec) || ec) {
            return false;
        }
    }
    const std::filesystem::path probe = dir / ".glider_write_test";
    {
        std::ofstream test(probe, std::ios::out | std::ios::binary);
        if (!test.is_open()) {
            return false;
        }
        test << "ok";
        if (test.fail()) {
            test.close();
            std::filesystem::remove(probe, ec);
            return false;
        }
    }
    std::filesystem::remove(probe, ec);
    return true;
}

static std::mutex s_storageMutex;
static std::filesystem::path s_resolvedDir;
static bool s_isResolved = false;
static bool s_isWritable = true;
static bool s_isFallback = false;

void resetDataDirectoryCache()
{
    std::lock_guard<std::mutex> lock(s_storageMutex);
    s_isResolved = false;
    s_resolvedDir.clear();
    s_isWritable = true;
    s_isFallback = false;
}

bool isDataDirectoryWritable()
{
    std::lock_guard<std::mutex> lock(s_storageMutex);
    if (!s_isResolved) {
        // Resolve with default ensureCreated = true
        std::filesystem::path candidateDir;
        const char* overrideEnv = std::getenv("GLIDER_DATA_DIR");
        if (overrideEnv != nullptr && std::strlen(overrideEnv) > 0) {
            candidateDir = std::filesystem::u8path(overrideEnv);
        } else {
            candidateDir = getExecutableDirectory() / "GliderData";
        }
        if (isPathWritable(candidateDir)) {
            s_resolvedDir = candidateDir;
            s_isWritable = true;
            s_isFallback = false;
            s_isResolved = true;
            logSettingsMessage("Using portable settings directory: " + s_resolvedDir.string());
        } else {
            std::filesystem::path appDataDir;
            const char* appData = std::getenv("APPDATA");
            if (appData != nullptr && std::strlen(appData) > 0) {
                appDataDir = std::filesystem::u8path(appData) / "Glider";
            } else {
                std::error_code ec;
                appDataDir = std::filesystem::temp_directory_path(ec) / "Glider";
            }
            if (isPathWritable(appDataDir)) {
                s_resolvedDir = appDataDir;
                s_isWritable = true;
                s_isFallback = true;
                s_isResolved = true;
                logSettingsMessage("Portable settings directory '" + candidateDir.string() +
                                   "' is not writable; falling back to AppData: '" + s_resolvedDir.string() + "'");
            } else {
                s_resolvedDir = candidateDir;
                s_isWritable = false;
                s_isFallback = false;
                s_isResolved = true;
                logSettingsMessage("Settings directory '" + candidateDir.string() +
                                   "' is not writable and fallback directory is not writable; running with in-memory settings.");
            }
        }
    }
    return s_isWritable;
}

std::filesystem::path getDataDirectory(bool ensureCreated)
{
    std::lock_guard<std::mutex> lock(s_storageMutex);
    if (s_isResolved) {
        return s_resolvedDir;
    }

    std::filesystem::path candidateDir;
    const char* overrideEnv = std::getenv("GLIDER_DATA_DIR");
    if (overrideEnv != nullptr && std::strlen(overrideEnv) > 0) {
        candidateDir = std::filesystem::u8path(overrideEnv);
    } else {
        candidateDir = getExecutableDirectory() / "GliderData";
    }

    if (!ensureCreated) {
        return candidateDir;
    }

    if (isPathWritable(candidateDir)) {
        s_resolvedDir = candidateDir;
        s_isWritable = true;
        s_isFallback = false;
        s_isResolved = true;
        logSettingsMessage("Using portable settings directory: " + s_resolvedDir.string());
    } else {
        std::filesystem::path appDataDir;
        const char* appData = std::getenv("APPDATA");
        if (appData != nullptr && std::strlen(appData) > 0) {
            appDataDir = std::filesystem::u8path(appData) / "Glider";
        } else {
            std::error_code ec;
            appDataDir = std::filesystem::temp_directory_path(ec) / "Glider";
        }
        if (isPathWritable(appDataDir)) {
            s_resolvedDir = appDataDir;
            s_isWritable = true;
            s_isFallback = true;
            s_isResolved = true;
            logSettingsMessage("Portable settings directory '" + candidateDir.string() +
                               "' is not writable; falling back to AppData: '" + s_resolvedDir.string() + "'");
        } else {
            s_resolvedDir = candidateDir;
            s_isWritable = false;
            s_isFallback = false;
            s_isResolved = true;
            logSettingsMessage("Settings directory '" + candidateDir.string() +
                               "' is not writable and fallback directory is not writable; running with in-memory settings.");
        }
    }
    return s_resolvedDir;
}

std::filesystem::path getDataFilePath(const std::string& filename, bool ensureCreated)
{
    const std::filesystem::path dataDir = getDataDirectory(ensureCreated);
    const std::filesystem::path target = dataDir / filename;
    if (!ensureCreated) {
        return target;
    }

    std::error_code ec;
    if (!std::filesystem::exists(target, ec)) {
        std::vector<std::filesystem::path> migrationSources;
        const char* appData = std::getenv("APPDATA");
        if (appData != nullptr && std::strlen(appData) > 0) {
            if (!s_isFallback) {
                migrationSources.push_back(std::filesystem::u8path(appData) / "Glider" / filename);
            }
            migrationSources.push_back(std::filesystem::u8path(appData) / "Gestaltor" / filename);
        }
        if (filename == "settings.ini") {
            migrationSources.push_back(getExecutableDirectory() / "settings.ini");
        }

        for (const auto& src : migrationSources) {
            std::error_code ecCheck;
            if (std::filesystem::exists(src, ecCheck) && !std::filesystem::is_directory(src, ecCheck)) {
                std::error_code ecCopy;
                std::filesystem::copy_file(src, target, std::filesystem::copy_options::none, ecCopy);
                if (!ecCopy) {
                    logSettingsMessage("Migrated " + filename + " from " + src.string() + " to " + target.string());
                    break;
                }
            }
        }
    }
    return target;
}

} // namespace ux3d::imgui

namespace ux3d::imgui::application {

std::string Application::getRecentFilesConfigPath()
{
    return getDataFilePath("recent_files.txt", !s_isolatedState).string();
}

std::vector<std::string> Application::loadRecentFiles()
{
    std::vector<std::string> files;
    const std::filesystem::path path = getDataFilePath("recent_files.txt", !s_isolatedState);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return files;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        static bool s_readFailLogged = false;
        if (!s_readFailLogged) {
            logSettingsMessage("Failed to read recent_files.txt: unable to open file; using defaults.", true);
            s_readFailLogged = true;
        }
        return files;
    }

    std::string line;
    bool hasBinary = false;
    while (std::getline(in, line)) {
        for (unsigned char c : line) {
            if (c < 0x09 || (c > 0x0D && c < 0x20)) {
                hasBinary = true;
                break;
            }
        }
        if (hasBinary) {
            break;
        }
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
            line.pop_back();
        }
        while (!line.empty() && (line.front() == ' ')) {
            line.erase(line.begin());
        }
        if (!line.empty()) {
            files.push_back(line);
        }
    }

    if (hasBinary) {
        static bool s_corruptLogged = false;
        if (!s_corruptLogged) {
            logSettingsMessage("Corrupt or invalid recent_files.txt detected; using defaults.", true);
            s_corruptLogged = true;
        }
        return {};
    }
    return files;
}

void Application::saveRecentFiles(const std::vector<std::string>& files)
{
    if (s_isolatedState) {
        return;
    }
    if (!isDataDirectoryWritable()) {
        static bool s_unwritableLogged = false;
        if (!s_unwritableLogged) {
            logSettingsMessage("Failed to write recent_files.txt: directory is not writable; changes kept in memory.", true);
            s_unwritableLogged = true;
        }
        return;
    }

    const std::filesystem::path path = getDataFilePath("recent_files.txt", true);
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open() || out.fail()) {
        static bool s_saveFailLogged = false;
        if (!s_saveFailLogged) {
            logSettingsMessage("Failed to write recent_files.txt: file is not writable; changes kept in memory.", true);
            s_saveFailLogged = true;
        }
        return;
    }

    for (const auto& filePath : files) {
        out << filePath << "\n";
    }

    if (out.fail()) {
        static bool s_writeErrLogged = false;
        if (!s_writeErrLogged) {
            logSettingsMessage("Failed to write recent_files.txt: write error; changes kept in memory.", true);
            s_writeErrLogged = true;
        }
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
    return getDataFilePath("settings.ini", !s_isolatedState).string();
}

std::string Application::getWindowStateConfigPath()
{
    return getDataFilePath("window_state.txt", !s_isolatedState).string();
}

WindowState Application::loadWindowState()
{
    WindowState state;
    const std::filesystem::path path = getDataFilePath("window_state.txt", !s_isolatedState);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return state;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        static bool s_readFailLogged = false;
        if (!s_readFailLogged) {
            logSettingsMessage("Failed to read window_state.txt: unable to open file; using defaults.", true);
            s_readFailLogged = true;
        }
        return state;
    }

    std::string rawContent((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    bool hasBinary = false;
    for (unsigned char c : rawContent) {
        if (c < 0x09 || (c > 0x0D && c < 0x20)) {
            hasBinary = true;
            break;
        }
    }
    if (hasBinary) {
        static bool s_corruptLogged = false;
        if (!s_corruptLogged) {
            logSettingsMessage("Corrupt or invalid window_state.txt detected; using defaults.", true);
            s_corruptLogged = true;
        }
        return state;
    }

    std::istringstream iss(rawContent);
    std::string key;
    int val = 0;
    bool parsedAny = false;
    while (iss >> key >> val) {
        if (key == "width" && val >= 640) {
            state.width = val;
            parsedAny = true;
        } else if (key == "height" && val >= 480) {
            state.height = val;
            parsedAny = true;
        } else if (key == "posX") {
            state.posX = val;
            parsedAny = true;
        } else if (key == "posY") {
            state.posY = val;
            parsedAny = true;
        } else if (key == "maximized") {
            state.maximized = (val != 0);
            parsedAny = true;
        }
    }

    if (!rawContent.empty() && !parsedAny) {
        static bool s_corruptLogged = false;
        if (!s_corruptLogged) {
            logSettingsMessage("Corrupt or invalid window_state.txt detected; using defaults.", true);
            s_corruptLogged = true;
        }
        return WindowState();
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

    if (!isDataDirectoryWritable()) {
        static bool s_unwritableLogged = false;
        if (!s_unwritableLogged) {
            logSettingsMessage("Failed to write window_state.txt: directory is not writable; changes kept in memory.", true);
            s_unwritableLogged = true;
        }
        return;
    }

    const std::filesystem::path path = getDataFilePath("window_state.txt", true);
    std::ofstream out(path, std::ios::out | std::ios::trunc);
    if (!out.is_open() || out.fail()) {
        static bool s_saveFailLogged = false;
        if (!s_saveFailLogged) {
            logSettingsMessage("Failed to write window_state.txt: file is not writable; changes kept in memory.", true);
            s_saveFailLogged = true;
        }
        return;
    }

    out << "width " << state.width << "\n";
    out << "height " << state.height << "\n";
    out << "posX " << state.posX << "\n";
    out << "posY " << state.posY << "\n";
    out << "maximized " << (state.maximized ? 1 : 0) << "\n";

    if (out.fail()) {
        static bool s_writeErrLogged = false;
        if (!s_writeErrLogged) {
            logSettingsMessage("Failed to write window_state.txt: write error; changes kept in memory.", true);
            s_writeErrLogged = true;
        }
    }
}

void Application::loadImguiLayout()
{
    if (s_isolatedState) {
        return;
    }
    const std::filesystem::path path = getDataFilePath("imgui.ini", true);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        static bool s_readFailLogged = false;
        if (!s_readFailLogged) {
            logSettingsMessage("Failed to read imgui.ini: unable to open file; using defaults.", true);
            s_readFailLogged = true;
        }
        return;
    }

    std::string rawContent((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    bool hasBinary = false;
    for (unsigned char c : rawContent) {
        if (c < 0x09 || (c > 0x0D && c < 0x20)) {
            hasBinary = true;
            break;
        }
    }
    if (hasBinary) {
        static bool s_corruptLogged = false;
        if (!s_corruptLogged) {
            logSettingsMessage("Corrupt or invalid imgui.ini detected; using defaults.", true);
            s_corruptLogged = true;
        }
        return;
    }

    if (!rawContent.empty() && rawContent.find('[') == std::string::npos) {
        static bool s_corruptLogged = false;
        if (!s_corruptLogged) {
            logSettingsMessage("Corrupt or invalid imgui.ini detected; using defaults.", true);
            s_corruptLogged = true;
        }
        return;
    }

    ImGui::LoadIniSettingsFromMemory(rawContent.c_str(), rawContent.size());
}

void Application::saveImguiLayout()
{
    if (s_isolatedState) {
        return;
    }
    size_t size = 0;
    const char* data = ImGui::SaveIniSettingsToMemory(&size);
    if (!data || size == 0) {
        return;
    }

    if (!isDataDirectoryWritable()) {
        static bool s_unwritableLogged = false;
        if (!s_unwritableLogged) {
            logSettingsMessage("Failed to write imgui.ini: directory is not writable; changes kept in memory.", true);
            s_unwritableLogged = true;
        }
        return;
    }

    const std::filesystem::path path = getDataFilePath("imgui.ini", true);
    std::ofstream out(path, std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out.is_open() || out.fail()) {
        static bool s_saveFailLogged = false;
        if (!s_saveFailLogged) {
            logSettingsMessage("Failed to write imgui.ini: file is not writable; changes kept in memory.", true);
            s_saveFailLogged = true;
        }
        return;
    }

    out.write(data, static_cast<std::streamsize>(size));
    if (out.fail()) {
        static bool s_writeErrLogged = false;
        if (!s_writeErrLogged) {
            logSettingsMessage("Failed to write imgui.ini: write error; changes kept in memory.", true);
            s_writeErrLogged = true;
        }
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
