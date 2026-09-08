// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "DangerousOperations.h"
#include "operations/Library.h"
#include "operations/Operations.h"
#include "operations/Schema.h"
#include "reactor/Reactor.h"
#include "reactor/Schema.h"

#include <fmt/format.h>
#include <string>

using namespace ux3d;
using namespace ux3d::operations;

const Operation dangerous::setValue = {
    .name = "dangerous.setValue",

    .schema = schema::operation(
        "Set value",
        "Set the value at the specified path. Be sure you know what you are doing, as the operation"
        "cannot check the validity of the value.",
        schema::object({ schema::parameter::path(".*"), { "value", schema::any() } })),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage&, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params["path"]);
        state[path] = params["value"];
        return state;
    }
};

const Operation dangerous::removeValue = {
    .name = "dangerous.removeValue",

    .schema = schema::operation(
        "Remove value",
        "Remove the value at the specified path. Be sure you know what you are doing, as the operation"
        "cannot check the validity of removing the value.",
        schema::object({ schema::parameter::path(".*") })),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage&, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params["path"]);
        auto parentPointer = path.parent_pointer();
        auto name = path.back();

        auto& parent = state.at(parentPointer);
        if (parent.is_array()) {
            parent.erase(std::stoi(name));
        } else {
            parent.erase(name);
        }

        return state;
    }
};

Operation dangerous::forEach(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "dangerous.forEach";

    // fill the operation schema header
    operation.schema = schema::operation(
        "For Each",
        "Apply given operation with given parameter set to each property defined via pattern. Parameters are passed to each call of the operation.",
        schema::object({
            { "pathPattern", schema::string() },
            { "operation", schema::string() },
            { "parameters", schema::any() },
        }));

    // define the operation function
    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto patternMap = reactor::schema::mapPatternsToPointers(linearizedGltfSchema, state.at("gltf"));
        auto operationName = params.at("operation").get<std::string>();
        auto pathPattern = params.at("pathPattern").get<std::string>();
        // remove leading "/gltf" to match patternMap
        pathPattern.erase(pathPattern.begin(), pathPattern.begin() + std::string("/gltf").length());

        if (!patternMap.contains(pathPattern)) {
            throw std::invalid_argument("No properties found for pattern: " + pathPattern);
        }
        std::vector<std::string> pathVector;

        for (auto propertyPath : patternMap.at(pathPattern)) {
            pathVector.push_back(propertyPath.get<std::string>());
        }

        std::sort(pathVector.begin(), pathVector.end(), [](const std::string& first, const std::string& second) -> bool {
            nlohmann::json::json_pointer firstPointer;
            nlohmann::json::json_pointer secondPointer;
            try {
                firstPointer = nlohmann::json::json_pointer(first);
                secondPointer = nlohmann::json::json_pointer(second);
            } catch (...) {
                return second < first;
            }
            if (firstPointer.parent_pointer() != secondPointer.parent_pointer()) {
                return second < first;
            }
            std::string firstBack = firstPointer.back();
            bool firstHasNumber = !firstBack.empty() && firstBack.find_first_not_of("0123456789") == std::string::npos;
            if (!firstHasNumber) {
                return second < first;
            }
            std::string secondBack = secondPointer.back();
            bool secondHasNumber = !secondBack.empty() && secondBack.find_first_not_of("0123456789") == std::string::npos;
            if (!secondHasNumber) {
                return second < first;
            }
            return std::stoi(secondBack) < std::stoi(firstBack);
        });

        for (auto propertyPath : pathVector) {
            auto taskDescription = nlohmann::json({ { operationName,
                params.at("parameters") } });
            taskDescription[operationName]["path"] = "/gltf" + propertyPath;
            state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
        }

        if (pathVector.size() == 0) {
            throw std::runtime_error(fmt::format("No property with pattern '{}' found", params["pathPattern"].get<std::string>()));
        }

        callback({ { "message", fmt::format("Applied {} succesfully to {} items", operationName, pathVector.size()) } });

        return state;
    };

    return operation;
}

