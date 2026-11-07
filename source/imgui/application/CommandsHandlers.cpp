// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Command registry of the agent control channel: registerAgentCommands (state and session, selection and
// visibility, camera, inspection and capture, settings commands).

#include "CommandsInternal.h"
#include "renderer/TransformHierarchyNode.h"
#include "imgui/Profiling.h"
#include "imgui/ImageViewer.h"
#include <imgui_internal.h>
#include <utility>

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
        spec.name = "dump_draw_calls";
        spec.help = "The draw-call list of Statistics -> Copy Draw Calls. The first call starts recording the next scene redraw and returns ready: false; call again until ready: true, which returns the text once.";
        spec.run = [](const json&, const CommandContext&) -> CommandOutcome {
            if (auto dump = ux3d::imgui::profiling::takeDrawCallDump()) {
                return CommandReply::success({{"ready", true}, {"total", dump->total}, {"text", dump->text}});
            }
            if (!ux3d::imgui::profiling::g_drawTraceActive) ux3d::imgui::profiling::requestDrawCallDump();
            return CommandReply::success({{"ready", false}});
        };
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
                if (node.hasNodeVisibility()) { // KHR_node_visibility: the file's value (own, not inherited)
                    entry["nodeVisibility"] = node.getAuthoredFileVisible();
                    entry["nodeVisibilityNow"] = node.isFileVisible();
                }
                if (node.hasNodeSelectability()) { // KHR_node_selectability: the file's value (own, not inherited)
                    entry["selectable"] = node.getAuthoredSelectable();
                    entry["selectableNow"] = node.isFileSelectable();
                }
                if (node.hasNodeHoverability()) { // KHR_node_hoverability
                    entry["hoverable"] = node.getAuthoredHoverable();
                    entry["hoverableNow"] = node.isFileHoverable();
                }
                if (node.hasGpuInstancing()) { // EXT_mesh_gpu_instancing
                    entry["gpuInstances"] = node.getGpuInstances().size();
                    entry["gpuInstancing"] = node.getGpuInstancingStatus();
                }
                nodes.push_back(std::move(entry));
            }
            return CommandReply::success({{"total", gltfModel.getNodes().size()}, {"matched", matched}, {"nodes", nodes}});
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "set_animation_time";
        spec.help = "Seek the animation timeline like the scrubber (time, seconds; the clips clamp it to their duration), "
                    "or advance it like playback by advance seconds (the player's speed, loop and end handling: past the "
                    "end it wraps with Loop on, else stops at the end). Returns the timeline state.";
        spec.args = objectSchema({{"time", prop("number", "Timeline time in seconds.")},
                                  {"advance", prop("number", "Playback step in seconds, applied after time.")},
                                  {"loop", prop("boolean", "Sets the Loop checkbox first.")}});
        spec.positional = {"time"};
        spec.run = [this](const json& args, const CommandContext&) -> CommandOutcome {
            if (!gltfLayer) return CommandReply::failure("no_scene", "no render layer");
            if (args.contains("loop") && args["loop"].is_boolean()) animationPlayer.setLooping(args["loop"].get<bool>());
            if (args.contains("time") && args["time"].is_number()) {
                animationPlayer.setCurrentTime(args["time"].get<float>(), gltfLayer.get());
            }
            if (args.contains("advance") && args["advance"].is_number()) {
                const bool wasPlaying = animationPlayer.isPlaying();
                animationPlayer.setPlaying(true);
                animationPlayer.update(args["advance"].get<float>(), gltfLayer.get());
                if (!wasPlaying) animationPlayer.setPlaying(false);
            }
            for (auto& vp : viewports) {
                if (vp) vp->antiAliasing.resetHistory();
            }
            sceneRevisions.mark(ux3d::imgui::SceneChange::AnimationTime);
            return CommandReply::success({{"time", animationPlayer.getCurrentTime()},
                                          {"duration", animationPlayer.getMaxDuration()},
                                          {"playing", animationPlayer.isPlaying()},
                                          {"looping", animationPlayer.isLooping()}});
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "get_node_pose";
        spec.help = "A node's current local pose as the animation set it: translation, rotation (x, y, z, w), scale, "
                    "morph weights and, with KHR_node_visibility, its current visible value; float32 values as stored.";
        spec.args = objectSchema({{"node", prop("string", "Node name or #index.")}}, {"node"});
        spec.positional = {"node"};
        spec.run = [this, resolveNode](const json& args, const CommandContext&) -> CommandOutcome {
            std::optional<CommandReply> failure;
            auto* node = resolveNode(args, "node", failure);
            if (failure) return *failure;
            if (!gltfLayer) return CommandReply::failure("no_scene", "no render layer");
            const auto& map = gltfLayer->getTransformHierarchy().map;
            const auto it = map.find(node);
            if (it == map.end() || it->second == nullptr) {
                return CommandReply::failure("no_pose", "node has no transform hierarchy entry");
            }
            const auto* pose = it->second;
            const auto& t = pose->translationTarget.getValue();
            const auto& r = pose->rotationTarget.getValue();
            const auto& sc = pose->scaleTarget.getValue();
            json result = {{"node", nodeRef(node)},
                           {"time", animationPlayer.getCurrentTime()},
                           {"translation", {t.x, t.y, t.z}},
                           {"rotation", {r.x, r.y, r.z, r.w}},
                           {"scale", {sc.x, sc.y, sc.z}},
                           {"weights", pose->weightTarget.getValue()}};
            if (node->hasNodeVisibility()) result["visibleNow"] = node->isFileVisible();
            return CommandReply::success(result);
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
                                  {"without_textures", withDefault(prop("boolean", "Skip image reads (File > Open without texture)."), false)},
                                  {"startup_state", withDefault(prop("boolean", "After the load, the state Glider started with: reset's "
                                                                                "startup settings plus the --select-node and --camera-* "
                                                                                "flags replayed on this model (regression sessions)."), false)}},
                                 {"path"});
        spec.positional = {"path"};
        spec.restIsText = true;
        spec.run = [this](const json& args, const CommandContext&) -> CommandOutcome {
            const std::string path = args["path"].get<std::string>();
            std::error_code error;
            if (!std::filesystem::is_regular_file(std::filesystem::u8path(path), error)) {
                return CommandReply::failure("file_not_found", "failed to open: " + path);
            }
            // The startup glTF extension switches before the load (a switch applied at load must not reload it).
            if (args.value("startup_state", false)) {
                ux3d::imgui::GltfExtensionSettings::instance().restore(agentStartupGltfExtensions);
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
            const bool startupState = args.value("startup_state", false);
            return agent::JobStep([this, path, job, startupState]() -> std::optional<CommandReply> {
                if (!queuedLoadPath.empty() || loadingActive) return std::nullopt;
                if (!job->loaded) {
                    job->loaded = true;
                    job->loadMs = ux3d::imgui::profiling::elapsedMs(job->start);
                    if (!modelLoaded) return CommandReply::failure("load_failed", "failed to open: " + path);
                    if (startupState) {
                        // As configure(): the startup state, then --select-node framing, then the camera flags.
                        restoreAgentStartupState();
                        if (!agentStartupCamera.selectNode.empty()) {
                            for (auto& candidate : gltfModel.getNodes()) {
                                if (candidate.hasName() && candidate.getName() == agentStartupCamera.selectNode) {
                                    selectNodeAction(&candidate);
                                    frameNodeAction(&candidate, std::nullopt, 1.0f);
                                    break;
                                }
                            }
                        }
                        applyStartupCameraFlags(agentStartupCamera);
                    }
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
            // The startup glTF extension switches before the reload, which reads the file with them.
            ux3d::imgui::GltfExtensionSettings::instance().restore(agentStartupGltfExtensions);
            // The reload is queued for the next frame; the reset state is applied after it.
            loadGltfModel(path, gltfModel, activeScene, selection, currentFilePath, viewports, gltfLayer.get(),
                          animationPlayer, sceneRevisions, sceneEnvironments);
            queuedLoadNoPersist = agentNoPersistScope;
            auto frames = std::make_shared<int>(-1);
            return agent::JobStep([this, path, frames]() -> std::optional<CommandReply> {
                if (!queuedLoadPath.empty() || loadingActive) return std::nullopt;
                if (*frames < 0) {
                    if (!modelLoaded) return CommandReply::failure("load_failed", "failed to reload: " + path);
                    restoreAgentStartupState();
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
            if (!found && extension == ux3d::gltf::Dispersion::ExtensionName && material->getDispersion().has_value()) {
                found = true; // KHR_materials_dispersion: the typed value (default 0)
                values["dispersion"] = material->getDispersion()->getDispersionOrDefault();
                if (material->getDispersion()->hasDispersion()) explicitKeys.push_back("dispersion");
            }
            if (!found && extension == ux3d::gltf::DiffuseTransmission::ExtensionName && material->getDiffuseTransmission().has_value()) {
                found = true; // typed values with their defaults (factor 0, colour white)
                const auto& dt = material->getDiffuseTransmission().value();
                const auto color = dt.getDiffuseTransmissionColorFactorOrDefault();
                values["diffuseTransmissionFactor"] = dt.getDiffuseTransmissionFactorOrDefault();
                values["diffuseTransmissionColorFactor"] = json::array({color.r, color.g, color.b});
                if (dt.hasDiffuseTransmissionFactor()) explicitKeys.push_back("diffuseTransmissionFactor");
                if (dt.hasDiffuseTransmissionColorFactor()) explicitKeys.push_back("diffuseTransmissionColorFactor");
                if (dt.getDiffuseTransmissionTextureInfo().has_value()) values["diffuseTransmissionTexture"] = true;
                if (dt.getDiffuseTransmissionColorTextureInfo().has_value()) values["diffuseTransmissionColorTexture"] = true;
            }
            if (!found && extension == ux3d::gltf::DiffuseRoughness::ExtensionName && material->getDiffuseRoughness().has_value()) {
                found = true; // typed value with its default 0
                const auto& dr = material->getDiffuseRoughness().value();
                values["diffuseRoughnessFactor"] = dr.getDiffuseRoughnessFactorOrDefault();
                if (dr.hasDiffuseRoughnessFactor()) explicitKeys.push_back("diffuseRoughnessFactor");
                if (dr.getDiffuseRoughnessTextureInfo().has_value()) values["diffuseRoughnessTexture"] = true;
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
    {
        CommandSpec spec;
        spec.name = "image_viewer";
        spec.help = "Open (or focus) the Image Viewer of glTF image `image`, like clicking its Inspector preview, and set "
                    "its view: zoom (fit or a percentage), channels (letters such as rgb, a, l or all), levels (full, data or "
                    "low,high in stored values), background (checker, white, gray, black), raw (on/off, decoded normal maps), "
                    "pixel (x,y: the readout pixel). '-' keeps a positional value. Crosshair and pixel grid (preferences "
                    "shared by every viewer; saved only after set_persist(true)): crosshair, grid (on/off), major / minor "
                    "(grid spacing in image pixels, 1..16384), major_color / minor_color (r,g,b,a or #rrggbbaa), "
                    "grid_defaults=true, cursor (x,y: the crosshair pixel without the mouse, off). In script lines these "
                    "are name=value tokens. Replies with the viewer state, including the stored values of the readout "
                    "pixel and the crosshair / grid state; close=true closes the window.";
        spec.args = objectSchema({{"image", prop("integer", "glTF image index.")},
                                  {"zoom", prop("", "fit, or a zoom percentage (1..3200).")},
                                  {"channels", prop("string", "Shown channels, e.g. rgba, rgb, a, l, rg, all.")},
                                  {"levels", prop("", "full, data, or low,high in stored values.")},
                                  {"background", prop("string", "checker, white, gray or black.")},
                                  {"raw", prop("", "on: stored values of a decoded normal map; off: decoded.")},
                                  {"pixel", prop("", "x,y or [x, y]: the image pixel of the readout (row 0 at the top).")},
                                  {"signed_square", prop("boolean", "Override the packaged normal-map decode (default: as the Inspector).")},
                                  {"close", withDefault(prop("boolean", "Close the image's viewer window."), false)},
                                  {"window_pos", prop("", "x,y: move the window there (relative to the main window's "
                                                          "top-left; outside it a window detaches with multi-viewport).")},
                                  {"window_size", prop("", "w,h: resize the window.")},
                                  {"crosshair", prop("", "on/off: lines through the centre of the hovered pixel (default off).")},
                                  {"grid", prop("", "on/off: the pixel grid (default off).")},
                                  {"major", prop("", "Major grid spacing in image pixels, 1..16384 (default 256).")},
                                  {"minor", prop("", "Minor grid spacing in image pixels, 1..16384 (default 64).")},
                                  {"major_color", prop("string", "Major grid colour r,g,b,a in 0..1 or #rrggbb[aa] (default 1,1,1,0.5).")},
                                  {"minor_color", prop("string", "Minor grid colour r,g,b,a in 0..1 or #rrggbb[aa] (default 1,1,1,0.25).")},
                                  {"grid_defaults", prop("boolean", "true: both grid levels back to their default colour and spacing.")},
                                  {"cursor", prop("", "x,y: draw the crosshair at this image pixel while the mouse is not over the "
                                                      "image; off removes it.")}},
                                 {"image"});
        spec.positional = {"image", "zoom", "channels", "levels", "background", "raw", "pixel", "window_pos", "window_size"};
        spec.run = [this](const json& args, const CommandContext& context) -> CommandOutcome {
            json result;
            std::string code;
            std::string message;
            if (!ux3d::imgui::ImageViewer::script(gltfModel, gltfLayer.get(), args, context.persist, result, code, message)) {
                return CommandReply::failure(code, message);
            }
            return CommandReply::success(result);
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "image_viewer_wait";
        spec.kind = CommandKind::MultiFrame;
        spec.timeoutSeconds = 120.0;
        spec.help = "Wait until the Image Viewer window of glTF image `image` is ready (full-resolution pixels, texture, "
                    "histogram and UV overlay; image_viewer and uv_layout reply while it still loads), then reply with "
                    "its state, which includes the per-phase load timings.";
        spec.args = objectSchema({{"image", prop("integer", "glTF image index.")}}, {"image"});
        spec.positional = {"image"};
        spec.run = [this](const json& args, const CommandContext&) -> CommandOutcome {
            const long long image = args.value("image", -1LL);
            return agent::JobStep([this, image]() -> std::optional<CommandReply> {
                json state;
                bool ready = false;
                std::string code;
                std::string message;
                if (!ux3d::imgui::ImageViewer::waitState(gltfModel, image, state, ready, code, message)) {
                    return CommandReply::failure(code, message);
                }
                if (!ready) return std::nullopt;
                return CommandReply::success(state);
            });
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "uv_layout";
        spec.help = "Open the UV layout overlay of a material's texture slot in the Image Viewer, like the UV layout button "
                    "of the Material Inspector: the triangle edges of every primitive with the material in the slot's "
                    "TEXCOORD set, over the slot's image. wrap / fill / transform / show: on, off or '-' (keep). Replies "
                    "with the viewer state; its uvLayout holds the counts.";
        spec.args = objectSchema({{"material", prop("integer", "glTF material index.")},
                                  {"slot", withDefault(prop("string", "baseColor, metallicRoughness, normal, occlusion or emissive."), "baseColor")},
                                  {"wrap", prop("", "on: move each triangle into 0..1 by the tile of its centre.")},
                                  {"fill", prop("", "on: a faint face fill.")},
                                  {"transform", prop("", "on: apply the slot's KHR_texture_transform (default); off: raw UVs.")},
                                  {"show", prop("", "off hides the overlay (the UV toggle).")},
                                  {"opacity", prop("number", "Line opacity 0.05..1.")}},
                                 {"material"});
        spec.positional = {"material", "slot", "wrap", "fill", "transform", "show"};
        spec.run = [this](const json& args, const CommandContext&) -> CommandOutcome {
            json result;
            std::string code;
            std::string message;
            if (!ux3d::imgui::ImageViewer::uvLayoutScript(gltfModel, gltfLayer.get(), args, result, code, message)) {
                return CommandReply::failure(code, message);
            }
            return CommandReply::success(result);
        };
        agentRegistry.add(std::move(spec));
    }
    {
        CommandSpec spec;
        spec.name = "capture_window";
        spec.kind = CommandKind::MultiFrame;
        spec.timeoutSeconds = 30.0;
        spec.help = "Save one ImGui window as a PNG: window = a case-insensitive part of its title. A window on the main "
                    "OS window is cropped from the main framebuffer; a detached window (multi-viewport) is read back from "
                    "its own OS window while it is rendered. Replies with path, size and whether it is detached.";
        spec.args = objectSchema({{"path", prop("string", "Output .png.")},
                                  {"window", prop("string", "Part of the window title, e.g. 'Image Viewer'.")},
                                  {"settle_frames", withDefault(prop("integer", "Frames rendered before reading."), 3)}},
                                 {"path", "window"});
        spec.positional = {"path", "window", "settle_frames"};
        spec.restIsText = false;
        spec.run = [this](const json& args, const CommandContext&) -> CommandOutcome {
            struct Job {
                std::string path;
                std::string window;
                int settle = 3;
                int elapsed = 0;
                std::shared_ptr<WindowCaptureRequest> request;
                bool detached = false;
                std::string title;
            };
            auto job = std::make_shared<Job>();
            job->path = args["path"].get<std::string>();
            job->window = toLowerAscii(args["window"].get<std::string>());
            job->settle = std::max<int>(1, static_cast<int>(args.value("settle_frames", 3LL)));
            return agent::JobStep([this, job]() -> std::optional<CommandReply> {
                if (job->request) {
                    if (!job->request->done) return std::nullopt;
                    const auto request = std::exchange(job->request, nullptr);
                    if (windowCapture == request) windowCapture.reset();
                    if (!request->ok) return CommandReply::failure("capture_failed", "capture failed: " + job->path);
                    return CommandReply::success(json{{"path", job->path}, {"window", job->title}, {"detached", true},
                                                      {"width", request->width}, {"height", request->height}});
                }
                if (++job->elapsed < job->settle) return std::nullopt;
                ImGuiContext& g = *GImGui;
                ImGuiWindow* found = nullptr;
                for (ImGuiWindow* w : g.Windows) {
                    if (w->ParentWindow != nullptr || w->Hidden || w->LastFrameActive < g.FrameCount - 1) continue;
                    if (toLowerAscii(w->Name).find(job->window) != std::string::npos) found = w;
                }
                if (found == nullptr) return CommandReply::failure("window_not_found", "no visible window matches " + job->window);
                job->title = found->Name;
                const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
                if (found->Viewport != nullptr && found->Viewport->ID != mainViewport->ID) {
                    job->request = std::make_shared<WindowCaptureRequest>();
                    job->request->viewportId = found->Viewport->ID;
                    job->request->path = job->path;
                    windowCapture = job->request;
                    return std::nullopt; // read back while that OS window renders next frame
                }
                int fbW = 0, fbH = 0;
                glfwGetFramebufferSize(window, &fbW, &fbH);
                const ImVec2 scale = ImGui::GetIO().DisplayFramebufferScale;
                const int x0 = std::clamp(static_cast<int>((found->Pos.x - mainViewport->Pos.x) * scale.x), 0, fbW);
                const int y0 = std::clamp(static_cast<int>((found->Pos.y - mainViewport->Pos.y) * scale.y), 0, fbH);
                const int x1 = std::clamp(static_cast<int>((found->Pos.x + found->Size.x - mainViewport->Pos.x) * scale.x), 0, fbW);
                const int y1 = std::clamp(static_cast<int>((found->Pos.y + found->Size.y - mainViewport->Pos.y) * scale.y), 0, fbH);
                if (x1 <= x0 || y1 <= y0) return CommandReply::failure("capture_failed", "the window is outside the main window");
                std::vector<uint8_t> pixels(static_cast<size_t>(x1 - x0) * (y1 - y0) * 4);
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glReadBuffer(GL_BACK);
                glReadPixels(x0, fbH - y1, x1 - x0, y1 - y0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                if (!saveImage(job->path.c_str(), x1 - x0, y1 - y0, pixels.data())) {
                    return CommandReply::failure("capture_failed", "capture failed: " + job->path);
                }
                return CommandReply::success(json{{"path", job->path}, {"window", job->title}, {"detached", false},
                                                  {"width", x1 - x0}, {"height", y1 - y0}});
            });
        };
        agentRegistry.add(std::move(spec));
    }
    registerAgentAreaCommands();
}

void Application::restoreAgentStartupState()
{
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
}

} // namespace ux3d::imgui::application
