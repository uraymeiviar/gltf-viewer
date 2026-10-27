// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <filesystem>

namespace ux3d::imgui {

struct HierarchyDisplayOptions {
    bool showIcon = true;
    bool showTitle = true;
    bool showIndex = true;
    bool showName = true;
};

using ResourceDisplayOptions = HierarchyDisplayOptions;

class AppSettings {
public:
    static AppSettings& instance();

    // Full path of the single Glider settings file, for display in the UI.
    std::string getIniFilePath() const;

    // Reads and writes the [SearchPaths], [SceneHierarchy] and [Resources] sections
    // of the shared settings file. A missing file keeps the built-in defaults;
    // saveSettings writes only values that differ from them.
    bool loadSettings();
    bool saveSettings();

    // Search paths (relative to active glTF model folder). The editable default
    // is ../texture*; an explicitly empty saved list suppresses that default.
    std::vector<std::string> getSearchPaths() const;
    std::vector<std::string> getExpandedSearchPaths() const;
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

    // Scene hierarchy display options
    HierarchyDisplayOptions getHierarchyOptions() const;
    void setHierarchyOptions(const HierarchyDisplayOptions& options);

    // Resources panel display options
    ResourceDisplayOptions getResourceOptions() const;
    void setResourceOptions(const ResourceDisplayOptions& options);

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
    void expandSearchPathsLocked() const;

    mutable std::mutex m_mutex;
    std::vector<std::string> m_searchPaths;
    mutable std::vector<std::string> m_expandedSearchPaths;
    mutable std::string m_expansionKey;
    std::string m_currentModelPath;
    std::string m_currentModelDir;
    HierarchyDisplayOptions m_hierarchyOptions;
    ResourceDisplayOptions m_resourceOptions;
};

} // namespace ux3d::imgui
