// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "GeneratedOperations.h"
#include "HelperFunctions.h"
#include "datastorage/DataStorage.h"
#include "operations/Library.h"
#include "operations/Operations.h"
#include "operations/Schema.h"
#include "reactor/Json.h"
#include "reactor/Reactor.h"
#include "reactor/Schema.h"

#include <gltf_schema/gltf_single_json.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

using namespace ux3d;
using namespace ux3d::operations;
using namespace std::string_literals;

// gather schemas for glTF properties for which we can automatically generate
// a setter operation. The setter operation can utilize the schema to validate its arguments automatically.
nlohmann::json gatherPrimitiveValueSetterSchemas(const nlohmann::json& schema, const nlohmann::json& linearizedGltfSchema)
{
    auto filtered = nlohmann::json::object();
    // filter the primitive value setter schemas
    for (auto& [pattern, subSchema] : linearizedGltfSchema.items()) {

        // skip generating operations for properties where it is
        // explicitly disabled in the schema
        if (subSchema.contains("ux3d_generateSetter")
            && subSchema.at("ux3d_generateSetter") == false) {
            continue;
        }

        // Gather primitive elements of an "allOf". This can e.g. happen with
        // enums, where each enum value is its own subschema in an allOf clause.
        if (subSchema.contains("allOf")) {
            auto allOfSchema = subSchema;
            allOfSchema["allOf"].clear();
            for (auto& allOfItem : subSchema.at("allOf")) {
                if (allOfItem.contains("type")
                    && allOfItem.at("type") != "array"
                    && allOfItem.at("type") != "object"
                    && subSchema.contains("ux3d_pathPrefix") == false) {
                    allOfSchema["allOf"].emplace_back(allOfItem);
                }
            }
            if (allOfSchema["allOf"].empty() == false) {
                filtered[pattern] = allOfSchema;
            }
        }

        // Same with "anyOf" clauses
        if (subSchema.contains("anyOf")) {
            auto anyOfSchema = subSchema;
            anyOfSchema["anyOf"].clear();
            for (auto& anyOfItem : subSchema.at("anyOf")) {
                if (anyOfItem.contains("type")
                    && anyOfItem.at("type") != "array"
                    && anyOfItem.at("type") != "object"
                    && subSchema.contains("ux3d_pathPrefix") == false) {
                    anyOfSchema["anyOf"].emplace_back(anyOfItem);
                } else if (anyOfItem.contains("enum")) {
                    anyOfSchema["anyOf"].emplace_back(anyOfItem);
                }
            }
            if (anyOfSchema["anyOf"].empty() == false) {
                filtered[pattern] = anyOfSchema;
            }
        }

        // arrays of primitive values should also pass the filter
        if (subSchema.contains("type") && subSchema.at("type") == "array") {
            if (subSchema.contains("items")) {
                auto items = subSchema.at("items");

                // arrays of primitive types are added to the pointerpatter list
                if (items.contains("type")
                    && items.at("type") != "array"
                    && items.at("type") != "object"
                    && items.contains("ux3d_pathPrefix") == false) {
                    filtered[pattern] = subSchema;
                }
            }
        }

        // add any truly primitive value (excluding pointer references, values of "glTF Id")
        if (subSchema.contains("type")
            && subSchema.at("type") != "array"
            && subSchema.at("type") != "object"
            && (!subSchema.contains("title") || subSchema.at("title") != "glTF Id")
            && !subSchema.contains("ux3d_pathPrefix")) {
            filtered[pattern] = subSchema;
        }
    }

    return filtered;
}

