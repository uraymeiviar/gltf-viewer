// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <string>

namespace ux3d::imgui {

// Central logger for settings/storage operations.
// Prints to stderr and logs via ux3d::core::log (info or warning).
void logSettingsMessage(const std::string& msg, bool isWarning = false);

} // namespace ux3d::imgui
