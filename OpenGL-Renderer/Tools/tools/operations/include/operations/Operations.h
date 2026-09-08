// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include "datastorage/DataStorage.h"
#include <nlohmann/json.hpp>
#include <vector>

namespace ux3d::operations {

class Library;

// Schema for the parameters of the operation. The schema can contain information about
// which values of parameters are allowed, giving the possibility for some automated verification of the
// operations parameters. At least it should specify the required parameters of the operation.
using schema_function = nlohmann::json (*)();

// Execution function for the operation
using operation_function = std::function<nlohmann::json(const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const operations::Library& library, std::function<void(const nlohmann::json&)> callback)>;

// The validity function returns true if the operation can be run with the current state and parameters. This
// function can mainly be used to disable the function in the interface.
// Note that the operation function can still throw exceptions even if this validity function returns true.
using validity_function = std::function<bool(const nlohmann::json& params, const nlohmann::json& state, const bool deemedValid)>;

// Operation Protocol defines the basic interface for all operations in this library
struct Operation {
    std::string name = "";
    nlohmann::json schema = {};
    operation_function operation = nullptr;
    validity_function validity = nullptr;
};

// Operations that run on a gltf state (as provided by mementos, expects a json
// {
//      "gltf": {...},
//      "files": {...},
//      "buffer": {...}
// }
std::vector<Operation> gltfOperations(const nlohmann::json& glTFSchema, const nlohmann::json& linearizedGltfSchema);

// Advanced operations that run on a gltf state (as provided by mementos, expects a json
// {
//      "gltf": {...},
//      "files": {...},
//      "buffer": {...}
// }
std::vector<Operation> advancedGltfOperations(const nlohmann::json& glTFSchema, const nlohmann::json& linearizedGltfSchema);

// Operations that run on an editor state
std::vector<Operation> editorOperations();

}