// gather schemas for glTF properties for which we can automatically generate
// a setter operation. The setter operation can utilize the schema to validate its arguments automatically.
nlohmann::json gatherCreateObjectSchemas(const nlohmann::json& schema, const nlohmann::json& linearizedGltfSchema)
{
    auto filtered = nlohmann::json::array();
    // filter the primitive value setter schemas
    for (auto& [pattern, subSchema] : linearizedGltfSchema.items()) {

        // skip generating operations for properties where it is
        // explicitly disabled in the schema
        if ((subSchema.contains("ux3d_generateSetter")
                && subSchema.at("ux3d_generateSetter") == false)
            || subSchema.contains("minProperties") || pattern == "") {
            continue;
        }

        // Also add objects without properties (and empty extensions/extras)
        if (subSchema.contains("type") && subSchema.at("type") == "object" && subSchema.contains("properties")) {
            if (subSchema.contains("additionalProperties") && subSchema.at("additionalProperties").is_object() && subSchema.at("additionalProperties").size() > 0) {
                if (subSchema.contains("title") && subSchema["title"] == "Extension") {
                    continue;
                }
            }
            nlohmann::json schema = {
                { "type", "object" },
                { "properties", nlohmann::json::object() },
                { "additionalProperties", false }
            };

            schema["properties"]["path"] = schema::pointer("/gltf" + pattern);

            for (auto& [key, value] : subSchema.at("properties").items()) {
                schema["properties"][key] = value;
                schema["properties"][key].erase("default");
            }

            schema["required"].push_back("path");
            filtered.push_back(schema);
        }
    }

    return filtered;
}

// gather schemas for glTF components for which we can automatically generate
// a setter operation. The setter operation can utilize the schema to validate its arguments automatically.
nlohmann::json gatherCreateComponentSchemas(const nlohmann::json& schema, const nlohmann::json& linearizedGltfSchema)
{
    auto filtered = nlohmann::json::array();
    // filter the primitive value setter schemas
    for (auto& [pattern, subSchema] : linearizedGltfSchema.items()) {

        // skip generating operations for properties where it is
        // explicitly disabled in the schema
        if ((subSchema.contains("ux3d_generateSetter")
                && subSchema.at("ux3d_generateSetter") == false)
            || subSchema.contains("minProperties") || pattern == "") {
            continue;
        }

        // Also add objects without properties (and empty extensions/extras)
        if (subSchema.contains("type") && subSchema.at("type") == "array" && subSchema.contains("items")) {
            if (subSchema.at("items").contains("type") && subSchema["items"]["type"] != "object" || !subSchema["items"].contains("properties")) {
                continue;
            }
            nlohmann::json schema = {
                { "type", "object" },
                { "properties", nlohmann::json::object() },
                { "additionalProperties", false }
            };

            if (subSchema["items"].contains("additionalProperties")) {
                schema["additionalProperties"] = subSchema["items"]["additionalProperties"];
            }

            schema["properties"]["path"] = schema::pointer("/gltf" + pattern);

            for (auto& [key, value] : subSchema["items"].at("properties").items()) {
                schema["properties"][key] = value;
                schema["properties"][key].erase("default");
            }

            schema["required"].push_back("path");
            filtered.push_back(schema);
        }
    }

    return filtered;
}

// Automatically generate an operation that allows setting primitive values in the gltf
Operation generated::setGltfValue(const nlohmann::json& glTFSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "set_gltf_value";

    auto subSchemas = gatherPrimitiveValueSetterSchemas(glTFSchema, linearizedGltfSchema);
    auto parameterSchemas = nlohmann::json::array();
    for (const auto& [pointerPattern, subSchema] : subSchemas.items()) {
        auto parameterSchema = schema::object({ schema::parameter::path("/gltf" + pointerPattern), { "value", subSchema } });
        parameterSchemas.push_back(parameterSchema);
    }
    operation.schema = schema::operation("Set glTF value", "Set primitive value in glTF.", schema::oneOf(parameterSchemas));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage&, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params.at(schema::parameter::pathKey));
        state[path] = params.at("value");
        return state;
    };

    return operation;
}

// Automatically generate an operation that allows setting primitive values in the gltf
Operation generated::createGltfObject(const nlohmann::json& glTFSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "create_gltf_object";

    auto subSchemas = gatherCreateObjectSchemas(glTFSchema, linearizedGltfSchema);
    operation.schema = schema::operation("Creates a glTF object", "Creates a glTF object. Needs to set all required properties", schema::oneOf(subSchemas));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage&, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params.at(schema::parameter::pathKey));
        auto paraCopy = params;
        paraCopy.erase("path");
        state[path] = paraCopy;
        return state;
    };

    return operation;
}

// Automatically generate an operation that allows setting primitive values in the gltf
Operation generated::createGltfComponent(const nlohmann::json& glTFSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "create_gltf_component";

    auto subSchemas = gatherCreateComponentSchemas(glTFSchema, linearizedGltfSchema);
    operation.schema = schema::operation("Creates a glTF component", "Creates a glTF component. Needs to set all required properties", schema::oneOf(subSchemas));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage&, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params.at(schema::parameter::pathKey));
        auto paraCopy = params;
        paraCopy.erase("path");
        state[path].push_back(paraCopy);
        return state;
    };

    return operation;
}

