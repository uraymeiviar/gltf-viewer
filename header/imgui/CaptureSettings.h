// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <chrono>
#include <filesystem>
#include <string>

namespace ux3d::imgui {

// Where viewport captures (Viewport -> Capture Viewport / Save Viewport View...,
// and the save_viewport_image command) are written, and how their files are named.
// The folder is the only persisted value: [Captures] folder= in settings.ini, stored
// only when it is set. Unset means the default, <Pictures>\Glider. With persistence
// disabled (--isolated) it is never read or written; edits stay in memory.
class CaptureSettings {
public:
    // <Pictures>\Glider (FOLDERID_Pictures), not created here.
    static std::string defaultFolder();
    // The saved choice, or empty when unset.
    static std::string configuredFolder();
    // configuredFolder() if set, else defaultFolder().
    static std::string effectiveFolder();
    // Empty or the default folder clears the setting. Saves unless persistence is off.
    static void setFolder(const std::string& folder);

    // Glider_<model stem>_<yyyyMMdd-HHmmss>.png; the stem is made file-name safe,
    // empty becomes "untitled".
    static std::string proposedFileName(const std::string& modelPath, std::chrono::system_clock::time_point now);
    // folder/fileName, or the same name with _2, _3, ... before the extension while it exists.
    static std::filesystem::path uniquePath(const std::filesystem::path& folder, const std::string& fileName);

    // Settings window "Captures" tab content.
    static void drawSettings();

private:
    static void ensureLoaded();
};

} // namespace ux3d::imgui
