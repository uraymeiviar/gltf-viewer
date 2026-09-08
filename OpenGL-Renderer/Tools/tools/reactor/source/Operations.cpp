// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "reactor/Operations.h"
#include "reactor/Json.h"
#include "reactor/Utils.h"
#include <regex>
#include <string>

using namespace ux3d;
using namespace ux3d::reactor;
using namespace std::string_literals;

nlohmann::json operations::byName(const nlohmann::json& librarySchema)
{
    auto operationsByName = nlohmann::json::object();
    const auto& operations = librarySchema.at("oneOf");
    for (const auto& operation : operations) {
        // extract the properties from the subschema
        const auto& properties = operation.at("properties");
        const auto& operationName = properties.begin().key();
        const auto& parameters = properties.at(operationName);
        // ensure the property at operationName is an array that we can push to
        if (!operationsByName.contains(operationName)) {
            operationsByName[operationName] = nlohmann::json::array();
        }
        // some operations contain variants for the accepted parameters --> add all variants individually
        if (parameters.contains("oneOf")) {
            for (auto parametersVariant : parameters.at("oneOf")) {
                // add title and description to each oneOf-variant since they will be treated as individual operation schemas from now on
                if (!parametersVariant.contains("title") && parameters.contains("title")) {
                    parametersVariant["title"] = parameters.at("title");
                }
                if (!parametersVariant.contains("description") && parameters.contains("description")) {
                    parametersVariant["description"] = parameters.at("description");
                }

                operationsByName[operationName].emplace_back(parametersVariant);
            }
        }
        // add the parameter schema
        else {
            operationsByName[operationName].emplace_back(parameters);
        }
    }
    return operationsByName;
}

nlohmann::json operations::byPathPattern(const nlohmann::json& operationsByName)
{
    auto operationsByPathPattern = nlohmann::json::object();

    for (const auto& [name, parameterSchemas] : operationsByName.items()) {
        for (const auto& parameterSchema : parameterSchemas) {
            if (!parameterSchema.contains("properties")) {
                // ignore operations without parameters
                continue;
            }
            const auto& properties = parameterSchema.at("properties");
            std::string strippedPattern = "";
            if (!properties.contains("path") || !properties.at("path").contains("pattern")) {
                // ignore operations without a "path" parameter
                // continue;
                strippedPattern = "";
            } else {
                std::string pattern = properties.at("path").at("pattern");
                // Remove ^( and )$ from regex pattern
                strippedPattern = pattern.substr(2, pattern.size() - 2 - 2);
            }

            // ensure we have an array to insert into
            if (!operationsByPathPattern.contains(strippedPattern)) {
                operationsByPathPattern[strippedPattern] = nlohmann::json::object();
            }
            // ensure the array only contains unique elements
            if (!operationsByPathPattern[strippedPattern].contains(name)) {
                operationsByPathPattern[strippedPattern][name] = nlohmann::json::array();
            }
            operationsByPathPattern[strippedPattern][name].emplace_back(parameterSchema);
        }
    }
    return operationsByPathPattern;
}

nlohmann::json operations::global(const nlohmann::json& operationsByName)
{
    auto globalOperations = nlohmann::json::object();

    for (const auto& [name, parameterSchemas] : operationsByName.items()) {
        for (const auto& parameterSchema : parameterSchemas) {
            if (parameterSchema.contains("properties") && parameterSchema.at("properties").contains("path")) {
                continue;
            }
            if (!globalOperations.contains(name)) {
                globalOperations[name] = nlohmann::json::array();
            }
            globalOperations[name].emplace_back(parameterSchema);
        }
    }
    return globalOperations;
}

nlohmann::json::object_t operations::byPath(std::string path, const nlohmann::json& operationsByPathPattern)
{
    reactor::utils::searchAndReplace(path, "\\d+", "0"); // handle patterns as well
    nlohmann::json collection
        = nlohmann::json::object();
    for (const auto& [pattern, schema] : operationsByPathPattern.items()) {
        std::regex regex { "^"s + pattern + "$"s };
        std::smatch match;
        if (std::regex_search(path, match, regex)) {
            collection = reactor::json::merge(collection, schema);
        }
    }

    return collection;
}