// gather schemas for glTF pointer properties for which we can automatically implement operations such as
// move, copy or unset
nlohmann::json gatherPointerSchemas(const nlohmann::json& schema, const nlohmann::json& linearizedGltfSchema)
{
    auto linearized = reactor::schema::linearize(schema);

    auto filtered = nlohmann::json::object();
    // filter the primitive value setter schemas
    for (auto& [pattern, subSchema] : linearized.items()) {
        // skip generating operations for properties where it is
        // explicitly disabled in the schema
        if (subSchema.contains("ux3d_generateSetter")
            && subSchema.at("ux3d_generateSetter") == false) {
            continue;
        }

        // only let schemas for gltf reference pointers (indices) pass the filter
        if (subSchema.contains("ux3d_pathPrefix")) {
            filtered[pattern] = subSchema;
        }
    }

    return filtered;
}

// Automatically generate an operation that allows setting pointer values in the gltf
Operation generated::setGltfPointer(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "set_gltf_pointer";

    auto subSchemas = gatherPointerSchemas(gltfSchema, linearizedGltfSchema);

    auto parameterSchemas = nlohmann::json::array();
    for (const auto& [pointerPattern, subSchema] : subSchemas.items()) {
        std::string valuePattern = "/gltf" + subSchema.at("ux3d_pathPrefix").get<std::string>();
        valuePattern += "/\\d+";
        auto parameterSchema = schema::object({ schema::parameter::path("/gltf" + pointerPattern), { "pointer", schema::pointer(valuePattern) } });
        parameterSchemas.push_back(parameterSchema);
    }
    operation.schema = schema::operation("Set glTF pointer", "Set pointer in glTF.", schema::oneOf(parameterSchemas));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage&, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params.at(schema::parameter::pathKey));
        auto pointer = nlohmann::json::json_pointer(params.at("pointer"));

        // Don't accept inputs that don't point to a valid glTF property
        if (state.contains(pointer) == false) {
            throw std::invalid_argument(std::string("") + "Parameter \"pointer\" with value \"" + pointer.to_string() + "\" does not point to a property in the glTF JSON");
        }

        // Apply the change
        state[path] = std::stoi(pointer.back());

        // If we're setting nodes in the node hierarchy, validate the hierarchy again as it could be broken by the change
        if ((!path.parent_pointer().parent_pointer().parent_pointer().empty() && path.parent_pointer().parent_pointer().parent_pointer().back() == "nodes" && path.parent_pointer().back() == "children")
            || (!path.parent_pointer().parent_pointer().empty() && path.parent_pointer().parent_pointer().back() == "scenes" && path.back() == "nodes")) {
            auto results = reactor::gltf::validateNodeHierarchies(state.at("gltf"));
            if (!results.empty()) {
                throw std::invalid_argument(std::string("") + "The node cannot be inserted at \"" + path.to_string() + "\" as it would produce an invalid node hierarchy.");
            }
        }

        return state;
    };

    return operation;
}

