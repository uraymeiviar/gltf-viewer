// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Maximize Area (Ctrl+Space), Full Screen (Ctrl+Alt+Space, Esc) and drag-and-drop open.
//
// Maximize: the docking layout is snapshot as ImGui ini text (SaveIniSettingsToMemory), the main dockspace is
// rebuilt with the target window as its only docked window, and every other docked panel is not submitted
// (areaShows). Restore loads the snapshot back with LoadIniSettingsFromMemory between two frames (before
// ImGui::NewFrame), which rebuilds the dock nodes with their sizes, tab order and selected tabs; the window that
// had focus is focused again. Neither the maximized layout nor the full-screen window placement is persisted:
// persistedLayoutIni returns the snapshot while maximized, and the exit path leaves full screen first.
//
// Full screen: a borderless window covering the monitor the window is on (GLFW_DECORATED off, position and size
// set to the monitor rectangle), not glfwSetWindowMonitor. The GL context, the swap chain and the ImGui GLFW
// backend stay untouched, there is no display-mode change, and the window does not iconify when it loses focus
// (exclusive full-screen GLFW windows do), so Alt+Tab, other monitors, the harness' no-focus runs and the file
// dialogs keep working. Full screen implies Maximize Area and also hides the menu bar, the status bar and the
// maximized window's tab bar.

