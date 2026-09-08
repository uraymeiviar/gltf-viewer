#include "OverloadOperations.h"
#include "HelperFunctions.h"
#include "operations/Library.h"
#include "operations/Schema.h"
#include "reactor/Json.h"
#include "reactor/Utils.h"

#include <string>
#include <vector>

using namespace ux3d;
using namespace ux3d::operations;

Operation overload::setTRSValue(const std::string& overloadOperation, const nlohmann::json& glTFSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = overloadOperation;

    auto parameterSchemas = nlohmann::json::array();
    std::vector<std::string> paths = { "/nodes/\\d+/translation", "/nodes/\\d+/rotation", "/nodes/\\d+/scale" };
    for (const auto& path : paths) {
        auto parameterSchema = schema::object({ schema::parameter::path("/gltf" + path), { "value", linearizedGltfSchema.at(path) } });
        parameterSchemas.push_back(parameterSchema);
    }
    operation.schema = schema::operation("Set glTF value (node TRS values)", "Sets any of the node's TRS values while also considering and resetting the matrix if present.", schema::oneOf(parameterSchemas));

    operation.operation = [overloadOperation](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params.at(schema::parameter::pathKey));
        if (state.contains(path.parent_pointer() / "matrix")) {
            std::string op = "change_transformation_format";
            auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
            taskDescription[op]["path"] = path.parent_pointer().to_string();
            taskDescription[op]["storageType"] = "TRS";
            state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
        }
        auto taskDescription = nlohmann::json({ { overloadOperation, params } });
        state = library.call(std::move(taskDescription), std::move(state), datastorage, callback, true);
        return state;
    };

    return operation;
}

Operation overload::joinPrimitives(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = overloadOperation;
    operation.schema = schema::operation(
        "Join Primitives (all primitives of a mesh)",
        "Joins all primitives of a single mesh into one primitive.",
        schema::object({ schema::parameter::path("/gltf/meshes/\\d+") }));

    operation.operation = [overloadOperation](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params.at(schema::parameter::pathKey));
        if (!state.contains(path / "primitives") || state.at(path / "primitives").size() < 2) {
            throw std::invalid_argument("Not enough primitives to join");
        }
        nlohmann::json pathArray = nlohmann::json::array();
        for (int i = 0; i < state.at(path / "primitives").size(); ++i) {
            pathArray.push_back((path / "primitives" / i).to_string());
        }
        auto taskDescription = nlohmann::json({ { overloadOperation, nlohmann::json::object() } });
        taskDescription["join_primitives"]["paths"] = pathArray;
        return library.call(std::move(taskDescription), std::move(state), dataStorage, callback, true);
    };

    return operation;
}

Operation overload::resolveDraco(const std::string& overloadOperation, const nlohmann::json& glTFSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = overloadOperation;

    operation.schema = schema::operation("Reset (KHR_draco_mesh_compression)",
        "Resets the KHR_draco_mesh_compression object and resolve the binary draco data, cleanly removing this extension from this primitive.",
        schema::object({ schema::parameter::path("/gltf/meshes/\\d+/primitives/\\d+/extensions/KHR_draco_mesh_compression") }));

    operation.operation = [overloadOperation](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params.at(schema::parameter::pathKey));
        nlohmann::json parameters;
        parameters["path"] = path.parent_pointer().parent_pointer().to_string();
        parameters["deleteEncodedData"] = true;
        parameters["cleanExtensionsUsed"] = true;
        auto taskDescription = nlohmann::json({ { "resolve_draco", parameters } });
        state = library.call(std::move(taskDescription), std::move(state), datastorage, callback, true);
        return state;
    };

    return operation;
}

Operation overload::setTexCoord(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = overloadOperation;

    operation.schema = schema::operation("Set glTF value (texCoord)", "Sets a texCoord value while making sure that the value is available in all primitives using this material.",
        schema::object({ schema::parameter::path("/gltf/materials/\\d+/.*/texCoord"), { "value", schema::integer(0, 0) } }));

    operation.operation = [overloadOperation](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        std::string stringPath = params.at(schema::parameter::pathKey);
        std::string stringValue = std::to_string(params.at("value").get<int>());
        for (const auto& mesh : state.at("/gltf/meshes"_json_pointer)) {
            if (mesh.contains("primitives")) {
                for (const auto& primitive : mesh.at("primitives")) {
                    if (primitive.contains("material") && stringPath.starts_with("/gltf/materials/" + std::to_string(primitive.at("material").get<int>()))) {
                        if (!primitive.contains(nlohmann::json::json_pointer("/attributes/TEXCOORD_" + stringValue))) {
                            throw std::invalid_argument("TexCoord not present in primitive");
                        }
                    }
                }
            }
        }
        auto taskDescription = nlohmann::json({ { overloadOperation, params } });
        state = library.call(std::move(taskDescription), std::move(state), datastorage, callback, true);
        return state;
    };

    return operation;
}

