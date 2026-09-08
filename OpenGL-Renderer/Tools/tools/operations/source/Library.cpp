// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "operations/Library.h"
#include "operations/Operations.h"
#include "reactor/Reactor.h"

#include <regex>

using namespace ux3d;
using namespace ux3d::operations;

Library::Library()
{
    reset();
}

nlohmann::json Library::call(const nlohmann::json& taskDescription, nlohmann::json&& state, datastorage::DataStorage& dataStorage, std::function<void(const nlohmann::json&)> callback, bool useGeneric) const
{
    // Select the operation based on the task description
    auto id = taskDescription.begin().key();

    auto operationIt = m_operationsMap.find(id);
    if (operationIt == m_operationsMap.end()) {
        throw std::invalid_argument("Scheduled operation (" + id + ") not found in available operations: " + std::to_string(m_operationsMap.size()) + " operations loaded");
    }

    auto& operationVector = operationIt->second;
    auto& schemaVector = m_operationSchemas.at(id);
    size_t vectorSize = operationVector.size();
    if (vectorSize != schemaVector.size()) {
        throw std::runtime_error("Library: Initializing of operation vectors failed");
    }

    // Extract the parameters from the task description
    auto params = taskDescription[id];
    size_t i = useGeneric ? vectorSize - 1 : 0;
    bool operationCalled = false;

    // First execute all non generice variants (this allows multiple overloads)
    for (; i < vectorSize - 1; ++i) {
        // Validate the parameters
        auto& validator = schemaVector[i];
        auto patch = nlohmann::json::array({});
        try {
            patch = validator.validate(params);
        } catch (const std::invalid_argument& e) {
            continue;
        }

        // Fill default values
        if (patch.size() != 0) {
            params = params.patch(patch);
        }

        state = operationVector[i].operation(params, std::move(state), dataStorage, *this, callback);
        operationCalled = true;
    }

    if (operationCalled) {
        return state;
    }
    // If no specific variant was called, try the generic one
    // Validate the parameters
    auto& validator = schemaVector.back();
    auto patch = nlohmann::json::array({});
    try {
        patch = validator.validate(params);
    } catch (const std::invalid_argument& e) {
        // Construct a readable error message
        std::string paramsString;
        if (params.is_object()) {
            for (auto& [key, value] : params.items()) {
                paramsString += key + "=" + value.dump() + ", ";
            }
        }
        // remove trailing characters (", ")
        if (!paramsString.empty()) {
            paramsString.pop_back();
            paramsString.pop_back();
        }

        throw std::invalid_argument("Invalid parameters for operation " + operationVector.back().name + ": " + paramsString + "  -  " + e.what());
    }

    // Fill default values
    if (patch.size() != 0) {
        params = params.patch(patch);
    }

    return operationVector.back().operation(params, std::move(state), dataStorage, *this, callback);
}

void Library::loadPlugin(const std::vector<Operation>& pluginOperations)
{
    auto operationSchemas = nlohmann::json::array();

    for (const auto& pluginOperation : pluginOperations) {
        // Add the operation to the library
        auto& operation = m_operationsMap[pluginOperation.name].emplace_back(pluginOperation);

        // Create a schema for all operations by putting them in one big schema "oneOf" array
        operationSchemas.emplace_back(nlohmann::json::object({ { "properties", nlohmann::json::object({ { operation.name, operation.schema } }) } }));

        // Create the validator for the operation inputs once after loading the operations so that
        // runtime validation inside the execute function uses the fast preinitialized validator.
        auto& schema = m_operationSchemas[operation.name].emplace_back(nlohmann::json_schema::json_validator(nullptr, [](const std::string& format, const std::string& value) {
            if (format == "json-pointer") {
                std::regex regex { "^(?:\\/(?:[^~\\/]|~0|~1)*)*$" };

                std::smatch match;
                if (!std::regex_search(value, match, regex)) {
                    throw std::invalid_argument("Value is not in json-pointer format");
                }
            } else {
                nlohmann::json_schema::default_string_format_check(format, value);
            }
        }));
        schema.set_root_schema(operation.schema);
    }

    std::copy(operationSchemas.begin(), operationSchemas.end(), std::back_inserter(this->schema["oneOf"]));
}

void Library::reset()
{
    m_operationSchemas.clear();
    m_operationsMap.clear();

    // Construct the schema for the whole library
    this->schema = nlohmann::json::object(
        { { "title", "Task Description" },
            { "description", "The task description with which operations in the library can be called." },
            { "type", "object" },
            { "oneOf", nlohmann::json::array() } });
}
