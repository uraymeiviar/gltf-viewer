// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace ux3d::imgui {

using SettingsEntry = std::pair<std::string, std::string>;

// One INI file holds every persisted preference: search paths, hierarchy options,
// window state, recent files, render quality, quality profiles and the ImGui layout.
// A missing file is valid; callers use their built-in defaults. Writers provide only
// the values that differ from the defaults, and save() skips identical content, so a
// pristine run never creates a file. Legacy per-feature files (GliderData, AppData,
// imgui.ini, ...) are merged in memory on first load and consolidated on the next
// real save.
class SettingsFile {
public:
    static SettingsFile& instance();

    // --isolated / --no-persist: never read or write any persistent state.
    void setPersistenceEnabled(bool enabled);
    bool persistenceEnabled() const;

    // Idempotent. Resolves the target file and merges legacy files (not with the
    // GLIDER_DATA_DIR test override, which must stay isolated from user state).
    bool load();
    // Serializes only non-empty sections and writes when the content changed.
    bool save();

    const std::filesystem::path& path() const;
    bool fileExists() const;

    bool empty() const;
    bool hasSection(const std::string& section) const;
    bool hasKey(const std::string& section, const std::string& key) const;
    std::string get(const std::string& section, const std::string& key,
                    const std::string& fallback = {}) const;
    std::vector<SettingsEntry> section(const std::string& section) const;

    // Replaces a section wholesale; an empty list removes it.
    void replaceSection(const std::string& section, std::vector<SettingsEntry> entries);
    void removeSection(const std::string& section);
    void set(const std::string& section, const std::string& key, const std::string& value);
    void remove(const std::string& section, const std::string& key);

private:
    SettingsFile() = default;

    struct Section {
        std::string name;
        std::vector<SettingsEntry> entries;
    };

    void resolvePath() const;
    void mergeLegacyFiles(const std::vector<Section>* legacyTargetSections);
    void mergeLegacyDirectory(const std::filesystem::path& directory);
    bool parseDocument(const std::string& text, std::vector<Section>& sections) const;
    void mergeSections(const std::vector<Section>& sections);
    bool writeText(const std::string& text, const std::filesystem::path& target);
    std::string serialize() const;
    Section* findSection(const std::string& name);
    const Section* findSection(const std::string& name) const;

    mutable bool m_pathResolved = false;
    bool m_loaded = false;
    bool m_enabled = true;
    mutable bool m_explicitOverride = false;
    bool m_preserveExistingFile = false;
    mutable std::filesystem::path m_path;
    std::vector<Section> m_sections;
    std::string m_savedContent;
};

// Full path of the single settings file. Creates nothing.
std::filesystem::path getSettingsFilePath();

} // namespace ux3d::imgui