#include "Application.h"
#include <imgui_internal.h>
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace ux3d::imgui::application {

namespace {

bool inDockTree(const ImGuiDockNode* node, ImGuiID rootId)
{
    while (node != nullptr && node->ParentNode != nullptr) node = node->ParentNode;
    return node != nullptr && node->ID == rootId;
}

std::string lowerExtension(const std::string& path)
{
    std::string ext;
    try {
        const auto u8 = std::filesystem::u8path(path).extension().u8string();
        ext.assign(u8.begin(), u8.end());
    } catch (...) {
        const size_t dot = path.find_last_of('.');
        ext = dot == std::string::npos ? std::string() : path.substr(dot);
    }
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

std::string hex64Text(uint64_t value)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%016llx", static_cast<unsigned long long>(value));
    return buffer;
}

// The monitor with the largest overlap with the rectangle (the primary monitor if none overlaps).
GLFWmonitor* monitorForRect(int x, int y, int w, int h, int& mx, int& my, int& mw, int& mh)
{
    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    GLFWmonitor* best = glfwGetPrimaryMonitor();
    long long bestArea = -1;
    for (int i = 0; i < count; ++i) {
        const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
        if (mode == nullptr) continue;
        int px = 0, py = 0;
        glfwGetMonitorPos(monitors[i], &px, &py);
        const long long ox = std::max(0, std::min(x + w, px + mode->width) - std::max(x, px));
        const long long oy = std::max(0, std::min(y + h, py + mode->height) - std::max(y, py));
        if (ox * oy > bestArea) {
            bestArea = ox * oy;
            best = monitors[i];
        }
    }
    mx = my = 0;
    mw = mh = 0;
    if (best != nullptr) {
        glfwGetMonitorPos(best, &mx, &my);
        if (const GLFWvidmode* mode = glfwGetVideoMode(best)) {
            mw = mode->width;
            mh = mode->height;
        }
    }
    return best;
}

} // namespace

std::string Application::viewportWindowName(const ux3d::imgui::ViewportInstance& vp)
{
    return std::string(ICON_MDI_CUBE_OUTLINE " ") + vp.name + "###Viewport_" + std::to_string(vp.id);
}

bool Application::areaShows(const char* windowName) const
{
    return !area.maximized || ImHashStr(windowName) == area.target.id;
}

bool Application::areaTargets(const ux3d::imgui::ViewportInstance* vp) const
{
    return area.maximized && vp != nullptr && ImHashStr(viewportWindowName(*vp).c_str()) == area.target.id;
}

uint64_t Application::areaVisibilitySignature() const
{
    // Panels shown or hidden and viewports opened, closed or created while maximized end the maximized state.
    uint64_t value = 14695981039346656037ULL;
    auto mix = [&value](uint64_t v) {
        value ^= v;
        value *= 1099511628211ULL;
    };
    mix(showInspector);
    mix(showHierarchy);
    mix(showResources);
    mix(showLogger);
    mix(showStatistics);
    mix(showValidation);
    mix(viewports.size());
    for (const auto& vp : viewports) {
        mix(vp->id);
        mix(vp->isOpen);
    }
    return value;
}

std::optional<Application::AreaTarget> Application::areaTargetForViewport(const ux3d::imgui::ViewportInstance* vp)
{
    if (vp == nullptr) return std::nullopt;
    const std::string name = viewportWindowName(*vp);
    return AreaTarget{ImHashStr(name.c_str()), name};
}

std::optional<Application::AreaTarget> Application::areaTargetAtMouse()
{
    ImGuiContext& g = *GImGui;
    auto docked = [this](ImGuiWindow* w) -> std::optional<AreaTarget> {
        if (w == nullptr) return std::nullopt;
        w = w->RootWindow;
        if (w == nullptr || w->DockNode == nullptr || !inDockTree(w->DockNode, mainDockspaceId)) return std::nullopt;
        if ((w->Flags & (ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip)) != 0) return std::nullopt;
        return AreaTarget{w->ID, w->Name};
    };
    if (auto target = docked(g.HoveredWindow)) return target;
    // Over a tab bar, a splitter or an empty node: the leaf node under the mouse and its visible window.
    const ImVec2 mouse = g.IO.MousePos;
    ImGuiDockNode* node = ImGui::DockBuilderGetNode(mainDockspaceId);
    if (node != nullptr && ImGui::IsMousePosValid(&mouse) && node->Rect().Contains(mouse)) {
        while (node != nullptr && node->IsSplitNode()) {
            ImGuiDockNode* next = nullptr;
            for (ImGuiDockNode* child : node->ChildNodes) {
                if (child != nullptr && child->IsVisible && child->Rect().Contains(mouse)) next = child;
            }
            node = next;
        }
        if (node != nullptr && node->VisibleWindow != nullptr) {
            if (auto target = docked(node->VisibleWindow)) return target;
        }
    }
    std::string ignored;
    return areaTargetByKey("focused", ignored);
}

std::optional<Application::AreaTarget> Application::areaTargetByKey(const std::string& key, std::string& error)
{
    ImGuiContext& g = *GImGui;
    auto existing = [&](const std::string& name) -> std::optional<AreaTarget> {
        const ImGuiID id = ImHashStr(name.c_str());
        ImGuiWindow* w = ImGui::FindWindowByID(id);
        if (w == nullptr || w->LastFrameActive < g.FrameCount - 1) {
            error = "window not open: " + name;
            return std::nullopt;
        }
        return AreaTarget{id, w->Name};
    };
    if (key.empty() || key == "mouse") return areaTargetAtMouse();
    if (key == "focused") {
        if (area.lastFocused.id != 0) {
            ImGuiWindow* w = ImGui::FindWindowByID(area.lastFocused.id);
            if (w != nullptr && w->LastFrameActive >= g.FrameCount - 1) return area.lastFocused;
        }
        if (auto target = areaTargetForViewport(activeViewport)) return target;
        error = "no focused panel and no active viewport";
        return std::nullopt;
    }
    if (key.rfind("viewport", 0) == 0) {
        ux3d::imgui::ViewportInstance* vp = activeViewport;
        const size_t colon = key.find(':');
        if (colon != std::string::npos) {
            vp = nullptr;
            try {
                vp = findViewportById(static_cast<uint32_t>(std::stoul(key.substr(colon + 1))));
            } catch (...) {
            }
        }
        if (vp == nullptr || !vp->isOpen) {
            error = "no such open viewport: " + key;
            return std::nullopt;
        }
        return existing(viewportWindowName(*vp));
    }
    static const std::pair<const char*, const char*> kPanels[] = {
        {"inspector", WND_INSPECTOR}, {"hierarchy", WND_HIERARCHY},   {"resources", WND_RESOURCES},
        {"logger", WND_LOGGER},       {"statistics", WND_STATISTICS}, {"validation", WND_VALIDATION},
    };
    for (const auto& [name, window] : kPanels) {
        if (key == name) return existing(window);
    }
    return existing(key);
}

void Application::requestArea(AreaAction action, const AreaTarget& target)
{
    areaRequests.emplace_back(action, target);
}

void Application::beginAreaMaximize(const AreaTarget& target)
{
    ImGuiContext& g = *GImGui;
    const float dirtyTimer = g.SettingsDirtyTimer; // a pending layout save still happens
    size_t size = 0;
    const char* data = ImGui::SaveIniSettingsToMemory(&size);
    g.SettingsDirtyTimer = dirtyTimer;
    area.layoutSnapshot.assign(data != nullptr ? data : "", data != nullptr ? size : 0);
    area.focusedName = area.lastFocused.name;
    area.target = target;
    area.maximized = true;
    area.buildPending = true;
    area.visibilitySignature = areaVisibilitySignature();
    logMessage("[Area] Maximized " + target.name.substr(0, target.name.find("###")) + " (Ctrl+Space restores the layout)");
}

void Application::restoreAreaLayout()
{
    if (!area.layoutSnapshot.empty()) {
        ImGui::LoadIniSettingsFromMemory(area.layoutSnapshot.data(), area.layoutSnapshot.size());
    }
    area.refocusName = area.focusedName.empty() ? area.target.name : area.focusedName;
    area.restoreGuardFrames = 2;
    area.maximized = false;
    area.buildPending = false;
    area.layoutSnapshot.clear();
    area.target = {};
    logMessage("[Area] Restored the docking layout");
}

void Application::enterFullscreenWindow()
{
    if (window == nullptr) return;
    int x = 0, y = 0, w = 0, h = 0;
    glfwGetWindowPos(window, &x, &y);
    glfwGetWindowSize(window, &w, &h);
    monitorForRect(x, y, w, h, area.monitorX, area.monitorY, area.monitorW, area.monitorH);
    area.windowWasMaximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED) != 0;
    if (area.windowWasMaximized) {
        // The normal (restored) rectangle is what leaving full screen returns to before maximizing again.
        glfwRestoreWindow(window);
        glfwGetWindowPos(window, &x, &y);
        glfwGetWindowSize(window, &w, &h);
    }
    area.windowX = x;
    area.windowY = y;
    area.windowW = w;
    area.windowH = h;
    if (area.monitorW <= 0 || area.monitorH <= 0) return;
    glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_FALSE);
    glfwSetWindowPos(window, area.monitorX, area.monitorY);
    glfwSetWindowSize(window, area.monitorW, area.monitorH);
    logMessage("[Area] Full screen on the monitor at " + std::to_string(area.monitorX) + "," +
               std::to_string(area.monitorY) + " (" + std::to_string(area.monitorW) + "x" +
               std::to_string(area.monitorH) + "); Esc or Ctrl+Alt+Space returns");
}