Operation generated::moveGltfPointer(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "move_gltf_pointer";

    auto subSchemas = gatherPointerSchemas(gltfSchema, linearizedGltfSchema);

    auto parameterSchemas = nlohmann::json::array();
    for (const auto& [pointerPattern, subSchema] : subSchemas.items()) {
        std::string valuePattern = "/gltf" + subSchema.at("ux3d_pathPrefix").get<std::string>();
        valuePattern += "/\\d+";
        auto parameterSchema = schema::object({ { "targetPath", schema::pointer("/gltf" + pointerPattern) }, { "pointer", schema::pointer(valuePattern) }, { "sourcePath", schema::pointer(".*") } }); // TODO make sourcePath pointer more specific
        parameterSchemas.push_back(parameterSchema);
    }

    operation.schema = schema::operation(
        "Move pointer",
        "Move pointer value between json objects.",
        schema::oneOf(parameterSchemas));

    operation.operation = [subSchemas](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto targetPath = nlohmann::json::json_pointer(params.at("targetPath"));
        auto pointer = nlohmann::json::json_pointer(params.at("pointer"));
        auto sourcePath = nlohmann::json::json_pointer(params.at("sourcePath"));

        if (state.contains(pointer) == false) {
            throw std::invalid_argument(std::string("") + "Parameter \"pointer\" with value \"" + pointer.to_string() + "\" does not exist.");
        }
        if (state.contains(sourcePath) == false) {
            throw std::invalid_argument(std::string("") + "Parameter \"parent\" with value \"" + sourcePath.to_string() + "\" does not exist.");
        }

        if (state.at(sourcePath) != std::stoi(pointer.back())) {
            throw std::invalid_argument(std::string("") + "Parameter \"parent\" with value \"" + sourcePath.to_string() + "\" does not contain parameter \"pointer\" with value \"" + pointer.to_string() + "\".");
        }

        auto taskDescription = R"(
        {
            "remove_gltf_pointer": {
                
            }
        }
        )"_json;
        taskDescription["/remove_gltf_pointer/path"_json_pointer] = sourcePath.to_string();
        state = library.call(taskDescription, std::move(state), dataStorage, callback);

        taskDescription = R"(
        {
            "set_gltf_pointer": {
                
            }
        }
        )"_json;
        taskDescription["/set_gltf_pointer/path"_json_pointer] = targetPath.to_string();
        taskDescription["/set_gltf_pointer/pointer"_json_pointer] = pointer.to_string();
        state = library.call(taskDescription, std::move(state), dataStorage, callback);

        return state;
    };

    return operation;
}

// gather schemas for glTF pointer array properties for which we can automatically implement operations such as
// move, copy or unset
nlohmann::json gatherPointerArraySchemas(const nlohmann::json& schema, const nlohmann::json& linearizedGltfSchema)
{
    auto filtered = nlohmann::json::object();
    // filter the primitive value setter schemas
    for (auto& [pattern, subSchema] : linearizedGltfSchema.items()) {
        // skip generating operations for properties where it is
        // explicitly disabled in the schema
        if (subSchema.contains("ux3d_generateSetter")
            && subSchema.at("ux3d_generateSetter") == false) {
            continue;
        }

        if (subSchema.contains("type") && subSchema.at("type") != "array") {
            continue;
        }

        // only let schemas for gltf reference pointers (indices) pass the filter
        if (subSchema.contains("items") && subSchema.at("items").contains("ux3d_pathPrefix")) {
            filtered[pattern] = subSchema;
        }
    }

    return filtered;
}

Operation generated::insertGltfPointer(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "insert_gltf_pointer";

    auto subSchemas = gatherPointerArraySchemas(gltfSchema, linearizedGltfSchema);

    auto parameterSchemas = nlohmann::json::array();
    for (const auto& [pointerPattern, subSchema] : subSchemas.items()) {
        std::string valuePattern = "/gltf" + subSchema.at("items").at("ux3d_pathPrefix").get<std::string>();
        valuePattern += "/\\d+";
        auto parameterSchema = schema::object({ schema::parameter::path("/gltf" + pointerPattern), { "pointer", schema::pointer(valuePattern) } });
        parameterSchemas.push_back(parameterSchema);
    }

    operation.schema = schema::operation(
        "Insert pointer",
        "Insert pointer value into an array.",
        schema::oneOf(parameterSchemas));

    operation.operation = [subSchemas](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage&, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params.at(schema::parameter::pathKey));
        auto pointer = nlohmann::json::json_pointer(params.at("pointer"));

        // Don't accept inputs that don't point to a valid glTF property
        if (state.contains(pointer) == false) {
            throw std::invalid_argument(std::string("") + "Parameter \"pointer\" with value \"" + pointer.to_string() + "\" does not point to a property in the glTF JSON");
        }

        // Ensure the array is present
        if (state.contains(path) == false) {
            state[path] = nlohmann::json::array();
        }

        int insertedElement = std::stoi(pointer.back());

        // Insert only if it isn't already present
        auto it = std::find(state.at(path).begin(), state.at(path).end(), insertedElement);
        if (it != state.at(path).end()) {
            throw std::invalid_argument(std::string("") + "Parameter \"pointer\" with value \"" + pointer.to_string() + "\" is already present in the array at \"" + path.to_string() + "\"");
        }

        state.at(path).emplace_back(insertedElement);

        // If we're inserting nodes into the node hierarchy, validate the hierarchy again
        if ((path.parent_pointer().parent_pointer().back() == "nodes" && path.back() == "children")
            || (path.parent_pointer().parent_pointer().back() == "scenes" && path.back() == "nodes")) {
            auto results = reactor::gltf::validateNodeHierarchies(state.at("gltf"));
            if (!results.empty()) {
                throw std::invalid_argument(std::string("") + "The node cannot be inserted at \"" + path.to_string() + "\" as it would produce an invalid node hierarchy.");
            }
        }

        return state;
    };

    return operation;
}

