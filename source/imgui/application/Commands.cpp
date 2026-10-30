// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

// Agent control channel (docs/AGENT_CONTROL_PLAN.md): the shared actions the CLI flags and registry commands
// call, script / pipe dispatch and the frame pump, the control pipe and --agent-guide. The registry contents
// are in CommandsSettings*.cpp (settings) and CommandsHandlers.cpp (commands); get_state builders in
// CommandsState.cpp, probes in CommandsProbe.cpp.

#include "CommandsInternal.h"

namespace ux3d::imgui::application {

// ---------------------------------------------------------------------------------------------
// Shared actions (CLI flags and registry commands)
// ---------------------------------------------------------------------------------------------

void Application::selectNodeAction(ux3d::gltf::Node* node)
{
    selection.selectNode(node);
    ux3d::imgui::SceneHierarchyPanel::requestScrollToSelectedNode();
}

void Application::isolateNodeAction(ux3d::gltf::Node* node, bool withChildren)
{
    gltfLayer->setIsolation(node, withChildren);
    sceneRevisions.mark(ux3d::imgui::SceneChange::Visibility);
}

void Application::frameNodeAction(ux3d::gltf::Node* node, std::optional<ux3d::editor::CameraOrientation> view,
                                  float margin)
{
    if (activeViewport == nullptr) return;
    if (view.has_value()) activeViewport->cameraActor.orientUserCamera(view.value());
    focusCameraOnNode(node, gltfModel, gltfLayer.get(), activeViewport->cameraActor, activeViewport->antiAliasing,
                      sceneRevisions, margin);
}

bool Application::setMaterialParamAction(const std::string& material, const std::string& extension,
                                         const std::string& param, const json& value, std::string& error)
{
    // Text, as --set-asobo-param passes it; JSON numbers keep their exact decimal form.
    std::string raw;
    if (value.is_string()) raw = value.get<std::string>();
    else if (value.is_boolean()) raw = value.get<bool>() ? "true" : "false";
    else raw = value.dump();
    auto& inspector = ux3d::imgui::ExtensionInspector::instance();
    if (ux3d::gltf::asobo::isAsoboExtension(extension)) {
        if (!inspector.setMaterialAsoboParameter(gltfModel, material, extension, param, raw, sceneRevisions)) {
            error = "cannot set " + extension + "." + param + " = " + raw + " on material '" + material +
                    "' (unknown material, extension not on the material, or a value of the wrong type)";
            return false;
        }
        return true;
    }
    ux3d::gltf::Material* target = findMaterial(gltfModel, material);
    if (target == nullptr) {
        error = "material not found: " + material;
        return false;
    }
    bool present = false;
    for (const auto& [name, text] : target->getUnknownExtensions()) {
        present = present || std::string(name.c_str()) == extension;
    }
    if (!present) {
        error = "material '" + material + "' has no " + extension + " (extensions are never added)";
        return false;
    }
    json parsed = value;
    if (value.is_string()) {
        try { parsed = json::parse(value.get<std::string>()); } catch (...) { parsed = value; }
    }
    if (!inspector.setExtensionParameter(target, extension, param, parsed, sceneRevisions)) {
        error = "cannot set " + extension + "." + param;
        return false;
    }
    return true;
}

bool Application::applySettingValue(SettingScope scope, const std::string& key, const json& value,
                                    ux3d::imgui::ViewportSettingsState* viewport,
                                    ux3d::imgui::SceneEnvironmentState* environment, std::string& error)
{
    registerAgentCommands();
    const SettingSpec* spec = agentRegistry.findSetting(scope, key);
    if (spec == nullptr) {
        error = "unknown " + std::string(agent::CommandRegistry::scopeName(scope)) + " setting: " + key;
        return false;
    }
    json coerced;
    if (!agent::CommandRegistry::coerceSetting(*spec, value, coerced, error)) return false;
    void* target = scope == SettingScope::Viewport ? static_cast<void*>(viewport)
                   : scope == SettingScope::Environment ? static_cast<void*>(environment)
                                                        : nullptr;
    if (scope != SettingScope::Renderer && target == nullptr) {
        error = "no target for setting " + key;
        return false;
    }
    const bool ok = spec->set(target, coerced, error);
    if (ok) ux3d::imgui::RenderQuality::observeSessionEdit();
    return ok;
}

bool Application::applyCliSetting(SettingScope scope, const std::string& key, const std::string& value, const char* flag)
{
    std::string error;
    auto* environment = scope == SettingScope::Environment ? &getSceneEnvironment(sceneEnvironments, gltfModel, activeScene)
                                                           : nullptr;
    if (!applySettingValue(scope, key, value, activeViewport ? &activeViewport->settings : nullptr, environment, error)) {
        std::fprintf(stderr, "Invalid %s %s: %s\n", flag, value.c_str(), error.c_str());
        return false;
    }
    const auto* spec = agentRegistry.findSetting(scope, key);
    if (spec && (spec->forwardsTo.value_or(scope) == SettingScope::Environment)) cliEnvironmentSettings.emplace_back(key, value);
    return true;
}

ux3d::imgui::ViewportInstance* Application::findViewportById(uint32_t id)
{
    for (const auto& vp : viewports) {
        if (vp && vp->id == id) return vp.get();
    }
    return nullptr;
}

// ---------------------------------------------------------------------------------------------
// Dispatch: scripts, pipe and the frame pump
// ---------------------------------------------------------------------------------------------

bool Application::runScriptCommand(const std::string& cmd, const std::vector<std::string>& tokens, const std::string& line,
                                   std::function<void(bool, const std::string&, const json&)> done)
{
    registerAgentCommands();
    const agent::CommandSpec* spec = agentRegistry.find(cmd);
    if (spec == nullptr) return false;
    const bool legacy = cmd != spec->name;
    std::string error;
    std::optional<json> args = legacy && spec->parseLegacy ? spec->parseLegacy(tokens, line, error)
                                                           : agent::CommandRegistry::parseText(*spec, tokens, line, error);
    if (!args) {
        done(false, error, nullptr);
        return true;
    }
    agent::CommandContext context;
    context.source = agent::CommandSource::Script;
    ++agentCommandsRun;
    const json argsCopy = *args;
    agentRegistry.dispatch(*spec, *args, context, [spec, legacy, argsCopy, done](const agent::CommandReply& reply) {
        if (!reply.ok) {
            done(false, reply.error.message, nullptr);
            return;
        }
        done(true, "", legacy && spec->legacyResult ? spec->legacyResult(argsCopy, reply.result) : reply.result);
    });
    return true;
}

void Application::runPipeRequest(const ux3d::imgui::ControlPipe::Request& request)
{
    const uint64_t session = request.session;
    if (session != agentPipeSession) {
        // A new client starts without persistence.
        agentPipeSession = session;
        agentPersist = false;
    }
    json id = nullptr;
    auto send = [this, session](json id, const agent::CommandReply& reply) {
        json out = {{"id", std::move(id)}, {"ok", reply.ok}};
        if (reply.ok) {
            out["result"] = reply.result;
        } else {
            out["error"] = {{"code", reply.error.code}, {"message", reply.error.message}};
            for (auto it = reply.error.details.begin(); it != reply.error.details.end(); ++it) out["error"][it.key()] = it.value();
        }
        if (controlPipe) controlPipe->reply(session, out.dump(-1, ' ', false, json::error_handler_t::replace));
    };
    json message;
    try {
        message = json::parse(request.line);
    } catch (const std::exception& e) {
        send(nullptr, agent::CommandReply::failure("bad_request", std::string("not JSON: ") + e.what()));
        return;
    }
    if (!message.is_object()) {
        send(nullptr, agent::CommandReply::failure("bad_request", "a request is a JSON object {\"id\", \"cmd\", \"args\"}"));
        return;
    }
    id = message.contains("id") ? message["id"] : json(nullptr);
    const std::string cmd = message.value("cmd", std::string());
    const agent::CommandSpec* spec = agentRegistry.find(cmd);
    if (spec == nullptr) {
        send(id, agent::CommandReply::failure("unknown_command", "unknown command: " + cmd + " (describe lists them)"));
        return;
    }
    agent::CommandContext context;
    context.source = agent::CommandSource::Pipe;
    context.persist = agentPersist;
    ++agentCommandsRun;
    // Pipe commands never write settings.ini (recent files included) unless set_persist(true).
    agentNoPersistScope = !agentPersist;
    agentRegistry.dispatch(*spec, message.contains("args") ? message["args"] : json::object(), context,
                           [send, id](const agent::CommandReply& reply) { send(id, reply); });
    agentNoPersistScope = false;
}

bool Application::agentWorkPending() const
{
    return agentRegistry.busy() || !agentPipeBacklog.empty();
}

void Application::pumpAgentCommands()
{
    if (!agentRegistryReady) return;
    // A pipe job keeps its session's no-persist scope while it runs.
    agentNoPersistScope = controlPipe != nullptr && !agentPersist && agentRegistry.busy();
    agentRegistry.tick();
    agentNoPersistScope = false;
    if (controlPipe == nullptr) return;
    for (auto& request : controlPipe->takeRequests()) agentPipeBacklog.push_back(std::move(request));
    // Commands run strictly in order: a multi-frame job holds back the requests behind it.
    while (!agentPipeBacklog.empty() && !agentRegistry.busy()) {
        const ux3d::imgui::ControlPipe::Request request = std::move(agentPipeBacklog.front());
        agentPipeBacklog.pop_front();
        runPipeRequest(request);
    }
}

bool Application::startControlPipe()
{
    registerAgentCommands();
    if (controlPipe && controlPipe->running()) return true;
    controlPipe = std::make_unique<ux3d::imgui::ControlPipe>();
    std::string error;
    if (!controlPipe->start([] { glfwPostEmptyEvent(); }, error)) {
        controlPipeError = error;
        logMessage("Agent control pipe failed: " + error, ImVec4(1.0f, 0.4f, 0.4f, 1.0f), ux3d::core::LogLevel::Error);
        std::fprintf(stderr, "Agent control pipe failed: %s\n", error.c_str());
        controlPipe.reset();
        return false;
    }
    controlPipeError.clear();
    logMessage("Agent control pipe: " + controlPipe->name());
    std::printf("Agent control pipe: %s\n", controlPipe->name().c_str());
    std::fflush(stdout);
    return true;
}

void Application::stopControlPipe()
{
    if (!controlPipe) return;
    controlPipe->stop();
    controlPipe.reset();
    agentPipeBacklog.clear();
    agentPersist = false;
    logMessage("Agent control pipe closed");
}

void Application::drawAgentControlSettings()
{
    bool enabled = controlPipe != nullptr && controlPipe->running();
    if (ImGui::Checkbox("Enable control pipe", &enabled)) {
        if (enabled) startControlPipe();
        else stopControlPipe();
    }
    ImGui::TextDisabled("Session only; not saved. --control-pipe enables it at startup.");
    ImGui::Separator();
    if (controlPipe != nullptr) {
        ImGui::Text("Pipe: %s", controlPipe->name().c_str());
        ImGui::Text("Client: %s", controlPipe->clientConnected() ? "connected" : "none");
        ImGui::Text("Clients accepted: %llu, refused: %llu", static_cast<unsigned long long>(controlPipe->clientsAccepted()),
                    static_cast<unsigned long long>(controlPipe->clientsRejected()));
        ImGui::Text("Agent changes saved to settings.ini: %s", agentPersist ? "yes (set_persist)" : "no");
    } else if (!controlPipeError.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", controlPipeError.c_str());
    }
    ImGui::Text("Commands run: %llu", static_cast<unsigned long long>(agentCommandsRun));
    ImGui::Separator();
    ImGui::TextWrapped("Agents connect with scripts/glider_mcp.py (MCP) or scripts/glider_ctl.py. "
                       "Glider.exe --agent-guide <file> writes the guide and command reference.");
}

// ---------------------------------------------------------------------------------------------
// --agent-guide
// ---------------------------------------------------------------------------------------------

std::string Application::buildAgentGuide()
{
    registerAgentCommands();
    std::string text;
    const auto file = ux3d::imgui::EmbeddedVFS::instance().getFile("agent/GLIDER_AGENT_GUIDE.md");
    if (file.isValid()) {
        text.assign(file.asStringView());
    } else {
        text = "# Glider agent control\n\n(The embedded guide is missing from this build.)\n";
    }
    if (!text.empty() && text.back() != '\n') text += '\n';
    text += "\n" + agentRegistry.markdownReference();
    return text;
}

int Application::writeAgentGuide(const std::string& path, const std::string& format)
{
    std::string text = buildAgentGuide();
    std::filesystem::path target;
    if (format == "skill") {
        if (path.empty()) {
            std::fprintf(stderr, "--agent-guide --format skill needs a directory\n");
            return 1;
        }
        target = std::filesystem::u8path(path) / "glider-control" / "SKILL.md";
    } else if (!path.empty()) {
        target = std::filesystem::u8path(path);
    }
    if (!target.empty()) {
        std::error_code error;
        if (target.has_parent_path()) std::filesystem::create_directories(target.parent_path(), error);
        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        out << text;
        if (!out) {
            std::fprintf(stderr, "Cannot write the agent guide to %s\n", pathText(target).c_str());
            return 1;
        }
        return 0;
    }
#ifdef _WIN32
    // Win32 GUI subsystem: use redirected stdout when there is one, else the parent console.
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    const DWORD type = (handle != nullptr && handle != INVALID_HANDLE_VALUE) ? GetFileType(handle) : FILE_TYPE_UNKNOWN;
    if (type == FILE_TYPE_UNKNOWN && AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* stream = nullptr;
        freopen_s(&stream, "CONOUT$", "w", stdout);
    }
#endif
    std::fwrite(text.data(), 1, text.size(), stdout);
    std::fflush(stdout);
    return 0;
}

} // namespace ux3d::imgui::application