void Application::leaveFullscreenWindow()
{
    if (window == nullptr) return;
    glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);
    if (area.windowW > 0 && area.windowH > 0) {
        glfwSetWindowSize(window, area.windowW, area.windowH);
        glfwSetWindowPos(window, area.windowX, area.windowY);
    }
    if (area.windowWasMaximized) glfwMaximizeWindow(window);
    logMessage("[Area] Left full screen");
}

void Application::applyAreaRequests()
{
    if (areaRequests.empty()) return;
    const auto requests = std::exchange(areaRequests, {});
    for (const auto& [action, target] : requests) {
        const bool wasMaximized = area.maximized;
        const bool wasFullscreen = area.fullscreen;
        switch (action) {
        case AreaAction::ToggleMaximize:
            if (area.fullscreen) {
                // Ctrl+Space in full screen: back to the window, the area stays maximized.
                leaveFullscreenWindow();
                area.fullscreen = false;
            } else if (area.maximized) {
                restoreAreaLayout();
            } else if (target.id != 0) {
                beginAreaMaximize(target);
            }
            break;
        case AreaAction::ToggleFullscreen:
            if (area.fullscreen) {
                leaveFullscreenWindow();
                area.fullscreen = false;
                if (!area.fullscreenFromMaximized && area.maximized) restoreAreaLayout();
            } else if (area.maximized || target.id != 0) {
                area.fullscreenFromMaximized = area.maximized;
                if (!area.maximized) beginAreaMaximize(target);
                enterFullscreenWindow();
                area.fullscreen = true;
            }
            break;
        case AreaAction::ExitFullscreen:
            if (area.fullscreen) {
                leaveFullscreenWindow();
                area.fullscreen = false;
                if (!area.fullscreenFromMaximized && area.maximized) restoreAreaLayout();
            }
            break;
        case AreaAction::ExitAll:
            if (area.fullscreen) {
                leaveFullscreenWindow();
                area.fullscreen = false;
            }
            if (area.maximized) restoreAreaLayout();
            break;
        }
        if (wasMaximized != area.maximized || wasFullscreen != area.fullscreen) ++area.transitions;
    }
}

void Application::applyAreaLayout(ImGuiID dockspaceId, ImVec2 size)
{
    if (!area.buildPending) return;
    area.buildPending = false;
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, size);
    ImGui::DockBuilderDockWindow(area.target.name.c_str(), dockspaceId);
    ImGui::DockBuilderFinish(dockspaceId);
    ImGui::SetWindowFocus(area.target.name.c_str());
    area.builtFrame = GImGui->FrameCount;
}