// TODO: add operations to remove properties

Operation generated::removeGltfPointer(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "remove_gltf_pointer";

    auto subSchemas = gatherPointerSchemas(gltfSchema, linearizedGltfSchema);

    auto parameterSchemas = nlohmann::json::array();
    for (const auto& [pointerPattern, subSchema] : subSchemas.items()) {
        auto parameterSchema = schema::object({ schema::parameter::path("/gltf" + pointerPattern) });
        parameterSchemas.push_back(parameterSchema);
    }
    operation.schema = schema::operation("Remove glTF pointer", "Removes a glTF pointer from arrays or objects.", schema::oneOf(parameterSchemas));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage&, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params.at(schema::parameter::pathKey));

        if (state.contains(path) == false) {
            throw std::invalid_argument("Parameter \"path\" with value \"" + path.to_string() + "\" does not point to a property in the glTF JSON");
        }

        auto& json = state.at(path.parent_pointer());
        if (json.is_array()) {
            auto index = path.back() == "-" ? (json.size() - 1) : std::stoi(path.back());
            if (json.size() > index) {
                json.erase(index);
            }
            // Avoid empty arrays
            if (json.size() == 0) {
                state.at(path.parent_pointer().parent_pointer()).erase(path.parent_pointer().back());
            }
        } else {
            json.erase(path.back());
        }

        return state;
    };

    return operation;
}

// TODO: add operations to remove elements from arrays

Operation generated::removeGltfPointerFromArray(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "remove_gltf_pointer_from_array";

    auto subSchemas = gatherPointerArraySchemas(gltfSchema, linearizedGltfSchema);

    auto parameterSchemas = nlohmann::json::array();
    for (const auto& [pointerPattern, subSchema] : subSchemas.items()) {
        auto parameterSchema = schema::object({ schema::parameter::path("/gltf" + pointerPattern + "/\\d+") });
        parameterSchemas.push_back(parameterSchema);
    }

    operation.schema = schema::operation(
        "Remove",
        "Remove pointer value from an array.",
        schema::oneOf(parameterSchemas));

    operation.operation = [subSchemas](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage&, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params.at(schema::parameter::pathKey));

        // Ensure the array is present
        if (state.contains(path.parent_pointer()) == false) {
            throw std::invalid_argument("Parent of \"path\" with value \"" + path.to_string() + "\" does not exist.");
        }

        auto& array = state.at(path.parent_pointer());
        auto index = std::stoi(path.back());
        if (index >= array.size()) {
            throw std::invalid_argument("\"path\" with value \"" + path.to_string() + "\" does not exist.");
        }
        array.erase(index);

        return state;
    };

    return operation;
}

