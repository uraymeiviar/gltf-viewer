// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <functional>

namespace ux3d::imgui {
class SceneRevisions;

// Renders the Asset Search Paths configuration dialog
void renderSearchPathsWindow(bool* pOpen, std::function<void()> onReloadModel = nullptr, SceneRevisions* revisions = nullptr);

} // namespace ux3d::imgui