Operation overload::setCameraOrthographicProperties(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = overloadOperation;

    operation.schema = schema::operation("Set glTF value (camera orthographic)", "Sets a primitive value in the orthographic object of a camera.Handles a potential switch of the camera type during this operation.",
        schema::object({ schema::parameter::path("/gltf/cameras/\\d+/orthographic/.*"), { "value", schema::any() } })); // value will be checked by generic function

    operation.operation = [overloadOperation](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        std::string stringPath = params.at(schema::parameter::pathKey);
        size_t index = stringPath.find("/orthographic/");
        auto cameraPath = nlohmann::json::json_pointer(stringPath.substr(0, index));
        if (state.contains(cameraPath / "perspective")) {
            std::vector<std::string> extensions;
            operations::utility::getExtensionsAtPath(cameraPath / "perspective", state, extensions);
            state.at(cameraPath).erase("perspective");
            if (!extensions.empty()) {
                auto taskDescription = R"(
            {
                "update_extension_flags": {
                }
            }
            )"_json;
                taskDescription["update_extension_flags"]["extensions"] = extensions;
                state = library.call(taskDescription, std::move(state), datastorage, callback);
            }
        }
        state[cameraPath / "type"] = "orthographic";
        auto taskDescription = nlohmann::json({ { overloadOperation, params } });
        state = library.call(std::move(taskDescription), std::move(state), datastorage, callback, true);
        return state;
    };

    return operation;
}

Operation overload::setCameraPerspectiveProperties(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = overloadOperation;

    operation.schema = schema::operation("Set glTF value (camera perspective)", "Sets a primitive value in the perspective object of a camera. Handles a potential switch of the camera type during this operation.",
        schema::object({ schema::parameter::path("/gltf/cameras/\\d+/perspective/.*"), { "value", schema::any() } })); // value will be checked by generic function

    operation.operation = [overloadOperation](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        std::string stringPath = params.at(schema::parameter::pathKey);
        size_t index = stringPath.find("/perspective/");
        auto cameraPath = nlohmann::json::json_pointer(stringPath.substr(0, index));
        if (state.contains(cameraPath / "orthographic")) {
            std::vector<std::string> extensions;
            operations::utility::getExtensionsAtPath(cameraPath / "orthographic", state, extensions);
            state.at(cameraPath).erase("orthographic");
            if (!extensions.empty()) {
                auto taskDescription = R"(
            {
                "update_extension_flags": {
                }
            }
            )"_json;
                taskDescription["update_extension_flags"]["extensions"] = extensions;
                state = library.call(taskDescription, std::move(state), datastorage, callback);
            }
        }
        state[cameraPath / "type"] = "perspective";
        auto taskDescription = nlohmann::json({ { overloadOperation, params } });
        state = library.call(std::move(taskDescription), std::move(state), datastorage, callback, true);
        return state;
    };

    return operation;
}