Operation dangerous::forEachElement(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "dangerous.forEachElement";

    // fill the operation schema header
    operation.schema = schema::operation(
        "For Each Element",
        "Apply given operation with given parameter set to each element passed to this operation. Parameters are passed to each call of the operation. If an individual operation fails for a given element the remaining operation calls are stil being executed. elementPaths has to be an array of valid path values.",
        schema::object({ { "elementPaths", schema::array(schema::string()) },
                           { "operation", schema::string() } },
            { { "parameters", schema::any() }, { "skipFailed", schema::boolean(true) } }));

    // define the operation function
    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        std::vector<std::string> elements;
        for (const auto& element : params["elementPaths"]) {
            elements.emplace_back(element.get<std::string>());
        }
        auto operationName = params.at("operation").get<std::string>();
        size_t operationCount = elements.size();

        std::sort(elements.begin(), elements.end(),
            [](const std::string& first, const std::string& second) {
                nlohmann::json::json_pointer firstPointer;
                nlohmann::json::json_pointer secondPointer;
                try {
                    firstPointer = nlohmann::json::json_pointer(first);
                    secondPointer = nlohmann::json::json_pointer(second);
                } catch (...) {
                    return second < first;
                }
                if (firstPointer.parent_pointer() != secondPointer.parent_pointer()) {
                    return second < first;
                }
                std::string firstBack = firstPointer.back();
                bool firstHasNumber = !firstBack.empty() && firstBack.find_first_not_of("0123456789") == std::string::npos;
                if (!firstHasNumber) {
                    return second < first;
                }
                std::string secondBack = secondPointer.back();
                bool secondHasNumber = !secondBack.empty() && secondBack.find_first_not_of("0123456789") == std::string::npos;
                if (!secondHasNumber) {
                    return second < first;
                }
                return std::stoi(secondBack) < std::stoi(firstBack);
            });

        bool skipFailed = params.at("skipFailed");
        nlohmann::json stateCopy;

        for (auto elementPath : elements) {
            auto taskDescription = nlohmann::json({ { operationName,
                params.value("parameters", nlohmann::json::object()) } });
            taskDescription[operationName]["path"] = "/gltf" + elementPath; // why?
            if (skipFailed) {
                stateCopy = state;
            }
            try {
                state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
            } catch (...) {
                if (skipFailed) {
                    operationCount--;
                    state = std::move(stateCopy);
                } else {
                    std::rethrow_exception(std::current_exception());
                }
            }
        }

        if (operationCount == 0) {
            throw std::runtime_error("Operation failed for all elements");
        }

        if (operationCount != elements.size()) {
            callback({ { "message", fmt::format("{}/{} Operations successfull", operationCount, elements.size()) } });
        }

        return state;
    };

    return operation;
}

Operation dangerous::forEachElementInArray(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "dangerous.forEachElementInArray";

    // fill the operation schema header
    operation.schema = schema::operation(
        "For Each Element in Array",
        "Apply given operation with given parameter to each element in a JSON array. Parameters are passed to each call of the operation. If an individual operation fails for a given element the remaining operation calls are stil being executed. path has to point to a json array.",
        schema::object({ { "path", schema::string() },
                           { "operation", schema::string() } },
            { { "parameters", schema::any() }, { "skipFailed", schema::boolean(true) } }));

    // define the operation function
    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        std::string pathString = params.at("path");
        if (pathString.rfind("/gltf", 0) == std::string::npos) {
            pathString = "/gltf" + pathString;
        }
        nlohmann::json::json_pointer path(pathString);
        if (!state.at(path).is_array()) {
            throw std::invalid_argument("Path is not an array");
        }
        size_t originalCount = state.at(path).size();
        size_t operationCount = originalCount;
        if (operationCount == 0) {
            throw std::invalid_argument("Array is empty");
        }
        auto operationName = params.at("operation").get<std::string>();

        bool skipFailed = params.at("skipFailed");
        nlohmann::json stateCopy;

        for (int i = state.at(path).size() - 1; i >= 0; --i) {
            auto taskDescription = nlohmann::json({ { operationName,
                params.value("parameters", nlohmann::json::object()) } });
            taskDescription[operationName]["path"] = (path / i).to_string();
            if (skipFailed) {
                stateCopy = state;
            }
            try {
                state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
            } catch (...) {
                if (skipFailed) {
                    operationCount--;
                    state = std::move(stateCopy);
                } else {
                    std::rethrow_exception(std::current_exception());
                }
            }
        }

        if (operationCount == 0) {
            throw std::runtime_error("Operation failed for all array elements");
        }

        if (operationCount != originalCount) {
            callback({ { "message", fmt::format("{}/{} Operations successfull", operationCount, originalCount) } });
        }

        return state;
    };

    return operation;
}

Operation dangerous::combineOperations(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "dangerous.combineOperations";

    // fill the operation schema header
    operation.schema = schema::operation(
        "Chain multiple operations",
        "The task descriptions will be called sequentially",
        schema::array(schema::object({}, {}, true)));

    // define the operation function
    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        for (const auto& operation : params) {
            state = library.call(operation, std::move(state), datastorage, callback);
        }

        return state;
    };

    return operation;
}
