// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ux3d::imgui::agent {

// Agent control channel (docs/AGENT_CONTROL_PLAN.md). One registry owns every command that
// the --ui-script / --ui-script-follow harness and the control pipe can run, and every
// setting that the CLI, the scripts and the pipe can change. All commands run on the
// main thread, between frames.

enum class CommandKind {
    Immediate,  // replies in the frame that runs it
    MultiFrame, // registers a job that the frame loop advances until it replies or times out
};

enum class CommandSource {
    Cli,
    Script, // --ui-script / --ui-script-follow (legacy names and result shapes)
    Pipe,   // \\.\pipe\glider-<pid>
};

struct CommandError {
    std::string code;
    std::string message;
    nlohmann::json details = nlohmann::json::object(); // e.g. {"candidates": [...]}
};

struct CommandReply {
    bool ok = true;
    nlohmann::json result;
    CommandError error;

    static CommandReply success(nlohmann::json value = nullptr)
    {
        CommandReply reply;
        reply.result = std::move(value);
        return reply;
    }
    static CommandReply failure(std::string code, std::string message, nlohmann::json details = nlohmann::json::object())
    {
        CommandReply reply;
        reply.ok = false;
        reply.error = {std::move(code), std::move(message), std::move(details)};
        return reply;
    }
};

struct CommandContext {
    CommandSource source = CommandSource::Pipe;
    bool persist = false; // pipe commands may write settings.ini only after set_persist(true)
};

// A multi-frame step is called once at the end of every later frame; it returns the reply
// when the job is done. The registry fails the job with "timeout" after timeoutSeconds.
using JobStep = std::function<std::optional<CommandReply>()>;

struct CommandOutcome {
    std::optional<CommandReply> reply;
    JobStep step;

    CommandOutcome(CommandReply r) : reply(std::move(r)) {}
    CommandOutcome(JobStep s) : step(std::move(s)) {}
};

using ReplyFn = std::function<void(const CommandReply&)>;

struct CommandSpec {
    std::string name;
    std::vector<std::string> aliases; // Phase 0 script names (frame, noderect, ...)
    std::string help;
    CommandKind kind = CommandKind::Immediate;
    double timeoutSeconds = 0.0;      // multi-frame only
    // JSON Schema of the argument object ({"type":"object","properties":...,"required":[...]}).
    nlohmann::json args = nlohmann::json{{"type", "object"}, {"properties", nlohmann::json::object()}};
    // Text form for scripts: tokens map to these properties in order. With restIsText the
    // last one takes the rest of the line (paths with spaces).
    std::vector<std::string> positional;
    bool restIsText = false;
    std::function<CommandOutcome(const nlohmann::json& args, const CommandContext& context)> run;
    // Optional legacy text parser for an alias whose syntax is not plain positional.
    std::function<std::optional<nlohmann::json>(const std::vector<std::string>& tokens, const std::string& line,
                                                 std::string& error)> parseLegacy;
    // Optional legacy result shape for scripts (Phase 0 JSONL results stay unchanged).
    std::function<nlohmann::json(const nlohmann::json& args, const nlohmann::json& result)> legacyResult;
};

enum class SettingScope { Viewport, Environment, Renderer };
enum class SettingType { Bool, Int, Float, Enum, Color3, Color4, Vec2, String };

// One registered setting. The getter/setter receive the target selected by scope: a viewport
// settings struct, a scene environment, or nothing (renderer-wide). Setters receive a value
// already coerced to the declared type (bool, number, enum name, colour array) and only write
// state; callers mark scene revisions. Add a new setting by registering it once: the CLI
// flag, set_*/get_* commands, list_settings and the agent guide all read this table.
struct SettingSpec {
    std::string key;
    SettingScope scope = SettingScope::Viewport;
    SettingType type = SettingType::Bool;
    std::string help;
    std::string cliFlag;               // documentation only, e.g. "--ssr <on|off>"
    std::vector<std::string> aliases;  // alternative keys
    std::vector<std::string> enumValues;
    std::map<std::string, std::string> enumAliases; // lower-case alias -> enum value
    bool hasRange = false;
    double minimum = 0.0;
    double maximum = 0.0;
    bool persisted = false;            // stored in settings.ini (pipe needs set_persist(true))
    // Settings scopes: a compatibility key keeps a moved setting working in its former scope; it reads and writes
    // the setting in forwardsTo (or, for the ssr / ssao viewport keys, the viewport's override).
    bool compat = false;
    std::optional<SettingScope> forwardsTo;
    // Optional: an app setting that is saved only on request. A pipe set changes this run only; while the client has
    // set_persist(true) the registry calls onPersist after a successful set so the setting commits that value.
    std::function<void()> onPersist;
    // C2: a scene environment (Environment scope) value is a frame value: the frame reads it (drawViewports,
    // resolveViewport, the sky / ocean / sky-occlusion keys), so a set marks SceneChange::Environment (redraw, TAA reset)
    // without updateGltf/updateScene. resync keeps the RenderSettings mark (the renderer sync) for a key that changes
    // what the renderer's model data is built against.
    bool resync = false;
    nlohmann::json defaultValue;
    std::function<nlohmann::json(void* target)> get;
    std::function<bool(void* target, const nlohmann::json& value, std::string& error)> set;
};

class CommandRegistry {
public:
    void add(CommandSpec spec);
    const CommandSpec* find(const std::string& name) const; // name or alias
    const std::vector<CommandSpec>& commands() const { return m_commands; }

    void addSetting(SettingSpec spec);
    const SettingSpec* findSetting(SettingScope scope, const std::string& key) const;
    const std::vector<SettingSpec>& settings() const { return m_settings; }
    // Converts a JSON or text value to the setting's type; checks range and enum membership.
    static bool coerceSetting(const SettingSpec& spec, const nlohmann::json& value, nlohmann::json& out,
                              std::string& error);
    static const char* scopeName(SettingScope scope);
    static const char* typeName(SettingType type);

    // Checks required properties and property types against spec.args; fills declared defaults.
    static bool validateArgs(const CommandSpec& spec, nlohmann::json& args, CommandError& error);
    // Builds the argument object from a script line's tokens (tokens[0] is the command).
    static std::optional<nlohmann::json> parseText(const CommandSpec& spec, const std::vector<std::string>& tokens,
                                                   const std::string& line, std::string& error);

    // Runs the command now. Immediate commands reply before returning; multi-frame commands
    // queue a job that tick() advances. Jobs run strictly in order.
    void dispatch(const CommandSpec& spec, nlohmann::json args, const CommandContext& context, ReplyFn reply);
    // Advances the front job by one frame. Call once per frame, before new commands dispatch.
    void tick();
    bool busy() const { return !m_jobs.empty(); }
    void cancelAll(const std::string& code, const std::string& message);

    nlohmann::json describe() const;
    std::string markdownReference() const;

private:
    struct Job {
        std::string name;
        JobStep step;
        ReplyFn reply;
        double deadline = 0.0;
    };
    std::vector<CommandSpec> m_commands;
    std::vector<SettingSpec> m_settings;
    std::deque<Job> m_jobs;
};

std::string base64Encode(const std::vector<uint8_t>& bytes);
// A float as the shortest decimal that reads back as the same float.
nlohmann::json floatJson(float value);

} // namespace ux3d::imgui::agent