Operation generated::moveGltfPointerFromArray(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "move_gltf_pointer_from_array";

    auto subSchemas = gatherPointerArraySchemas(gltfSchema, linearizedGltfSchema);

    auto parameterSchemas = nlohmann::json::array();
    for (const auto& [pointerPattern, subSchema] : subSchemas.items()) {
        std::string valuePattern = "/gltf" + subSchema.at("items").at("ux3d_pathPrefix").get<std::string>();
        valuePattern += "/\\d+";
        auto parameterSchema = schema::object({ schema::parameter::path("/gltf" + pointerPattern), { "pointer", schema::pointer(valuePattern) }, { "parent", schema::pointer(".*") } }); // TODO make parent pointer more specific
        parameterSchemas.push_back(parameterSchema);
    }

    operation.schema = schema::operation(
        "Move",
        "Move pointer value between arrays.",
        schema::oneOf(parameterSchemas));

    operation.operation = [subSchemas](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params.at(schema::parameter::pathKey));
        auto pointer = nlohmann::json::json_pointer(params.at("pointer"));
        auto parent = nlohmann::json::json_pointer(params.at("parent"));

        // Ensure the array is present
        if (state.contains(path) == false) {
            state[path] = nlohmann::json::array();
        }
        if (state.contains(pointer) == false) {
            throw std::invalid_argument(std::string("") + "Parameter \"pointer\" with value \"" + pointer.to_string() + "\" does not exist.");
        }
        if (state.contains(parent) == false) {
            throw std::invalid_argument(std::string("") + "Parameter \"parent\" with value \"" + parent.to_string() + "\" does not exist.");
        }
        if (state.at(parent).is_array() == false) {
            throw std::invalid_argument(std::string("") + "Parameter \"parent\" with value \"" + parent.to_string() + "\" is not an array.");
        }
        auto it = std::find(state.at(parent).begin(), state.at(parent).end(), std::stoi(pointer.back()));
        if (it == state.at(parent).end()) {
            throw std::invalid_argument(std::string("") + "Parameter \"parent\" with value \"" + parent.to_string() + "\" does not contain parameter \"pointer\" with value \"" + pointer.to_string() + "\".");
        }

        auto pointerToRemove = parent / pointer.back();

        auto taskDescription = R"(
        {
            "remove_gltf_pointer_from_array": {
                
            }
        }
        )"_json;
        taskDescription["/remove_gltf_pointer_from_array/path"_json_pointer] = pointerToRemove.to_string();
        state = library.call(taskDescription, std::move(state), dataStorage, callback);

        taskDescription = R"(
        {
            "insert_gltf_pointer": {
                
            }
        }
        )"_json;
        taskDescription["/insert_gltf_pointer/path"_json_pointer] = path.to_string();
        taskDescription["/insert_gltf_pointer/pointer"_json_pointer] = pointer.to_string();
        state = library.call(taskDescription, std::move(state), dataStorage, callback);

        return state;
    };

    return operation;
}