Operation overload::setMaterialExtensionsWithUnlitConflict(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema, const std::string& description, const std::string& detailedDescription)
{
    Operation operation;
    operation.name = overloadOperation;

    auto parameterSchemas = nlohmann::json::array();
    std::vector<std::string> paths = {
        "/gltf/materials/\\d+/extensions/KHR_materials_clearcoat(/.*)?",
        "/gltf/materials/\\d+/extensions/KHR_materials_emissive_strength(/.*)?",
        "/gltf/materials/\\d+/extensions/KHR_materials_ior(/.*)?",
        "/gltf/materials/\\d+/extensions/KHR_materials_iridescence(/.*)?",
        "/gltf/materials/\\d+/extensions/KHR_materials_sheen(/.*)?",
        "/gltf/materials/\\d+/extensions/KHR_materials_specular(/.*)?",
        "/gltf/materials/\\d+/extensions/KHR_materials_transmission(/.*)?",
        "/gltf/materials/\\d+/extensions/KHR_materials_volume(/.*)?"
    };
    for (const auto& path : paths) {
        std::map<std::string, nlohmann::json> params = { schema::parameter::path(path) };
        if (overloadOperation == "set_gltf_value") {
            params["value"] = schema::any();
        } else if (overloadOperation == "insert_gltf_pointer" || overloadOperation == "set_gltf_pointer") {
            params["pointer"] = schema::any();
        }
        auto parameterSchema = schema::object(params);
        parameterSchemas.push_back(parameterSchema);
    }
    operation.schema = schema::operation(description, detailedDescription, schema::oneOf(parameterSchemas));

    operation.operation = [overloadOperation](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        std::string stringPath = params.at(schema::parameter::pathKey);
        nlohmann::json::json_pointer extensions = reactor::json::getPointerPrefix(nlohmann::json::json_pointer(stringPath), 4); // If we omit /gltf this needs to change
        if (state.contains(extensions / "KHR_materials_unlit")) {
            auto taskDescription = R"(
            {
                "reset": {
                }
            }
            )"_json;
            taskDescription["reset"]["path"] = (extensions / "KHR_materials_unlit").to_string();
            state = library.call(taskDescription, std::move(state), datastorage, callback);
        }
        auto taskDescription = nlohmann::json({ { overloadOperation, params } });
        state = library.call(std::move(taskDescription), std::move(state), datastorage, callback, true); // Material extensions can have multiple conflicts
        return state;
    };

    return operation;
}

Operation overload::setMaterialUnlit(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema, const std::string& description, const std::string& detailedDescription)
{
    Operation operation;
    operation.name = overloadOperation;

    std::map<std::string, nlohmann::json> params = { schema::parameter::path("/gltf/materials/\\d+/extensions/KHR_materials_unlit(/.*)?") };
    if (overloadOperation == "set_gltf_value") {
        params["value"] = schema::any();
    } else if (overloadOperation == "insert_gltf_pointer" || overloadOperation == "set_gltf_pointer") {
        params["pointer"] = schema::any();
    }

    operation.schema = schema::operation(description, detailedDescription, schema::object(params)); // value will be checked by generic function

    operation.operation = [overloadOperation](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        const std::vector<std::string> extensionVector = {
            "KHR_materials_clearcoat",
            "KHR_materials_emissive_strength",
            "KHR_materials_ior",
            "KHR_materials_iridescence",
            "KHR_materials_sheen",
            "KHR_materials_specular",
            "KHR_materials_transmission",
            "KHR_materials_volume"
        };
        std::string stringPath = params.at(schema::parameter::pathKey);
        nlohmann::json::json_pointer extensions = reactor::json::getPointerPrefix(nlohmann::json::json_pointer(stringPath), 4); // If we omit /gltf this needs to change
        for (auto& extension : extensionVector) {
            if (state.contains(extensions / extension)) {
                auto taskDescription = R"(
                {
                    "reset": {
                    }
                }
                )"_json;
                taskDescription["reset"]["path"] = (extensions / extension).to_string();
                state = library.call(taskDescription, std::move(state), datastorage, callback);
            }
        }
        auto taskDescription = nlohmann::json({ { overloadOperation, params } });
        state = library.call(std::move(taskDescription), std::move(state), datastorage, callback, true);
        return state;
    };

    return operation;
}

