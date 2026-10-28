// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "gltf/extensions/AsoboExtensions.h"

#include "gltf/Gltf.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace ux3d::gltf::asobo {

namespace {

const char* typeName(ValueType type)
{
    switch (type) {
    case ValueType::Boolean: return "a boolean";
    case ValueType::Number: return "a number";
    case ValueType::String: return "a string";
    case ValueType::NumberArray: return "an array of numbers";
    case ValueType::Texture: return "a textureInfo object";
    case ValueType::Json: return "JSON";
    }
    return "?";
}

std::string formatNumber(double value)
{
    std::string text = nlohmann::json(value).dump();
    return text;
}

std::string escapePointerToken(std::string_view token)
{
    std::string escaped;
    for (const char c : token) {
        if (c == '~') {
            escaped += "~0";
        } else if (c == '/') {
            escaped += "~1";
        } else {
            escaped += c;
        }
    }
    return escaped;
}

template <typename T>
const T* get(const Value* value)
{
    return value != nullptr ? std::get_if<T>(value) : nullptr;
}

} // namespace

const PropertySpec* ExtensionSpec::findProperty(std::string_view key) const
{
    for (const PropertySpec& property : properties) {
        if (property.name == key || (!property.alias.empty() && property.alias == key)) {
            return &property;
        }
    }
    return nullptr;
}

const ExtensionSpec* findExtensionSpec(std::string_view name)
{
    static const std::unordered_map<std::string_view, const ExtensionSpec*> index = [] {
        std::unordered_map<std::string_view, const ExtensionSpec*> map;
        for (const ExtensionSpec& spec : extensionSpecs()) {
            map.emplace(spec.name, &spec);
        }
        return map;
    }();
    const auto it = index.find(name);
    return it != index.end() ? it->second : nullptr;
}

bool isAsoboExtension(std::string_view name)
{
    return name.substr(0, 6) == "ASOBO_";
}

bool ExtensionData::isExplicit(std::string_view key) const
{
    return std::find(m_explicitKeys.begin(), m_explicitKeys.end(), key) != m_explicitKeys.end();
}

const Value* ExtensionData::value(std::string_view key) const
{
    for (const auto& [name, value] : m_values) {
        if (name == key) {
            return &value;
        }
    }
    return nullptr;
}

std::optional<bool> ExtensionData::boolean(std::string_view key) const
{
    const bool* value = get<bool>(this->value(key));
    return value != nullptr ? std::optional<bool>(*value) : std::nullopt;
}

std::optional<double> ExtensionData::number(std::string_view key) const
{
    const double* value = get<double>(this->value(key));
    return value != nullptr ? std::optional<double>(*value) : std::nullopt;
}

const std::string* ExtensionData::string(std::string_view key) const
{
    return get<std::string>(value(key));
}

const std::vector<double>* ExtensionData::numbers(std::string_view key) const
{
    return get<std::vector<double>>(value(key));
}

const TextureRef* ExtensionData::texture(std::string_view key) const
{
    return get<TextureRef>(value(key));
}

const nlohmann::json* ExtensionData::json(std::string_view key) const
{
    return get<nlohmann::json>(value(key));
}

const ExtensionData* ExtensionSet::find(std::string_view name) const
{
    for (const ExtensionData& extension : m_extensions) {
        if (extension.name() == name) {
            return &extension;
        }
    }
    return nullptr;
}

class Parser {
public:
    Parser(const std::string& pointer, std::vector<Diagnostic>* diagnostics)
        : m_pointer(pointer)
        , m_diagnostics(diagnostics)
    {
    }

    void report(Diagnostic::Severity severity, const char* code, const std::string& pointer, std::string message)
    {
        if (m_diagnostics != nullptr) {
            m_diagnostics->push_back({severity, code, pointer, std::move(message)});
        }
    }