void Application::trackAreaFrame()
{
    ImGuiContext& g = *GImGui;
    if (area.restoreGuardFrames > 0) --area.restoreGuardFrames;
    if (g.NavWindow != nullptr) {
        ImGuiWindow* w = g.NavWindow->RootWindow;
        if (w != nullptr && w->DockNode != nullptr && inDockTree(w->DockNode, mainDockspaceId) &&
            (w->Flags & (ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip)) == 0) {
            area.lastFocused = AreaTarget{w->ID, w->Name};
        }
    }
    if (!area.refocusName.empty()) {
        ImGui::SetWindowFocus(area.refocusName.c_str());
        area.refocusName.clear();
    }
    if (area.maximized && !area.buildPending && areaRequests.empty() && g.FrameCount > area.builtFrame) {
        ImGuiWindow* w = ImGui::FindWindowByID(area.target.id);
        const bool targetClosed = w == nullptr || w->LastFrameActive < g.FrameCount - 1;
        if (targetClosed || areaVisibilitySignature() != area.visibilitySignature) {
            requestArea(AreaAction::ExitAll);
        }
    }
    // A viewport created while maximized waits for the restored layout, so it docks like any new viewport.
    if ((deferredNewViewport || deferredNewViewportCamera != nullptr) && !area.maximized && areaRequests.empty() &&
        area.restoreGuardFrames == 0) {
        pendingNewViewport = deferredNewViewport;
        pendingNewViewportCamera = deferredNewViewportCamera;
        deferredNewViewport = false;
        deferredNewViewportCamera = nullptr;
    }
}

void Application::discardAreaState()
{
    if (area.fullscreen) leaveFullscreenWindow();
    const AreaTarget lastFocused = area.lastFocused;
    const uint64_t transitions = area.transitions + ((area.maximized || area.fullscreen) ? 1 : 0);
    area = AreaState();
    area.lastFocused = lastFocused;
    area.transitions = transitions;
    areaRequests.clear();
}

void Application::leaveAreaForExit()
{
    areaRequests.clear();
    if (area.fullscreen) {
        leaveFullscreenWindow();
        area.fullscreen = false;
    }
    // area.maximized stays: persistedLayoutIni writes the snapshot, never the maximized layout.
}

std::string Application::persistedLayoutIni()
{
    if (area.maximized && !area.layoutSnapshot.empty()) return area.layoutSnapshot;
    ImGuiContext& g = *GImGui;
    const float dirtyTimer = g.SettingsDirtyTimer; // reading the layout never cancels a pending save
    size_t size = 0;
    const char* data = ImGui::SaveIniSettingsToMemory(&size);
    g.SettingsDirtyTimer = dirtyTimer;
    return std::string(data != nullptr ? data : "", data != nullptr ? size : 0);
}

