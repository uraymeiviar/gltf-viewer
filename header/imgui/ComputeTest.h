// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "imgui/SelfCheckResult.h"

namespace ux3d::imgui {

// --compute-selfcheck: on a 4.3+ context dispatches a #version 430 compute shader that writes a known
// pattern into an r32ui image with imageStore and verifies the read-back. Below 4.3 the report says
// "skipped" and the check succeeds (compute features are not available there).
SelfCheckResult runComputeSelfCheck(bool forceFailure);

} // namespace ux3d::imgui
