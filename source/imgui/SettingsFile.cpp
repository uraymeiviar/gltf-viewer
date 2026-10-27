// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/SettingsFile.h"
#include "imgui/SettingsStorage.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace ux3d::imgui {

namespace {

const char* kSettingsFileName = "settings.ini";
// Comment line at the top of every file written by this store. An existing
// settings.ini without it comes from the release that wrote preferences next to
// the executable, so its values must not shadow the newer GliderData/AppData files.
const char* kSchemaMarker = "; Glider settings file (schema 1)";

fs::path executableDirectory()
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
        return fs::path(buffer).parent_path();
    }
#endif
    std::error_code ec;
    return fs::current_path(ec);
}

fs::path tempGliderDirectory()
{
    std::error_code ec;
    fs::path temp = fs::temp_directory_path(ec);
    if (ec) {
        return executableDirectory();
    }
    return temp / "Glider";
}

fs::path appDataGliderDirectory()
{
    const char* appData = std::getenv("APPDATA");
    if (appData != nullptr && std::strlen(appData) > 0) {
        return fs::u8path(appData) / "Glider";
    }
    return tempGliderDirectory();
}

std::string trim(const std::string& text)
{
    const size_t first = text.find_first_not_of(" \t\r");
    if (first == std::string::npos) {
        return {};
    }
    const size_t last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

bool readTextFile(const fs::path& path, std::string& text)
{
    std::error_code ec;
    if (!fs::exists(path, ec) || fs::is_directory(path, ec)) {
        return false;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return !in.bad();
}

bool hasBinaryControlCharacters(const std::string& text)
{
    for (unsigned char c : text) {
        if (c < 0x09 || (c > 0x0D && c < 0x20)) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> splitLines(const std::string& text)
{
    std::vector<std::string> lines;
    std::string line;
    std::istringstream input(text);
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

bool hasSchemaMarker(const std::string& text)
{
    for (const std::string& line : splitLines(text)) {
        const std::string trimmed = trim(line);
        if (trimmed.empty()) {
            continue;
        }
        return trimmed == kSchemaMarker;
    }
    return false;
}

} // namespace

SettingsFile& SettingsFile::instance()
{
    static SettingsFile s_instance;
    return s_instance;
}

void SettingsFile::setPersistenceEnabled(bool enabled)
{
    m_enabled = enabled;
}

bool SettingsFile::persistenceEnabled() const
{
    return m_enabled;
}

void SettingsFile::resolvePath() const
{
    if (m_pathResolved) {
        return;
    }
    m_pathResolved = true;
    const char* overrideDirectory = std::getenv("GLIDER_DATA_DIR");
    if (overrideDirectory != nullptr && std::strlen(overrideDirectory) > 0) {
        // Test hook: the directory contains settings.ini. Never merge user state.
        m_explicitOverride = true;
        m_path = fs::u8path(overrideDirectory) / kSettingsFileName;
        return;
    }

    const fs::path portable = executableDirectory() / kSettingsFileName;
    // Always prefer the portable file. If the directory is not writable, save()
    // falls back to %APPDATA%/Glider and keeps using it for the rest of the run.
    // An existing AppData file is merged during load, so its values carry over.
    m_path = portable;
}

const fs::path& SettingsFile::path() const
{
    resolvePath();
    return m_path;
}

bool SettingsFile::fileExists() const
{
    std::error_code ec;
    return fs::exists(path(), ec);
}

bool SettingsFile::empty() const
{
    for (const Section& section : m_sections) {
        if (!section.entries.empty()) {
            return false;
        }
    }
    return true;
}

SettingsFile::Section* SettingsFile::findSection(const std::string& name)
{
    for (Section& section : m_sections) {
        if (section.name == name) {
            return &section;
        }
    }
    return nullptr;
}

const SettingsFile::Section* SettingsFile::findSection(const std::string& name) const
{
    for (const Section& section : m_sections) {
        if (section.name == name) {
            return &section;
        }
    }
    return nullptr;
}

bool SettingsFile::hasSection(const std::string& section) const
{
    return findSection(section) != nullptr;
}

bool SettingsFile::hasKey(const std::string& section, const std::string& key) const
{
    const Section* found = findSection(section);
    if (found == nullptr) {
        return false;
    }
    return std::any_of(found->entries.begin(), found->entries.end(),
                       [&](const SettingsEntry& entry) { return entry.first == key; });
}

std::string SettingsFile::get(const std::string& section, const std::string& key,
                              const std::string& fallback) const
{
    const Section* found = findSection(section);
    if (found != nullptr) {
        for (const SettingsEntry& entry : found->entries) {
            if (entry.first == key) {
                return entry.second;
            }
        }
    }
    return fallback;
}

std::vector<SettingsEntry> SettingsFile::section(const std::string& name) const
{
    const Section* found = findSection(name);
    return found != nullptr ? found->entries : std::vector<SettingsEntry>{};
}

void SettingsFile::replaceSection(const std::string& name, std::vector<SettingsEntry> entries)
{
    if (entries.empty()) {
        removeSection(name);
        return;
    }
    if (Section* section = findSection(name); section != nullptr) {
        section->entries = std::move(entries);
        return;
    }
    m_sections.push_back({name, std::move(entries)});
}

void SettingsFile::removeSection(const std::string& name)
{
    m_sections.erase(std::remove_if(m_sections.begin(), m_sections.end(),
                                    [&](const Section& section) { return section.name == name; }),
                     m_sections.end());
}

void SettingsFile::set(const std::string& section, const std::string& key, const std::string& value)
{
    if (Section* found = findSection(section); found != nullptr) {
        for (SettingsEntry& entry : found->entries) {
            if (entry.first == key) {
                entry.second = value;
                return;
            }
        }
        found->entries.emplace_back(key, value);
        return;
    }
    m_sections.push_back({section, {{key, value}}});
}

void SettingsFile::remove(const std::string& section, const std::string& key)
{
    Section* found = findSection(section);
    if (found == nullptr) {
        return;
    }
    found->entries.erase(std::remove_if(found->entries.begin(), found->entries.end(),
                                        [&](const SettingsEntry& entry) { return entry.first == key; }),
                         found->entries.end());
}

bool SettingsFile::parseDocument(const std::string& text, std::vector<Section>& sections) const
{
    std::string content = text;
    if (content.size() >= 3 && static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB && static_cast<unsigned char>(content[2]) == 0xBF) {
        content.erase(0, 3);
    }
    if (hasBinaryControlCharacters(content)) {
        return false;
    }

    Section* current = nullptr;
    for (const std::string& rawLine : splitLines(content)) {
        const std::string line = trim(rawLine);
        if (line.empty() || line.front() == ';' || line.front() == '#') {
            continue;
        }
        if (line.front() == '[') {
            const size_t close = line.find(']');
            if (close != std::string::npos) {
                const std::string name = trim(line.substr(1, close - 1));
                if (!name.empty()) {
                    sections.push_back({name, {}});
                    current = &sections.back();
                }
            }
            continue;
        }
        if (current == nullptr) {
            continue;
        }
        const size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, equals));
        if (key.empty()) {
            continue;
        }
        const bool known = std::any_of(current->entries.begin(), current->entries.end(),
                                       [&](const SettingsEntry& entry) { return entry.first == key; });
        if (!known) {
            current->entries.emplace_back(key, trim(line.substr(equals + 1)));
        }
    }
    return true;
}

namespace {

bool isAtomicSection(const std::string& name)
{
    // Lists and composite records must come from a single source. Merging them
    // key-by-key would mix, for example, one layout's line0..lineN with another's.
    return name == "SearchPaths" || name == "RecentFiles" || name == "Layout" ||
           name == "QualityProfiles";
}

} // namespace

void SettingsFile::mergeSections(const std::vector<Section>& sections)
{
    for (const Section& incoming : sections) {
        Section* target = findSection(incoming.name);
        if (target == nullptr) {
            m_sections.push_back(incoming);
            continue;
        }
        if (isAtomicSection(incoming.name)) {
            // The first (highest-priority) source wins as a whole.
            continue;
        }
        for (const SettingsEntry& entry : incoming.entries) {
            const bool known = std::any_of(target->entries.begin(), target->entries.end(),
                                           [&](const SettingsEntry& existing) { return existing.first == entry.first; });
            if (!known) {
                target->entries.push_back(entry);
            }
        }
    }
}

bool SettingsFile::load()
{
    if (!m_enabled) {
        return false;
    }
    if (m_loaded) {
        return true;
    }
    m_loaded = true;
    resolvePath();

    std::string text;
    std::vector<Section> parsed;
    std::vector<Section> legacyTargetSections;
    std::error_code ec;
    const bool exists = fs::exists(m_path, ec);
    const bool haveFile = readTextFile(m_path, text);
    bool newFormat = true;
    bool migrateLegacy = !exists;
    if (exists && !haveFile) {
        // Never overwrite a file we could not read: its contents are unknown, so this
        // session keeps every change in memory and leaves the file untouched.
        m_preserveExistingFile = true;
        static bool s_readFailLogged = false;
        if (!s_readFailLogged) {
            logSettingsMessage("Failed to read " + m_path.filename().string() +
                                   ": unable to open file; using defaults and keeping changes in memory.", true);
            s_readFailLogged = true;
        }
    }
    if (haveFile) {
        if (!parseDocument(text, parsed)) {
            static bool s_corruptLogged = false;
            if (!s_corruptLogged) {
                logSettingsMessage("Corrupt or invalid " + m_path.filename().string() +
                                       "; using defaults and rewriting on the next change.", true);
                s_corruptLogged = true;
            }
        } else if (!hasSchemaMarker(text)) {
            // Written next to the exe by the release that predates GliderData.
            // Its values are merged after GliderData/AppData, so the most recent
            // state wins, and the file is rewritten in the new format on save.
            newFormat = false;
            migrateLegacy = true;
            legacyTargetSections = std::move(parsed);
        }
    }
    if (newFormat) {
        m_sections = std::move(parsed);
        // Saving only compares values, not comments or whitespace, so a pristine
        // file is never rewritten just because its formatting differs.
        m_savedContent = haveFile ? serialize() : std::string();
    }
    // A legacy or corrupt file is consolidated on the first real save. Once the
    // new-format file exists, legacy files are never consulted again, so values the
    // user resets to their defaults are not resurrected on the next launch.
    if (migrateLegacy) {
        mergeLegacyFiles(legacyTargetSections.empty() ? nullptr : &legacyTargetSections);
    }
    return true;
}

void SettingsFile::mergeLegacyFiles(const std::vector<Section>* legacyTargetSections)
{
    if (m_explicitOverride) {
        return;
    }
    // Newest location first. Earlier entries win, so the GliderData folder written
    // by the previous release takes precedence over AppData fallbacks and over the
    // bare settings.ini/features from releases before it.
    std::vector<fs::path> directories = {executableDirectory() / "GliderData"};
    if (const char* appData = std::getenv("APPDATA"); appData != nullptr && std::strlen(appData) > 0) {
        directories.push_back(fs::u8path(appData) / "Glider");
        directories.push_back(fs::u8path(appData) / "Gestaltor");
    }
    directories.push_back(executableDirectory());
    std::error_code ec;
    for (const fs::path& directory : directories) {
        if (!fs::is_directory(directory, ec)) {
            continue;
        }
        mergeLegacyDirectory(directory);
    }
    if (legacyTargetSections != nullptr) {
        mergeSections(*legacyTargetSections);
    }
}

void SettingsFile::mergeLegacyDirectory(const fs::path& directory)
{
    std::error_code ec;
    const fs::path legacySettings = directory / kSettingsFileName;
    if (fs::exists(legacySettings, ec)) {
        std::error_code compareError;
        const bool sameFile = fs::equivalent(legacySettings, m_path, compareError);
        if (!sameFile) {
            std::string text;
            if (readTextFile(legacySettings, text)) {
                std::vector<Section> parsed;
                if (parseDocument(text, parsed)) {
                    mergeSections(parsed);
                }
            }
        }
    }

    const auto importFeature = [&](const char* sectionName, std::vector<SettingsEntry> entries,
                                   const fs::path& source) {
        if (entries.empty() || hasSection(sectionName)) {
            return;
        }
        replaceSection(sectionName, std::move(entries));
        logSettingsMessage("Imported legacy " + source.filename().string() + " from " + directory.string());
    };

    const fs::path windowState = directory / "window_state.txt";
    {
        std::string text;
        if (readTextFile(windowState, text) && !hasSection("Window")) {
            std::vector<SettingsEntry> entries;
            std::istringstream input(text);
            std::string key;
            std::string value;
            while (input >> key >> value) {
                if (key == "width" || key == "height" || key == "posX" || key == "posY" || key == "maximized") {
                    entries.emplace_back(key, value);
                }
            }
            importFeature("Window", std::move(entries), windowState);
        }
    }

    const fs::path recentFiles = directory / "recent_files.txt";
    {
        std::string text;
        if (readTextFile(recentFiles, text) && !hasSection("RecentFiles") &&
            !hasBinaryControlCharacters(text)) {
            std::vector<SettingsEntry> entries;
            for (const std::string& line : splitLines(text)) {
                const std::string path = trim(line);
                if (!path.empty()) {
                    entries.emplace_back("file" + std::to_string(entries.size()), path);
                }
            }
            importFeature("RecentFiles", std::move(entries), recentFiles);
        }
    }

    const fs::path renderQuality = directory / "render_quality.txt";
    {
        std::string text;
        if (readTextFile(renderQuality, text) && !hasSection("RenderQuality") &&
            !hasBinaryControlCharacters(text)) {
            static const char* keys[] = {"autoQuality", "preset", "iblCubeSize", "iblIrradianceSize",
                                         "iblLutSize", "iblHalfFloat", "iblSpecularSamples",
                                         "gridMode", "renderScale"};
            std::vector<SettingsEntry> entries;
            std::istringstream input(text);
            std::string key;
            std::string value;
            while (input >> key >> value) {
                if (std::find(std::begin(keys), std::end(keys), key) != std::end(keys)) {
                    entries.emplace_back(key, value);
                }
            }
            importFeature("RenderQuality", std::move(entries), renderQuality);
        }
    }

    const fs::path qualityProfiles = directory / "quality_presets.json";
    {
        std::string text;
        if (readTextFile(qualityProfiles, text) && !hasSection("QualityProfiles")) {
            try {
                const nlohmann::json data = nlohmann::json::parse(text);
                const int selection = data.at("selection").get<int>();
                const auto& profiles = data.at("profiles");
                if (profiles.size() != 4) {
                    throw std::runtime_error("invalid profile count");
                }
                std::vector<SettingsEntry> entries;
                entries.emplace_back("selection", std::to_string(selection));
                for (size_t i = 0; i < 4; ++i) {
                    const auto& profile = profiles.at(i);
                    const std::string prefix = "profile" + std::to_string(i) + ".";
                    const auto add = [&](const char* key, const nlohmann::json& value) {
                        if (value.is_boolean()) {
                            entries.emplace_back(prefix + key, value.get<bool>() ? "1" : "0");
                        } else if (value.is_number_integer()) {
                            entries.emplace_back(prefix + key, std::to_string(value.get<long long>()));
                        } else if (value.is_number()) {
                            std::ostringstream number;
                            number << value.get<double>();
                            entries.emplace_back(prefix + key, number.str());
                        }
                    };
                    add("textureMaxSize", profile.at("textureMaxSize"));
                    add("msaa", profile.at("msaa"));
                    add("postAA", profile.at("postAA"));
                    add("ssrQuality", profile.at("ssrQuality"));
                    add("skybox", profile.at("skybox"));
                    add("ibl", profile.at("ibl"));
                    add("lights", profile.at("lights"));
                    add("ssr", profile.at("ssr"));
                    add("bloom", profile.at("bloom"));
                    add("cube", profile.at("cube"));
                    add("irradiance", profile.at("irradiance"));
                    add("lut", profile.at("lut"));
                    add("samples", profile.at("samples"));
                    add("halfFloat", profile.at("halfFloat"));
                    add("grid", profile.at("grid"));
                    add("scale", profile.at("scale"));
                }
                importFeature("QualityProfiles", std::move(entries), qualityProfiles);
            } catch (const std::exception&) {
                logSettingsMessage("Ignoring unreadable legacy quality_presets.json: " + qualityProfiles.string(), true);
            }
        }
    }

    const fs::path imguiIni = directory / "imgui.ini";
    {
        std::string text;
        if (readTextFile(imguiIni, text) && !hasSection("Layout") && !hasBinaryControlCharacters(text)) {
            std::vector<SettingsEntry> entries;
            for (const std::string& line : splitLines(text)) {
                entries.emplace_back("line" + std::to_string(entries.size()), line);
            }
            importFeature("Layout", std::move(entries), imguiIni);
        }
    }
}

std::string SettingsFile::serialize() const
{
    std::ostringstream output;
    bool wroteSection = false;
    for (const Section& section : m_sections) {
        if (section.entries.empty() || section.name.empty()) {
            continue;
        }
        if (wroteSection) {
            output << "\n";
        }
        output << "[" << section.name << "]\n";
        for (const SettingsEntry& entry : section.entries) {
            if (entry.first.empty()) {
                continue;
            }
            output << entry.first << "=" << entry.second << "\n";
        }
        wroteSection = true;
    }
    if (!wroteSection) {
        return {};
    }
    return std::string(kSchemaMarker) + "\n\n" + output.str();
}

bool SettingsFile::writeText(const std::string& text, const fs::path& target)
{
    std::error_code ec;
    const fs::path parent = target.parent_path();
    if (!parent.empty() && !fs::is_directory(parent, ec)) {
        fs::create_directories(parent, ec);
        if (ec) {
            return false;
        }
    }

    fs::path temporary = target;
    temporary += ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return false;
        }
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        out.flush();
        if (!out) {
            out.close();
            std::error_code removeError;
            fs::remove(temporary, removeError);
            return false;
        }
    }
#ifdef _WIN32
    if (!MoveFileExW(temporary.wstring().c_str(), target.wstring().c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::error_code removeError;
        fs::remove(temporary, removeError);
        return false;
    }
#else
    fs::rename(temporary, target, ec);
    if (ec) {
        std::error_code removeError;
        fs::remove(target, removeError);
        fs::rename(temporary, target, ec);
    }
    if (ec) {
        std::error_code removeError;
        fs::remove(temporary, removeError);
        return false;
    }
#endif
    return true;
}

bool SettingsFile::save()
{
    if (!m_enabled || m_preserveExistingFile) {
        return false;
    }
    if (!m_loaded) {
        load();
    }
    resolvePath();

    const std::string text = serialize();
    std::error_code ec;
    if (text.empty()) {
        if (fs::exists(m_path, ec)) {
            fs::remove(m_path, ec);
        }
        m_savedContent.clear();
        return true;
    }
    if (text == m_savedContent && fs::exists(m_path, ec)) {
        return true;
    }

    std::vector<fs::path> candidates = {m_path};
    if (!m_explicitOverride) {
        candidates.push_back(appDataGliderDirectory() / kSettingsFileName);
        candidates.push_back(tempGliderDirectory() / kSettingsFileName);
    }
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (i > 0 && fs::equivalent(candidates[i], m_path, ec)) {
            continue;
        }
        if (!writeText(text, candidates[i])) {
            continue;
        }
        if (i > 0) {
            static bool s_fallbackLogged = false;
            if (!s_fallbackLogged) {
                logSettingsMessage("Settings file '" + m_path.string() +
                                   "' is not writable; using '" + candidates[i].string() + "'.");
                s_fallbackLogged = true;
            }
            m_path = candidates[i];
            m_pathResolved = true;
        }
        m_savedContent = text;
        return true;
    }

    static bool s_writeFailLogged = false;
    if (!s_writeFailLogged) {
        logSettingsMessage(std::string("Failed to write ") + kSettingsFileName + "; changes kept in memory.", true);
        s_writeFailLogged = true;
    }
    return false;
}

fs::path getSettingsFilePath()
{
    return SettingsFile::instance().path();
}

} // namespace ux3d::imgui