Operation overload::setMaterialExtensionsWithSpecGlossConflict(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema, const std::string& description, const std::string& detailedDescription)
{
    Operation operation;
    operation.name = overloadOperation;

    auto parameterSchemas = nlohmann::json::array();
    std::vector<std::string> paths = {
        "/gltf/materials/\\d+/extensions/KHR_materials_clearcoat(/.*)?",
        "/gltf/materials/\\d+/extensions/KHR_materials_ior(/.*)?",
        "/gltf/materials/\\d+/extensions/KHR_materials_iridescence(/.*)?",
        "/gltf/materials/\\d+/extensions/KHR_materials_sheen(/.*)?",
        "/gltf/materials/\\d+/extensions/KHR_materials_specular(/.*)?",
        "/gltf/materials/\\d+/extensions/KHR_materials_transmission(/.*)?",
        "/gltf/materials/\\d+/extensions/KHR_materials_volume(/.*)?"
    };
    for (const auto& path : paths) {

        std::map<std::string, nlohmann::json> params = { schema::parameter::path(path) };
        if (overloadOperation == "set_gltf_value") {
            params["value"] = schema::any();
        } else if (overloadOperation == "insert_gltf_pointer" || overloadOperation == "set_gltf_pointer") {
            params["pointer"] = schema::any();
        }
        auto parameterSchema = schema::object(params);
        parameterSchemas.push_back(parameterSchema);
    }
    operation.schema = schema::operation(description, detailedDescription, schema::oneOf(parameterSchemas));

    operation.operation = [overloadOperation](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        std::string stringPath = params.at(schema::parameter::pathKey);
        nlohmann::json::json_pointer extensions = reactor::json::getPointerPrefix(nlohmann::json::json_pointer(stringPath), 4); // If we omit /gltf this needs to change
        if (state.contains(extensions / "KHR_materials_pbrSpecularGlossiness")) {
            auto taskDescription = R"(
            {
                "reset": {
                }
            }
            )"_json;
            taskDescription["reset"]["path"] = (extensions / "KHR_materials_pbrSpecularGlossiness").to_string();
            state = library.call(taskDescription, std::move(state), datastorage, callback);
        }
        auto taskDescription = nlohmann::json({ { overloadOperation, params } });
        state = library.call(std::move(taskDescription), std::move(state), datastorage, callback, true);
        return state;
    };

    return operation;
}

Operation overload::setMaterialSpecGloss(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema, const std::string& description, const std::string& detailedDescription)
{
    Operation operation;
    operation.name = overloadOperation;

    std::map<std::string, nlohmann::json> params = { schema::parameter::path("/gltf/materials/\\d+/extensions/KHR_materials_pbrSpecularGlossiness(/.*)?") };
    if (overloadOperation == "set_gltf_value") {
        params["value"] = schema::any();
    } else if (overloadOperation == "insert_gltf_pointer" || overloadOperation == "set_gltf_pointer") {
        params["pointer"] = schema::any();
    }

    operation.schema = schema::operation(description, detailedDescription, schema::object(params));

    operation.operation = [overloadOperation](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        const std::vector<std::string> extensionVector = {
            "KHR_materials_clearcoat",
            "KHR_materials_ior",
            "KHR_materials_iridescence",
            "KHR_materials_sheen",
            "KHR_materials_specular",
            "KHR_materials_transmission",
            "KHR_materials_volume"
        };
        std::string stringPath = params.at(schema::parameter::pathKey);
        nlohmann::json::json_pointer extensions = reactor::json::getPointerPrefix(nlohmann::json::json_pointer(stringPath), 4); // If we omit /gltf this needs to change
        for (auto& extension : extensionVector) {
            if (state.contains(extensions / extension)) {
                auto taskDescription = R"(
                {
                    "reset": {
                    }
                }
                )"_json;
                taskDescription["reset"]["path"] = (extensions / extension).to_string();
                state = library.call(taskDescription, std::move(state), datastorage, callback);
            }
        }
        auto taskDescription = nlohmann::json({ { overloadOperation, params } });
        state = library.call(std::move(taskDescription), std::move(state), datastorage, callback, true);
        return state;
    };

    return operation;
}

Operation overload::setAnimationSamplerReference(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = overloadOperation;

    operation.schema = schema::operation("Set glTF pointer (animation sampler)", "Sets pointer to an animation sampler. Makes sure that the sampler exists in the same animation as the channel.",
        schema::object(
            { schema::parameter::path("/gltf/animations/\\d+/channels/\\d+/sampler"),
                { "pointer", schema::pointer("/gltf" + linearizedGltfSchema["/animations/\\d+/channels/\\d+/sampler"]["ux3d_pathPrefix"].get<std::string>() + "/\\d+") } }));

    operation.operation = [overloadOperation](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params.at(schema::parameter::pathKey));
        auto pointer = nlohmann::json::json_pointer(params.at("pointer"));

        if (path.parent_pointer().parent_pointer().parent_pointer() != pointer.parent_pointer().parent_pointer()) {
            throw std::invalid_argument("Sampler does not part of the same animation as channel");
        }

        auto taskDescription = nlohmann::json({ { overloadOperation, params } });
        state = library.call(std::move(taskDescription), std::move(state), datastorage, callback, true);
        return state;
    };

    return operation;
}

