// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/AppSettings.h"
#include "imgui/SettingsFile.h"
#include "imgui/SettingsStorage.h"
#include "core/Logger.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <functional>
#include <set>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace ux3d::imgui {

static constexpr const char* kDefaultSearchPath = "../texture*";

AppSettings& AppSettings::instance()
{
    static AppSettings s_instance;
    return s_instance;
}

static std::string trimString(const std::string& str)
{
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static std::string toGenericSlash(const std::string& path)
{
    std::string result = path;
    for (char& c : result) {
        if (c == '\\') c = '/';
    }
    return result;
}

static bool stringEqualsIgnoreCase(const std::string& a, const std::string& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

AppSettings::AppSettings()
{
    // Default search paths when no config exists
    m_searchPaths = { kDefaultSearchPath };
}

std::string AppSettings::normalizePath(const std::string& path) const
{
    std::string trimmed = trimString(path);
    return toGenericSlash(trimmed);
}

void AppSettings::setCurrentModelPath(const std::string& modelPath)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_currentModelPath = toGenericSlash(modelPath);
    m_expansionKey.clear();

    if (!m_currentModelPath.empty()) {
        std::error_code ec;
        fs::path p = fs::u8path(m_currentModelPath);
        if (fs::exists(p, ec)) {
            m_currentModelDir = toGenericSlash(p.parent_path().generic_string());
        } else {
            // Path might be relative
            fs::path parent = p.parent_path();
            m_currentModelDir = parent.empty() ? "." : toGenericSlash(parent.generic_string());
        }
    } else {
        m_currentModelDir.clear();
    }
}

std::string AppSettings::getCurrentModelPath() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentModelPath;
}

std::string AppSettings::getCurrentModelDir() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentModelDir;
}

std::vector<std::string> AppSettings::getSearchPaths() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_searchPaths;
}

void AppSettings::setSearchPaths(const std::vector<std::string>& paths)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_searchPaths.clear();
    for (const auto& p : paths) {
        std::string norm = normalizePath(p);
        if (!norm.empty() && std::find(m_searchPaths.begin(), m_searchPaths.end(), norm) == m_searchPaths.end()) {
            m_searchPaths.push_back(norm);
        }
    }
}

bool AppSettings::addSearchPath(const std::string& path)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string norm = normalizePath(path);
    if (norm.empty()) return false;

    if (std::find(m_searchPaths.begin(), m_searchPaths.end(), norm) == m_searchPaths.end()) {
        m_searchPaths.push_back(norm);
        return true;
    }
    return false;
}

bool AppSettings::removeSearchPath(size_t index)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index < m_searchPaths.size()) {
        m_searchPaths.erase(m_searchPaths.begin() + index);
        return true;
    }
    return false;
}

bool AppSettings::moveSearchPathUp(size_t index)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index > 0 && index < m_searchPaths.size()) {
        std::swap(m_searchPaths[index], m_searchPaths[index - 1]);
        return true;
    }
    return false;
}

bool AppSettings::moveSearchPathDown(size_t index)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index + 1 < m_searchPaths.size()) {
        std::swap(m_searchPaths[index], m_searchPaths[index + 1]);
        return true;
    }
    return false;
}

void AppSettings::clearSearchPaths()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_searchPaths.clear();
}

HierarchyDisplayOptions AppSettings::getHierarchyOptions() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_hierarchyOptions;
}

void AppSettings::setHierarchyOptions(const HierarchyDisplayOptions& options)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hierarchyOptions = options;
}

ResourceDisplayOptions AppSettings::getResourceOptions() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_resourceOptions;
}

void AppSettings::setResourceOptions(const ResourceDisplayOptions& options)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resourceOptions = options;
}

std::string AppSettings::getIniFilePath() const
{
    return getSettingsFilePath().string();
}

