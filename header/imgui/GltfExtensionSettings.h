// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "imgui/SceneRevisions.h"
#include "renderer/gltf/GltfRenderLayer.h"

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <set>
#include <map>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace ux3d::imgui {

// One extension with a switch in File > Settings > glTF Extensions (a row of gltf::supportedExtensions()).
struct GltfExtensionEntry {
    std::string name;     // the glTF extension name, e.g. "ASOBO_material_detail_map"; also the registry key
    std::string label;    // display name
    std::string note;     // what Glider does with it (tooltip)
    std::string status;   // "Rendered", "Partial", "Read", "Metadata", "Not applicable"
    std::string offMeans; // what switching it off does (tooltip)
    // Applied by the renderer (the ASOBO viewport switches, also off with the group's master switch); every other
    // switch is applied when the file is read, so changing it reloads the open file.
    bool renderTime = false;
};

// The extensions of one vendor (the name prefix before the first '_'), listed under one collapsible heading. The
// groups are built from gltf::supportedExtensions(), the one support table: a supported extension gets its switch,
// registry key and get_state entry from there. Only the ASOBO group has a master switch.
struct GltfExtensionGroup {
    std::string vendor;      // "ASOBO", "KHR", "EXT", "MSFT", "ux3d"
    std::string title;       // "MSFS (ASOBO)"
    std::string masterLabel; // "Apply MSFS (ASOBO) extensions" (empty: no master switch)
    std::string masterKey;   // the registry key of the master switch ("asobo"; empty: none)
    std::string description; // one line under the heading
    std::vector<GltfExtensionEntry> extensions;
};

// Which glTF extensions Glider applies when it renders: an app setting (docs/SETTINGS_SCOPES.md), the same for every
// viewport and every model. It is a set of switched-off names: an extension is applied unless its own name or its
// vendor group is in the set. Both kinds of key share one namespace ("ASOBO" is the group, "ASOBO_material_detail_map"
// an extension), so the stored form is keyed by extension name and a new vendor needs no new storage.
//
// Two sets are kept: the effective set (what this run renders with: the saved set, then command-line flags and
// registry or script edits) and the saved set (what settings.ini holds). User edits in the Settings tab change both
// and save; command-line flags and script / pipe edits change only the effective set, so they never reach the file,
// except an edit made while the pipe client has set_persist(true), which commits that one key.
class GltfExtensionSettings {
public:
    static GltfExtensionSettings& instance();
    static const std::vector<GltfExtensionGroup>& groups();
    // The vendor prefix of an extension name ("ASOBO_x" -> "ASOBO", "KHR_materials_ior" -> "KHR"); a key without
    // '_' is already a vendor.
    static std::string vendorOf(std::string_view key);

    // Effective state. A key is a vendor group or an extension name.
    bool enabled(std::string_view key) const;
    // Group and extension: true when the extension is applied.
    bool applies(std::string_view extension) const;
    // The state of one switch: applies() for a render-time (ASOBO) switch, enabled() for one applied at load.
    bool isOn(const GltfExtensionEntry& entry) const;
    // The extensions switched off that are applied at load (gltf::ImportOptions::disabledExtensions).
    std::unordered_set<std::string> loaderDisabled() const;
    // Every supported extension whose switch is off, sorted (the validator's "switched off" notices).
    std::vector<std::string> switchedOff() const;
    // The viewport mirror (ViewportSettingsState::asobo) of the ASOBO group.
    ux3d::renderer::AsoboOptions asoboOptions() const;
    // Effective set (disabled keys) and saved set.
    const std::set<std::string>& disabled() const { return m_effective; }
    const std::set<std::string>& savedDisabled() const { return m_saved; }
    // Counts up on every change of the effective set.
    uint64_t revision() const { return m_revision; }

    // Edits. persist = true is a user edit (Settings tab, a pipe edit under set_persist): the saved set follows
    // and settings.ini is rewritten (nothing with --isolated).
    void setEnabled(const std::string& key, bool enabled, bool persist);
    // Copies the effective state of one key into the saved set and saves.
    void commit(const std::string& key);
    float tuning(const std::string& key) const;
    void setTuning(const std::string& key, float value, bool persist);
    const std::map<std::string, float>& tuningSnapshot() const { return m_tuning; }
    void restoreTuning(const std::map<std::string, float>& values);

    // Everything applied again, in both sets.
    void resetToDefaults();
    // Back to the saved set (this run's overrides dropped).
    void discardSessionChanges();

    // Reset support: the effective set at startup.
    std::set<std::string> snapshot() const { return m_effective; }
    void restore(const std::set<std::string>& effective);

    // settings.ini [GltfExtensions]: switched-off names (0) plus non-default vendor numeric tuning.
    void load();
    void save();

    nlohmann::json stateJson() const;

    // File > Settings > glTF Extensions. Marks the scene revisions when a switch changes.
    void drawSettings(SceneRevisions& revisions);

private:
    GltfExtensionSettings() = default;

    std::set<std::string> m_effective;
    std::set<std::string> m_saved;
    std::map<std::string, float> m_tuning{{"interior_sky_reflection", 0.5f}, {"decal_tolerance_mm", 1.2f}};
    std::map<std::string, float> m_savedTuning = m_tuning;
    uint64_t m_revision = 0;
    std::string m_filter; // the tab's search filter
};

} // namespace ux3d::imgui
