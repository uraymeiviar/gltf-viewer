// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/GltfExtensionSettings.h"
#include "imgui/ExtensionInspector.h"
#include "imgui/SettingsFile.h"
#include "imgui/SettingsStorage.h"
#include "imgui/UiProbe.h"
#include "gltf/extensions/AsoboExtensions.h"
#include "gltf/extensions/AsoboSupport.h"
#include "gltf/Extensions.h"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <tuple>

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

std::string lowerAscii(std::string text)
{
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

GltfExtensionEntry entryOf(const ux3d::gltf::ExtensionSupport& info)
{
    GltfExtensionEntry entry;
    entry.name = info.name;
    entry.label = ExtensionInspector::formatExtensionLabel(entry.name);
    entry.note = info.description;
    entry.status = info.status;
    entry.offMeans = info.offMeans;
    entry.renderTime = info.renderTime;
    return entry;
}

std::vector<GltfExtensionGroup> buildGroups()
{
    std::vector<GltfExtensionGroup> groups;

    GltfExtensionGroup msfs;
    msfs.vendor = "ASOBO";
    msfs.title = "MSFS (ASOBO)";
    msfs.masterLabel = "Apply MSFS (ASOBO) extensions";
    msfs.masterKey = "asobo";
    msfs.description = "Microsoft Flight Simulator extensions. Off renders the file as if it had no ASOBO extensions.";
    // The rendering switches first, in the order of the A0.2 support table (the switches this tab always had), then
    // the ASOBO extensions read at load (metadata). ASOBO_asset_optimized (how the file stores its data) has none.
    for (const ux3d::gltf::ExtensionSupport& info : ux3d::gltf::supportedExtensions()) {
        if (info.vendor == "ASOBO" && info.switchable && info.renderTime) msfs.extensions.push_back(entryOf(info));
    }
    for (const ux3d::gltf::ExtensionSupport& info : ux3d::gltf::supportedExtensions()) {
        if (info.vendor == "ASOBO" && info.switchable && !info.renderTime) msfs.extensions.push_back(entryOf(info));
    }
    groups.push_back(std::move(msfs));

    // Every other vendor of the support table, Khronos first, then EXT, MSFT and any other vendor in table order.
    const auto titleOf = [](const std::string& vendor) -> std::pair<std::string, std::string> {
        if (vendor == "KHR") return {"Khronos (KHR)", "Khronos ratified and draft extensions."};
        if (vendor == "EXT") return {"Multi-vendor (EXT)", "Extensions shared by several vendors."};
        if (vendor == "MSFT") return {"Microsoft (MSFT)", "Microsoft extensions."};
        if (vendor == "ux3d") return {"UX3D (ux3d)", "UX3D Gestaltor's own extension."};
        return {vendor, vendor + " extensions."};
    };
    std::vector<std::string> vendors = {"KHR", "EXT", "MSFT"};
    for (const ux3d::gltf::ExtensionSupport& info : ux3d::gltf::supportedExtensions()) {
        if (info.vendor != "ASOBO" && std::find(vendors.begin(), vendors.end(), info.vendor) == vendors.end()) {
            vendors.push_back(info.vendor);
        }
    }
    for (const std::string& vendor : vendors) {
        GltfExtensionGroup group;
        group.vendor = vendor;
        std::tie(group.title, group.description) = titleOf(vendor);
        group.description += " Applied when the file is read: changing a switch reloads the open file.";
        for (const ux3d::gltf::ExtensionSupport& info : ux3d::gltf::supportedExtensions()) {
            if (info.vendor == vendor && info.switchable) group.extensions.push_back(entryOf(info));
        }
        if (!group.extensions.empty()) groups.push_back(std::move(group));
    }
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

bool GltfExtensionSettings::isOn(const GltfExtensionEntry& entry) const
{
    return entry.renderTime ? applies(entry.name) : enabled(entry.name);
}

std::unordered_set<std::string> GltfExtensionSettings::loaderDisabled() const
{
    std::unordered_set<std::string> names;
    for (const ux3d::gltf::ExtensionSupport& info : ux3d::gltf::supportedExtensions()) {
        if (info.switchable && !info.renderTime && !enabled(info.name)) names.insert(info.name);
    }
    return names;
}

std::vector<std::string> GltfExtensionSettings::switchedOff() const
{
    std::vector<std::string> names;
    for (const ux3d::gltf::ExtensionSupport& info : ux3d::gltf::supportedExtensions()) {
        if (info.switchable && !(info.renderTime ? applies(info.name) : enabled(info.name))) names.push_back(info.name);
    }
    std::sort(names.begin(), names.end());
    return names;
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
    nlohmann::json appliedAtLoad = nlohmann::json::array();
    for (const GltfExtensionGroup& group : groups()) {
        int applied = 0;
        for (const GltfExtensionEntry& entry : group.extensions) {
            const bool on = isOn(entry);
            extensions[entry.name] = on;
            applied += on ? 1 : 0;
            if (!entry.renderTime) appliedAtLoad.push_back(entry.name);
        }
        vendors[group.vendor] = {{"title", group.title},
                                 {"enabled", enabled(group.vendor)},
                                 {"key", group.masterKey.empty() ? nlohmann::json(nullptr) : nlohmann::json(group.masterKey)},
                                 {"applied", applied},
                                 {"total", group.extensions.size()}};
    }
    std::vector<std::string> loader;
    for (const std::string& name : loaderDisabled()) loader.push_back(name);
    std::sort(loader.begin(), loader.end());
    return {{"vendors", vendors},
            {"extensions", extensions},
            {"appliedAtLoad", appliedAtLoad},
            {"loaderDisabled", loader},
            {"switchedOff", switchedOff()},
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
    ImGui::TextWrapped("Which glTF extensions Glider reads and renders. These are app settings: every viewport and "
                       "every model uses them, and a switch you turn off stays off after a restart. Off means Glider "
                       "behaves as a viewer without that extension: the file is read as if it were absent (core "
                       "fallbacks), and a file that requires it is not loaded. The MSFS rendering switches apply at once; "
                       "every other switch is applied when the file is read, so changing one reloads the open file "
                       "(framed again, as File > Open does). The file itself is never changed.");
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
    char filter[128] = {};
    std::snprintf(filter, sizeof(filter), "%s", m_filter.c_str());
    ImGui::SetNextItemWidth(std::min(360.0f, ImGui::GetContentRegionAvail().x));
    if (ImGui::InputTextWithHint("##gltfext_filter", "Filter (name, description or support)", filter, sizeof(filter))) {
        m_filter = filter;
    }
    UiProbe::item("settings.ext.filter", 0);
    const std::string needle = lowerAscii(m_filter);
    const auto matches = [&needle](const GltfExtensionGroup& group, const GltfExtensionEntry& entry) {
        if (needle.empty()) return true;
        for (const std::string* text : {&entry.name, &entry.label, &entry.note, &entry.status, &group.title}) {
            if (lowerAscii(*text).find(needle) != std::string::npos) return true;
        }
        return false;
    };
    ImGui::Spacing();

    // One table per group: switch (name), support, what Glider does with it. The tooltip says what off means.
    const auto drawRows = [&](const GltfExtensionGroup& group, bool renderTime) {
        bool any = false;
        for (const GltfExtensionEntry& entry : group.extensions) {
            any |= entry.renderTime == renderTime && matches(group, entry);
        }
        if (!any) return;
        const std::string tableId = std::string("##gltfext_table_") + group.vendor + (renderTime ? "_render" : "_load");
        if (!ImGui::BeginTable(tableId.c_str(), 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg |
                                                       ImGuiTableFlags_NoSavedSettings)) return;
        ImGui::TableSetupColumn("Extension", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Support", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
        for (const GltfExtensionEntry& entry : group.extensions) {
            if (entry.renderTime != renderTime || !matches(group, entry)) continue;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            bool on = enabled(entry.name);
            const std::string label = entry.label + "##gltfext_" + entry.name;
            if (ImGui::Checkbox(label.c_str(), &on)) {
                setEnabled(entry.name, on, true);
                revisions.mark(SceneChange::ViewportSettings);
            }
            UiProbe::item(("settings.ext." + entry.name).c_str(), 0);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f));
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(entry.name.c_str());
                ImGui::Separator();
                if (!entry.note.empty()) ImGui::TextWrapped("%s", entry.note.c_str());
                ImGui::TextWrapped("Off: %s", entry.offMeans.c_str());
                ImGui::EndTooltip();
            }
            ImGui::TableNextColumn();
            if (entry.status == "Rendered") ImGui::TextUnformatted(entry.status.c_str());
            else ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.35f, 1.0f), "%s", entry.status.c_str());
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", entry.note.c_str());
        }
        ImGui::EndTable();
    };

    int groupIndex = 0;
    for (const GltfExtensionGroup& group : groups()) {
        bool groupMatches = false;
        for (const GltfExtensionEntry& entry : group.extensions) groupMatches |= matches(group, entry);
        if (!groupMatches) {
            ++groupIndex;
            continue;
        }
        ImGui::PushID(group.vendor.c_str());
        if (!needle.empty()) ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        const std::string header = group.title + "##gltfext_header_" + group.vendor;
        const bool open = ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
        UiProbe::item("settings.ext.header", groupIndex);
        if (!open) {
            ImGui::PopID();
            ++groupIndex;
            continue;
        }
        ImGui::TextDisabled("%s", group.description.c_str());

        bool allOn = group.masterKey.empty() || enabled(group.vendor);
        for (const GltfExtensionEntry& entry : group.extensions) allOn &= enabled(entry.name);
        bool groupOn = true;
        if (!group.masterKey.empty()) {
            groupOn = enabled(group.vendor);
            const std::string masterLabel = group.masterLabel + "##gltfext_group_" + group.vendor;
            if (ImGui::Checkbox(masterLabel.c_str(), &groupOn)) {
                setEnabled(group.vendor, groupOn, true);
                revisions.mark(SceneChange::ViewportSettings);
            }
            UiProbe::item("settings.ext.group", groupIndex);
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Master switch of the %s rendering extensions. The individual switches below keep "
                                  "their state; the extensions read at load are not affected.", group.vendor.c_str());
            }
            ImGui::SameLine();
        }
        ImGui::BeginDisabled(allOn);
        if (ImGui::SmallButton(("Enable all##gltfext_enable_all_" + group.vendor).c_str())) {
            if (!group.masterKey.empty()) setEnabled(group.vendor, true, true);
            for (const GltfExtensionEntry& entry : group.extensions) setEnabled(entry.name, true, true);
            revisions.mark(SceneChange::ViewportSettings);
        }
        ImGui::EndDisabled();
        UiProbe::item("settings.ext.enable_all", groupIndex);

        ImGui::BeginDisabled(!groupOn);
        ImGui::Indent();
        if (group.vendor == "ASOBO" && needle.empty()) { // the tuning is not an extension: hidden while filtering
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
        drawRows(group, true);
        ImGui::Unindent();
        ImGui::EndDisabled();
        bool loadRows = false;
        for (const GltfExtensionEntry& entry : group.extensions) loadRows |= !entry.renderTime && matches(group, entry);
        if (loadRows) {
            ImGui::Indent();
            if (group.vendor == "ASOBO") {
                ImGui::TextDisabled("Read at load (not affected by the master switch; changing one reloads the open file):");
            }
            drawRows(group, false);
            ImGui::Unindent();
        }
        ImGui::Spacing();
        ImGui::PopID();
        ++groupIndex;
    }
}

} // namespace ux3d::imgui
