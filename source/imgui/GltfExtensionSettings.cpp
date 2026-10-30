// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/GltfExtensionSettings.h"
#include "imgui/ExtensionInspector.h"
#include "imgui/SettingsFile.h"
#include "imgui/SettingsStorage.h"
#include "imgui/UiProbe.h"
#include "gltf/extensions/AsoboExtensions.h"
#include "gltf/extensions/AsoboSupport.h"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace ux3d::imgui {

namespace {

constexpr const char* kSection = "GltfExtensions";

bool validKey(const std::string& key)
{
    if (key.empty() || key.size() > 128) return false;
    return std::all_of(key.begin(), key.end(), [](unsigned char c) { return std::isalnum(c) || c == '_' || c == '-' || c == '.'; });
}

bool parseFalse(const std::string& text, bool& isFalse)
{
    if (text == "0" || text == "false" || text == "False") { isFalse = true; return true; }
    if (text == "1" || text == "true" || text == "True") { isFalse = false; return true; }
    return false;
}

std::vector<GltfExtensionGroup> buildGroups()
{
    namespace asobo = ux3d::gltf::asobo;
    std::vector<GltfExtensionGroup> groups;

    GltfExtensionGroup msfs;
    msfs.vendor = "ASOBO";
    msfs.title = "MSFS (ASOBO)";
    msfs.masterLabel = "Apply MSFS (ASOBO) extensions";
    msfs.masterKey = "asobo";
    msfs.description = "Microsoft Flight Simulator extensions. Off renders the file as if it had no ASOBO extensions.";
    // One switch per extension Glider renders, from the A0.2 support table; extensions that are part of how a file is
    // stored or read (viewportSwitch false) have none.
    for (const asobo::SupportInfo& info : asobo::supportTable()) {
        if (!asobo::isAsoboExtension(info.name)) continue;
        if (info.status != asobo::Support::Rendered && info.status != asobo::Support::Partial) continue;
        if (!info.viewportSwitch) continue;
        GltfExtensionEntry entry;
        entry.name = std::string(info.name);
        entry.label = ExtensionInspector::formatExtensionLabel(entry.name);
        entry.note = std::string(info.note);
        msfs.extensions.push_back(std::move(entry));
    }
    groups.push_back(std::move(msfs));

    // Further vendors go here: a group with its vendor prefix (KHR, EXT, ...), a title, a master key and its
    // extensions. Nothing else changes: the tab, the registry keys, the saved keys and get_state read this table.
    return groups;
}

} // namespace

GltfExtensionSettings& GltfExtensionSettings::instance()
{
    static GltfExtensionSettings s_instance;
    return s_instance;
}

const std::vector<GltfExtensionGroup>& GltfExtensionSettings::groups()
{
    static const std::vector<GltfExtensionGroup> s_groups = buildGroups();
    return s_groups;
}

std::string GltfExtensionSettings::vendorOf(std::string_view key)
{
    const size_t underscore = key.find('_');
    return std::string(underscore == std::string_view::npos ? key : key.substr(0, underscore));
}

bool GltfExtensionSettings::enabled(std::string_view key) const
{
    return m_effective.find(std::string(key)) == m_effective.end();
}

bool GltfExtensionSettings::applies(std::string_view extension) const
{
    return enabled(vendorOf(extension)) && enabled(extension);
}

ux3d::renderer::AsoboOptions GltfExtensionSettings::asoboOptions() const
{
    ux3d::renderer::AsoboOptions options;
    options.enabled = enabled("ASOBO");
    for (const std::string& key : m_effective) {
        if (key.rfind("ASOBO_", 0) == 0) options.disabled.push_back(key); // the set is sorted like AsoboOptions::disabled
    }
    return options;
}

void GltfExtensionSettings::setEnabled(const std::string& key, bool enabledValue, bool persist)
{
    if (key.empty()) return;
    const bool changed = enabledValue ? m_effective.erase(key) > 0 : m_effective.insert(key).second;
    if (changed) ++m_revision;
    if (!persist) return;
    const bool savedChanged = enabledValue ? m_saved.erase(key) > 0 : m_saved.insert(key).second;
    if (savedChanged) save();
}

float GltfExtensionSettings::tuning(const std::string& key) const
{
    const auto found = m_tuning.find(key);
    return found == m_tuning.end() ? 0.0f : found->second;
}

