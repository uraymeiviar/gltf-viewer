#include "operations/Schema.h"

using namespace ux3d;
using namespace ux3d::operations;

nlohmann::json schema::anyOf(const nlohmann::json schemaArray)
{
    // TODO: Why not `anyOf`?
    return {
        { "oneOf", schemaArray }
    };
}

nlohmann::json schema::oneOf(const nlohmann::json schemaArray)
{
    return {
        { "oneOf", schemaArray }
    };
}

nlohmann::json schema::pointer(const std::string& pattern)
{
    return {
        { "type", "string" },
        { "format", "json-pointer" },
        { "pattern", "^(" + pattern + ")$" }
    };
}

nlohmann::json schema::enumeration(const nlohmann::json enumeration, const std::optional<nlohmann::json> defaultValue)
{

    auto schema = nlohmann::json::object();
    schema["enum"] = enumeration;

    if (defaultValue.has_value()) {
        schema["default"] = defaultValue.value();
    }
    return schema;
}

nlohmann::json schema::enumerationSet(const nlohmann::json& set)
{
    return {
        { "type", "array" },
        { "items", { { "enum", set["enum"] } } },
        { "minItems", 1 },
        { "uniqueItems", true }
    };
}

nlohmann::json schema::uuid()
{
    return {
        { "type", "string" },
        { "pattern", "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$" }
    };
}

nlohmann::json schema::string(const std::string& pattern, const std::optional<std::string> defaultValue)
{
    auto schema = nlohmann::json::object();
    schema["type"] = "string";
    if (pattern.length() > 0) {
        schema["pattern"] = "^" + pattern + "$";
    }
    if (defaultValue.has_value()) {
        schema["default"] = defaultValue.value();
    }
    return schema;
}

nlohmann::json schema::number(const std::optional<double> defaultValue, const std::optional<double> minimum, const std::optional<double> maximum, bool exclusiveMinimum, bool exclusiveMaximum)
{
    auto schema = nlohmann::json::object();
    schema["type"] = "number";

    if (defaultValue) {
        schema["default"] = *defaultValue;
    }
    if (minimum) {
        if (exclusiveMinimum) {
            schema["exclusiveMinimum"] = *minimum;
        } else {
            schema["minimum"] = *minimum;
        }
    }
    if (maximum) {
        if (exclusiveMaximum) {
            schema["exclusiveMaximum"] = *maximum;
        } else {
            schema["maximum"] = *maximum;
        }
    }

    return schema;
}

nlohmann::json schema::integer(const std::optional<int> defaultValue, const std::optional<int> minimum, const std::optional<int> maximum)
{
    auto schema = nlohmann::json::object();
    schema["type"] = "integer";

    if (defaultValue) {
        schema["default"] = *defaultValue;
    }
    if (minimum) {
        schema["minimum"] = *minimum;
    }
    if (maximum) {
        schema["maximum"] = *maximum;
    }

    return schema;
}

nlohmann::json schema::boolean(const std::optional<bool> defaultValue)
{
    auto schema = nlohmann::json::object();
    schema["type"] = "boolean";

    if (defaultValue.has_value()) {
        schema["default"] = defaultValue.value();
    }
    return schema;
}

nlohmann::json schema::any()
{
    return nlohmann::json::object();
}

nlohmann::json schema::array(const nlohmann::json& items, std::optional<uint32_t> minItems, std::optional<uint32_t> maxItems)
{
    nlohmann::json schema = {
        { "type", "array" },
        { "items", items },
        { "additionalItems", false }
    };

    if (minItems.has_value()) {
        schema["minItems"] = minItems.value();
    }

    if (maxItems.has_value()) {
        schema["maxItems"] = maxItems.value();
    }

    return schema;
}

nlohmann::json schema::set(const nlohmann::json& items)
{
    return {
        { "type", "array" },
        { "items", items },
        { "additionalItems", false },
        { "uniqueItems", true }
    };
}

nlohmann::json schema::object(const std::map<std::string, nlohmann::json>& requiredKeys, const std::map<std::string, nlohmann::json>& optionalKeys, bool additionalProperties)
{
    nlohmann::json schema = {
        { "type", "object" },
        { "required", nlohmann::json::array() },
        { "properties", nlohmann::json::object() },
        { "additionalProperties", additionalProperties }
    };

    for (const auto& [name, paramSchema] : requiredKeys) {
        schema["properties"][name] = paramSchema;
        schema["required"].emplace_back(name);
    }

    for (const auto& [name, paramSchema] : optionalKeys) {
        schema["properties"][name] = paramSchema;
    }

    return schema;
}

nlohmann::json schema::operation(const std::string& title, const std::string& description, const nlohmann::json& schema)
{
    auto metaSchema = schema;
    metaSchema["title"] = title;
    metaSchema["description"] = description;
    return metaSchema;
}

nlohmann::json schema::properties(const nlohmann::json& properties)
{
    return { { "properties", properties } };
}

nlohmann::json schema::parameter::path(const std::string& pattern, const std::string& key)
{
    return nlohmann::json::array({ key, schema::pointer(pattern) });
}

nlohmann::json schema::parameter::uuid()
{
    return nlohmann::json::array({ "uuid", schema::uuid() });
}
