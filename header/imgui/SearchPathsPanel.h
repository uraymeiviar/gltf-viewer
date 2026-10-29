// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <functional>

namespace ux3d::renderer {
class GltfRenderLayer;
}

namespace ux3d::imgui {
class SceneRevisions;

// The tabs of File > Settings that other panels open directly.
enum class SettingsTab { None, SearchPaths, Quality, GltfExtensions, Captures, AgentControl };

// Asks for the Settings window to open on a tab (a button in a panel, a link). The application opens the window and
// the window selects the tab once.
void requestSettingsWindow(SettingsTab tab);
// True once after requestSettingsWindow; the application then shows the window.
bool takeSettingsWindowRequest();

// Renders the Asset Search Paths configuration dialog
// drawAgentControl, when set, adds the Agent Control tab.
// layer: the render layer whose IBL filter the Quality tab re-filters.
void renderSearchPathsWindow(bool* pOpen, std::function<void()> onReloadModel = nullptr, SceneRevisions* revisions = nullptr,
                             std::function<void()> drawAgentControl = nullptr,
                             ux3d::renderer::GltfRenderLayer* layer = nullptr);

} // namespace ux3d::imgui
