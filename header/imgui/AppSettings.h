// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <filesystem>

namespace ux3d::imgui {

class AppSettings {
public:
    static AppSettings& instance();

    // Configuration file path
    const std::string& getIniFilePath() const { return m_iniPath; }
    void setIniFilePath(const std::string& path) { m_iniPath = path; }

    // Load and save settings.ini
    bool loadSettings(const std::string& iniPath = "");
    bool saveSettings(const std::string& iniPath = "");

    // Search paths (relative to active glTF model folder)
    std::vector<std::string> getSearchPaths() const;
    void setSearchPaths(const std::vector<std::string>& paths);
    bool addSearchPath(const std::string& path);
    bool removeSearchPath(size_t index);
    bool moveSearchPathUp(size_t index);
    bool moveSearchPathDown(size_t index);
    void clearSearchPaths();

    // Active model context
    void setCurrentModelPath(const std::string& modelPath);
    std::string getCurrentModelPath() const;
    std::string getCurrentModelDir() const;

    // Asset resolution hook:
    // Resolves missing external asset paths against the active model directory and configured search paths.
    // Returns the resolved absolute or canonical path if found on disk, or empty string if not found.
    std::string resolveAssetPath(const std::string& requestedPath) const;

private:
    AppSettings();
    ~AppSettings() = default;
    AppSettings(const AppSettings&) = delete;
    AppSettings& operator=(const AppSettings&) = delete;

    std::string normalizePath(const std::string& path) const;

    mutable std::mutex m_mutex;
    std::string m_iniPath = "settings.ini";
    std::vector<std::string> m_searchPaths;
    std::string m_currentModelPath;
    std::string m_currentModelDir;
};

} // namespace ux3d::imgui