bool AppSettings::loadSettings()
{
    SettingsFile& settings = SettingsFile::instance();
    if (!settings.load()) {
        // Persistence is disabled (--isolated): keep the built-in defaults.
        std::lock_guard<std::mutex> lock(m_mutex);
        m_searchPaths = { kDefaultSearchPath };
        m_hierarchyOptions = HierarchyDisplayOptions{};
        m_resourceOptions = ResourceDisplayOptions{};
        return false;
    }

    const auto readBool = [&](const char* section, const char* key, bool fallback) {
        const std::string value = settings.get(section, key);
        if (value == "1" || value == "true" || value == "True") {
            return true;
        }
        if (value == "0" || value == "false" || value == "False") {
            return false;
        }
        return fallback;
    };

    std::lock_guard<std::mutex> lock(m_mutex);
    m_searchPaths.clear();
    if (settings.hasSection("SearchPaths")) {
        // An existing section is authoritative: count=0 means the user cleared it.
        for (const SettingsEntry& entry : settings.section("SearchPaths")) {
            if (entry.first.rfind("path", 0) == 0 && entry.first != "paths") {
                const std::string norm = toGenericSlash(trimString(entry.second));
                if (!norm.empty() && std::find(m_searchPaths.begin(), m_searchPaths.end(), norm) == m_searchPaths.end()) {
                    m_searchPaths.push_back(norm);
                }
            } else if (entry.first == "paths") {
                std::stringstream ss(entry.second);
                std::string item;
                while (std::getline(ss, item, ';')) {
                    const std::string norm = toGenericSlash(trimString(item));
                    if (!norm.empty() && std::find(m_searchPaths.begin(), m_searchPaths.end(), norm) == m_searchPaths.end()) {
                        m_searchPaths.push_back(norm);
                    }
                }
            }
        }
    } else {
        m_searchPaths = { kDefaultSearchPath };
    }

    const HierarchyDisplayOptions hierarchyDefaults;
    m_hierarchyOptions.showIcon = readBool("SceneHierarchy", "showIcon", hierarchyDefaults.showIcon);
    m_hierarchyOptions.showTitle = readBool("SceneHierarchy", "showTitle", hierarchyDefaults.showTitle);
    m_hierarchyOptions.showIndex = readBool("SceneHierarchy", "showIndex", hierarchyDefaults.showIndex);
    m_hierarchyOptions.showName = readBool("SceneHierarchy", "showName", hierarchyDefaults.showName);

    const ResourceDisplayOptions resourceDefaults;
    m_resourceOptions.showIcon = readBool("Resources", "showIcon", resourceDefaults.showIcon);
    m_resourceOptions.showTitle = readBool("Resources", "showTitle", resourceDefaults.showTitle);
    m_resourceOptions.showIndex = readBool("Resources", "showIndex", resourceDefaults.showIndex);
    m_resourceOptions.showName = readBool("Resources", "showName", resourceDefaults.showName);

    // Drop values that equal the defaults so a legacy file is consolidated without
    // re-writing defaults into the new one.
    const std::vector<std::string> defaultSearchPaths = { kDefaultSearchPath };
    if (m_searchPaths == defaultSearchPaths) {
        settings.removeSection("SearchPaths");
    }
    if (m_hierarchyOptions.showIcon == hierarchyDefaults.showIcon &&
        m_hierarchyOptions.showTitle == hierarchyDefaults.showTitle &&
        m_hierarchyOptions.showIndex == hierarchyDefaults.showIndex &&
        m_hierarchyOptions.showName == hierarchyDefaults.showName) {
        settings.removeSection("SceneHierarchy");
    }
    if (m_resourceOptions.showIcon == resourceDefaults.showIcon &&
        m_resourceOptions.showTitle == resourceDefaults.showTitle &&
        m_resourceOptions.showIndex == resourceDefaults.showIndex &&
        m_resourceOptions.showName == resourceDefaults.showName) {
        settings.removeSection("Resources");
    }
    return true;
}