Operation overload::setExtensionUsed(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema, const std::string& description, const std::string& detailedDescription)
{
    Operation operation;
    operation.name = overloadOperation;

    std::map<std::string, nlohmann::json> params = { schema::parameter::path(".*/extensions/.*") };
    if (overloadOperation == "set_gltf_value") {
        params["value"] = schema::any();
    } else if (overloadOperation == "insert_gltf_pointer" || overloadOperation == "set_gltf_pointer") {
        params["pointer"] = schema::any();
    }

    nlohmann::json schema = schema::object(params);

    if (overloadOperation == "create_gltf_component") {
        schema["additionalProperties"] = true;
    }

    operation.schema = schema::operation(description, detailedDescription, schema);

    operation.operation = [overloadOperation](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto pathParts = reactor::utils::splitString(params.at(schema::parameter::pathKey), "/");
        for (size_t i = 0; i < pathParts.size() - 1; ++i) {
            if (pathParts[i] == "extensions") {
                utility::useExtension(state, pathParts[i + 1], false);
            }
        }
        auto taskDescription = nlohmann::json({ { overloadOperation, params } });
        try {
            state = library.call(std::move(taskDescription), std::move(state), datastorage, callback, true); // Material extensions can have multiple conflicts
        } catch (std::exception e) {
            throw e;
        }
        return state;
    };

    return operation;
}

Operation overload::applyOperationToArrayProperty(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema, const std::string& propertyPatternPath, const std::string& description, const std::string& detailedDescription)
{
    Operation operation;
    operation.name = overloadOperation;

    nlohmann::json::json_pointer pointer(propertyPatternPath);
    std::string property = pointer.back();
    pointer.pop_back();

    std::map<std::string, nlohmann::json> params = { schema::parameter::path(pointer.to_string()) };

    nlohmann::json schema = schema::object(params);

    schema["additionalProperties"] = true;

    operation.schema = schema::operation(description, detailedDescription, schema);

    operation.operation = [overloadOperation, property](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        nlohmann::json::json_pointer path(params.at("path"));
        if (!state.contains(path / property)) {
            throw std::invalid_argument(path.to_string() + " does not contain " + property);
        }
        nlohmann::json forEachParams = nlohmann::json::object();
        forEachParams["operation"] = overloadOperation;
        forEachParams["path"] = (path / property).to_string();
        nlohmann::json parameters = params;
        parameters.erase("path");
        if (!parameters.empty()) {
            forEachParams["parameters"] = parameters;
        }
        auto taskDescription = nlohmann::json({ { "dangerous.forEachElementInArray", forEachParams } });
        try {
            state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
        } catch (...) {
            std::rethrow_exception(std::current_exception());
        }
        return state;
    };

    return operation;
}

Operation overload::setImageURI(const std::string& overloadOperation, const nlohmann::json& glTFSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = overloadOperation;

    operation.schema = schema::operation("Set glTF value (image uri)",
        "Sets the image uri if allowed",
        schema::object({ schema::parameter::path("/gltf/images/\\d+/uri"), { "value", linearizedGltfSchema.at("/images/\\d+/uri") } }));

    operation.operation = [overloadOperation](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params.at(schema::parameter::pathKey));
        if (state.contains(path.parent_pointer() / "bufferView")) {
            throw std::runtime_error("Cannot set uri if bufferView is set");
        }
        if (!state.contains(path)) {
            throw std::runtime_error("Please convert storage type to file first");
        }
        if (reactor::utils::isHashedDataUri(state.at(path))) {
            throw std::runtime_error("Cannot change data uri");
        }

        if (reactor::utils::isHashedDataUri(params.at("value"))) {
            throw std::invalid_argument("This uri is reserved for embedded data");
        }

        std::string oldUri = state.at(path);
        nlohmann::json::json_pointer filePath = "/files"_json_pointer / oldUri;
        if (state.contains(filePath)) {
            auto id = state.at(filePath);
            state["files"].erase(oldUri);
            state["files"][params.at("value")] = id;
        }

        auto taskDescription = nlohmann::json({ { overloadOperation, params } });
        state = library.call(std::move(taskDescription), std::move(state), datastorage, callback, true);
        return state;
    };

    return operation;
}