void GltfExtensionSettings::setTuning(const std::string& key, float value, bool persist)
{
    const auto found = m_tuning.find(key);
    if (found == m_tuning.end() || !std::isfinite(value) || value < 0 || value > (key == "decal_tolerance_mm" ? 10 : 1)) return;
    if (found->second != value) { found->second = value; ++m_revision; }
    if (persist && m_savedTuning[key] != value) { m_savedTuning[key] = value; save(); }
}

void GltfExtensionSettings::restoreTuning(const std::map<std::string, float>& values)
{
    if (m_tuning != values) { m_tuning = values; ++m_revision; }
}

void GltfExtensionSettings::commit(const std::string& key)
{
    const bool on = enabled(key);
    const bool savedChanged = on ? m_saved.erase(key) > 0 : m_saved.insert(key).second;
    if (savedChanged) save();
}

void GltfExtensionSettings::resetToDefaults()
{
    const bool changed = !m_effective.empty() || m_tuning != std::map<std::string,float>{{"interior_sky_reflection",0.5f},{"decal_tolerance_mm",1.2f}};
    const bool savedChanged = !m_saved.empty() || m_savedTuning != std::map<std::string,float>{{"interior_sky_reflection",0.5f},{"decal_tolerance_mm",1.2f}};
    m_tuning = {{"interior_sky_reflection", 0.5f}, {"decal_tolerance_mm", 1.2f}};
    m_savedTuning = m_tuning;
    m_effective.clear();
    m_saved.clear();
    if (changed) ++m_revision;
    if (savedChanged) save();
}

void GltfExtensionSettings::discardSessionChanges()
{
    if (m_effective == m_saved && m_tuning == m_savedTuning) return;
    m_tuning = m_savedTuning;
    m_effective = m_saved;
    ++m_revision;
}

void GltfExtensionSettings::restore(const std::set<std::string>& effective)
{
    if (m_effective == effective) return;
    m_effective = effective;
    ++m_revision;
}

void GltfExtensionSettings::load()
{
    SettingsFile& store = SettingsFile::instance();
    if (!store.persistenceEnabled()) return; // --isolated: never read.
    if (!store.load()) return;
    std::set<std::string> parsed;
    bool invalid = false;
    for (const SettingsEntry& entry : store.section(kSection)) {
        if (m_tuning.contains(entry.first)) {
            char* end = nullptr;
            const float value = std::strtof(entry.second.c_str(), &end);
            if (end == entry.second.c_str() || *end || !std::isfinite(value) || value < 0 || value > (entry.first == "decal_tolerance_mm" ? 10 : 1)) invalid = true;
            else setTuning(entry.first, value, false);
            continue;
        }
        bool isFalse = false;
        if (!validKey(entry.first) || !parseFalse(entry.second, isFalse)) {
            invalid = true;
            continue;
        }
        if (isFalse) parsed.insert(entry.first);
    }
    if (invalid) {
        logSettingsMessage("Ignoring invalid glTF extension values in settings.ini.", true);
    }
    m_savedTuning = m_tuning;
    m_saved = parsed;
    if (m_effective != parsed) ++m_revision;
    m_effective = parsed;
}

void GltfExtensionSettings::save()
{
    SettingsFile& store = SettingsFile::instance();
    if (!store.persistenceEnabled()) return; // --isolated: kept in memory for the session.
    store.load();
    std::vector<SettingsEntry> entries;
    for (const std::string& key : m_saved) entries.emplace_back(key, "0"); // only the switched-off names
    for (const auto& [key, value] : m_savedTuning) {
        const float def = key == "decal_tolerance_mm" ? 1.2f : 0.5f;
        if (value != def) { std::ostringstream out; out.precision(9); out << value; entries.emplace_back(key, out.str()); }
    }
    store.replaceSection(kSection, std::move(entries));
    store.save();
}

nlohmann::json GltfExtensionSettings::stateJson() const
{
    nlohmann::json vendors = nlohmann::json::object();
    nlohmann::json extensions = nlohmann::json::object();
    for (const GltfExtensionGroup& group : groups()) {
        int applied = 0;
        for (const GltfExtensionEntry& entry : group.extensions) {
            const bool on = applies(entry.name);
            extensions[entry.name] = on;
            applied += on ? 1 : 0;
        }
        vendors[group.vendor] = {{"title", group.title},
                                 {"enabled", enabled(group.vendor)},
                                 {"key", group.masterKey},
                                 {"applied", applied},
                                 {"total", group.extensions.size()}};
    }
    return {{"vendors", vendors},
            {"extensions", extensions},
            {"disabled", std::vector<std::string>(m_effective.begin(), m_effective.end())},
            {"saved", std::vector<std::string>(m_saved.begin(), m_saved.end())},
            {"tuning", m_tuning},
            {"savedTuning", m_savedTuning},
            {"skyPortalStrength", 0.35f},
            {"differsFromSaved", m_effective != m_saved || m_tuning != m_savedTuning},
            {"persistence", SettingsFile::instance().persistenceEnabled()}};
}

