// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Command registry of the agent control channel: registerAgentCommands (state and session, selection and
// visibility, camera, inspection and capture, settings commands).

#include "CommandsInternal.h"

namespace ux3d::imgui::application {

// ---------------------------------------------------------------------------------------------
// Command registry
// ---------------------------------------------------------------------------------------------

void Application::registerAgentCommands()
{
    if (agentRegistryReady) return;
    agentRegistryReady = true;
    registerAgentSettings();
    using agent::CommandKind;
    using agent::CommandOutcome;
    using agent::CommandSpec;
    using agent::CommandContext;

    // Resolves args[key] to a node (exact, case-insensitive, unique substring or #index).
    auto resolveNode = [this](const json& args, const char* key, std::optional<CommandReply>& failure) -> ux3d::gltf::Node* {
        const std::string spec = args.value(key, std::string());
        std::vector<std::string> candidates;
        ux3d::gltf::Node* node = findNode(spec, &candidates);
        if (node == nullptr) {
            std::string text = "node not found: " + spec;
            if (!candidates.empty()) {
                text += " (candidates: ";
                for (size_t i = 0; i < candidates.size(); ++i) text += (i ? ", " : "") + candidates[i];
                text += ")";
            }
            failure = CommandReply::failure("node_not_found", text, {{"candidates", candidates}});
        }
        return node;
    };
    auto resolveViewport = [this](const json& args, std::optional<CommandReply>& failure) -> ux3d::imgui::ViewportInstance* {
        if (args.contains("viewport") && !args["viewport"].is_null()) {
            auto* vp = findViewportById(static_cast<uint32_t>(args["viewport"].get<long long>()));
            if (vp == nullptr) failure = CommandReply::failure("viewport_not_found", "no viewport " + args["viewport"].dump());
            return vp;
        }
        if (activeViewport == nullptr) failure = CommandReply::failure("no_viewport", "no active viewport");
        return activeViewport;
    };
    auto windowMinimized = [this] {
        int w = 0, h = 0;
        glfwGetFramebufferSize(window, &w, &h);
        return w <= 0 || h <= 0 || glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0;
    };
    auto markViewport = [this](ux3d::imgui::ViewportInstance* vp) {
        vp->antiAliasing.resetHistory();
        sceneRevisions.mark(ux3d::imgui::SceneChange::ViewportSettings);
    };

    // ---- State and session ----
    {
        CommandSpec spec;
        spec.name = "describe";
        spec.help = "List every command with its JSON-schema arguments, kind and script form.";
        spec.run = [this](const json&, const CommandContext&) -> CommandOutcome {
            json out;
            out["app"] = "Glider";
            out["protocol"] = kAgentProtocolVersion;
            out["guideVersion"] = kAgentProtocolVersion;
            out["pipe"] = controlPipe ? json(controlPipe->name()) : json(nullptr);
            out["commands"] = agentRegistry.describe();
            out["settingCount"] = agentRegistry.settings().size();
            return CommandReply::success(out);
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "guide";
        spec.help = "The agent guide (Markdown) with the generated command and settings reference.";
        spec.run = [this](const json&, const CommandContext&) -> CommandOutcome {
            return CommandReply::success({{"text", buildAgentGuide()}});
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "get_state";
        spec.aliases = {"state"};
        spec.help = "Model, counts, selection, isolation, viewports, camera, render mode, validation summary, loading state.";
        spec.run = [this](const json&, const CommandContext&) -> CommandOutcome { return CommandReply::success(agentStateJson()); };
        spec.legacyResult = [this](const json&, const json&) { return legacyStateJson(); };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "list_nodes";
        spec.help = "Nodes with index, name, parent, children, mesh, materials and visibility.";
        spec.args = objectSchema({{"filter", prop("string", "Case-insensitive substring of the node name.")},
                                  {"limit", withDefault(prop("integer", "Maximum number of nodes returned."), 200)}});
        spec.positional = {"filter"};
        spec.run = [this](const json& args, const CommandContext&) -> CommandOutcome {
            const std::string filter = lowerText(args.value("filter", std::string()));
            const long long limit = args.value("limit", 200LL);
            std::map<uint32_t, uint32_t> parents;
            for (const auto& node : gltfModel.getNodes()) {
                for (const auto* child : node.getChildren()) parents[child->m_ID] = node.m_ID;
            }
            json nodes = json::array();
            size_t matched = 0;
            for (const auto& node : gltfModel.getNodes()) {
                if (!filter.empty() && lowerText(nodeName(&node)).find(filter) == std::string::npos) continue;
                ++matched;
                if (static_cast<long long>(nodes.size()) >= limit) continue;
                json entry = {{"index", node.m_ID}, {"name", nodeName(&node)}, {"visible", node.getVisible()},
                              {"children", node.getChildren().size()}};
                const auto parent = parents.find(node.m_ID);
                entry["parent"] = parent != parents.end() ? json(parent->second) : json(nullptr);
                if (const auto* mesh = node.getMesh()) {
                    entry["mesh"] = mesh->hasName() ? std::string(mesh->getName()) : "Mesh " + std::to_string(mesh->m_ID);
                    json materials = json::array();
                    for (const auto& primitive : mesh->getPrimitives()) {
                        const auto* material = primitive.getMaterial();
                        materials.push_back(material ? json(materialName(material)) : json(nullptr));
                    }
                    entry["materials"] = materials;
                }
                if (node.getCamera() != nullptr) entry["camera"] = true;
                nodes.push_back(std::move(entry));
            }
            return CommandReply::success({{"total", gltfModel.getNodes().size()}, {"matched", matched}, {"nodes", nodes}});
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "log_tail";
        spec.help = "Recent logger lines.";
        spec.args = objectSchema({{"lines", withDefault(prop("integer", "Number of lines."), 20)},
                                  {"level", prop("string", "Minimum level: info, warning or error.")}});
        spec.positional = {"lines"};
        spec.run = [this](const json& args, const CommandContext&) -> CommandOutcome {
            const long long lines = std::max(1LL, args.value("lines", 20LL));
            const std::string level = lowerText(args.value("level", std::string("info")));
            const int minimum = level == "error" ? 2 : level == "warning" ? 1 : 0;
            json out = json::array();
            const auto& entries = s_consoleLogs.entries();
            for (auto it = entries.rbegin(); it != entries.rend() && static_cast<long long>(out.size()) < lines; ++it) {
                const int severity = (it->level == ux3d::core::LogLevel::Error || it->level == ux3d::core::LogLevel::Fatal) ? 2
                                     : it->level == ux3d::core::LogLevel::Warning ? 1 : 0;
                if (severity < minimum) continue;
                out.insert(out.begin(), json{{"level", severity == 2 ? "error" : severity == 1 ? "warning" : "info"}, {"text", it->text}});
            }
            return CommandReply::success({{"lines", out}});
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "set_persist";
        spec.help = "Allow this pipe client's setting changes to be saved to settings.ini (default off).";
        spec.args = objectSchema({{"enabled", prop("boolean", "true allows saving.")}}, {"enabled"});
        spec.positional = {"enabled"};
        spec.run = [this](const json& args, const CommandContext&) -> CommandOutcome {
            agentPersist = args["enabled"].get<bool>();
            return CommandReply::success({{"persist", agentPersist}});
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "quit";
        spec.help = "Close Glider.";
        spec.run = [this](const json&, const CommandContext&) -> CommandOutcome {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            return CommandReply::success({{"quitting", true}});
        };
        spec.legacyResult = [](const json&, const json&) { return json("quitting"); };
        agentRegistry.add(std::move(spec));
    }

    // ---- Selection and visibility ----
    {
        CommandSpec spec;
        spec.name = "select";
        spec.help = "Select a node, like clicking it in the hierarchy.";
        spec.args = objectSchema({{"node", prop("string", "Node name (exact, case-insensitive, unique substring) or #index.")}}, {"node"});
        spec.positional = {"node"};
        spec.run = [this, resolveNode](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* node = resolveNode(args, "node", failure);
            if (failure) return *failure;
            selectNodeAction(node);
            sceneRevisions.mark(ux3d::imgui::SceneChange::Selection);
            return CommandReply::success({{"node", nodeRef(node)}});
        };
        spec.legacyResult = [](const json& args, const json&) { return args["node"]; };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "clear_selection";
        spec.help = "Clear the selection.";
        spec.run = [this](const json&, const CommandContext&) -> CommandOutcome {
            selection.type = ux3d::imgui::SelectionType::None;
            selection.selectedNode = nullptr;
            ++selection.revision;
            selection.returnMaterial = nullptr;
            sceneRevisions.mark(ux3d::imgui::SceneChange::Selection);
            return CommandReply::success({{"selection", "none"}});
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "isolate";
        spec.help = "Isolate a node (Object > Isolate): only it, optionally with its children, is drawn. Selects it.";
        spec.args = objectSchema({{"node", prop("string", "Node name or #index.")},
                                  {"with_children", withDefault(prop("boolean", "Also draw its descendants."), false)}},
                                 {"node"});
        spec.positional = {"node", "with_children"};
        spec.parseLegacy = [](const std::vector<std::string>& tokens, const std::string& line, std::string& error) -> std::optional<json> {
            if (tokens.size() < 2) {
                error = "missing arguments: " + line;
                return std::nullopt;
            }
            const bool children = tokens.size() > 2 && (tokens[2] == "children" || tokens[2] == "childs" ||
                                                         tokens[2] == "with_children" || tokens[2] == "1" || tokens[2] == "true");
            return json{{"node", tokens[1]}, {"with_children", children}};
        };
        spec.run = [this, resolveNode](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* node = resolveNode(args, "node", failure);
            if (failure) return *failure;
            if (!gltfLayer) return CommandReply::failure("no_scene", "no render layer");
            const bool children = args.value("with_children", false);
            isolateNodeAction(node, children);
            selectNodeAction(node);
            return CommandReply::success({{"node", nodeRef(node)}, {"withChildren", children}});
        };
        spec.legacyResult = [](const json& args, const json&) { return args["node"]; };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "exit_isolate";
        spec.aliases = {"exitisolate"};
        spec.help = "Leave isolation; the whole scene is drawn again.";
        spec.run = [this](const json&, const CommandContext&) -> CommandOutcome {
            if (!gltfLayer) return CommandReply::failure("no_scene", "exit isolate failed");
            gltfLayer->setIsolation(nullptr, false);
            sceneRevisions.mark(ux3d::imgui::SceneChange::Visibility);
            return CommandReply::success({{"isolation", nullptr}});
        };
        spec.legacyResult = [](const json&, const json&) { return json("none"); };
        agentRegistry.add(std::move(spec));
    }

    // ---- Camera ----
    {
        CommandSpec spec;
        spec.name = "frame_node";
        spec.aliases = {"frame"};
        spec.help = "Frame a node like F, optionally from a preset direction; selects it. Without node: frame the whole scene.";
        spec.args = objectSchema({{"node", prop("string", "Node name or #index (omit for the whole scene).")},
                                  {"view", viewProp()},
                                  {"margin", withDefault(prop("number", "Distance multiplier (2 = twice as far)."), 1.0)}});
        spec.positional = {"node", "view", "margin"};
        spec.parseLegacy = [](const std::vector<std::string>& tokens, const std::string& line, std::string& error) -> std::optional<json> {
            if (tokens.size() < 2) {
                error = "missing arguments: " + line;
                return std::nullopt;
            }
            json args = {{"node", tokens[1]}};
            // Phase 0 ignored an unknown view and an unparsable margin.
            if (tokens.size() > 2 && parseCameraOrientation(tokens[2]).has_value()) args["view"] = tokens[2];
            if (tokens.size() > 3) {
                try { args["margin"] = std::stof(tokens[3]); } catch (...) {}
            }
            return args;
        };
        spec.run = [this, resolveNode](const json& args, const CommandContext&) -> CommandOutcome {
            if (activeViewport == nullptr) return CommandReply::failure("no_viewport", "no active viewport");
            std::optional<ux3d::editor::CameraOrientation> view;
            if (args.contains("view") && !args["view"].is_null()) {
                view = parseCameraOrientation(args["view"].get<std::string>());
                if (!view) return CommandReply::failure("unknown_view", "unknown view: " + args["view"].get<std::string>());
            }
            const float margin = static_cast<float>(args.value("margin", 1.0));
            if (!args.contains("node") || args["node"].is_null() || args["node"].get<std::string>().empty()) {
                if (activeScene == nullptr) return CommandReply::failure("no_scene", "no scene loaded");
                if (view) activeViewport->cameraActor.orientUserCamera(*view);
                activeViewport->cameraActor.fitCameraToScene(*activeScene, &gltfModel);
                activeViewport->antiAliasing.resetHistory();
                sceneRevisions.mark(ux3d::imgui::SceneChange::ViewCamera);
                return CommandReply::success({{"node", nullptr}, {"camera", getCameraJson(activeViewport)}});
            }
            std::optional<CommandReply> failure;
            auto* node = resolveNode(args, "node", failure);
            if (failure) return *failure;
            selectNodeAction(node);
            sceneRevisions.mark(ux3d::imgui::SceneChange::Selection);
            frameNodeAction(node, view, margin);
            return CommandReply::success({{"node", nodeRef(node)}, {"camera", getCameraJson(activeViewport)}});
        };
        spec.legacyResult = [](const json&, const json& result) { return result["camera"]; };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "set_view";
        spec.aliases = {"view"};
        spec.help = "Orientation preset around the current target.";
        spec.args = objectSchema({{"view", viewProp()}}, {"view"});
        spec.positional = {"view"};
        spec.run = [this](const json& args, const CommandContext&) -> CommandOutcome {
            if (activeViewport == nullptr) return CommandReply::failure("no_viewport", "no active viewport");
            const std::string name = args["view"].get<std::string>();
            const auto view = parseCameraOrientation(name);
            if (!view) return CommandReply::failure("unknown_view", "unknown view: " + name);
            activeViewport->cameraActor.orientUserCamera(*view);
            activeViewport->antiAliasing.resetHistory();
            sceneRevisions.mark(ux3d::imgui::SceneChange::ViewCamera);
            return CommandReply::success({{"camera", getCameraJson(activeViewport)}});
        };
        spec.legacyResult = [](const json& args, const json&) { return args["view"]; };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "get_camera";
        spec.help = "Exact camera: eye, target, up, fovY, near, far, projection and the CLI flags that reproduce it.";
        spec.args = objectSchema({{"viewport", viewportProp()}});
        spec.run = [this, resolveViewport](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* vp = resolveViewport(args, failure);
            if (failure) return *failure;
            return CommandReply::success(getCameraJson(vp));
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "set_camera";
        spec.aliases = {"camera"};
        spec.help = "Exact camera (world units, glTF Y-up): eye and target, optional vertical FoV in degrees and clip planes.";
        spec.args = objectSchema({{"eye", prop("array", "[x, y, z]")},
                                  {"target", prop("array", "[x, y, z]")},
                                  {"fov_y", prop("number", "Vertical field of view in degrees.")},
                                  {"near", prop("number", "Near clipping plane.")},
                                  {"far", prop("number", "Far clipping plane.")}},
                                 {"eye", "target"});
        spec.positional = {"eye", "target", "fov_y"};
        spec.parseLegacy = [](const std::vector<std::string>& args, const std::string& line, std::string& error) -> std::optional<json> {
            // camera <ex,ey,ez> <tx,ty,tz> [fov] [near,far]   or   camera ex ey ez tx ty tz [fov] [near far]
            json out;
            bool parsed = false;
            if (args.size() >= 3 && args[1].find(',') != std::string::npos && args[2].find(',') != std::string::npos) {
                float ex = 0, ey = 0, ez = 0, tx = 0, ty = 0, tz = 0;
                char c1, c2, c3, c4;
                std::istringstream s1(args[1]);
                std::istringstream s2(args[2]);
                if ((s1 >> ex >> c1 >> ey >> c2 >> ez) && (s2 >> tx >> c3 >> ty >> c4 >> tz)) {
                    out["eye"] = {ex, ey, ez};
                    out["target"] = {tx, ty, tz};
                    if (args.size() > 3) {
                        try { out["fov_y"] = std::stof(args[3]); } catch (...) {}
                    }
                    if (args.size() > 4 && args[4].find(',') != std::string::npos) {
                        float zn = 0, zf = 0;
                        char c5;
                        std::istringstream s4(args[4]);
                        if (s4 >> zn >> c5 >> zf) {
                            out["near"] = zn;
                            out["far"] = zf;
                        }
                    }
                    parsed = true;
                }
            } else if (args.size() >= 7) {
                try {
                    out["eye"] = {std::stof(args[1]), std::stof(args[2]), std::stof(args[3])};
                    out["target"] = {std::stof(args[4]), std::stof(args[5]), std::stof(args[6])};
                    if (args.size() > 7) out["fov_y"] = std::stof(args[7]);
                    if (args.size() > 9) {
                        out["near"] = std::stof(args[8]);
                        out["far"] = std::stof(args[9]);
                    }
                    parsed = true;
                } catch (...) {}
            }
            if (!parsed) {
                error = "invalid camera arguments: " + line;
                return std::nullopt;
            }
            return out;
        };
        spec.run = [this](const json& args, const CommandContext&) -> CommandOutcome {
            if (activeViewport == nullptr) return CommandReply::failure("no_viewport", "no active viewport");
            glm::vec3 eye(0.0f), target(0.0f);
            if (!vec3Arg(args["eye"], eye) || !vec3Arg(args["target"], target)) {
                return CommandReply::failure("bad_arguments", "eye and target must be [x, y, z]");
            }
            activeViewport->cameraActor.setCameraEyeAndTarget(eye, target);
            if (auto* cam = activeViewport->cameraActor.getUserCamera()) {
                const bool hasFov = args.contains("fov_y") && !args["fov_y"].is_null();
                const bool hasNear = args.contains("near") && !args["near"].is_null();
                const bool hasFar = args.contains("far") && !args["far"].is_null();
                const float fovDegrees = hasFov ? static_cast<float>(args["fov_y"].get<double>()) : 0.0f;
                const float fovY = (hasFov && fovDegrees > 0.1f) ? glm::radians(fovDegrees) : cam->getYFoV();
                const float zn = hasNear ? static_cast<float>(args["near"].get<double>()) : cam->getZNearPlane();
                const float zf = hasFar ? static_cast<float>(args["far"].get<double>()) : cam->getZFarPlane();
                if (hasFov || hasNear || hasFar) cam->setPerspective(cam->getAspectRatio(), fovY, zn, zf);
            }
            activeViewport->antiAliasing.resetHistory();
            sceneRevisions.mark(ux3d::imgui::SceneChange::ViewCamera);
            return CommandReply::success(getCameraJson(activeViewport));
        };
        agentRegistry.add(std::move(spec));
    }

    // ---- Inspection and capture ----
    {
        CommandSpec spec;
        spec.name = "capture";
        spec.kind = CommandKind::MultiFrame;
        spec.timeoutSeconds = 120.0;
        spec.help = "Render settle_frames frames, then save the viewport image (PNG) to path or a session temp folder; "
                    "encoding png-base64 also returns the bytes.";
        json encoding = withDefault(prop("string", "path: return the file path; png-base64: also return the PNG bytes."), "path");
        encoding["enum"] = {"path", "png-base64"};
        spec.args = objectSchema({{"path", prop("string", "Output .png (default: %TEMP%/glider-agent/<pid>/capture_<n>.png).")},
                                  {"settle_frames", withDefault(prop("integer", "Frames rendered before reading (like --dump-frame)."), 8)},
                                  {"encoding", encoding},
                                  {"overlays", withDefault(prop("boolean", "false hides grid, scene box and selection box for this capture."), true)},
                                  {"viewport", viewportProp()}});
        spec.positional = {"path", "settle_frames"};
        spec.run = [this, resolveViewport, windowMinimized](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* vp = resolveViewport(args, failure);
            if (failure) return *failure;
            if (windowMinimized()) return CommandReply::failure("window_minimized", "the window is minimised; restore it to capture");
            struct CaptureJob {
                uint32_t viewport = 0;
                std::string path;
                int settle = 8;
                int elapsed = 0;
                bool base64 = false;
                bool restoreOverlays = false;
                bool grid = false, sceneBox = false, selectionBox = false;
            };
            auto job = std::make_shared<CaptureJob>();
            job->viewport = vp->id;
            job->settle = std::max<int>(1, static_cast<int>(args.value("settle_frames", 8LL)));
            job->base64 = args.value("encoding", std::string("path")) == "png-base64";
            if (args.contains("path") && !args["path"].is_null() && !args["path"].get<std::string>().empty()) {
                job->path = args["path"].get<std::string>();
            } else {
                job->path = pathText(agentTempDirectory() / ("capture_" + std::to_string(++agentCaptureSerial) + ".png"));
            }
            if (!args.value("overlays", true)) {
                job->restoreOverlays = true;
                job->grid = vp->settings.showGrid;
                job->sceneBox = vp->settings.showSceneBoundingBox;
                job->selectionBox = vp->settings.showSelectedBoundingBox;
                vp->settings.showGrid = vp->settings.showSceneBoundingBox = vp->settings.showSelectedBoundingBox = false;
                sceneRevisions.mark(ux3d::imgui::SceneChange::ViewportSettings);
            }
            return agent::JobStep([this, job, windowMinimized]() -> std::optional<CommandReply> {
                if (++job->elapsed < job->settle) return std::nullopt;
                auto* target = findViewportById(job->viewport);
                if (target == nullptr) return CommandReply::failure("viewport_not_found", "the viewport was closed");
                auto restore = [&] {
                    if (!job->restoreOverlays) return;
                    target->settings.showGrid = job->grid;
                    target->settings.showSceneBoundingBox = job->sceneBox;
                    target->settings.showSelectedBoundingBox = job->selectionBox;
                    sceneRevisions.mark(ux3d::imgui::SceneChange::ViewportSettings);
                };
                if (windowMinimized()) {
                    restore();
                    return CommandReply::failure("window_minimized", "the window is minimised; restore it to capture");
                }
                GLuint texture = 0;
                int width = 0, height = 0;
                viewportImageSource(target, texture, width, height);
                ux3d::imgui::profiling::CaptureRecord record;
                const bool ok = captureViewportTexture(texture, width, height, job->path, "viewport-texture", frameCount,
                                                       false, 0.0, record);
                restore();
                if (!ok) return CommandReply::failure("capture_failed", "capture failed: " + job->path);
                json out;
                out["path"] = job->path;
                out["width"] = width;
                out["height"] = height;
                out["viewport"] = target->id;
                out["frame"] = frameCount;
                out["rgbaHash"] = hex64(record.rgbaHash);
                out["camera"] = getCameraJson(target);
                if (job->base64) {
                    std::ifstream file(std::filesystem::u8path(job->path), std::ios::binary);
                    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                    out["encoding"] = "png-base64";
                    out["mimeType"] = "image/png";
                    out["data"] = agent::base64Encode(bytes);
                } else {
                    out["encoding"] = "path";
                }
                return CommandReply::success(out);
            });
        };
        spec.legacyResult = [](const json&, const json& result) {
            return json{{"path", result["path"]}, {"camera", result["camera"]}};
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "save_viewport_image";
        spec.help = "Save the viewport's current image (the same pixels as capture and the Viewport menu) as a PNG. "
                    "Without path: the capture folder (File > Settings > Captures, default <Pictures>/Glider) with a unique "
                    "Glider_<model>_<yyyyMMdd-HHmmss>.png name. Never opens a dialog. Returns the path.";
        spec.args = objectSchema({{"path", prop("string", "Output .png (default: unique name in the capture folder).")},
                                  {"viewport", viewportProp()}});
        spec.positional = {"path"};
        spec.run = [this, resolveViewport](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* vp = resolveViewport(args, failure);
            if (failure) return *failure;
            std::string path;
            if (args.contains("path") && !args["path"].is_null()) path = args["path"].get<std::string>();
            ViewportImageResult result;
            std::string error;
            if (!saveViewportImage(vp, path, result, error)) return CommandReply::failure("capture_failed", error);
            json out;
            out["path"] = result.path;
            out["width"] = result.width;
            out["height"] = result.height;
            out["viewport"] = result.viewport;
            out["frame"] = frameCount;
            out["rgbaHash"] = hex64(result.hash);
            return CommandReply::success(out);
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "copy_viewport_image";
        spec.help = "Copy the viewport's current image to the Windows clipboard as a bitmap (CF_DIB) and as PNG. "
                    "The same pixels as capture and the Viewport menu; never opens a dialog.";
        spec.args = objectSchema({{"viewport", viewportProp()}});
        spec.run = [this, resolveViewport](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* vp = resolveViewport(args, failure);
            if (failure) return *failure;
            ViewportImageResult result;
            std::string error;
            if (!copyViewportImage(vp, result, error)) return CommandReply::failure("copy_failed", error);
            json out;
            out["width"] = result.width;
            out["height"] = result.height;
            out["viewport"] = result.viewport;
            out["frame"] = frameCount;
            out["rgbaHash"] = hex64(result.hash);
            out["formats"] = {"CF_DIB", "PNG"};
            return CommandReply::success(out);
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "node_screen_rect";
        spec.aliases = {"noderect"};
        spec.help = "Projected OBB rectangle [x0, y0, x1, y1] in capture pixels with onScreen/clipped/behindCamera.";
        spec.args = objectSchema({{"node", prop("string", "Node name or #index.")}, {"viewport", viewportProp()}}, {"node"});
        spec.positional = {"node"};
        spec.run = [this, resolveNode, resolveViewport](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* node = resolveNode(args, "node", failure);
            if (failure) return *failure;
            auto* vp = resolveViewport(args, failure);
            if (failure) return *failure;
            return CommandReply::success(computeNodeScreenRect(node, vp));
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "pick";
        spec.help = "Node, mesh, primitive and material at a pixel in capture-image coordinates (null: background).";
        spec.args = objectSchema({{"x", prop("integer", "Pixel column (0 = left).")},
                                  {"y", prop("integer", "Pixel row (0 = top).")},
                                  {"viewport", viewportProp()}},
                                 {"x", "y"});
        spec.positional = {"x", "y"};
        spec.parseLegacy = [](const std::vector<std::string>& tokens, const std::string& line, std::string& error) -> std::optional<json> {
            if (tokens.size() < 3) {
                error = "missing arguments: " + line;
                return std::nullopt;
            }
            try {
                return json{{"x", std::stoi(tokens[1])}, {"y", std::stoi(tokens[2])}};
            } catch (...) {
                error = "invalid coordinates: " + line;
                return std::nullopt;
            }
        };
        spec.run = [this, resolveViewport](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* vp = resolveViewport(args, failure);
            if (failure) return *failure;
            GLint previousDraw = 0;
            GLint previousViewport[4] = {0, 0, 0, 0};
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDraw);
            glGetIntegerv(GL_VIEWPORT, previousViewport);
            json result = performPickAtPixel(static_cast<int>(args["x"].get<long long>()), static_cast<int>(args["y"].get<long long>()), vp);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previousDraw));
            glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
            return CommandReply::success(result);
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "probe";
        spec.help = "At a pixel (capture-image coordinates): linear HDR scene colour before tone mapping, final LDR colour, "
                    "depth, world position and the picked node/mesh/primitive/material. 1x1 reads; no rendering change.";
        spec.args = objectSchema({{"x", prop("integer", "Pixel column (0 = left).")},
                                  {"y", prop("integer", "Pixel row (0 = top).")},
                                  {"viewport", viewportProp()}},
                                 {"x", "y"});
        spec.positional = {"x", "y"};
        spec.run = [this, resolveViewport](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* vp = resolveViewport(args, failure);
            if (failure) return *failure;
            const int x = static_cast<int>(args["x"].get<long long>());
            const int y = static_cast<int>(args["y"].get<long long>());
            if (!vp->sceneFbo.isValid() || x < 0 || y < 0 || x >= vp->sceneFbo.getWidth() || y >= vp->sceneFbo.getHeight()) {
                return CommandReply::failure("out_of_bounds", "pixel outside the viewport image",
                                             {{"width", vp->sceneFbo.getWidth()}, {"height", vp->sceneFbo.getHeight()}});
            }
            return CommandReply::success(probePixel(vp, x, y, true));
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "probe_region";
        spec.help = "Min/max/mean linear HDR luminance and RGB over the rectangle [x0, x1) x [y0, y1) (capture-image coordinates).";
        spec.args = objectSchema({{"x0", prop("integer", "Left.")}, {"y0", prop("integer", "Top.")},
                                  {"x1", prop("integer", "Right (exclusive).")}, {"y1", prop("integer", "Bottom (exclusive).")},
                                  {"viewport", viewportProp()}},
                                 {"x0", "y0", "x1", "y1"});
        spec.positional = {"x0", "y0", "x1", "y1"};
        spec.run = [this, resolveViewport](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* vp = resolveViewport(args, failure);
            if (failure) return *failure;
            if (!vp->sceneFbo.isValid()) return CommandReply::failure("no_image", "the viewport has not rendered yet");
            json out = probeRegion(vp, static_cast<int>(args["x0"].get<long long>()), static_cast<int>(args["y0"].get<long long>()),
                                   static_cast<int>(args["x1"].get<long long>()), static_cast<int>(args["y1"].get<long long>()));
            if (out.is_null()) return CommandReply::failure("out_of_bounds", "empty rectangle inside the viewport image");
            return CommandReply::success(out);
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "set_render_mode";
        spec.aliases = {"debugview"};
        spec.help = "Shaded, wireframe or a debug render mode (same names as --debug-view).";
        spec.args = objectSchema({{"mode", prop("string", "e.g. shaded, wireframe, roughness, shadingnormal, ssr.")},
                                  {"viewport", viewportProp()}},
                                 {"mode"});
        spec.positional = {"mode"};
        spec.run = [this, resolveViewport, markViewport](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* vp = resolveViewport(args, failure);
            if (failure) return *failure;
            const std::string mode = args["mode"].get<std::string>();
            std::string error;
            if (!getRenderModeFromName(mode).has_value() ||
                !applySettingValue(SettingScope::Viewport, "render_mode", mode, &vp->settings, nullptr, error)) {
                return CommandReply::failure("unknown_render_mode", "unknown debug view: " + mode);
            }
            markViewport(vp);
            const std::string resolved = vp->settings.renderMode == ux3d::renderer::RenderMode::None && vp->settings.postEffects
                                             ? "shaded_post" : renderModeName(vp->settings.renderMode);
            return CommandReply::success({{"renderMode", resolved}, {"viewport", vp->id}});
        };
        spec.legacyResult = [](const json& args, const json&) { return args["mode"]; };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "open_model";
        spec.aliases = {"open"};
        spec.kind = CommandKind::MultiFrame;
        spec.timeoutSeconds = 600.0;
        spec.help = "Open a glTF/GLB (File > Open); replies when the first model frames have rendered. Environment settings reset with the model.";
        spec.args = objectSchema({{"path", prop("string", "Model path.")},
                                  {"without_textures", withDefault(prop("boolean", "Skip image reads (File > Open without texture)."), false)}},
                                 {"path"});
        spec.positional = {"path"};
        spec.restIsText = true;
        spec.run = [this](const json& args, const CommandContext&) -> CommandOutcome {
            const std::string path = args["path"].get<std::string>();
            std::error_code error;
            if (!std::filesystem::is_regular_file(std::filesystem::u8path(path), error)) {
                return CommandReply::failure("file_not_found", "failed to open: " + path);
            }
            // Inside a frame the load is queued; the next frame runs it (as File > Open does).
            loadGltfModel(path, gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(),
                          animationPlayer, sceneRevisions, sceneEnvironments, false, args.value("without_textures", false));
            queuedLoadNoPersist = agentNoPersistScope;
            struct OpenJob {
                int frames = 0;
                bool loaded = false;
                ux3d::imgui::profiling::Clock::time_point start = ux3d::imgui::profiling::Clock::now();
                double loadMs = 0.0;
            };
            auto job = std::make_shared<OpenJob>();
            return agent::JobStep([this, path, job]() -> std::optional<CommandReply> {
                if (!queuedLoadPath.empty() || loadingActive) return std::nullopt;
                if (!job->loaded) {
                    job->loaded = true;
                    job->loadMs = ux3d::imgui::profiling::elapsedMs(job->start);
                    if (!modelLoaded) return CommandReply::failure("load_failed", "failed to open: " + path);
                }
                if (++job->frames < kSettleAfterLoadFrames) return std::nullopt;
                return CommandReply::success({{"path", path}, {"loaded", true}, {"loadMs", job->loadMs}, {"state", agentStateJson()}});
            });
        };
        spec.legacyResult = [this](const json&, const json& result) {
            return json{{"path", result["path"]}, {"loaded", true}, {"state", legacyStateJson()}};
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "reset";
        spec.kind = CommandKind::MultiFrame;
        spec.timeoutSeconds = 600.0;
        spec.help = "Known state: reload the model (or the default sample), default camera, no selection or isolation, "
                    "viewport settings and environment as at startup, fresh TAA/exposure history.";
        spec.run = [this](const json&, const CommandContext&) -> CommandOutcome {
            const std::string path = currentFilePath.empty() ? resolveSampleModelPath() : currentFilePath;
            // The reload is queued for the next frame; the reset state is applied after it.
            loadGltfModel(path, gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(),
                          animationPlayer, sceneRevisions, sceneEnvironments);
            queuedLoadNoPersist = agentNoPersistScope;
            auto frames = std::make_shared<int>(-1);
            return agent::JobStep([this, path, frames]() -> std::optional<CommandReply> {
                if (!queuedLoadPath.empty() || loadingActive) return std::nullopt;
                if (*frames < 0) {
                    if (!modelLoaded) return CommandReply::failure("load_failed", "failed to reload: " + path);
                    for (auto& env : sceneEnvironments) env = agentStartupEnvironment;
                    // Settings scopes: the AA and effect quality were viewport settings, which reset restored.
                    ux3d::imgui::RenderQuality::settings() = agentStartupQuality;
                    ux3d::imgui::RenderQuality::observeSessionEdit();
                    // The glTF extension switches are an app setting: back to the startup (saved + flags) set.
                    ux3d::imgui::GltfExtensionSettings::instance().restore(agentStartupGltfExtensions);
                    ux3d::imgui::GltfExtensionSettings::instance().restoreTuning(agentStartupVendorTuning);
                    for (const auto& vp : viewports) {
                        if (!vp) continue;
                        vp->settings = agentStartupViewportSettings;
                        if (activeScene != nullptr && !vp->isAttachedToCamera) vp->cameraActor.fitCameraToScene(*activeScene, &gltfModel);
                        vp->antiAliasing.resetHistory();
                    }
                    gltfLayer->setIsolation(nullptr, false);
                    selection.type = ux3d::imgui::SelectionType::None;
                    selection.selectedNode = nullptr;
                    ++selection.revision;
                    sceneRevisions.mark(ux3d::imgui::SceneChange::ViewportSettings);
                    sceneRevisions.mark(ux3d::imgui::SceneChange::RenderSettings);
                    sceneRevisions.mark(ux3d::imgui::SceneChange::ViewCamera);
                    sceneRevisions.mark(ux3d::imgui::SceneChange::Selection);
                    sceneRevisions.mark(ux3d::imgui::SceneChange::Visibility);
                    *frames = 0;
                }
                if (++*frames < kSettleAfterLoadFrames) return std::nullopt;
                return CommandReply::success(agentStateJson());
            });
        };
        agentRegistry.add(std::move(spec));
    }

    // ---- Settings (Phase 3) ----
    auto settingResult = [](const SettingSpec& spec, void* target) {
        return json{{"key", spec.key}, {"value", spec.get(target)}, {"default", spec.defaultValue}};
    };
    auto setSetting = [this, settingResult](SettingScope scope, const json& args, const CommandContext& context,
                                            ux3d::imgui::ViewportInstance* vp) -> CommandOutcome {
        const std::string key = args["key"].get<std::string>();
        const SettingSpec* spec = agentRegistry.findSetting(scope, key);
        if (spec == nullptr && scope == SettingScope::Viewport) {
            spec = agentRegistry.findSetting(SettingScope::Renderer, key);
            if (spec != nullptr) scope = SettingScope::Renderer;
        }
        if (spec == nullptr) {
            return CommandReply::failure("unknown_setting", "unknown setting: " + key + " (list_settings shows them all)");
        }
        if (spec->persisted && context.source == agent::CommandSource::Pipe && !context.persist) {
            return CommandReply::failure("persist_required", spec->key + " is a saved preference; call set_persist(true) first");
        }
        std::string error;
        auto* environment = &getSceneEnvironment(sceneEnvironments, gltfModel, activeScene);
        if (!applySettingValue(scope, spec->key, args["value"], vp ? &vp->settings : nullptr, environment, error)) {
            return CommandReply::failure("bad_value", error);
        }
        void* target = scope == SettingScope::Viewport ? static_cast<void*>(&vp->settings)
                       : scope == SettingScope::Environment ? static_cast<void*>(environment)
                                                            : nullptr;
        // Settings scopes: a compatibility key changes its canonical scope (the scene or the global quality).
        const SettingScope affected = spec->compat && spec->forwardsTo.has_value() ? *spec->forwardsTo : scope;
        resolveAllViewportSettings();
        if (affected == SettingScope::Viewport) {
            vp->antiAliasing.resetHistory();
            sceneRevisions.mark(ux3d::imgui::SceneChange::ViewportSettings);
        } else if (affected == SettingScope::Environment && !spec->resync) {
            // C2: a frame value (SettingSpec::resync): redraw every viewport without a renderer sync.
            sceneRevisions.mark(ux3d::imgui::SceneChange::Environment);
            for (const auto& other : viewports) {
                if (other) other->antiAliasing.resetHistory();
            }
        } else {
            sceneRevisions.mark(ux3d::imgui::SceneChange::RenderSettings);
            for (const auto& other : viewports) {
                if (other) other->antiAliasing.resetHistory();
            }
        }
        if (affected == SettingScope::Renderer && context.source == agent::CommandSource::Pipe && context.persist)
            ux3d::imgui::RenderQuality::commitSetting(spec->key);
        if (spec->persisted && context.source == agent::CommandSource::Pipe && context.persist) {
            ux3d::imgui::RenderQuality::save();
        }
        // App settings saved on request (the glTF extension switches): commit this key's value.
        if (spec->onPersist && context.source == agent::CommandSource::Pipe && context.persist) {
            spec->onPersist();
        }
        json out = settingResult(*spec, target);
        out["scope"] = agent::CommandRegistry::scopeName(scope);
        if (scope == SettingScope::Viewport) out["viewport"] = vp->id;
        return CommandReply::success(out);
    };
    auto getSettings = [this, settingResult](SettingScope scope, const json& args, ux3d::imgui::ViewportInstance* vp) -> CommandOutcome {
        auto* environment = &getSceneEnvironment(sceneEnvironments, gltfModel, activeScene);
        auto targetFor = [&](SettingScope s) -> void* {
            return s == SettingScope::Viewport ? static_cast<void*>(&vp->settings)
                   : s == SettingScope::Environment ? static_cast<void*>(environment) : nullptr;
        };
        if (args.contains("key") && !args["key"].is_null()) {
            const std::string key = args["key"].get<std::string>();
            const SettingSpec* spec = agentRegistry.findSetting(scope, key);
            SettingScope found = scope;
            if (spec == nullptr && scope == SettingScope::Viewport) {
                spec = agentRegistry.findSetting(SettingScope::Renderer, key);
                found = SettingScope::Renderer;
            }
            if (spec == nullptr) return CommandReply::failure("unknown_setting", "unknown setting: " + key);
            return CommandReply::success(settingResult(*spec, targetFor(found)));
        }
        json all = json::object();
        for (const SettingSpec& spec : agentRegistry.settings()) {
            if (spec.scope == scope || (scope == SettingScope::Viewport && spec.scope == SettingScope::Renderer)) {
                all[spec.key] = spec.get(targetFor(spec.scope));
            }
        }
        return CommandReply::success({{"values", all}});
    };
    {
        CommandSpec spec;
        spec.name = "set_viewport_setting";
        spec.help = "Set a viewport (or renderer-wide) setting: AA, SSR, grid, render mode, wireframe, ASOBO switches... "
                    "Same code path as the matching CLI flag.";
        spec.args = objectSchema({{"key", prop("string", "Setting key (list_settings).")},
                                  {"value", prop("", "New value: bool, number, enum name or [r, g, b(, a)].")},
                                  {"viewport", viewportProp()}},
                                 {"key", "value"});
        spec.positional = {"key", "value"};
        spec.run = [resolveViewport, setSetting](const json& args, const CommandContext& context) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* vp = resolveViewport(args, failure);
            if (failure) return *failure;
            return setSetting(SettingScope::Viewport, args, context, vp);
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "get_viewport_setting";
        spec.help = "One viewport/renderer setting (key) or all of them.";
        spec.args = objectSchema({{"key", prop("string", "Setting key (omit for all).")}, {"viewport", viewportProp()}});
        spec.positional = {"key"};
        spec.run = [resolveViewport, getSettings](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* vp = resolveViewport(args, failure);
            if (failure) return *failure;
            return getSettings(SettingScope::Viewport, args, vp);
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "set_environment";
        spec.help = "Set a scene environment setting: exposure, tone mapping, HDR rotation, time of day, emissive scale, bloom...";
        spec.args = objectSchema({{"key", prop("string", "Setting key (list_settings).")},
                                  {"value", prop("", "New value.")}},
                                 {"key", "value"});
        spec.positional = {"key", "value"};
        spec.run = [this, setSetting](const json& args, const CommandContext& context) -> CommandOutcome {
            return setSetting(SettingScope::Environment, args, context, activeViewport);
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "get_environment";
        spec.help = "One environment setting (key) or all of them.";
        spec.args = objectSchema({{"key", prop("string", "Setting key (omit for all).")}});
        spec.positional = {"key"};
        spec.run = [this, getSettings](const json& args, const CommandContext&) -> CommandOutcome {
            return getSettings(SettingScope::Environment, args, activeViewport);
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "list_settings";
        spec.help = "Every setting key with scope, type, range or values, current value, default and CLI flag.";
        spec.args = objectSchema({{"scope", prop("string", "viewport, environment or renderer (default: all).")},
                                  {"viewport", viewportProp()}});
        spec.positional = {"scope"};
        spec.run = [this, resolveViewport](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* vp = resolveViewport(args, failure);
            const std::string scope = lowerText(args.value("scope", std::string()));
            auto* environment = &getSceneEnvironment(sceneEnvironments, gltfModel, activeScene);
            json list = json::array();
            for (const SettingSpec& spec : agentRegistry.settings()) {
                const std::string name = agent::CommandRegistry::scopeName(spec.scope);
                if (!scope.empty() && scope != name) continue;
                json entry = {{"key", spec.key}, {"scope", name}, {"type", agent::CommandRegistry::typeName(spec.type)},
                              {"default", spec.defaultValue}, {"help", spec.help}, {"persisted", spec.persisted}};
                if (!spec.aliases.empty()) entry["aliases"] = spec.aliases;
                if (!spec.cliFlag.empty()) entry["cli"] = spec.cliFlag;
                if (spec.type == SettingType::Enum) entry["values"] = spec.enumValues;
                if (spec.hasRange) entry["range"] = {spec.minimum, spec.maximum};
                void* target = spec.scope == SettingScope::Viewport ? (vp ? static_cast<void*>(&vp->settings) : nullptr)
                               : spec.scope == SettingScope::Environment ? static_cast<void*>(environment) : nullptr;
                entry["value"] = (spec.scope == SettingScope::Viewport && vp == nullptr) ? json(nullptr) : spec.get(target);
                list.push_back(std::move(entry));
            }
            return CommandReply::success({{"settings", list}});
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "set_material_param";
        spec.help = "Edit an existing material extension parameter in memory (Inspector / --set-asobo-param path). "
                    "Extensions are never added or removed.";
        spec.args = objectSchema({{"material", prop("string", "Material name or index.")},
                                  {"extension", prop("string", "Extension present on the material, e.g. ASOBO_material_fresnel_fade.")},
                                  {"param", prop("string", "Parameter name.")},
                                  {"value", prop("", "New value (number, bool, string or array).")}},
                                 {"material", "extension", "param", "value"});
        spec.positional = {"material", "extension", "param", "value"};
        spec.run = [this](const json& args, const CommandContext&) -> CommandOutcome {
            std::string error;
            if (!setMaterialParamAction(args["material"].get<std::string>(), args["extension"].get<std::string>(),
                                        args["param"].get<std::string>(), args["value"], error)) {
                return CommandReply::failure("material_param_failed", error);
            }
            return agentRegistry.find("get_material_param")->run(args, CommandContext{});
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "get_material_param";
        spec.help = "Values of a material extension (typed ASOBO values with defaults, or the vendor JSON).";
        spec.args = objectSchema({{"material", prop("string", "Material name or index.")},
                                  {"extension", prop("string", "Extension name.")},
                                  {"param", prop("string", "Parameter (omit for all).")},
                                  {"value", prop("", "Ignored (set_material_param passes it through).")}},
                                 {"material", "extension"});
        spec.positional = {"material", "extension", "param"};
        spec.run = [this](const json& args, const CommandContext&) -> CommandOutcome {
            const std::string materialSpec = args["material"].get<std::string>();
            const std::string extension = args["extension"].get<std::string>();
            const std::string param = args.contains("param") && !args["param"].is_null() ? args["param"].get<std::string>() : std::string();
            ux3d::gltf::Material* material = findMaterial(gltfModel, materialSpec);
            if (material == nullptr) return CommandReply::failure("material_not_found", "material not found: " + materialSpec);
            json values = json::object();
            json explicitKeys = json::array();
            bool found = false;
            if (ux3d::gltf::asobo::isAsoboExtension(extension)) {
                const auto set = ux3d::gltf::asobo::parseExtensions(gltfModel, *material, ux3d::gltf::asobo::Scope::Material,
                                                                    "/materials/" + std::to_string(material->m_ID));
                for (const auto& data : set.all()) {
                    if (std::string(data.name()) != extension || data.spec() == nullptr) continue;
                    found = true;
                    for (const auto& property : data.spec()->properties) {
                        const std::string key(property.name);
                        if (const auto* value = data.value(key)) values[key] = asoboValueToJson(*value);
                        if (data.isExplicit(key)) explicitKeys.push_back(key);
                    }
                }
            }
            if (!found) {
                for (const auto& [name, text] : material->getUnknownExtensions()) {
                    if (std::string(name.c_str()) != extension) continue;
                    found = true;
                    try { values = json::parse(text.c_str()); } catch (...) { values = json::object(); }
                    for (auto it = values.begin(); it != values.end(); ++it) explicitKeys.push_back(it.key());
                }
            }
            if (!found) return CommandReply::failure("extension_not_found", "material '" + materialSpec + "' has no " + extension);
            json out = {{"material", materialName(material)}, {"extension", extension}};
            if (!param.empty()) {
                if (!values.is_object() || !values.contains(param)) {
                    return CommandReply::failure("param_not_found", extension + " has no parameter " + param);
                }
                out["param"] = param;
                out["value"] = values[param];
                out["explicit"] = std::find(explicitKeys.begin(), explicitKeys.end(), json(param)) != explicitKeys.end();
            } else {
                out["values"] = values;
                out["explicit"] = explicitKeys;
            }
            return CommandReply::success(out);
        };
        agentRegistry.add(std::move(spec));
    }
}

} // namespace ux3d::imgui::application