Operation generated::remove(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "remove";

    auto objectsInArrays = nlohmann::json::object();
    for (const auto& [pattern, schema] : linearizedGltfSchema.items()) {
        if (schema.value("type", "") == "object" && pattern.length() >= 3 && pattern.substr(pattern.length() - 3) == "\\d+") {
            objectsInArrays[pattern] = schema;
        }
    }
    auto parameters = nlohmann::json::array();
    for (const auto& [pattern, schema] : objectsInArrays.items()) {
        auto parameterSingle = schema::object({ schema::parameter::path("/gltf" + pattern) });
        parameters.push_back(parameterSingle);
        auto parameterMulti = schema::object({ { "path", schema::array(schema::pointer("/gltf" + pattern)) } });
        parameters.push_back(parameterMulti);
    }

    // fill the operation schema header
    operation.schema = schema::operation(
        "Remove",
        "Remove property from the glTF.",
        schema::oneOf(parameters));

    // define the operation function
    auto backwardReferences = reactor::gltf::schema::backwardReferences(linearizedGltfSchema);
    operation.operation = [objectsInArrays, backwardReferences, linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto analyzeFun = [](auto path, std::vector<uint32_t>& indexShift) -> void {
            int removalIndex = std::stoi(path.back());
            indexShift[removalIndex]++;
        };

        auto removeRefencesFun = [&state, &objectsInArrays, &backwardReferences](auto path) -> void {
            std::string stringPath = path.to_string();
            bool removeExtensionItem = stringPath.find("/extensions/") != std::string::npos;
            auto parent = path.parent_pointer();
            int removalIndex = std::stoi(path.back());
            auto& gltf = state["gltf"];
            auto localPath = path.to_string().substr("/gltf"s.size());
            auto pattern = reactor::schema::mapToPattern(objectsInArrays, localPath);
            // Fix up all references to this item by resetting them
            // avoid applying this to objects that do not have backreferences (e.g. animations)
            if (backwardReferences.contains(pattern)) {
                for (const auto& reference : backwardReferences[pattern]) {
                    reactor::json::for_each_match(reference, gltf, [&removalIndex, &gltf, removeExtensionItem](const nlohmann::json::json_pointer& pointer) {
                        // TODO: it might be possible that some references are required? in this case we would have to throw an exception
                        auto parentPointer = pointer.parent_pointer();
                        auto& parent = gltf.at(parentPointer);
                        if (parent.is_array()) {
                            auto it = std::find(parent.begin(), parent.end(), removalIndex);
                            if (it != parent.end()) {
                                parent.erase(it);
                            }

                            // Remove the array itself if we have removed the last item
                            if (gltf.at(pointer.parent_pointer()).empty()) {
                                gltf.at(pointer.parent_pointer().parent_pointer()).erase(pointer.parent_pointer().back());
                            }

                            if (removeExtensionItem) {
                                parentPointer = parentPointer.parent_pointer();
                                while (gltf.at(parentPointer).empty()) {
                                    gltf.at(parentPointer.parent_pointer()).erase(parentPointer.back());
                                    if (parentPointer.back() == "extensions") {
                                        break;
                                    }
                                    parentPointer = parentPointer.parent_pointer();
                                }
                            }
                        } else if (parent.is_object()) {
                            if (gltf.at(pointer) == removalIndex) {
                                parent.erase(pointer.back());

                                if (removeExtensionItem) {
                                    while (gltf.at(parentPointer).empty()) {
                                        gltf.at(parentPointer.parent_pointer()).erase(parentPointer.back());
                                        if (parentPointer.back() == "extensions") {
                                            break;
                                        }
                                        parentPointer = parentPointer.parent_pointer();
                                    }
                                }
                            }
                        }
                    });
                }
            }
        };

        std::vector<std::string> extensions;
        std::map<std::string, std::vector<nlohmann::json::json_pointer>> extensionPaths;

        auto removeFun = [&state, &extensions, &extensionPaths](auto path) -> void {
            std::string stringPath = path.to_string();
            bool removeExtensionItem = stringPath.find("/extensions/") != std::string::npos;
            auto parent = path.parent_pointer();
            int removalIndex = std::stoi(path.back());

            state.at(parent).erase(removalIndex);

            // Remove the array itself if we have removed the last item
            if (state.at(parent).is_array() && state.at(parent).size() == 0) {
                state.at(parent.parent_pointer()).erase(parent.back());
            }

            if (removeExtensionItem) {
                parent = parent.parent_pointer();
                while (state.at(parent).empty()) {
                    utility::getExtensionsAtPath(parent, state, extensions, &extensionPaths);
                    state.at(parent.parent_pointer()).erase(parent.back());
                    if (parent.back() == "extensions") {
                        break;
                    }
                    parent = parent.parent_pointer();
                }
            }
        };

        auto fixReferencesFun = [&state, &objectsInArrays, &backwardReferences](auto path, auto indexShiftFun) -> void {
            auto arraypath = path.parent_pointer();
            if (!state.contains(arraypath)) {
                return;
            }
            for (int i = 0; i < state.at(arraypath).size(); i++) {
                if (indexShiftFun(i) == 0) {
                    continue;
                }
                auto& gltf = state["gltf"];
                auto path = arraypath / i;
                auto localPath = path.to_string().substr("/gltf"s.size());
                auto pattern = reactor::schema::mapToPattern(objectsInArrays, localPath);
                if (backwardReferences.contains(pattern)) {
                    for (const auto& reference : backwardReferences[pattern]) {
                        reactor::schema::patternForEach(reference, gltf, [&](const nlohmann::json::json_pointer& pointer) {
                            if (gltf.contains(pointer) && gltf.at(pointer) == i) {
                                gltf.at(pointer) = i - indexShiftFun(i);
                            }
                        });
                    }
                }
            }
        };

        auto accumulateShiftFun = [](std::vector<uint32_t>& indexShift) -> void {
            int acc = 0;
            for (int i = 0; i < indexShift.size(); i++) {
                indexShift[i] = indexShift[i] + acc;
                acc = indexShift[i];
            }
        };
        if (params.at("path").is_array()) {
            std::vector<nlohmann::json::json_pointer> pathVector;
            for (auto path : params.at("path")) {
                nlohmann::json::json_pointer pointer = nlohmann::json::json_pointer(path);
                operations::utility::getExtensionsAtPath(pointer, state, extensions, &extensionPaths);
                pathVector.push_back(pointer);
            }

            // lexical sort is not possible because of indices
            std::sort(pathVector.begin(), pathVector.end(), [](const nlohmann::json::json_pointer& a, const nlohmann::json::json_pointer& b) -> bool {
                return std::stoi(a.back()) > std::stoi(b.back());
            });

            std::vector<uint32_t> indexShift;
            indexShift.resize(state.at(pathVector[0].parent_pointer()).size());

            for (auto path : pathVector) {
                analyzeFun(path, indexShift);
            }

            accumulateShiftFun(indexShift);

            for (auto path : pathVector) {
                removeRefencesFun(path);
            }
            auto indexShiftFun = [&indexShift](uint32_t i) -> uint32_t { return indexShift[i]; };
            fixReferencesFun(pathVector[0], indexShiftFun);
            for (auto path : pathVector) {
                removeFun(path);
            }
        } else {
            auto path = nlohmann::json::json_pointer(params.at("path"));
            operations::utility::getExtensionsAtPath(path, state, extensions, &extensionPaths);
            removeRefencesFun(path);
            // If we only remove one item, index shift gets much simpler:
            int removalIndex = std::stoi(path.back());
            auto indexShiftFun = [removalIndex](uint32_t i) -> uint32_t { return (i < removalIndex) ? 0 : 1; };
            fixReferencesFun(path, indexShiftFun);
            removeFun(path);
        }

        if (!extensions.empty()) {
            auto taskDescription = nlohmann::json({ { "update_extension_flags", nlohmann::json::object() } });
            taskDescription["update_extension_flags"]["extensions"] = extensions;
            state = library.call(taskDescription, std::move(state), dataStorage, callback);
        }

        return state;
    };

    return operation;
}