void GltfExtensionSettings::drawSettings(SceneRevisions& revisions)
{
    ImGui::TextWrapped("Which glTF extensions Glider applies when it renders. These are app settings: every viewport and "
                       "every model uses them, and a switch you turn off stays off after a restart. Switching an extension "
                       "off only changes the display; the file is not touched.");
    ImGui::Spacing();

    const bool differs = m_effective != m_saved || m_tuning != m_savedTuning;
    const bool anyOff = !m_effective.empty() || !m_saved.empty() || tuning("interior_sky_reflection") != 0.5f ||
                        tuning("decal_tolerance_mm") != 1.2f || m_savedTuning.at("interior_sky_reflection") != 0.5f ||
                        m_savedTuning.at("decal_tolerance_mm") != 1.2f;
    ImGui::BeginDisabled(!anyOff);
    if (ImGui::Button("Reset to defaults")) {
        resetToDefaults();
        revisions.mark(SceneChange::ViewportSettings);
    }
    ImGui::EndDisabled();
    UiProbe::item("settings.ext.reset", 0);
    if (differs) {
        ImGui::SameLine();
        if (ImGui::Button("Restore saved settings")) {
            discardSessionChanges();
            revisions.mark(SceneChange::ViewportSettings);
        }
        UiProbe::item("settings.ext.restore", 0);
        ImGui::TextDisabled("This run differs from the saved settings (a command-line flag or a script). A change made here "
                            "saves only the switch or tuning value you change.");
    }
    if (!SettingsFile::instance().persistenceEnabled()) {
        ImGui::TextDisabled("Settings are not saved in this session (--isolated).");
    }
    ImGui::Spacing();
    int groupIndex = 0;
    for (const GltfExtensionGroup& group : groups()) {
        ImGui::PushID(group.vendor.c_str());
        ImGui::SeparatorText(group.title.c_str());
        ImGui::TextDisabled("%s", group.description.c_str());

        bool groupOn = enabled(group.vendor);
        const std::string masterLabel = group.masterLabel + "##gltfext_group_" + group.vendor;
        if (ImGui::Checkbox(masterLabel.c_str(), &groupOn)) {
            setEnabled(group.vendor, groupOn, true);
            revisions.mark(SceneChange::ViewportSettings);
        }
        UiProbe::item("settings.ext.group", groupIndex);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Master switch of the %s group. The individual switches below keep their state.", group.vendor.c_str());
        }

        ImGui::BeginDisabled(!groupOn);
        ImGui::Indent();
        if (group.vendor == "ASOBO") {
            ImGui::PushID("vendor_tuning");
            float reflection = tuning("interior_sky_reflection");
            if (ImGui::SliderFloat("Interior sky reflection", &reflection, 0, 1, "%.2f")) {
                setTuning("interior_sky_reflection", reflection, true); revisions.mark(SceneChange::RenderSettings);
            }
            UiProbe::item("settings.ext.interior_sky_reflection", 0);
            float tolerance = tuning("decal_tolerance_mm");
            if (ImGui::DragFloat("Decal depth tolerance (mm)", &tolerance, 0.05f, 0, 10, "%.2f", ImGuiSliderFlags_AlwaysClamp)) {
                setTuning("decal_tolerance_mm", tolerance, true); revisions.mark(SceneChange::RenderSettings);
            }
            UiProbe::item("settings.ext.decal_tolerance", 0);
            ImGui::TextDisabled("Sky portals: implicit, strength 0.35 (disable ASOBO_sky_portal below).");
            ImGui::PopID();
        }
        for (const GltfExtensionEntry& entry : group.extensions) {
            bool on = enabled(entry.name);
            const std::string label = entry.label + "##gltfext_" + entry.name;
            if (ImGui::Checkbox(label.c_str(), &on)) {
                setEnabled(entry.name, on, true);
                revisions.mark(SceneChange::ViewportSettings);
            }
            UiProbe::item(("settings.ext." + entry.name).c_str(), 0);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("%s%s%s", entry.name.c_str(), entry.note.empty() ? "" : ": ", entry.note.c_str());
            }
        }
        ImGui::Unindent();
        ImGui::EndDisabled();
        ImGui::PopID();
        ++groupIndex;
    }

}

} // namespace ux3d::imgui