nlohmann::json Application::areaStateJson()
{
    ImGuiContext& g = *GImGui;
    nlohmann::json out;
    out["maximized"] = area.maximized;
    out["fullscreen"] = area.fullscreen;
    out["fullscreenFromMaximized"] = area.fullscreen && area.fullscreenFromMaximized;
    out["target"] = area.maximized ? nlohmann::json(area.target.name) : nlohmann::json(nullptr);
    out["menuBar"] = !area.fullscreen;
    out["statusBar"] = !area.fullscreen;
    out["transitions"] = area.transitions;
    out["pending"] = areaRequestPending();
    if (window != nullptr) {
        int x = 0, y = 0, w = 0, h = 0;
        glfwGetWindowPos(window, &x, &y);
        glfwGetWindowSize(window, &w, &h);
        out["windowRect"] = {x, y, w, h};
        out["decorated"] = glfwGetWindowAttrib(window, GLFW_DECORATED) != 0;
        out["windowMaximized"] = glfwGetWindowAttrib(window, GLFW_MAXIMIZED) != 0;
        int mx = 0, my = 0, mw = 0, mh = 0;
        monitorForRect(x, y, w, h, mx, my, mw, mh);
        out["monitorRect"] = {mx, my, mw, mh};
    }
    if (area.fullscreen) {
        out["restoreRect"] = {area.windowX, area.windowY, area.windowW, area.windowH};
        out["restoreMaximized"] = area.windowWasMaximized;
    }
    const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    out["displaySize"] = {mainViewport->Size.x, mainViewport->Size.y};
    if (const ImGuiDockNode* node = ImGui::DockBuilderGetNode(mainDockspaceId)) {
        out["dockRect"] = {node->Pos.x, node->Pos.y, node->Size.x, node->Size.y};
    }
    if (area.maximized) {
        if (const ImGuiWindow* w = ImGui::FindWindowByID(area.target.id)) {
            out["targetRect"] = {w->Pos.x, w->Pos.y, w->Size.x, w->Size.y};
        }
    }
    nlohmann::json windows = nlohmann::json::array();
    for (const ImGuiWindow* w : g.Windows) {
        if (!w->Active && !w->WasActive) continue;
        if ((w->Flags & (ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_Popup)) != 0) continue;
        // Docked windows carry the child flag; plain child windows are parts of a panel.
        if ((w->Flags & ImGuiWindowFlags_ChildWindow) != 0 && w->DockNode == nullptr) continue;
        const std::string name = w->Name;
        if (name.rfind("##", 0) == 0 || name == "GliderDockSpace" || name.find("##Default") != std::string::npos) continue;
        if (w->DockNode != nullptr && w->DockNode->VisibleWindow != w) continue; // a hidden tab
        windows.push_back({{"name", name}, {"docked", w->DockNode != nullptr},
                           {"rect", {w->Pos.x, w->Pos.y, w->Size.x, w->Size.y}}});
    }
    out["visibleWindows"] = windows;
    const float dirtyTimer = g.SettingsDirtyTimer;
    size_t size = 0;
    const char* data = ImGui::SaveIniSettingsToMemory(&size);
    g.SettingsDirtyTimer = dirtyTimer;
    out["layoutHash"] = hex64Text(hashBytesFnv1a64(reinterpret_cast<const uint8_t*>(data), data != nullptr ? size : 0));
    const std::string persisted = area.maximized && !area.layoutSnapshot.empty() ? area.layoutSnapshot
                                                                                 : std::string(data != nullptr ? data : "", size);
    out["persistedLayoutHash"] = hex64Text(hashBytesFnv1a64(reinterpret_cast<const uint8_t*>(persisted.data()), persisted.size()));
    out["drops"] = lastDropResult;
    return out;
}

// ---------------------------------------------------------------------------------------------
// Drag and drop
// ---------------------------------------------------------------------------------------------

void Application::glfwDropCallback(GLFWwindow* droppedOn, int count, const char** paths)
{
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(droppedOn));
    if (app == nullptr || count <= 0 || paths == nullptr) return;
    std::vector<std::string> batch;
    batch.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        if (paths[i] != nullptr) batch.emplace_back(paths[i]); // UTF-8 from GLFW
    }
    if (!batch.empty()) app->pendingDrops.push_back(std::move(batch));
}

nlohmann::json Application::handleDroppedFiles(const std::vector<std::string>& paths)
{
    std::vector<std::string> models;
    std::vector<std::string> others;
    for (const std::string& path : paths) {
        const std::string ext = lowerExtension(path);
        (ext == ".gltf" || ext == ".glb" ? models : others).push_back(path);
    }
    nlohmann::json out;
    out["serial"] = ++dropSerial;
    out["dropped"] = paths;
    const ImVec4 warning(1.0f, 0.75f, 0.3f, 1.0f);
    if (models.empty()) {
        std::string list;
        for (size_t i = 0; i < others.size(); ++i) list += (i ? ", " : "") + others[i];
        logMessage("[Drop] Not a glTF/GLB file, nothing opened: " + list, warning, ux3d::core::LogLevel::Warning);
        out["opened"] = nullptr;
        out["ignored"] = others;
        lastDropResult = out;
        return out;
    }
    const std::string opened = models.front();
    std::vector<std::string> ignored(models.begin() + 1, models.end());
    ignored.insert(ignored.end(), others.begin(), others.end());
    logMessage("[Drop] Opening " + opened);
    if (!ignored.empty()) {
        std::string list;
        for (size_t i = 0; i < ignored.size(); ++i) list += (i ? ", " : "") + ignored[i];
        logMessage("[Drop] Ignored " + std::to_string(ignored.size()) +
                       " other dropped file(s); only the first glTF/GLB opens: " + list,
                   warning, ux3d::core::LogLevel::Warning);
    }
    out["opened"] = opened;
    out["ignored"] = ignored;
    lastDropResult = out;
    // The File > Open path: inside a frame the load is queued for the next frame (recent files, progress, errors).
    loadGltfModel(opened, gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(),
                  animationPlayer, sceneRevisions, sceneEnvironments);
    updateWindowTitle(window, currentFilePath);
    return out;
}

} // namespace ux3d::imgui::application
