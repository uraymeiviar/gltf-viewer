// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/CommandRegistry.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace ux3d::imgui::agent {

namespace {

double nowSeconds()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::string lower(std::string text)
{
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

// Lower case without spaces, underscores and hyphens: "Shading Normal" == "shading_normal".
std::string squash(const std::string& text)
{
    std::string out;
    for (char c : lower(text)) {
        if (c != ' ' && c != '_' && c != '-') out.push_back(c);
    }
    return out;
}

std::optional<bool> parseBool(const std::string& text)
{
    const std::string value = lower(text);
    if (value == "on" || value == "true" || value == "1" || value == "yes") return true;
    if (value == "off" || value == "false" || value == "0" || value == "no") return false;
    return std::nullopt;
}

// Parses a whole string as a float with std::stof, as the CLI always has: the CLI and the text
// forms then produce the same bits.
std::optional<double> parseFloatText(const std::string& text)
{
    try {
        size_t used = 0;
        const float value = std::stof(text, &used);
        if (used != text.size() || !std::isfinite(value)) return std::nullopt;
        return static_cast<double>(value);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::vector<double>> parseNumberList(const nlohmann::json& value)
{
    std::vector<double> numbers;
    if (value.is_array()) {
        for (const auto& item : value) {
            if (item.is_number()) numbers.push_back(item.get<double>());
            else if (item.is_string()) {
                auto parsed = parseFloatText(item.get<std::string>());
                if (!parsed) return std::nullopt;
                numbers.push_back(*parsed);
            } else return std::nullopt;
        }
        return numbers;
    }
    if (!value.is_string()) return std::nullopt;
    std::stringstream stream(value.get<std::string>());
    std::string item;
    while (std::getline(stream, item, ',')) {
        auto parsed = parseFloatText(item);
        if (!parsed) return std::nullopt;
        numbers.push_back(*parsed);
    }
    return numbers;
}

std::string jsonTypeOf(const nlohmann::json& schema)
{
    return schema.contains("type") && schema["type"].is_string() ? schema["type"].get<std::string>() : std::string();
}

std::optional<nlohmann::json> coerceText(const nlohmann::json& schema, const std::string& token)
{
    const std::string type = jsonTypeOf(schema);
    if (type == "number") {
        auto value = parseFloatText(token);
        if (!value) {
            try { return nlohmann::json(std::stod(token)); } catch (...) { return std::nullopt; }
        }
        return nlohmann::json(*value);
    }
    if (type == "integer") {
        try {
            size_t used = 0;
            const long long value = std::stoll(token, &used);
            if (used != token.size()) return std::nullopt;
            return nlohmann::json(value);
        } catch (...) {
            return std::nullopt;
        }
    }
    if (type == "boolean") {
        auto value = parseBool(token);
        if (!value) return std::nullopt;
        return nlohmann::json(*value);
    }
    if (type == "array") {
        auto values = parseNumberList(token);
        if (!values) return std::nullopt;
        return nlohmann::json(*values);
    }
    return nlohmann::json(token);
}

bool typeMatches(const std::string& type, const nlohmann::json& value)
{
    if (type.empty()) return true;
    if (type == "string") return value.is_string();
    if (type == "number") return value.is_number();
    if (type == "integer") return value.is_number_integer() || (value.is_number_float() && std::floor(value.get<double>()) == value.get<double>());
    if (type == "boolean") return value.is_boolean();
    if (type == "array") return value.is_array();
    if (type == "object") return value.is_object();
    return true;
}

// Markdown table cells: a literal | would end the cell.
std::string cell(const std::string& text)
{
    std::string out;
    for (char c : text) {
        if (c == '|') out += "\\|";
        else if (c == '\n') out += ' ';
        else out += c;
    }
    return out;
}

} // namespace

nlohmann::json floatJson(float value)
{
    // The shortest decimal that reads back as the same float: 1.2 instead of 1.2000000476837158.
    char buffer[32];
    for (int precision = 6; precision <= 9; ++precision) {
        std::snprintf(buffer, sizeof(buffer), "%.*g", precision, static_cast<double>(value));
        if (std::strtof(buffer, nullptr) == value) break;
    }
    return nlohmann::json(std::strtod(buffer, nullptr));
}

void CommandRegistry::add(CommandSpec spec)
{
    if (!spec.args.contains("type")) spec.args["type"] = "object";
    if (!spec.args.contains("properties")) spec.args["properties"] = nlohmann::json::object();
    m_commands.push_back(std::move(spec));
}

const CommandSpec* CommandRegistry::find(const std::string& name) const
{
    for (const CommandSpec& spec : m_commands) {
        if (spec.name == name) return &spec;
    }
    for (const CommandSpec& spec : m_commands) {
        if (std::find(spec.aliases.begin(), spec.aliases.end(), name) != spec.aliases.end()) return &spec;
    }
    return nullptr;
}

void CommandRegistry::addSetting(SettingSpec spec)
{
    m_settings.push_back(std::move(spec));
}

const SettingSpec* CommandRegistry::findSetting(SettingScope scope, const std::string& key) const
{
    const std::string wanted = lower(key);
    for (const SettingSpec& spec : m_settings) {
        if (spec.scope != scope) continue;
        if (lower(spec.key) == wanted) return &spec;
        for (const std::string& alias : spec.aliases) {
            if (lower(alias) == wanted) return &spec;
        }
    }
    return nullptr;
}

const char* CommandRegistry::scopeName(SettingScope scope)
{
    switch (scope) {
    case SettingScope::Viewport: return "viewport";
    case SettingScope::Environment: return "environment";
    case SettingScope::Renderer: return "renderer";
    }
    return "viewport";
}

const char* CommandRegistry::typeName(SettingType type)
{
    switch (type) {
    case SettingType::Bool: return "bool";
    case SettingType::Int: return "int";
    case SettingType::Float: return "float";
    case SettingType::Enum: return "enum";
    case SettingType::Color3: return "color3";
    case SettingType::Color4: return "color4";
    }
    return "bool";
}

bool CommandRegistry::coerceSetting(const SettingSpec& spec, const nlohmann::json& value, nlohmann::json& out,
                                    std::string& error)
{
    auto checkRange = [&](double number) {
        if (spec.hasRange && (number < spec.minimum || number > spec.maximum)) {
            std::ostringstream message;
            message << spec.key << " must be in [" << spec.minimum << ", " << spec.maximum << "]";
            error = message.str();
            return false;
        }
        return true;
    };
    switch (spec.type) {
    case SettingType::Bool:
        if (value.is_boolean()) { out = value; return true; }
        if (value.is_number()) { out = value.get<double>() != 0.0; return true; }
        if (value.is_string()) {
            if (auto parsed = parseBool(value.get<std::string>())) { out = *parsed; return true; }
        }
        error = spec.key + " expects on/off (true/false)";
        return false;
    case SettingType::Int:
    case SettingType::Float: {
        std::optional<double> number;
        if (value.is_number()) number = value.get<double>();
        else if (value.is_string()) number = parseFloatText(value.get<std::string>());
        if (!number || !std::isfinite(*number)) {
            error = spec.key + " expects a number";
            return false;
        }
        if (spec.type == SettingType::Int) number = std::round(*number);
        if (!checkRange(*number)) return false;
        if (spec.type == SettingType::Int) out = static_cast<long long>(*number);
        else out = *number;
        return true;
    }
    case SettingType::Enum: {
        std::string text;
        if (value.is_string()) text = lower(value.get<std::string>());
        else if (value.is_number_integer()) text = std::to_string(value.get<long long>());
        else if (value.is_boolean()) text = value.get<bool>() ? "on" : "off";
        for (const std::string& name : spec.enumValues) {
            if (lower(name) == text) { out = name; return true; }
        }
        if (auto alias = spec.enumAliases.find(text); alias != spec.enumAliases.end()) {
            out = alias->second;
            return true;
        }
        for (const std::string& name : spec.enumValues) {
            if (!text.empty() && squash(name) == squash(text)) { out = name; return true; }
        }
        if (!text.empty() && std::all_of(text.begin(), text.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; })) {
            const size_t index = static_cast<size_t>(std::stoul(text));
            if (index < spec.enumValues.size()) { out = spec.enumValues[index]; return true; }
        }
        std::string allowed;
        for (size_t i = 0; i < spec.enumValues.size(); ++i) allowed += (i ? ", " : "") + spec.enumValues[i];
        error = spec.key + " expects one of: " + allowed;
        return false;
    }
    case SettingType::Color3:
    case SettingType::Color4: {
        auto numbers = parseNumberList(value);
        const size_t minimum = 3;
        const size_t maximum = spec.type == SettingType::Color3 ? 3 : 4;
        if (!numbers || numbers->size() < minimum || numbers->size() > maximum) {
            error = spec.key + (spec.type == SettingType::Color3 ? " expects [r, g, b]" : " expects [r, g, b] or [r, g, b, a]");
            return false;
        }
        for (double component : *numbers) {
            if (!checkRange(component)) return false;
        }
        if (spec.type == SettingType::Color4 && numbers->size() == 3) numbers->push_back(1.0);
        out = *numbers;
        return true;
    }
    }
    return false;
}

bool CommandRegistry::validateArgs(const CommandSpec& spec, nlohmann::json& args, CommandError& error)
{
    if (args.is_null()) args = nlohmann::json::object();
    if (!args.is_object()) {
        error = {"bad_arguments", "args must be a JSON object", nlohmann::json::object()};
        return false;
    }
    const nlohmann::json& properties = spec.args["properties"];
    for (auto it = args.begin(); it != args.end(); ++it) {
        if (!properties.contains(it.key())) {
            nlohmann::json known = nlohmann::json::array();
            for (auto p = properties.begin(); p != properties.end(); ++p) known.push_back(p.key());
            error = {"unknown_argument", "unknown argument '" + it.key() + "' for " + spec.name, {{"known", known}}};
            return false;
        }
        const std::string type = jsonTypeOf(properties[it.key()]);
        if (!it.value().is_null() && !typeMatches(type, it.value())) {
            // Text values from scripts and loosely typed clients are accepted when they parse.
            if (it.value().is_string()) {
                if (auto coerced = coerceText(properties[it.key()], it.value().get<std::string>())) {
                    it.value() = *coerced;
                    continue;
                }
            }
            error = {"bad_arguments", "argument '" + it.key() + "' must be of type " + type, nlohmann::json::object()};
            return false;
        }
    }
    if (spec.args.contains("required")) {
        for (const auto& required : spec.args["required"]) {
            const std::string key = required.get<std::string>();
            if (!args.contains(key) || args[key].is_null()) {
                error = {"missing_argument", "missing argument '" + key + "' for " + spec.name, nlohmann::json::object()};
                return false;
            }
        }
    }
    for (auto p = properties.begin(); p != properties.end(); ++p) {
        if (!args.contains(p.key()) && p.value().contains("default")) args[p.key()] = p.value()["default"];
    }
    return true;
}

std::optional<nlohmann::json> CommandRegistry::parseText(const CommandSpec& spec, const std::vector<std::string>& tokens,
                                                         const std::string& line, std::string& error)
{
    nlohmann::json args = nlohmann::json::object();
    const nlohmann::json& properties = spec.args["properties"];
    for (size_t i = 0; i < spec.positional.size(); ++i) {
        const std::string& key = spec.positional[i];
        const bool last = i + 1 == spec.positional.size();
        if (i + 1 >= tokens.size()) break;
        std::string token = tokens[i + 1];
        if (last && spec.restIsText) {
            // The rest of the line after the command and the earlier positionals.
            size_t pos = 0;
            for (size_t skipped = 0; skipped <= i; ++skipped) {
                pos = line.find_first_not_of(" \t", pos);
                pos = line.find_first_of(" \t", pos);
            }
            pos = line.find_first_not_of(" \t", pos);
            token = pos == std::string::npos ? std::string() : line.substr(pos);
            while (!token.empty() && (token.back() == ' ' || token.back() == '\t')) token.pop_back();
            if (token.size() >= 2 && token.front() == '"' && token.back() == '"') token = token.substr(1, token.size() - 2);
        }
        const nlohmann::json schema = properties.contains(key) ? properties[key] : nlohmann::json::object();
        auto value = coerceText(schema, token);
        if (!value) {
            error = "invalid value '" + token + "' for " + key + ": " + line;
            return std::nullopt;
        }
        args[key] = *value;
    }
    if (spec.args.contains("required")) {
        for (const auto& required : spec.args["required"]) {
            if (!args.contains(required.get<std::string>())) {
                error = "missing arguments: " + line;
                return std::nullopt;
            }
        }
    }
    return args;
}

void CommandRegistry::dispatch(const CommandSpec& spec, nlohmann::json args, const CommandContext& context, ReplyFn reply)
{
    CommandError error;
    if (!validateArgs(spec, args, error)) {
        CommandReply failed;
        failed.ok = false;
        failed.error = error;
        reply(failed);
        return;
    }
    CommandOutcome outcome = [&]() -> CommandOutcome {
        try {
            return spec.run(args, context);
        } catch (const std::exception& e) {
            return CommandReply::failure("internal_error", std::string("command failed: ") + e.what());
        }
    }();
    if (outcome.reply) {
        reply(*outcome.reply);
        return;
    }
    Job job;
    job.name = spec.name;
    job.step = std::move(outcome.step);
    job.reply = std::move(reply);
    job.deadline = nowSeconds() + (spec.timeoutSeconds > 0.0 ? spec.timeoutSeconds : 60.0);
    m_jobs.push_back(std::move(job));
}

void CommandRegistry::tick()
{
    if (m_jobs.empty()) return;
    Job& job = m_jobs.front();
    std::optional<CommandReply> reply;
    try {
        reply = job.step();
    } catch (const std::exception& e) {
        reply = CommandReply::failure("internal_error", std::string("command failed: ") + e.what());
    }
    if (!reply && nowSeconds() > job.deadline) {
        reply = CommandReply::failure("timeout", job.name + " did not finish in time");
    }
    if (reply) {
        ReplyFn callback = std::move(job.reply);
        m_jobs.pop_front();
        callback(*reply);
    }
}

void CommandRegistry::cancelAll(const std::string& code, const std::string& message)
{
    while (!m_jobs.empty()) {
        ReplyFn callback = std::move(m_jobs.front().reply);
        m_jobs.pop_front();
        callback(CommandReply::failure(code, message));
    }
}

nlohmann::json CommandRegistry::describe() const
{
    nlohmann::json commands = nlohmann::json::array();
    for (const CommandSpec& spec : m_commands) {
        nlohmann::json entry;
        entry["name"] = spec.name;
        entry["description"] = spec.help;
        entry["kind"] = spec.kind == CommandKind::Immediate ? "immediate" : "multi-frame";
        if (spec.kind == CommandKind::MultiFrame) entry["timeoutSeconds"] = spec.timeoutSeconds;
        entry["inputSchema"] = spec.args;
        if (!spec.aliases.empty()) entry["scriptAliases"] = spec.aliases;
        if (!spec.positional.empty()) entry["scriptPositional"] = spec.positional;
        commands.push_back(std::move(entry));
    }
    return commands;
}

std::string CommandRegistry::markdownReference() const
{
    std::ostringstream out;
    out << "## Command reference (generated from the registry)\n\n";
    out << "| Command | Kind | Arguments | Script form | Description |\n|---|---|---|---|---|\n";
    for (const CommandSpec& spec : m_commands) {
        std::string arguments;
        const nlohmann::json& properties = spec.args["properties"];
        std::vector<std::string> required;
        if (spec.args.contains("required")) {
            for (const auto& r : spec.args["required"]) required.push_back(r.get<std::string>());
        }
        for (auto p = properties.begin(); p != properties.end(); ++p) {
            const bool isRequired = std::find(required.begin(), required.end(), p.key()) != required.end();
            const std::string type = jsonTypeOf(p.value());
            arguments += (arguments.empty() ? "" : ", ") + std::string("`") + p.key() + (isRequired ? "" : "?") +
                         (type.empty() ? "" : ": " + type) + "`";
        }
        std::string script = "`" + spec.name;
        for (const std::string& positional : spec.positional) script += " <" + positional + ">";
        script += "`";
        for (const std::string& alias : spec.aliases) script += ", `" + alias + "`";
        out << "| `" << spec.name << "` | " << (spec.kind == CommandKind::Immediate ? "immediate" : "multi-frame") << " | "
            << (arguments.empty() ? "-" : cell(arguments)) << " | " << cell(script) << " | " << cell(spec.help) << " |\n";
    }
    out << "\n## Settings reference (generated from the registry)\n\n";
    out << "`set_viewport_setting` / `get_viewport_setting` take the viewport and renderer keys, "
           "`set_environment` / `get_environment` the environment keys; `list_settings` returns them all "
           "with their current values.\n\n";
    out << "| Key | Scope | Type | Values | Default | CLI | Description |\n|---|---|---|---|---|---|---|\n";
    for (const SettingSpec& spec : m_settings) {
        std::string values;
        if (spec.type == SettingType::Enum) {
            for (size_t i = 0; i < spec.enumValues.size(); ++i) values += (i ? ", " : "") + spec.enumValues[i];
        } else if (spec.hasRange) {
            std::ostringstream range;
            range << spec.minimum << " .. " << spec.maximum;
            values = range.str();
        } else if (spec.type == SettingType::Bool) {
            values = "on, off";
        }
        out << "| `" << spec.key << "` | " << scopeName(spec.scope) << " | " << typeName(spec.type) << " | "
            << (values.empty() ? "-" : cell(values)) << " | `" << spec.defaultValue.dump() << "` | "
            << (spec.cliFlag.empty() ? "-" : "`" + cell(spec.cliFlag) + "`") << " | " << cell(spec.help)
            << (spec.persisted ? " (saved in settings.ini)" : "") << " |\n";
    }
    return out.str();
}

std::string base64Encode(const std::vector<uint8_t>& bytes)
{
    static constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((bytes.size() + 2) / 3) * 4);
    size_t i = 0;
    for (; i + 2 < bytes.size(); i += 3) {
        const uint32_t v = (uint32_t(bytes[i]) << 16) | (uint32_t(bytes[i + 1]) << 8) | bytes[i + 2];
        out += kAlphabet[(v >> 18) & 63];
        out += kAlphabet[(v >> 12) & 63];
        out += kAlphabet[(v >> 6) & 63];
        out += kAlphabet[v & 63];
    }
    if (i < bytes.size()) {
        uint32_t v = uint32_t(bytes[i]) << 16;
        if (i + 1 < bytes.size()) v |= uint32_t(bytes[i + 1]) << 8;
        out += kAlphabet[(v >> 18) & 63];
        out += kAlphabet[(v >> 12) & 63];
        out += i + 1 < bytes.size() ? kAlphabet[(v >> 6) & 63] : '=';
        out += '=';
    }
    return out;
}

} // namespace ux3d::imgui::agent