bool AppSettings::saveSettings()
{
    SettingsFile& settings = SettingsFile::instance();
    settings.load();

    std::vector<SettingsEntry> searchPathEntries;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const std::vector<std::string> defaults = { kDefaultSearchPath };
        if (m_searchPaths != defaults) {
            if (m_searchPaths.empty()) {
                // Keep an explicit empty list distinct from "no value stored".
                searchPathEntries.emplace_back("count", "0");
            } else {
                for (size_t i = 0; i < m_searchPaths.size(); ++i) {
                    searchPathEntries.emplace_back("path" + std::to_string(i), m_searchPaths[i]);
                }
            }
        }
    }
    settings.replaceSection("SearchPaths", std::move(searchPathEntries));

    std::vector<SettingsEntry> hierarchyEntries;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const HierarchyDisplayOptions defaults;
        if (m_hierarchyOptions.showIcon != defaults.showIcon) hierarchyEntries.emplace_back("showIcon", m_hierarchyOptions.showIcon ? "1" : "0");
        if (m_hierarchyOptions.showTitle != defaults.showTitle) hierarchyEntries.emplace_back("showTitle", m_hierarchyOptions.showTitle ? "1" : "0");
        if (m_hierarchyOptions.showIndex != defaults.showIndex) hierarchyEntries.emplace_back("showIndex", m_hierarchyOptions.showIndex ? "1" : "0");
        if (m_hierarchyOptions.showName != defaults.showName) hierarchyEntries.emplace_back("showName", m_hierarchyOptions.showName ? "1" : "0");
    }
    settings.replaceSection("SceneHierarchy", std::move(hierarchyEntries));

    std::vector<SettingsEntry> resourceEntries;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const ResourceDisplayOptions defaults;
        if (m_resourceOptions.showIcon != defaults.showIcon) resourceEntries.emplace_back("showIcon", m_resourceOptions.showIcon ? "1" : "0");
        if (m_resourceOptions.showTitle != defaults.showTitle) resourceEntries.emplace_back("showTitle", m_resourceOptions.showTitle ? "1" : "0");
        if (m_resourceOptions.showIndex != defaults.showIndex) resourceEntries.emplace_back("showIndex", m_resourceOptions.showIndex ? "1" : "0");
        if (m_resourceOptions.showName != defaults.showName) resourceEntries.emplace_back("showName", m_resourceOptions.showName ? "1" : "0");
    }
    settings.replaceSection("Resources", std::move(resourceEntries));

    return settings.save();
}

static bool folderPatternMatches(std::string pattern, std::string name)
{
    auto lower = [](std::string& s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
    lower(pattern); lower(name);
    size_t p = 0, n = 0, star = std::string::npos, retry = 0;
    while (n < name.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == name[n])) { ++p; ++n; }
        else if (p < pattern.size() && pattern[p] == '*') { star = p++; retry = n; }
        else if (star != std::string::npos) { p = star+1; n = ++retry; }
        else return false;
    }
    while (p < pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
}

void AppSettings::expandSearchPathsLocked() const
{
    std::string key = m_currentModelDir;
    for (const auto& path : m_searchPaths) key += '\n' + path;
    if (key == m_expansionKey) return;
    m_expansionKey = key;
    m_expandedSearchPaths.clear();
    std::set<std::string> seen;
    auto add = [&](const fs::path& path) {
        std::error_code ec;
        if (!fs::is_directory(path, ec)) return;
        // Expansion starts from the model directory, which is itself relative when the
        // model was opened by a relative path. Store absolute paths so resolveAssetPath
        // never prepends the model directory to a path that already contains it.
        fs::path absolute = fs::absolute(path, ec);
        if (ec || absolute.empty()) {
            ec.clear();
            absolute = path;
        }
        const auto normalized = absolute.lexically_normal().generic_string();
        std::string identity = normalized;
        for (char& c : identity) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (seen.insert(identity).second) m_expandedSearchPaths.push_back(normalized);
    };
    for (const auto& text : m_searchPaths) {
        auto pattern = fs::u8path(text);
        if (pattern.is_relative()) {
            if (m_currentModelDir.empty()) continue;
            pattern = fs::u8path(m_currentModelDir)/pattern;
        }
        std::vector<std::string> segments;
        for (const auto& component : pattern.relative_path()) segments.push_back(component.generic_string());
        std::function<void(fs::path,size_t)> expand = [&](fs::path base, size_t index) {
            if (index == segments.size()) { add(base); return; }
            const auto& segment = segments[index];
            if (segment.find_first_of("*?") == std::string::npos) { expand(base/segment, index+1); return; }
            if (segment == "**") expand(base, index+1); // zero directory levels
            std::error_code ec;
            std::vector<fs::path> children;
            for (fs::directory_iterator it(base, fs::directory_options::skip_permission_denied, ec), end;
                 it != end; it.increment(ec)) {
                if (ec) break;
                const auto& entry = *it;
                bool reparse = entry.is_symlink(ec);
#ifdef _WIN32
                const DWORD attributes = GetFileAttributesW(entry.path().c_str());
                reparse |= attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT);
#endif
                if (!reparse && entry.is_directory(ec) && (segment == "**" || folderPatternMatches(segment, entry.path().filename().generic_string())))
                    children.push_back(entry.path());
            }
            std::sort(children.begin(), children.end());
            for (const auto& child : children) expand(child, segment == "**" ? index : index+1);
        };
        expand(pattern.root_path(), 0);
    }
}

std::vector<std::string> AppSettings::getExpandedSearchPaths() const
{
    std::lock_guard lock(m_mutex);
    expandSearchPathsLocked();
    return m_expandedSearchPaths;
}

