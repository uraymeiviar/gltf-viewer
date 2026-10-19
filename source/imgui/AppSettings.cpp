// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/AppSettings.h"
#include "core/Logger.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace ux3d::imgui {

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
    m_searchPaths = { "../texture" };
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

bool AppSettings::loadSettings(const std::string& iniPath)
{
    std::string targetFile = iniPath.empty() ? m_iniPath : iniPath;

    std::error_code ec;
    // Check if target file exists in current directory
    if (!fs::exists(fs::u8path(targetFile), ec)) {
        // Fallback: check alongside executable
#ifdef _WIN32
        char exeBuf[MAX_PATH];
        DWORD len = GetModuleFileNameA(nullptr, exeBuf, MAX_PATH);
        if (len > 0) {
            fs::path exeDir = fs::path(exeBuf).parent_path();
            fs::path alt = exeDir / targetFile;
            if (fs::exists(alt, ec)) {
                targetFile = alt.generic_string();
            }
        }
#endif
    }

    if (!fs::exists(fs::u8path(targetFile), ec)) {
        // File does not exist yet; save default settings
        saveSettings(targetFile);
        return false;
    }

    std::ifstream in(targetFile);
    if (!in.is_open()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_searchPaths.clear();

    std::string line;
    bool inSearchPathsSection = false;
    bool inSceneHierarchySection = false;
    bool inResourcesSection = false;

    while (std::getline(in, line)) {
        std::string trimmed = trimString(line);
        if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#') {
            continue;
        }

        if (trimmed[0] == '[') {
            // Check section header
            if (trimmed.rfind("[SearchPaths]", 0) == 0 || trimmed.rfind("[SearchPaths", 0) == 0) {
                inSearchPathsSection = true;
                inSceneHierarchySection = false;
                inResourcesSection = false;
            } else if (trimmed.rfind("[SceneHierarchy]", 0) == 0 || trimmed.rfind("[SceneHierarchy", 0) == 0) {
                inSearchPathsSection = false;
                inSceneHierarchySection = true;
                inResourcesSection = false;
            } else if (trimmed.rfind("[Resources]", 0) == 0 || trimmed.rfind("[Resources", 0) == 0) {
                inSearchPathsSection = false;
                inSceneHierarchySection = false;
                inResourcesSection = true;
            } else {
                inSearchPathsSection = false;
                inSceneHierarchySection = false;
                inResourcesSection = false;
            }
            continue;
        }

        if (inSearchPathsSection) {
            auto eqPos = trimmed.find('=');
            if (eqPos != std::string::npos) {
                std::string key = trimString(trimmed.substr(0, eqPos));
                std::string val = trimString(trimmed.substr(eqPos + 1));

                if (key == "count") {
                    continue;
                } else if (key.rfind("path", 0) == 0 && key != "paths") {
                    std::string norm = toGenericSlash(val);
                    if (!norm.empty() && std::find(m_searchPaths.begin(), m_searchPaths.end(), norm) == m_searchPaths.end()) {
                        m_searchPaths.push_back(norm);
                    }
                } else if (key == "paths") {
                    std::stringstream ss(val);
                    std::string item;
                    while (std::getline(ss, item, ';')) {
                        std::string norm = toGenericSlash(trimString(item));
                        if (!norm.empty() && std::find(m_searchPaths.begin(), m_searchPaths.end(), norm) == m_searchPaths.end()) {
                            m_searchPaths.push_back(norm);
                        }
                    }
                }
            }
        } else if (inSceneHierarchySection) {
            auto eqPos = trimmed.find('=');
            if (eqPos != std::string::npos) {
                std::string key = trimString(trimmed.substr(0, eqPos));
                std::string val = trimString(trimmed.substr(eqPos + 1));
                bool bVal = (val == "true" || val == "1" || val == "True");

                if (key == "showIcon") m_hierarchyOptions.showIcon = bVal;
                else if (key == "showTitle") m_hierarchyOptions.showTitle = bVal;
                else if (key == "showIndex") m_hierarchyOptions.showIndex = bVal;
                else if (key == "showName") m_hierarchyOptions.showName = bVal;
            }
        } else if (inResourcesSection) {
            auto eqPos = trimmed.find('=');
            if (eqPos != std::string::npos) {
                std::string key = trimString(trimmed.substr(0, eqPos));
                std::string val = trimString(trimmed.substr(eqPos + 1));
                bool bVal = (val == "true" || val == "1" || val == "True");

                if (key == "showIcon") m_resourceOptions.showIcon = bVal;
                else if (key == "showTitle") m_resourceOptions.showTitle = bVal;
                else if (key == "showIndex") m_resourceOptions.showIndex = bVal;
                else if (key == "showName") m_resourceOptions.showName = bVal;
            }
        }
    }

    m_iniPath = targetFile;
    return true;
}

