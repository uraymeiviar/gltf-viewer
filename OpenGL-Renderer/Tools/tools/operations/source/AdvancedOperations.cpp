// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "AdvancedOperations.h"
#include "operations/Library.h"
#include "operations/Schema.h"
#include "reactor/Gltf.h"
#include "reactor/Reactor.h"

using namespace ux3d;
using namespace ux3d::operations;

Operation advanced::paste(const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "advanced.paste",

    operation.schema = schema::operation(
        "Paste value",
        "First check if the value can be inserted as a reference in the parent. If not insert it as a copy."
        "If this also fails, try to insert it with the same parent.",
        schema::object({ schema::parameter::path("\\/gltf.*"), schema::parameter::path("\\/gltf.*", "parent"), schema::parameter::path("\\/gltf.*", "absoluteParent") })),

    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        std::string path = nlohmann::json::json_pointer(params["path"]);
        std::string truncatedPath = path.substr(5);
        std::string parentPath = nlohmann::json::json_pointer(params["parent"]);
        std::string absoluteParent = nlohmann::json::json_pointer(params["absoluteParent"]);
        std::string copyTarget = "";
        nlohmann::json stateCopy = state;

        std::vector<std::string> pasteArrayCandidates;
        std::vector<std::string> pasteCandidates;
        reactor::gltf::getSettableProperties(parentPath.substr(5), truncatedPath, linearizedGltfSchema, pasteCandidates, pasteArrayCandidates);
        if (pasteArrayCandidates.size() == 1) {
            copyTarget = parentPath + pasteArrayCandidates.front();
            nlohmann::json operation = {
                { "insert_gltf_pointer",
                    { { "path", copyTarget },
                        { "pointer", path } } }
            };
            try {
                return library.call(operation, std::move(state), datastorage, callback);
            } catch (...) {
            }
        } else if (pasteArrayCandidates.size() == 0 && pasteCandidates.size() == 1) {
            copyTarget = parentPath + pasteCandidates.front();
            nlohmann::json operation = {
                { "set_gltf_pointer",
                    { { "path", copyTarget },
                        { "pointer", path } } }
            };
            try {
                return library.call(operation, std::move(state), datastorage, callback);
            } catch (...) {
            }
        } else if (nlohmann::json::json_pointer parentPointer = nlohmann::json::json_pointer(absoluteParent).parent_pointer();
                   state.at(parentPointer).is_array() && path == parentPath) {
            copyTarget = parentPointer.to_string();
        } else {
            throw std::runtime_error("No clear target for paste");
        }

        // Create a shallow copy and insert it
        if (!copyTarget.empty()) {
            nlohmann::json operation = {
                { "clone_property", { { "path", path }, { "cloneModeShallow", true }, { "target", parentPath } } }
            };
            return library.call(operation, std::move(stateCopy), datastorage, callback);
        }

        return stateCopy;
    };
    return operation;
}

Operation advanced::cutAndPaste(const nlohmann::json& linearizedGltfSchema)
{
    /**
     * @brief expected input:
     * {
     *      "targetPath": "absolut path to target element",
     *      "elements": [
     *              list of moved elements
     *          {
     *              "referencePath": "reference path (e.g "/nodes/0/mesh")",
     *              "absolutePath": "absolute path to element (e.g "/meshes/2")"
     *          }
     *      ]
     * }
     */

    auto copyElementSignature = schema::object({ schema::parameter::path("\\/gltf.*", "absolutePath"), schema::parameter::path("\\/gltf.*", "referencePath") });

    Operation operation;
    operation.name = "advanced.cutAndPaste",

    operation.schema = schema::operation(
        "Cut and Paste value",
        "Cuts the value from its original location, then does a paste action.",
        schema::object({ schema::parameter::path("\\/gltf.*", "targetPath"), { "elements", schema::array(copyElementSignature) } })),

    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        std::string targetPath = nlohmann::json::json_pointer(params["targetPath"]);

        // get cleaned input elements (remove all elements contained in subtrees of other elements)
        std::vector<nlohmann::json> elementsToProcess = reactor::cleanSubTreeElementsFromArray(params["elements"], state["gltf"], linearizedGltfSchema);

        // sort elements in descending order to avoid conflicts arrising form removal in wrong order
        const auto compElem = [](const nlohmann::json& p1, const nlohmann::json& p2) {
            std::string path1 = p1["referencePath"];
            std::string path2 = p2["referencePath"];
            return path1.compare(path2) >= 0;
        };
        std::sort(elementsToProcess.begin(), elementsToProcess.end(), compElem);

        // iterate all elements that are to be added
        for (auto& elem : elementsToProcess) {
            std::string referencePath = nlohmann::json::json_pointer(elem["referencePath"]);
            std::string absolutePath = nlohmann::json::json_pointer(elem["absolutePath"]);
            std::string truncatedAbsolutePath = absolutePath.substr(5);

            nlohmann::json operationCut = { { "remove_gltf_pointer", { { "path", referencePath } } } };
            nlohmann::json operation;

            std::vector<std::string> pasteArrayCandidates;
            std::vector<std::string> pasteCandidates;
            reactor::gltf::getSettableProperties(targetPath.substr(5), truncatedAbsolutePath, linearizedGltfSchema, pasteCandidates, pasteArrayCandidates);
            if (pasteArrayCandidates.size() == 1) {
                std::string copyTarget = targetPath + pasteArrayCandidates.front();
                operation = {
                    { "insert_gltf_pointer",
                        { { "path", copyTarget },
                            { "pointer", absolutePath } } }
                };
            } else if (pasteArrayCandidates.size() == 0 && pasteCandidates.size() == 1) {
                std::string copyTarget = targetPath + pasteCandidates.front();
                operation = {
                    { "set_gltf_pointer",
                        { { "path", copyTarget },
                            { "pointer", absolutePath } } }
                };
            } else {
                throw std::runtime_error("No clear target for paste");
            }

            state = library.call(operationCut, std::move(state), datastorage, callback);
            state = library.call(operation, std::move(state), datastorage, callback);
        }

        return state;
    };
    return operation;
}
