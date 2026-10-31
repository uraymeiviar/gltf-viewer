// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Registry commands of Maximize Area, Full Screen and drag-and-drop open (AreaLayout.cpp): toggle_maximize_area,
// toggle_fullscreen and open_dropped (the drop callback's path with given files). get_state reports "area".

#include "CommandsInternal.h"
#include <imgui_internal.h>

namespace ux3d::imgui::application {

namespace {
constexpr int kAreaSettleFrames = 3; // frames after the change before the reply (layout and viewport sizes settled)
}

void Application::registerAgentAreaCommands()
{
    using agent::CommandKind;
    using agent::CommandOutcome;
    using agent::CommandSpec;
    using agent::CommandContext;

    const json targetProp = prop("string", "Area: viewport[:<id>], inspector, hierarchy, resources, logger, statistics, "
                                           "validation, mouse (the panel under the mouse, the default), focused, or an "
                                           "exact window name.");
    // Waits until the request has been applied and the layout settled, then replies with the area state.
    auto settleJob = [this]() {
        auto frames = std::make_shared<int>(0);
        return agent::JobStep([this, frames]() -> std::optional<CommandReply> {
            if (areaRequestPending()) return std::nullopt;
            if (++*frames < kAreaSettleFrames) return std::nullopt;
            return CommandReply::success(areaStateJson());
        });
    };
    {
        CommandSpec spec;
        spec.name = "toggle_maximize_area";
        spec.kind = CommandKind::MultiFrame;
        spec.timeoutSeconds = 30.0;
        spec.help = "Maximize Area (Ctrl+Space): the panel fills the main window (menu and status bar stay), the others "
                    "are hidden; again restores the exact docking layout. state=true/false sets it. Replies with get_state.area.";
        spec.args = objectSchema({{"target", targetProp}, {"state", prop("boolean", "true maximizes, false restores; omitted toggles.")}});
        spec.positional = {"target", "state"};
        spec.run = [this, settleJob](const json& args, const CommandContext&) -> CommandOutcome {
            const bool active = area.maximized || area.fullscreen;
            const bool wanted = args.contains("state") && !args["state"].is_null() ? args["state"].get<bool>() : !active;
            if (wanted == active && !area.fullscreen) return CommandReply::success(areaStateJson());
            if (!wanted) {
                requestArea(AreaAction::ExitAll);
            } else if (area.fullscreen) {
                requestArea(AreaAction::ToggleMaximize); // full screen -> maximized in the window
            } else {
                std::string error;
                const auto target = areaTargetByKey(args.value("target", std::string("mouse")), error);
                if (!target) return CommandReply::failure("target_not_found", error);
                requestArea(AreaAction::ToggleMaximize, *target);
            }
            return settleJob();
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "toggle_fullscreen";
        spec.kind = CommandKind::MultiFrame;
        spec.timeoutSeconds = 30.0;
        spec.help = "Full Screen (Ctrl+Alt+Space): borderless window over its monitor, the area fills it without menu, "
                    "status or tab bar; again (or Esc) restores the window and the layout. state=true/false sets it.";
        spec.args = objectSchema({{"target", targetProp}, {"state", prop("boolean", "true enters, false leaves; omitted toggles.")}});
        spec.positional = {"target", "state"};
        spec.run = [this, settleJob](const json& args, const CommandContext&) -> CommandOutcome {
            const bool wanted = args.contains("state") && !args["state"].is_null() ? args["state"].get<bool>() : !area.fullscreen;
            if (wanted == area.fullscreen) return CommandReply::success(areaStateJson());
            if (!wanted || area.maximized) {
                requestArea(AreaAction::ToggleFullscreen);
            } else {
                std::string error;
                const auto target = areaTargetByKey(args.value("target", std::string("mouse")), error);
                if (!target) return CommandReply::failure("target_not_found", error);
                requestArea(AreaAction::ToggleFullscreen, *target);
            }
            return settleJob();
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "get_layout";
        spec.help = "The docking layout as ImGui ini text: live (what is on screen) and persisted (what settings.ini "
                    "gets: the pre-maximize layout while an area is maximized).";
        spec.run = [this](const json&, const CommandContext&) -> CommandOutcome {
            ImGuiContext& g = *GImGui;
            const float dirtyTimer = g.SettingsDirtyTimer;
            size_t size = 0;
            const char* data = ImGui::SaveIniSettingsToMemory(&size);
            g.SettingsDirtyTimer = dirtyTimer;
            const std::string live(data != nullptr ? data : "", data != nullptr ? size : 0);
            return CommandReply::success({{"live", live}, {"persisted", persistedLayoutIni()}, {"maximized", area.maximized}});
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "open_dropped";
        spec.kind = CommandKind::MultiFrame;
        spec.timeoutSeconds = 600.0;
        spec.help = "Simulate dropping files on the window (the GLFW drop callback): the first .gltf/.glb opens as "
                    "File > Open does, the rest is logged as ignored. Replies when the model has rendered.";
        spec.args = objectSchema({{"paths", prop("array", "Dropped file paths (UTF-8), in drop order (script form: one line, '|' separated).")}}, {"paths"});
        spec.positional = {"paths"};
        spec.restIsText = true;
        spec.run = [this](const json& args, const CommandContext&) -> CommandOutcome {
            std::vector<std::string> paths;
            if (args["paths"].is_array()) {
                for (const auto& p : args["paths"]) paths.push_back(p.is_string() ? p.get<std::string>() : p.dump());
            } else if (args["paths"].is_string()) {
                // Script form: the rest of the line, several paths separated by '|'.
                const std::string text = args["paths"].get<std::string>();
                size_t start = 0;
                while (start <= text.size()) {
                    const size_t bar = text.find('|', start);
                    const std::string part = text.substr(start, bar == std::string::npos ? std::string::npos : bar - start);
                    if (!part.empty()) paths.push_back(part);
                    if (bar == std::string::npos) break;
                    start = bar + 1;
                }
            }
            if (paths.empty()) return CommandReply::failure("invalid_args", "paths is empty");
            pendingDrops.push_back(paths);
            const uint64_t serial = dropSerial;
            const bool noPersist = agentNoPersistScope;
            struct DropJob {
                int frames = 0;
                bool processed = false;
                bool opens = false;
            };
            auto job = std::make_shared<DropJob>();
            return agent::JobStep([this, serial, job, noPersist]() -> std::optional<CommandReply> {
                if (!job->processed) {
                    if (dropSerial == serial) return std::nullopt; // the next frame runs the drop
                    job->processed = true;
                    job->opens = !lastDropResult.value("opened", json()).is_null();
                    if (job->opens) queuedLoadNoPersist = noPersist; // like open_model: no recent files without set_persist
                }
                json out = lastDropResult;
                if (job->opens) {
                    if (!queuedLoadPath.empty() || loadingActive) return std::nullopt;
                    if (++job->frames < kSettleAfterLoadFrames) return std::nullopt;
                    out["loaded"] = modelLoaded;
                    out["currentFile"] = currentFilePath;
                } else {
                    out["loaded"] = false;
                }
                out["state"] = agentStateJson();
                return CommandReply::success(out);
            });
        };
        agentRegistry.add(std::move(spec));
    }
}

} // namespace ux3d::imgui::application