bool AppSettings::saveSettings(const std::string& iniPath)
{
    std::string targetFile = iniPath.empty() ? m_iniPath : iniPath;

    std::vector<std::string> preservedLines;
    bool fileExisted = false;

    std::error_code ec;
    if (fs::exists(fs::u8path(targetFile), ec)) {
        std::ifstream in(targetFile);
        if (in.is_open()) {
            fileExisted = true;
            std::string line;
            bool skippingSection = false;

            while (std::getline(in, line)) {
                std::string trimmed = trimString(line);
                if (trimmed[0] == '[') {
                    if (trimmed.rfind("[SearchPaths]", 0) == 0 || trimmed.rfind("[SearchPaths", 0) == 0 ||
                        trimmed.rfind("[SceneHierarchy]", 0) == 0 || trimmed.rfind("[SceneHierarchy", 0) == 0 ||
                        trimmed.rfind("[Resources]", 0) == 0 || trimmed.rfind("[Resources", 0) == 0) {
                        skippingSection = true;
                        continue;
                    } else {
                        skippingSection = false;
                    }
                }

                if (!skippingSection) {
                    preservedLines.push_back(line);
                }
            }
        }
    }

    std::ofstream out(targetFile);
    if (!out.is_open()) {
        return false;
    }

    if (!fileExisted) {
        out << "; Gestaltor glTF Viewer Configuration\n\n";
    }

    // Write [SearchPaths] section
    out << "[SearchPaths]\n";
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        out << "count=" << m_searchPaths.size() << "\n";
        for (size_t i = 0; i < m_searchPaths.size(); ++i) {
            out << "path" << i << "=" << m_searchPaths[i] << "\n";
        }
    }
    out << "\n";

    // Write [SceneHierarchy] section
    out << "[SceneHierarchy]\n";
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        out << "showIcon=" << (m_hierarchyOptions.showIcon ? "true" : "false") << "\n";
        out << "showTitle=" << (m_hierarchyOptions.showTitle ? "true" : "false") << "\n";
        out << "showIndex=" << (m_hierarchyOptions.showIndex ? "true" : "false") << "\n";
        out << "showName=" << (m_hierarchyOptions.showName ? "true" : "false") << "\n";
    }
    out << "\n";

    // Write [Resources] section
    out << "[Resources]\n";
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        out << "showIcon=" << (m_resourceOptions.showIcon ? "true" : "false") << "\n";
        out << "showTitle=" << (m_resourceOptions.showTitle ? "true" : "false") << "\n";
        out << "showIndex=" << (m_resourceOptions.showIndex ? "true" : "false") << "\n";
        out << "showName=" << (m_resourceOptions.showName ? "true" : "false") << "\n";
    }
    out << "\n";

    // Write any preserved other sections
    for (const auto& pl : preservedLines) {
        out << pl << "\n";
    }

    m_iniPath = targetFile;
    return true;
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

    for (const auto& sp : m_searchPaths) {
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
