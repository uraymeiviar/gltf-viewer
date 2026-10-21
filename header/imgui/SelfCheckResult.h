// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <string>

namespace ux3d::imgui {

// Structured result for command-line diagnostic checks. Keeping the success
// flag separate from the report text lets main() return a reliable exit code
// without parsing the human-readable report.
struct SelfCheckResult {
    bool success = false;
    std::string report;
};

} // namespace ux3d::imgui