    // Converts one JSON value to the property's type; returns false (with a diagnostic) when it does not fit.
    bool convert(const PropertySpec& property, const nlohmann::json& json, const std::string& pointer, Value& out)
    {
        switch (property.type) {
        case ValueType::Boolean:
            if (json.is_boolean()) {
                out = json.get<bool>();
                return true;
            }
            break;
        case ValueType::Number:
            if (json.is_number()) {
                double number = json.get<double>();
                if (!std::isfinite(number)) {
                    break;
                }
                const double clamped = std::clamp(number, property.minimum.value_or(-INFINITY), property.maximum.value_or(INFINITY));
                if (clamped != number) {
                    report(Diagnostic::Severity::Warning, "ASOBO_VALUE_RANGE", pointer,
                           "Value " + formatNumber(number) + " is outside the schema range; clamped to " + formatNumber(clamped) + ".");
                }
                out = clamped;
                return true;
            }
            break;
        case ValueType::String:
            if (json.is_string()) {
                out = json.get<std::string>();
                return true;
            }
            break;
        case ValueType::NumberArray:
            if (json.is_array() && std::all_of(json.begin(), json.end(), [](const nlohmann::json& item) { return item.is_number(); })) {
                const uint32_t size = static_cast<uint32_t>(json.size());
                if ((property.minItems != 0 && size < property.minItems) || (property.maxItems != 0 && size > property.maxItems)) {
                    report(Diagnostic::Severity::Warning, "ASOBO_VALUE_RANGE", pointer, "Array has " + std::to_string(size) + " items; the schema expects "
                               + std::to_string(property.minItems) + (property.maxItems != property.minItems ? "-" + std::to_string(property.maxItems) : "") + ". The default is used.");
                    return false;
                }
                out = json.get<std::vector<double>>();
                return true;
            }
            break;
        case ValueType::Texture:
            if (json.is_object() && json.contains("index") && json["index"].is_number_integer() && json["index"].get<int64_t>() >= 0) {
                TextureRef texture;
                texture.index = static_cast<int32_t>(json["index"].get<int64_t>());
                if (json.contains("texCoord") && json["texCoord"].is_number_integer()) {
                    texture.texCoord = static_cast<int32_t>(json["texCoord"].get<int64_t>());
                }
                for (const char* scaleKey : {"scale", "strength"}) {
                    if (json.contains(scaleKey) && json[scaleKey].is_number()) {
                        texture.scale = json[scaleKey].get<double>();
                    }
                }
                if (json.contains("extensions")) {
                    texture.extensions = json["extensions"];
                }
                out = std::move(texture);
                return true;
            }
            break;
        case ValueType::Json:
            out = json;
            return true;
        }
        report(Diagnostic::Severity::Warning, "ASOBO_VALUE_TYPE", pointer, std::string("Expected ") + typeName(property.type) + "; the default is used.");
        return false;
    }

    ExtensionData parse(const std::string& name, const std::string& text, const ExtensionSpec* spec)
    {
        ExtensionData data;
        data.m_name = name;
        data.m_spec = spec;
        const std::string extensionPointer = m_pointer + "/extensions/" + escapePointerToken(name);

        nlohmann::json json = nlohmann::json::parse(text, nullptr, false);
        if (json.is_discarded() || !json.is_object()) {
            report(Diagnostic::Severity::Warning, "ASOBO_EXTENSION_MALFORMED", extensionPointer, "The extension is not a JSON object; its defaults are used.");
            json = nlohmann::json::object();
        }

        if (spec != nullptr) {
            for (const PropertySpec& property : spec->properties) {
                Value value = property.defaultValue;
                auto it = json.find(std::string(property.name));
                if (it == json.end() && !property.alias.empty()) {
                    it = json.find(std::string(property.alias));
                }
                if (it != json.end()) {
                    Value parsed;
                    if (convert(property, *it, extensionPointer + "/" + escapePointerToken(it.key()), parsed)) {
                        value = std::move(parsed);
                        data.m_explicitKeys.emplace_back(property.name);
                    }
                }
                if (!std::holds_alternative<std::monostate>(value)) {
                    data.m_values.emplace_back(std::string(property.name), std::move(value));
                }
            }
        }
        for (auto it = json.begin(); it != json.end(); ++it) {
            if (it.key() == "extensions" || it.key() == "extras") {
                continue;
            }
            if (spec == nullptr || spec->findProperty(it.key()) == nullptr) {
                data.m_unknownKeys[it.key()] = it.value();
            }
        }
        return data;
    }

