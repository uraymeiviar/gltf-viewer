// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "reactor/Schema.h"
#include "reactor/Json.h"
#include <deque>
#include <regex>
#include <string>

using namespace ux3d;
using namespace ux3d::reactor;
using namespace std::string_literals;

nlohmann::json schema::resolveRefs(const nlohmann::json& schema, const std::string& rootKey)
{
    std::function<nlohmann::json(nlohmann::json)> traverse;
    traverse = [&](nlohmann::json subSchema) -> nlohmann::json {
        // follow $ref references
        if (subSchema.contains("$ref")) {
            auto reference = subSchema.at("$ref").get<std::string>();
            auto referencedSchema = traverse(schema.at(reference));
            subSchema.erase("$ref");
            subSchema.merge_patch(referencedSchema);
        }

        // traverse into sub schemas
        if (subSchema.contains("allOf")) {
            auto allOfs = subSchema.at("allOf");
            for (int i = 0; i < allOfs.size(); ++i) {
                subSchema["allOf"][i] = traverse(allOfs[i]);
            }
        }

        if (subSchema.contains("anyOf")) {
            auto anyOfs = subSchema.at("anyOf");
            for (int i = 0; i < anyOfs.size(); ++i) {
                subSchema["anyOf"][i] = traverse(anyOfs[i]);
            }
        }
        // glTF doesn't use "one of" (XOR) properties

        // objects require traversal into the key value pairs (and matching them with the schema)
        if (subSchema.contains("type") && subSchema.at("type") == "object") {
            if (subSchema.contains("properties")) {
                auto properties = subSchema.at("properties");
                for (auto& [key, value] : properties.items()) {
                    subSchema["properties"][key] = traverse(value);
                }
            }

            if (subSchema.contains("additionalProperties")) {
                auto properties = subSchema.at("additionalProperties");
                subSchema["additionalProperties"] = traverse(properties);
            }

            // glTF doesn't use "pattern properties"
        }

        // schemas for array items are available under the "items" property in the schema
        if (subSchema.contains("type") && subSchema.at("type") == "array") {
            if (subSchema.contains("items")) {
                size_t numItems = subSchema.at("items").size();
                for (int i = 0; i < subSchema.at("items").size(); ++i) {
                    subSchema["items"] = traverse(subSchema.at("items"));
                }
            }
        }

        return subSchema;
    };

    return traverse(schema.at(rootKey));
}

nlohmann::json schema::linearize(const nlohmann::json& schema)
{
    // Traverse the glTF and extract all subschemas for primitive properties together with a json pointer
    // pattern corresponding to the value
    std::function<nlohmann::json(const nlohmann::json&, const std::string&)> traverse;
    traverse = [&](const nlohmann::json& subSchema, const std::string& pointerPattern) -> nlohmann::json {
        nlohmann::json linearized = nlohmann::json::object();

        // traverse into sub schemas
        if (subSchema.contains("allOf")) {
            for (auto& allOfItem : subSchema.at("allOf")) {
                auto childZipped = traverse(allOfItem, pointerPattern);
                linearized.merge_patch(childZipped);
            }
        }

        // traverse into sub schemas
        if (subSchema.contains("anyOf")) {
            for (auto& anyOfItem : subSchema.at("anyOf")) {
                auto childZipped = traverse(anyOfItem, pointerPattern);
                linearized.merge_patch(childZipped);
            }
        }

        // TODO: "oneOf properties"

        // objects require traversal into the key value pairs (and matching them with the schema)
        if (subSchema.contains("type") && subSchema.at("type") == "object") {
            if (subSchema.contains("properties")) {
                auto properties = subSchema.at("properties");
                for (auto& [key, value] : properties.items()) {
                    auto childZipped = traverse(value, pointerPattern + "/" + key);
                    linearized.merge_patch(childZipped);
                }
            }
        }

        // schemas for array items are available under the "items" property in the schema
        else if (subSchema.contains("type") && subSchema.at("type") == "array") {
            if (subSchema.contains("items")) {
                auto childZipped = traverse(subSchema.at("items"), pointerPattern + "/\\d+");
                linearized.merge_patch(childZipped);
            }
        }

        linearized[pointerPattern] = subSchema;

        return linearized;
    };

    return traverse(schema, "");
}

// TODO: replace all use with json::for_each_match
void schema::patternForEach(const std::string& pattern, const nlohmann::json& json, std::function<void(const nlohmann::json::json_pointer& pointer)> func)
{
    json::for_each_match(pattern, json, func);
}

std::map<std::string, std::string> schema::mapPointersToPatterns(const nlohmann::json& linearizedSchema, const nlohmann::json& json)
{
    std::map<std::string, std::string> mapping;
    for (const auto& [pattern, schema] : linearizedSchema.items()) {
        patternForEach(pattern, json, [&, pattern = pattern](const nlohmann::json::json_pointer& pointer) {
            mapping[pointer.to_string()] = pattern;
        });
    }

    return mapping;
}

nlohmann::json schema::mapPatternsToPointers(const nlohmann::json& linearizedSchema, const nlohmann::json& json)
{
    nlohmann::json mapping;
    for (const auto& [pattern, schema] : linearizedSchema.items()) {
        patternForEach(pattern, json, [&, pattern = pattern](const nlohmann::json::json_pointer& pointer) {
            if (!mapping.contains(pattern)) {
                mapping[pattern] = nlohmann::json::array();
            }
            mapping[pattern].push_back(pointer.to_string());
        });
    }

    return mapping;
}

std::string schema::mapToPattern(const nlohmann::json& linearizedSchema, const std::string& pointer)
{
    for (const auto& [pattern, schema] : linearizedSchema.items()) {
        std::regex regex { "^"s + pattern + "$"s };
        std::smatch match;
        if (std::regex_search(pointer, match, regex)) {
            return pattern;
        }
    }
    throw std::runtime_error("Could not match pointer (" + pointer + ") to any pattern in the schema");
    return "";
}
