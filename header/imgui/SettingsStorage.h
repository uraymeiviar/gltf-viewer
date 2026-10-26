// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <filesystem>
#include <string>

namespace ux3d::imgui {

// Resolves the single portable directory where all Glider settings, state,
// and temporary files live.
// Resolution order:
// 1. If GLIDER_DATA_DIR environment variable is set and non-empty (test hook),
//    use that directory. Otherwise, use <exeDir>/GliderData.
// 2. If the candidate directory is writable (or can be created and written to),
//    it is used.
// 3. Fallback: If candidate directory is not writable (e.g. under Program Files),
//    use %APPDATA%/Glider (or %TEMP%/Glider).
// 4. If neither is writable, candidate directory is used in in-memory mode.
// Logs once which directory is selected and whether fallback occurred.
// If ensureCreated is false (e.g. in --isolated mode), does not create directories or test write.
std::filesystem::path getDataDirectory(bool ensureCreated = true);

// Resolves the full path to a named configuration/state file in the data directory.
// If the destination file does not exist yet and ensureCreated is true, performs a
// one-time migration from:
//   1. %APPDATA%/Glider/<filename> (if data directory is not %APPDATA%/Glider)
//   2. %APPDATA%/Gestaltor/<filename>
//   3. (if filename == "settings.ini") <exeDir>/settings.ini
// Never fails if migration copy fails.
std::filesystem::path getDataFilePath(const std::string& filename, bool ensureCreated = true);

// Returns true if the active data directory was verified to be writable.
bool isDataDirectoryWritable();

// Central logger for settings/storage operations.
// Prints to stderr and logs via ux3d::core::log (info or warning).
void logSettingsMessage(const std::string& msg, bool isWarning = false);

// Resets cached directory resolution state (useful for test hooks).
void resetDataDirectoryCache();

} // namespace ux3d::imgui