    ExtensionSet parseObject(const Gltf& gltf, const GltfObject& object, uint32_t scope)
    {
        ExtensionSet set;
        // Sorted so the order (and the diagnostics) do not depend on hash-map iteration.
        std::vector<std::pair<std::string, std::string>> extensions;
        for (const auto& [name, text] : object.getUnknownExtensions()) {
            if (isAsoboExtension(name.c_str())) {
                extensions.emplace_back(name.c_str(), text.c_str());
            }
        }
        std::sort(extensions.begin(), extensions.end());

        for (const auto& [name, text] : extensions) {
            const std::string extensionPointer = m_pointer + "/extensions/" + escapePointerToken(name);
            if (!gltf.isExtensionDeclared(name)) {
                report(Diagnostic::Severity::Info, "ASOBO_EXTENSION_UNDECLARED", extensionPointer, name + " is used but not declared in extensionsUsed; it is ignored.");
                continue;
            }
            const ExtensionSpec* spec = findExtensionSpec(name);
            if (spec == nullptr) {
                report(Diagnostic::Severity::Info, "ASOBO_EXTENSION_NO_SCHEMA", extensionPointer, name + " has no schema; its values are kept as they are.");
            } else if ((spec->scopes & scope) == 0) {
                report(Diagnostic::Severity::Warning, "ASOBO_EXTENSION_SCOPE", extensionPointer, name + " is not expected on this kind of object.");
            }
            set.m_extensions.push_back(parse(name, text, spec));
        }
        return set;
    }

private:
    std::string m_pointer;
    std::vector<Diagnostic>* m_diagnostics;
};

ExtensionSet parseExtensions(const Gltf& gltf, const GltfObject& object, uint32_t scope,
                             const std::string& pointer, std::vector<Diagnostic>* diagnostics)
{
    return Parser(pointer, diagnostics).parseObject(gltf, object, scope);
}

std::vector<Diagnostic> validateExtensions(const Gltf& gltf)
{
    std::vector<Diagnostic> diagnostics;
    parseExtensions(gltf, gltf, Scope::Root, "", &diagnostics);
    parseExtensions(gltf, gltf.getAsset(), Scope::Asset, "/asset", &diagnostics);
    auto parseAll = [&](const auto& elements, uint32_t scope, const char* collection) {
        size_t index = 0;
        for (const auto& element : elements) {
            parseExtensions(gltf, element, scope, std::string("/") + collection + "/" + std::to_string(index++), &diagnostics);
        }
    };
    parseAll(gltf.getElements<Material>(), Scope::Material, "materials");
    parseAll(gltf.getElements<Node>(), Scope::Node, "nodes");
    parseAll(gltf.getElements<Mesh>(), Scope::Mesh, "meshes");
    parseAll(gltf.getElements<Animation>(), Scope::Animation, "animations");
    return diagnostics;
}

double SkyPortal::innerCircleRadiusMetres() const
{
    constexpr double halfDegree = 3.14159265358979323846 / 360.0;
    const double outer = std::clamp(outerConeDeg, 1.0, 179.0);
    const double inner = std::clamp(innerConeDeg, 0.0, outer);
    return radiusMetres() * std::sin(inner * halfDegree) / std::sin(outer * halfDegree);
}

std::optional<SkyPortal> readSkyPortal(const ExtensionSet& nodeExtensions)
{
    const ExtensionData* data = nodeExtensions.find("ASOBO_sky_portal");
    if (data == nullptr) {
        return std::nullopt;
    }
    SkyPortal portal;
    portal.shapeType = static_cast<int>(std::lround(data->number("shape_type").value_or(1.0)));
    portal.sourceRadiusCm = std::max(0.0, data->number("source_radius").value_or(portal.sourceRadiusCm));
    portal.outerConeDeg = std::clamp(data->number("outer_cone_angle").value_or(portal.outerConeDeg), 0.0, 180.0);
    portal.innerConeDeg = std::clamp(data->number("inner_cone_angle").value_or(portal.innerConeDeg), 0.0, portal.outerConeDeg);
    return portal;
}

} // namespace ux3d::gltf::asobo