Operation generated::reset(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "reset";

    // we need to resolve "allOf" objects to easily access the parent schema's "required" array
    const nlohmann::json linearizedGltfSchemaResolved = reactor::json::resolveAllOf(linearizedGltfSchema);

    auto parameters = nlohmann::json::array();
    for (const auto& [pattern, schema] : linearizedGltfSchemaResolved.items()) {
        auto pathPattern = nlohmann::json::json_pointer(pattern);
        // Do not reset items of arrays/items without setter
        if (pathPattern.empty() || pathPattern.back() == "\\d+"
            || (!schema.value("ux3d_generateSetter", true) && !schema.value("ux3d_generateReset", false)) // If ux3d_generateReset is True but ux3d_generateSetter is False generate reset
            || (!schema.value("ux3d_generateReset", true))) { // Don't generate if ux3d_generateReset is false
            continue;
        }
        // Check if a default value exists, otherwise check in the parent if the value is required
        if (!schema.contains("default")) {
            // Do not reset arrays without default
            if ((schema.contains("type") && schema.at("type") == "array")) {
                continue;
            }
            std::string currentProperty = pathPattern.back();
            auto& parentSchema = linearizedGltfSchemaResolved.at(pathPattern.parent_pointer().to_string());
            if (parentSchema.contains("required")) {
                auto& required = parentSchema["required"];
                auto found = std::find(required.begin(), required.end(), currentProperty);
                if (found != required.end()) {
                    continue;
                }
            }
        }
        auto parameter = schema::object({ schema::parameter::path("/gltf" + pattern) });
        parameters.push_back(parameter);
    }

    // fill the operation schema header
    operation.schema = schema::operation(
        "Reset",
        "Reset property from the glTF",
        schema::oneOf(parameters));

    // define the operation function
    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params.at("path"));
        std::vector<std::string> extensions;
        std::map<std::string, std::vector<nlohmann::json::json_pointer>> extensionPaths;
        operations::utility::getExtensionsAtPath(path, state, extensions, &extensionPaths);
        auto taskDescription = R"(
        {
            "dangerous.removeValue": {
                
            }
        }
        )"_json;
        taskDescription["/dangerous.removeValue/path"_json_pointer] = path.to_string();
        state = library.call(taskDescription, std::move(state), dataStorage, callback);

        if (!extensions.empty()) {
            if (extensionPaths.contains("KHR_draco_mesh_compression")) {
                for (auto path : extensionPaths["KHR_draco_mesh_compression"]) {
                    auto taskDescription = R"(
                    {
                        "resolve_draco": {
                        }
                    }
                    )"_json;
                    // Get path to primitive (strip extensions/KHR_draco_mesh_compression)
                    taskDescription["resolve_draco"]["path"] = path.parent_pointer().parent_pointer().to_string();
                    state = library.call(taskDescription, std::move(state), dataStorage, callback);
                }
            }

            auto taskDescription = R"(
            {
                "update_extension_flags": {
                }
            }
            )"_json;
            taskDescription["update_extension_flags"]["extensions"] = extensions;
            state = library.call(taskDescription, std::move(state), dataStorage, callback);
        }
        return state;
    };

    return operation;
}