std::string AppSettings::resolveAssetPath(const std::string& requestedPath) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (requestedPath.empty()) {
        return "";
    }

    std::error_code ec;
    fs::path reqPath = fs::u8path(requestedPath);

    // 1. If file already exists on disk at requested path, return it immediately
    if (fs::is_regular_file(reqPath, ec)) {
        return requestedPath;
    }

    // 2. Identify model directory and asset filename
    fs::path modelDir;
    if (!m_currentModelDir.empty()) {
        modelDir = fs::u8path(m_currentModelDir);
    } else if (reqPath.has_parent_path()) {
        modelDir = reqPath.parent_path();
    }

    std::string filename = reqPath.filename().generic_string();
    if (filename.empty()) {
        return "";
    }

    // 3. Prepare candidate filenames for cross-format resolution (png, dds, jpg, tga, ktx2)
    std::vector<std::string> candidateNames;
    candidateNames.push_back(filename);

    std::string stem = reqPath.stem().generic_string();
    std::string ext = reqPath.extension().generic_string();
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    std::string baseStem = stem;
    if (stem.size() > 4) {
        std::string stemLower = stem;
        for (char& c : stemLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (stemLower.rfind(".png") == stemLower.size() - 4 ||
            stemLower.rfind(".jpg") == stemLower.size() - 4 ||
            stemLower.rfind(".dds") == stemLower.size() - 4) {
            baseStem = stem.substr(0, stem.size() - 4);
        }
    }

    if (ext == ".png") {
        candidateNames.push_back(filename + ".dds");
    }

    const std::string supportedExtensions[] = {
        ".dds", ".png.dds", ".png", ".jpg", ".jpeg", ".tga", ".ktx2"
    };

    for (const auto& altExt : supportedExtensions) {
        std::string name1 = stem + altExt;
        if (std::find(candidateNames.begin(), candidateNames.end(), name1) == candidateNames.end()) {
            candidateNames.push_back(name1);
        }
        std::string name2 = baseStem + altExt;
        if (std::find(candidateNames.begin(), candidateNames.end(), name2) == candidateNames.end()) {
            candidateNames.push_back(name2);
        }
    }

    // 4. Assemble search directories: model directory first (implicit), then configured search paths
    std::vector<fs::path> dirsToCheck;
    if (!modelDir.empty()) {
        dirsToCheck.push_back(modelDir);
    }

    expandSearchPathsLocked();
    for (const auto& sp : m_expandedSearchPaths) {
        if (sp.empty()) continue;
        fs::path searchDir = fs::u8path(sp);
        if (searchDir.is_relative() && !modelDir.empty()) {
            searchDir = modelDir / searchDir;
        }
        if (std::find(dirsToCheck.begin(), dirsToCheck.end(), searchDir) == dirsToCheck.end()) {
            dirsToCheck.push_back(searchDir);
        }
    }

    // 5. Test candidate paths across all directories
    for (const auto& dir : dirsToCheck) {
        for (const auto& cName : candidateNames) {
            fs::path candidate = dir / fs::u8path(cName);
            ec.clear();
            if (fs::is_regular_file(candidate, ec)) {
                std::string resolved = toGenericSlash(candidate.generic_string());
                try {
                    resolved = toGenericSlash(fs::canonical(candidate, ec).generic_string());
                } catch (...) {
                    resolved = toGenericSlash(candidate.generic_string());
                }
                ux3d::core::logInfo("[SearchPaths] Resolved '%s' -> '%s'", filename.c_str(), resolved.c_str());
                return resolved;
            }
        }

        // Case-insensitive directory scan fallback
        ec.clear();
        if (fs::is_directory(dir, ec)) {
            for (const auto& entry : fs::directory_iterator(dir, ec)) {
                if (entry.is_regular_file(ec)) {
                    std::string entryFn = entry.path().filename().generic_string();
                    for (const auto& cName : candidateNames) {
                        if (stringEqualsIgnoreCase(entryFn, cName)) {
                            std::string resolved = toGenericSlash(entry.path().generic_string());
                            try {
                                resolved = toGenericSlash(fs::canonical(entry.path(), ec).generic_string());
                            } catch (...) {
                                resolved = toGenericSlash(entry.path().generic_string());
                            }
                            ux3d::core::logInfo("[SearchPaths] Resolved '%s' -> '%s' (case-insensitive)", filename.c_str(), resolved.c_str());
                            return resolved;
                        }
                    }
                }
            }
        }
    }

    return "";
}

} // namespace ux3d::imgui
