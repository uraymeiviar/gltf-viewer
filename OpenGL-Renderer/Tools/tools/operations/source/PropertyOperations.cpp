// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "PropertyOperations.h"
#include "HelperFunctions.h"
#include "StorageOperations.h"
#include "TangentGenerator.h"
#include "TexCoordGeneration.h"
#include "datastorage/DataStorage.h"
#include "imageformats/imageformats.h"
#include "operations/Library.h"
#include "operations/Operations.h"
#include "operations/Schema.h"
#include "reactor/GltfEnums.h"
#include "reactor/Json.h"
#include "reactor/Reactor.h"
#include "reactor/Schema.h"
#include "reactor/Utils.h"

#include <fstream>
#include <functional>
#include <gltf_schema/gltf_single_json.h>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <unordered_set>

#include <fmt/format.h>
#include <gtc/quaternion.hpp>
#include <gtx/matrix_decompose.hpp>
#include <vec2.hpp>
#include <vec3.hpp>
#include <vec4.hpp>

using namespace ux3d;
using namespace ux3d::operations;
using namespace std::string_literals;

const std::vector<uint8_t>& gltf::getBufferData(std::string fileID, const nlohmann::json& state, const datastorage::DataStorage& datastorage)
{
    uint64_t dataID = state.at("files").at(fileID);
    const std::vector<uint8_t>& bufferData = datastorage.at(dataID);
    return bufferData;
}

nlohmann::json::json_pointer gltf::cloneProperty(const nlohmann::json::json_pointer path, nlohmann::json& state, datastorage::DataStorage& datastorage, const Library& library, bool clonemodeShallow, const nlohmann::json& linearizedGltfSchemaRaw, std::function<void(const nlohmann::json&)> callback)
{
    int removalIndex = std::stoi(path.back());
    auto& gltf = state["gltf"];
    auto localPath = nlohmann::json::json_pointer(path.to_string().substr("/gltf"s.size()));
    auto linearizedGltfSchema = reactor::gltf::schema::resolveAdditionalProperties(linearizedGltfSchemaRaw, gltf);

    struct PatchInfo {
        std::string referenceLocation;
        std::string targetLocation;
    };
    std::vector<PatchInfo> patchVector;

    std::map<std::string, uint32_t> cloneMap;

    auto cloneSinglePropertyFunc = [&state, &library, &datastorage](const nlohmann::json::json_pointer path) -> uint32_t {
        auto parentPath = path.parent_pointer();
        auto property = state.at(path); // copy

        // handle buffer clone
        if (path.parent_pointer().back().compare("buffers") == 0) {
            bool bufferIsEmbedded = false;
            if (property.contains("uri")) {
                auto uri = property["uri"].get<std::string>();
                bufferIsEmbedded = reactor::utils::isHashedDataUri(uri);
            }

            nlohmann::json::json_pointer dataIDPath = reactor::utils::getDataIDPath(path, state);

            if (!state.contains(dataIDPath)) {
                throw std::runtime_error("No valid data found");
            }
            uint64_t datastorageID = state.at(dataIDPath);
            auto& data = datastorage.at(datastorageID);

            // create file or embedded buffer depending on original buffer
            reactor::utils::createBuffer(state, datastorage, data, bufferIsEmbedded);

            return state.at(parentPath).size() - 1;
        }

        // handle image clone
        if (path.parent_pointer().back().compare("images") == 0) {
            // if image has no uri, data is handled by respective buffer
            if (property.contains("uri") && !property["uri"].empty()) {
                auto uri = property["uri"].get<std::string>();
                nlohmann::json::json_pointer dataIDPath = reactor::utils::getDataIDPath(path, state);

                if (reactor::utils::isHashedDataUri(uri)) {
                    // embedded
                    property["uri"] = std::string("base64_embedded_data_").append(reactor::utils::getRandomString());
                } else {
                    // file
                    size_t dotIndex = uri.find_last_of('.');
                    std::string fileExtension = "";
                    if (dotIndex != std::string::npos) {
                        fileExtension = uri.substr(dotIndex);
                    }
                    property["uri"] = reactor::utils::createURIStem(state) + fileExtension;
                }
                nlohmann::json::json_pointer newDataIDPath = reactor::utils::getDataIDPath(property, state);

                if (state.contains(dataIDPath)) {
                    uint64_t datastorageID = state.at(dataIDPath);
                    auto& data = datastorage.at(datastorageID);
                    datastorage::DataStorage::ID dataStorageIDClone = datastorage.insert(data);
                    state[newDataIDPath] = dataStorageIDClone;
                }
            }
        }

        state.at(parentPath).push_back(property);
        return state.at(parentPath).size() - 1;
    };

    int targetElement = -1;
    std::function<void(const nlohmann::json::json_pointer path, nlohmann::json& jsonElement, bool cloneMode)> recursiveTraversal;
    recursiveTraversal = [&recursiveTraversal, &cloneSinglePropertyFunc, &state, &targetElement, &patchVector, &cloneMap](const nlohmann::json::json_pointer path, nlohmann::json jsonElement, bool cloneMode) -> void {
        if (jsonElement.contains("/data/path"_json_pointer)) {
            if (path.to_string().compare(jsonElement["/data/path"_json_pointer].get<std::string>()) == 0) {
                cloneMode = true;
                cloneMap[path] = 0;
            }
        }

        if (jsonElement.contains("children") && jsonElement["children"].is_array()) {
            for (auto& el : jsonElement["children"].items()) {
                recursiveTraversal(path, el.value(), cloneMode);

                if (cloneMode) {
                    auto childPath = nlohmann::json::json_pointer(el.value()["/data/path"_json_pointer].get<std::string>());
                    auto sourcePath = nlohmann::json::json_pointer(el.value()["/data/source"_json_pointer].get<std::string>());

                    if (childPath.parent_pointer().back().compare("primitives") != 0) {
                        // skip primitives since they are no Child of Root objects
                        cloneMap[childPath] = 0;
                        patchVector.push_back({ sourcePath.to_string(), childPath.to_string() });
                    }
                }
            }
        }
    };

    nlohmann::json filteredSchema;
    for (auto item : linearizedGltfSchema.items()) {
        if (item.value().contains("ux3d_pathPrefix")) {
            filteredSchema[item.key()] = item.value();
        }
    }

    nlohmann::json hierarchy = reactor::getCloneHierarchy(filteredSchema, linearizedGltfSchema, gltf, localPath);

    recursiveTraversal(localPath, hierarchy, false);

    std::vector<std::string> bufferVector;
    for (auto& clone : cloneMap) {
        if (clonemodeShallow == true) {
            // if we clone shallow, we only clone nodes and scenes
            if (nlohmann::json::json_pointer(clone.first).parent_pointer().back().compare("nodes") != 0 && nlohmann::json::json_pointer(clone.first).parent_pointer().back().compare("scenes") != 0) {
                clone.second = std::stoi(nlohmann::json::json_pointer(clone.first).back());
                continue;
            }
        }
        clone.second = cloneSinglePropertyFunc(nlohmann::json::json_pointer("/gltf" + clone.first));

        if (nlohmann::json::json_pointer(clone.first).parent_pointer().back().compare("buffers") == 0) {
            // we have created a copy of this buffer
            // write this buffer in a list to optimize it later.
            bufferVector.emplace_back((nlohmann::json::json_pointer(clone.first).parent_pointer() / clone.second).to_string());
        }
    }

    auto getTwin = [&](std::string s) -> std::string {
        if (cloneMap.count(s) == 0) {
            return "";
        }
        uint32_t id = cloneMap[s];
        std::string twinPath = (nlohmann::json::json_pointer(s).parent_pointer() / id).to_string();
        return twinPath;
    };

    auto getCloneReferenceLocation = [&](std::string s) -> std::string {
        std::vector<std::string> tokens = reactor::utils::splitString(s, "/");

        if (tokens.size() < 3) {
            return "";
        }

        std::string originalProperty = "/" + tokens[1] + "/" + tokens[2];

        if (getTwin(originalProperty).size() == 0) {
            throw std::runtime_error("Unable to find twin of cloned property");
        }

        std::string cloneProperty = getTwin(originalProperty);
        std::vector<std::string> cloneTokens = reactor::utils::splitString(cloneProperty, "/");
        if (cloneTokens.size() < 3) {
            return "";
        }

        tokens[2] = cloneTokens[2]; // this actually fixes the path (only works with CoRs)

        return reactor::utils::mergeString(tokens, "/"); // put back together the updated path and return
    };

    for (auto& patch : patchVector) {
        std::string cloneReferenceLocation = getCloneReferenceLocation(patch.referenceLocation);
        if (cloneReferenceLocation.size() == 0) {
            throw std::runtime_error("Invalid clone path for " + patch.referenceLocation);
            continue;
        }
        state[nlohmann::json::json_pointer("/gltf" + cloneReferenceLocation)] = cloneMap[patch.targetLocation];
    }

    // Cleanup Buffers:

    for (auto& bufferPath : bufferVector) {
        auto taskDescription = R"(
            {
                "trim_buffer": {
                    "path": "$bufferPath"
                }
            }
            )"_json;

        taskDescription["trim_buffer"]["path"] = "/gltf" + bufferPath;
        state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
    }

    return nlohmann::json::json_pointer("/gltf" + getTwin(localPath.to_string())); // ToDo: return path of clone
}

void gltf::addNodeHelper(nlohmann::json& state, nlohmann::json::json_pointer pathTarget, uint32_t nodeID)
{
    if (state.at(pathTarget).is_array()) {
        state.at(pathTarget).emplace_back(nodeID);
    } else if (pathTarget.to_string().find("scenes") != std::string::npos) {
        // parent is scene
        if (not state.at(pathTarget).contains("nodes")) {
            auto& scene = state.at(pathTarget);
            scene["nodes"] = nlohmann::json::array();
        }
        state.at(pathTarget / "nodes").emplace_back(nodeID);
    } else if (pathTarget.to_string().find("nodes") != std::string::npos) {
        // parent is node
        if (not state.at(pathTarget).contains("children")) {
            auto& node = state.at(pathTarget);
            node["children"] = nlohmann::json::array();
        }
        state.at(pathTarget / "children").emplace_back(nodeID);
    }
};

const Operation gltf::addNode = {
    .name = "add_node",

    .schema = schema::operation(
        "Add Node",
        "Adds another node",
        schema::object(
            { schema::parameter::path("(/gltf/nodes/\\d+)|(/gltf/scenes/\\d+)"),
                { "pathSrc", schema::pointer("/gltf/nodes/\\d+") } })),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage&, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto pathSrc = nlohmann::json::json_pointer(params["pathSrc"]);
        auto pathTarget = nlohmann::json::json_pointer(params["path"]);
        if (pathSrc == pathTarget) {
            throw std::runtime_error("No node added. Target and source path where identical.");
        }

        auto nodeID = std::stoi(pathSrc.back());

        addNodeHelper(state, pathTarget, nodeID);

        auto results = reactor::gltf::validateNodeHierarchies(state.at("gltf"));
        if (!results.empty()) {
            throw std::invalid_argument(std::string("") + "The node cannot be inserted at \"" + pathTarget.to_string() + "\" as it would produce an invalid node hierarchy.");
        }
        return state;
    }
};

const Operation gltf::addNewNode = {
    .name = "add_new_node",

    .schema = schema::operation(
        "Add New Node",
        "Adds a newly created node",
        schema::object({ schema::parameter::path("(/gltf/nodes/\\d+)|(/gltf/scenes/\\d+)") })),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage&, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        if (not state.at(nlohmann::json::json_pointer("/gltf")).contains("nodes")) {
            auto& gltf = state.at(nlohmann::json::json_pointer("/gltf"));
            gltf["nodes"] = nlohmann::json::array();
        }
        state.at(nlohmann::json::json_pointer("/gltf/nodes")).emplace_back(nlohmann::json::object());
        int newNodeID = state.at(nlohmann::json::json_pointer("/gltf/nodes")).size() - 1;

        auto pathTarget = nlohmann::json::json_pointer(params["path"]);

        addNodeHelper(state, pathTarget, newNodeID);

        return state;
    }
};

const Operation gltf::createScene = {
    .name = "create_scene",

    // fill the operation schema header
    .schema = schema::operation(
        "Add New Scene",
        "Adds a newly created scene.",
        schema::object({})),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage&, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        if (not state.at(nlohmann::json::json_pointer("/gltf")).contains("scenes")) {
            auto& gltf = state.at(nlohmann::json::json_pointer("/gltf"));
            gltf["scenes"] = nlohmann::json::array();
        }
        state.at(nlohmann::json::json_pointer("/gltf/scenes")).emplace_back(nlohmann::json::object());
        return state;
    }
};

const Operation gltf::removeNodeReference = {
    .name = "remove_node_reference",

    .schema = schema::operation(
        "Remove Node Reference",
        "Removes the references to the specified node",
        schema::object({
            schema::parameter::path("/gltf/nodes/\\d+"),
        })),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage&, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params["path"]);
        auto removalIndex = std::stoi(path.back());

        // check scenes
        for (auto& scene : state.at(nlohmann::json::json_pointer("/gltf/scenes"))) {
            if (scene.contains("nodes")) {
                auto& children = scene.at("nodes");
                auto it = std::find(children.begin(), children.end(), removalIndex);
                if (it != children.end()) {
                    children.erase(it);
                    return state;
                }
            }
        }
        // check nodes
        for (auto& node : state.at(nlohmann::json::json_pointer("/gltf/nodes"))) {
            if (node.contains("children") && node.at("children").is_array()) {
                auto& children = node.at("children");
                auto it = std::find(children.begin(), children.end(), removalIndex);
                if (it != children.end()) {
                    children.erase(it);
                    return state;
                }
            }
        }
        return state;
    }
};

nlohmann::json::json_pointer gltf::getParent(nlohmann::json::json_pointer propertyPath, const nlohmann::json& state, bool resultIsRequired)
{
    if (propertyPath.parent_pointer().back().compare("nodes") != 0) {
        throw std::runtime_error("Unable to find parent of " + propertyPath.to_string());
        return nlohmann::json::json_pointer();
    }
    uint32_t nodeID = std::stoi(propertyPath.back());

    // check nodes
    for (uint32_t i = 0; i < state.at(nlohmann::json::json_pointer("/gltf/nodes")).size(); i++) {
        auto& node = state.at(nlohmann::json::json_pointer("/gltf/nodes"))[i];
        auto children = nlohmann::json::array();
        if (node.contains("children")) {
            children = node.at("children");
        }
        for (auto& childID : children.items()) {
            if (childID.value() == nodeID) {
                return nlohmann::json::json_pointer("/gltf/nodes") / i;
            }
        }
    }

    // check scenes
    if (state.at(nlohmann::json::json_pointer("/gltf")).contains("scenes")) {
        for (uint32_t i = 0; i < state.at(nlohmann::json::json_pointer("/gltf/scenes")).size(); i++) {
            auto& scene = state.at(nlohmann::json::json_pointer("/gltf/scenes"))[i];
            auto children = nlohmann::json::array();
            if (scene.contains("nodes")) {
                children = scene.at("nodes");
            }
            for (auto& childID : children.items()) {
                if (childID.value() == nodeID) {
                    return nlohmann::json::json_pointer("/gltf/scenes") / i;
                }
            }
        }
    }

    if (resultIsRequired) {
        throw std::runtime_error("Unable to find parent of " + propertyPath.to_string());
    }

    return nlohmann::json::json_pointer();
}

Operation gltf::mergeBuffers(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "merge_buffers";
    // filter child of root properties and create the operation signature for them
    auto linearized = linearizedGltfSchema;
    auto childOfRoots = nlohmann::json::object();
    auto backwardReferences = reactor::gltf::schema::backwardReferences(linearized);

    operation.schema = schema::operation(
        "Merge Buffers",
        "Merge all buffers listed in the array. Merged buffers get deleted. The newly created buffer is appended to the buffer list.",
        schema::object({ { "bufferPaths", schema::array(schema::string("/gltf/buffers/\\d+"), 2) } }));

    operation.operation = [linearized, childOfRoots, backwardReferences](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        if (!state["gltf"].contains("bufferViews") || !state["gltf"].contains("buffers")) {
            throw std::invalid_argument("No bufferViews/buffers found");
        }
        if (!state["gltf"]["bufferViews"].is_array() || !state["gltf"]["buffers"].is_array()) {
            throw std::invalid_argument("Expected array");
        }

        auto& bufferViews = state["gltf"]["bufferViews"];
        const auto& bufferPathsJson = params.at("bufferPaths").get<std::vector<nlohmann::json>>();

        std::vector<nlohmann::json> buffers;
        std::vector<std::string> bufferPaths;
        std::unordered_set<uint32_t> bufferIndices;

        for (const auto& path : bufferPathsJson) {
            if (!state.contains(nlohmann::json::json_pointer(path.get<std::string>()))) {
                throw std::invalid_argument("Element not found");
            }
            bufferPaths.push_back(path.get<std::string>());
            buffers.push_back(state.at(nlohmann::json::json_pointer(path.get<std::string>())));
            uint32_t index = std::stoi(nlohmann::json::json_pointer(path).back());
            bufferIndices.insert(index);
        }

        std::string bufferURI = "";
        std::string bufferName = "";

        // Create bufferView to Buffer mapping
        std::map<uint32_t, uint32_t> bufferViewToBuffer;

        for (int i = 0; i < bufferViews.size(); i++) {
            if (bufferViews[i].contains("buffer")) {
                if (bufferIndices.contains(bufferViews[i].at("buffer").get<uint32_t>())) {
                    bufferViewToBuffer[i] = bufferViews[i].at("buffer");
                }
            }
        }

        auto getPadding = [](uint64_t offset) -> uint64_t {
            // force 4 byte alignment by adding padding to the data
            const uint64_t alignmentOverflow = offset % 4;
            uint64_t alignmentFix = 0;
            if (alignmentOverflow != 0) {
                alignmentFix = 4 - alignmentOverflow;
            }
            return alignmentFix;
        };

        // Calculate size of final buffer
        uint32_t finalBufferSize = 0;
        for (uint32_t i = 0; i < buffers.size(); i++) {
            auto bufferSize = buffers[i].at("byteLength").get<uint32_t>();
            finalBufferSize += bufferSize + getPadding(bufferSize); // 4 byte alignment

            if (bufferURI == "" && buffers[i].contains("uri")) {
                bufferURI = buffers[i].at("uri");
                if (buffers[i].contains("name")) {
                    bufferName = buffers[i].at("name");
                }
            }
        }

        // Copy data to new buffer and store correspondign offset in this map:
        std::map<uint32_t, uint32_t> bufferPosition; // maps each buffer to the corresponding location in the merged buffer
        std::vector<uint8_t> newData(finalBufferSize);

        uint64_t accumulatedOffset = 0;
        for (uint32_t i = 0; i < buffers.size(); i++) {
            nlohmann::json::json_pointer dataIDPath = reactor::utils::getDataIDPath(nlohmann::json::json_pointer(bufferPaths[i]), state);
            uint64_t dataStorageID = state.at(dataIDPath);
            const std::vector<uint8_t>& data = datastorage.at(dataStorageID);
            size_t dataSize = buffers[i].at("byteLength").get<uint32_t>();

            if (dataSize != data.size()) {
                throw std::runtime_error("Json buffer size (" + std::to_string(dataSize) + ") does not match actual buffer size (" + std::to_string(data.size()) + ")");
            }

            std::memcpy(newData.data() + accumulatedOffset, data.data(), dataSize);
            int originalBufferIDX = std::stoi(nlohmann::json::json_pointer(bufferPaths[i]).back());
            bufferPosition[(uint32_t)originalBufferIDX]
                = accumulatedOffset;
            accumulatedOffset += dataSize + getPadding(dataSize); // 4 byte alignment
        }

        if (accumulatedOffset != finalBufferSize) {
            throw std::runtime_error("Buffer size mismatch");
        }

        size_t newBufferIndex = state.at(nlohmann::json::json_pointer("/gltf/buffers")).size();

        const auto bufferPointer = reactor::utils::createBuffer(state, datastorage, newData, false, bufferURI);

        if (!bufferName.empty()) {
            state.at(bufferPointer)["name"] = bufferName;
        }

        // sort buffers in asscending order
        std::sort(bufferPaths.begin(), bufferPaths.end(), [](const std::string& a, const std::string& b) -> bool {
            return std::stoi(nlohmann::json::json_pointer(a).back()) < std::stoi(nlohmann::json::json_pointer(b).back());
        });
        // reverde order for correct deletion later
        std::reverse(bufferPaths.begin(), bufferPaths.end());

        // Update offset and buffer of all affected bufferviews
        for (uint32_t i = 0; i < bufferViews.size(); i++) {
            if (!bufferViewToBuffer.contains(i)) {
                continue; // only update affected bufferViews
            }
            uint32_t referencedBuffer = bufferViewToBuffer[i];
            uint32_t dataOffset = bufferPosition[referencedBuffer];
            if (bufferViews[i].contains("byteOffset")) {
                dataOffset += bufferViews[i].at("byteOffset").get<uint32_t>();
            }
            bufferViews[i]["byteOffset"] = dataOffset;
            bufferViews[i]["buffer"] = newBufferIndex;
        }

        // Delete old buffers in reverse order
        for (const auto& path : bufferPaths) {
            auto taskDescription = nlohmann::json({ { "remove", nlohmann::json::object() } });
            taskDescription["remove"]["path"] = path;

            state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
        }

        return state;
    };
    return operation;
}

/*
- Gather all BufferViews that are referencing this buffer.
- Strip Buffer to only contain data of these BufferViews.
*/
Operation gltf::trimBuffer(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "trim_buffer";

    operation.schema = schema::operation(
        "Trim Buffer",
        "Trim Buffer goes through all referencing BufferViews that have the respective Buffer set as target and trims the Buffer to that data",
        schema::object({ schema::parameter::path("/gltf/buffers/\\d+") }));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        if (!state["gltf"].contains("bufferViews")) {
            return state;
        }
        if (!state["gltf"]["bufferViews"].is_array()) {
            return state;
        }

        auto path = nlohmann::json::json_pointer(params.at("path"));
        auto& buffer = state[path];
        uint32_t targetBufferIndex = std::stoi(path.back());

        // Collect all referenced bufferViews
        std::vector<uint32_t> referencedBufferViews;

        auto& bufferViews = state["gltf"]["bufferViews"];

        for (int i = 0; i < bufferViews.size(); i++) {
            if (bufferViews[i].contains("buffer") && bufferViews[i]["buffer"].get<uint32_t>() == targetBufferIndex) {
                referencedBufferViews.push_back(i);
            }
        }

        // Acquire old buffer data
        nlohmann::json::json_pointer dataIDPath = reactor::utils::getDataIDPath(path, state);
        uint64_t dataStorageID = state.at(dataIDPath);
        const std::vector<uint8_t>& bufferData = datastorage.at(dataStorageID);

        // Calculate max buffer size
        uint32_t newBufferSize = 0;
        for (auto& bufferViewIndex : referencedBufferViews) {
            auto& bufferView = bufferViews[bufferViewIndex];
            uint32_t bufferViewSize = bufferView["byteLength"].get<uint32_t>();
            // force 4 byte alignment in all cases
            const uint32_t alignmentOverflow = bufferViewSize % 4;
            if (alignmentOverflow != 0) {
                bufferViewSize += 4 - alignmentOverflow;
            }
            newBufferSize += bufferViewSize;
        }

        // Check if we have the possibility to trim the buffer
        // Otherwise skip the creation of a new buffer
        if (buffer["byteLength"] <= newBufferSize) {
            return state;
        }

        // Create new data out of bufferviews
        std::vector<uint8_t> newData;
        uint32_t currentOffset = 0;
        for (auto& bufferViewIndex : referencedBufferViews) {
            auto& bufferView = bufferViews[bufferViewIndex];
            uint32_t offset = 0u;
            if (bufferView.contains("byteOffset")) {
                offset = bufferView["byteOffset"];
            }
            newData.insert(newData.begin() + currentOffset, bufferData.begin() + offset, bufferData.begin() + offset + bufferView["byteLength"]);

            bufferView["byteOffset"] = currentOffset;
            currentOffset += bufferView["byteLength"].get<uint32_t>();

            // force 4 byte alignment by adding padding to the data
            const uint32_t alignmentOverflow = currentOffset % 4;
            if (alignmentOverflow != 0) {
                const uint32_t alignmentFix = 4 - alignmentOverflow;
                newData.insert(newData.end(), alignmentFix, 0);

                currentOffset += alignmentFix;
            }
        }

        if (newBufferSize == 0) {
            // Delete buffer if size is 0
            std::string op = "remove";
            auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
            taskDescription[op]["path"] = path.to_string();
            state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
            return state;
        }

        // Update old buffer
        buffer["byteLength"] = newBufferSize;

        uint64_t newDataID = datastorage.insert(newData);
        state.at(reactor::utils::getDataIDPath(path, state)) = newDataID;

        return state;
    };
    return operation;
}

Operation gltf::clone(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "clone_property";

    // filter child of root properties and create the operation signature for them
    auto linearized = linearizedGltfSchema;
    auto parameters = nlohmann::json::array();
    for (const auto& [pattern, schema] : linearized.items()) {
        // TODO: use a custom schema entry to identify
        if (schema.contains("allOf")
            && schema.at("allOf").at(0).contains("title")
            && schema.at("allOf").at(0).at("title") == "glTF Child of Root Property") {
            auto parameter = schema::object({ schema::parameter::path("/gltf" + pattern),
                { "cloneModeShallow", nlohmann::json::object() },
                { "target", nlohmann::json::object() } });
            parameters.push_back(parameter);
        }
    }

    // fill the operation schema header
    operation.schema = schema::operation(
        "Clone Property",
        "Clones a property and adds the clone to the target if set. If cloneModeShallow is set, leafs are referencing the same properties.",
        schema::oneOf(parameters));

    // define the operation function
    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        bool cloneModeShallow = params["cloneModeShallow"].get<bool>();
        std::string target = params["target"].get<std::string>();

        auto path = nlohmann::json::json_pointer(params.at("path"));
        if (cloneModeShallow && !path.parent_pointer().empty() && path.parent_pointer().back() != "nodes") {
            state.at(path.parent_pointer()).emplace_back(state.at(path));
            return state;
        }
        auto clonedPropertyPath = cloneProperty(path, state, datastorage, library, cloneModeShallow, linearizedGltfSchema, callback);

        if (path.parent_pointer().back().compare("nodes") == 0) {
            // we have cloned a node
            if (target.compare("parent") == 0) {
                auto parentPath = getParent(path, state, true);
                addNodeHelper(state, parentPath, std::stoi(clonedPropertyPath.back()));
            } else if (target.compare("parent_optional") == 0) {
                auto parentPath = getParent(path, state, false);
                if (!parentPath.empty()) {
                    addNodeHelper(state, parentPath, std::stoi(clonedPropertyPath.back()));
                }
            } else {
                addNodeHelper(state, nlohmann::json::json_pointer(target), std::stoi(clonedPropertyPath.back()));
            }
        }
        return state;
    };

    return operation;
}

Operation gltf::cleanAll(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "clean_gltf";

    // fill the operation schema header
    operation.schema = schema::operation(
        "Clean glTF",
        "Remove all or optionally filtered unused properties from the glTF",
        schema::object({}, { { "filter", schema::array(schema::pointer(".*")) } }));

    // define the operation function
    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        nlohmann::json& gltf = state["gltf"];
        nlohmann::json usedObjects = nlohmann::json::array();

        // this object looks like this:
        // {
        //     /nodes/\\d+/ : [ nodes/0, nodes/4],
        //     /cameras/\\d+/ : [ cameras/4],
        // }
        nlohmann::json unusedObjects;

        nlohmann::json topLevelProperties = reactor::gltf::schema::topLevelProperties(linearizedGltfSchema);

        std::function<void(const nlohmann::json::json_pointer, nlohmann::json&)> addToUsed =
            [&](const nlohmann::json::json_pointer subJsonPtr, nlohmann::json& outArray) {
                // only add to unused if not already contained
                auto itrUnused = std::find(outArray.begin(), outArray.end(), subJsonPtr.to_string());
                if (itrUnused == outArray.end()) {
                    outArray.push_back(subJsonPtr.to_string());
                }
            };

        // There is room for optimization, e.g. by passing a reference to the json object and
        // only emplacing the path prefix there
        std::function<void(const nlohmann::json::json_pointer, nlohmann::json&)> recursiveTraversal =
            [&](const nlohmann::json::json_pointer subJsonPtr, nlohmann::json& outArray) {
                const auto& subJson = gltf.at(subJsonPtr);
                std::string regexPath = reactor::gltf::substituteIndices(subJsonPtr.to_string());
                std::string parentRegexPath = reactor::gltf::substituteIndices(subJsonPtr.parent_pointer().to_string());

                if (linearizedGltfSchema.contains(parentRegexPath) && linearizedGltfSchema.at(parentRegexPath).contains("/additionalProperties/ux3d_pathPrefix"_json_pointer) && gltf.at(subJsonPtr).is_number_integer()) {
                    auto resolvedSubJsonPointer = nlohmann::json::json_pointer(linearizedGltfSchema.at(parentRegexPath).at("/additionalProperties/ux3d_pathPrefix"_json_pointer)) / gltf.at(subJsonPtr).get<int>();
                    addToUsed(resolvedSubJsonPointer, outArray);
                    recursiveTraversal(resolvedSubJsonPointer, outArray);
                }

                if (linearizedGltfSchema.contains(regexPath) == false) {
                    return;
                }
                const auto& subSchema = linearizedGltfSchema.at(regexPath);

                // traverse into json objects with children (non primitives)
                if (false == subJson.is_primitive()) {
                    // traverse all parameters
                    for (const auto& [key, value] : subJson.items()) {
                        recursiveTraversal(subJsonPtr / key, outArray);
                    }
                }

                // replace the context less reference (glTFid) with a full json pointer
                // by reading the ux3d_pathPrefix from the schema
                if (subSchema.contains("ux3d_pathPrefix")) {
                    std::string resolvedPrefix = reactor::gltf::applyParentToReference(subJsonPtr.to_string(), subSchema.at("ux3d_pathPrefix"));
                    auto resolvedSubJsonPointer = nlohmann::json::json_pointer(resolvedPrefix) / gltf.at(subJsonPtr).get<int>();
                    addToUsed(resolvedSubJsonPointer, outArray);
                    recursiveTraversal(resolvedSubJsonPointer, outArray);
                }
            };

        std::function<void(const nlohmann::json::json_pointer, const std::string)> addToUnused =
            [&](const nlohmann::json::json_pointer subJsonPtr, const std::string regexPath) {
                if (unusedObjects.contains(regexPath) == false) {
                    unusedObjects[regexPath] = nlohmann::json::array();
                }
                // only add to unused if not already contained
                auto itrUnused = std::find(unusedObjects[regexPath].begin(), unusedObjects[regexPath].end(), subJsonPtr.to_string());
                if (itrUnused == unusedObjects[regexPath].end()) {
                    unusedObjects[regexPath].push_back(subJsonPtr.to_string());
                }
            };

        std::function<void(const nlohmann::json::json_pointer)> recursiveTraversalForUnused =
            [&](const nlohmann::json::json_pointer subJsonPtr) {
                const auto& subJson = gltf.at(subJsonPtr);
                std::string regexPath = reactor::gltf::substituteIndices(subJsonPtr.to_string());
                std::string parentRegexPath = reactor::gltf::substituteIndices(subJsonPtr.parent_pointer().to_string());
                if (linearizedGltfSchema.contains(regexPath) == false) {
                    return;
                }
                const auto& subSchema = linearizedGltfSchema.at(regexPath);

                // traverse into json objects with children (non primitives)
                if (false == subJson.is_primitive()) {
                    // traverse all parameters
                    for (const auto& [key, value] : subJson.items()) {
                        recursiveTraversalForUnused(subJsonPtr / key);
                    }
                }

                // replace the context less reference (glTFid) with a full json pointer
                // by reading the ux3d_pathPrefix from the schema
                if (subSchema.contains("ux3d_pathPrefix")) {
                    std::string resolvedPrefix = reactor::gltf::applyParentToReference(subJsonPtr.to_string(), subSchema.at("ux3d_pathPrefix"));
                    auto resolvedSubJsonPointer = nlohmann::json::json_pointer(resolvedPrefix) / gltf.at(subJsonPtr).get<int>();
                    recursiveTraversalForUnused(resolvedSubJsonPointer);
                } else if (linearizedGltfSchema.contains(parentRegexPath) && linearizedGltfSchema.at(parentRegexPath).contains("/additionalProperties/ux3d_pathPrefix"_json_pointer) && gltf.at(subJsonPtr).is_number_integer()) {
                    auto resolvedSubJsonPointer = nlohmann::json::json_pointer(linearizedGltfSchema.at(parentRegexPath).at("/additionalProperties/ux3d_pathPrefix"_json_pointer)) / gltf.at(subJsonPtr).get<int>();
                    recursiveTraversalForUnused(resolvedSubJsonPointer);
                }

                // if object is child of root element
                auto isTopLevelproperty = std::find(topLevelProperties.begin(), topLevelProperties.end(), regexPath);
                if (isTopLevelproperty != topLevelProperties.end()) {
                    // check if current item is unused
                    auto itr = std::find(usedObjects.begin(), usedObjects.end(), subJsonPtr.to_string());
                    if (itr == usedObjects.end()) {
                        addToUnused(subJsonPtr, regexPath);
                    }
                }
            };

        // iterate all scenes to find all used objects
        if (gltf.contains("scenes")) {
            for (const auto& scene : gltf["scenes"].items()) {
                nlohmann::json::json_pointer scenePointer("/scenes/" + scene.key());
                usedObjects.push_back(scenePointer.to_string());
                recursiveTraversal(scenePointer, usedObjects);
            }
        }
        if (gltf.contains("animations")) {
            for (const auto& animation : gltf["animations"].items()) {
                nlohmann::json::json_pointer animationPointer("/animations/" + animation.key());
                nlohmann::json usedAnimationObjects = nlohmann::json::array();

                addToUsed(animationPointer, usedAnimationObjects);
                recursiveTraversal(animationPointer, usedAnimationObjects);

                bool found = false;
                for (const auto& usedAnimationObject : usedAnimationObjects) {
                    auto itr = std::find(usedObjects.begin(), usedObjects.end(), usedAnimationObject);
                    if (itr != usedObjects.end()) {
                        for (const std::string& usedAnimationObject : usedAnimationObjects) {
                            addToUsed(nlohmann::json::json_pointer(usedAnimationObject), usedObjects);
                        }
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    std::string regexPath = reactor::gltf::substituteIndices(animationPointer.to_string());
                    addToUnused(animationPointer, regexPath);
                }
            }
        }
        if (gltf.contains("extensions") && gltf["extensions"].contains("KHR_materials_variants") && gltf["extensions"]["KHR_materials_variants"].contains("variants")) {
            for (const auto& variant : gltf["extensions"]["KHR_materials_variants"]["variants"].items()) {
                nlohmann::json::json_pointer variantPointer("/extensions/KHR_materials_variants/variants/" + variant.key());
                addToUsed(variantPointer, usedObjects);
                usedObjects.push_back(variantPointer.to_string());
            }
        }

        // find all unused objects
        nlohmann::json::json_pointer gltfPointer("");
        recursiveTraversalForUnused(gltfPointer);

        // TODO Add user check if you really wanna do this (see GltfCleanup.cpp line 48)

        // if (unusedObjects.empty()) {
        //     throw std::runtime_error("No elements were removed.");
        // }

        std::vector<std::regex> regexFilters;
        if (params.contains("filter")) {
            for (const auto& filter : params["filter"]) {
                regexFilters.emplace_back(filter.get<std::string>());
            }
        }

        for (auto& [key, value] : unusedObjects.items()) {
            // first sort values in reverse order to avoid pointer mismatch problems
            std::sort(value.begin(), value.end(), [](const std::string& a, const std::string& b) -> bool {
                return std::stoi(nlohmann::json::json_pointer(a).back()) > std::stoi(nlohmann::json::json_pointer(b).back());
            });

            // delete elements
            for (std::string elem : value) {
                auto path = "/gltf" + elem;

                bool atLeastOneFilterMatched = false;
                for (const auto& re : regexFilters) {
                    std::smatch matches;
                    if (std::regex_match(path, matches, re)) {
                        atLeastOneFilterMatched = true;
                        break;
                    }
                }
                if (!atLeastOneFilterMatched && !regexFilters.empty()) {
                    continue;
                }

                // this try and catch is here to avoid that the whole cleanup process is stopped
                // if one element cannot be removed.
                // This can for example happen with animations and there samplers, if the sampler is to be deleted after the animation.
                try {
                    nlohmann::json taskDescription = { { "remove", { { "path", path } } } };
                    state = library.call(taskDescription, std::move(state), datastorage, callback);
                } catch (std::exception& e) {
                    // throw std::runtime_error("Error while removing " + path + ": " + e.what());
                }
            }
        }

        if (unusedObjects.size() == 0) {
            throw std::runtime_error("Nothing to clean up");
        }
        callback({ { "message", fmt::format("Cleaned up {} items", unusedObjects.size()) } });

        return state;
    };

    return operation;
}

Operation gltf::calculateMinMax(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "calculate_min_max";

    // fill the operation schema header
    operation.schema = schema::operation(
        "Calculate Min Max",
        "Calculate min and max values of an accessor",
        schema::object({ schema::parameter::path("/gltf/accessors/\\d+") }));

    // define the operation function
    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params.at("path"));
        if (!state.contains(path)) {
            throw std::invalid_argument("Accessor not found");
        }
        auto& accessor = state.at(path);

        auto componentCount = reactor::gltf::componentCount(accessor["type"]);
        std::vector<double> minValues(componentCount, std::numeric_limits<double>::max());
        std::vector<double> maxValues(componentCount, -std::numeric_limits<double>::max());

        uint32_t count = accessor.at("count");

        auto scanData = [&minValues, &maxValues, count, componentCount](auto* data) -> void {
            uint32_t totalPadding = 0;
            for (size_t i = 0; i < count * componentCount; ++i) {
                minValues[i % componentCount] = std::min(minValues[i % componentCount], (double)data[i]);
                maxValues[i % componentCount] = std::max(maxValues[i % componentCount], (double)data[i]);
            }
        };

        reactor::gltf::ComponentType componentType = accessor["componentType"];
        if (componentType != reactor::gltf::ComponentType::Float) {
            // TODO fix handling other types than float
            throw std::invalid_argument("Calculating min/max only supports float values");
        }
        const std::vector<uint8_t> data = reactor::utils::getAccessorByteDataPacked(path, state, datastorage);
        switch (componentType) {
        case reactor::gltf::ComponentType::Byte:
            scanData(reinterpret_cast<const int8_t*>(&(data[0])));
            break;
        case reactor::gltf::ComponentType::UnsignedByte:
            scanData(reinterpret_cast<const uint8_t*>(&(data[0])));
            break;
        case reactor::gltf::ComponentType::Short:
            scanData(reinterpret_cast<const int16_t*>(&(data[0])));
            break;
        case reactor::gltf::ComponentType::UnsignedShort:
            scanData(reinterpret_cast<const uint16_t*>(&(data[0])));
            break;
        case reactor::gltf::ComponentType::UnsignedInt:
            scanData(reinterpret_cast<const uint32_t*>(&(data[0])));
            break;
        case reactor::gltf::ComponentType::Float:
            scanData(reinterpret_cast<const float*>(&(data[0])));
            break;
        }

        accessor["min"] = minValues;
        accessor["max"] = maxValues;

        return state;
    };

    return operation;
}

Operation gltf::cleanNodes()
{
    Operation operation;
    operation.name = "clean_nodes";

    // fill the operation schema header
    operation.schema = schema::operation(
        "Remove empty nodes",
        "Removes all empty nodes from the gltf (also removes all nodes which become empty because their children where deleted).",
        schema::object({}));

    // define the operation function
    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        uint32_t counter = 0;
        nlohmann::json& gltf = state["gltf"];

        std::function<bool(uint32_t)> isJoint = [&](uint32_t nodeIndex) {
            if (gltf.contains("skins")) {
                for (const auto& skin : gltf["skins"]) {
                    for (const int joint : skin["joints"]) {
                        if (joint == nodeIndex) {
                            return true;
                        }
                    }
                }
            }
            return false;
        };
        std::function<bool(uint32_t)> isAnimationTarget = [&](uint32_t nodeIndex) {
            if (gltf.contains("animations")) {
                for (const auto& animation : gltf["animations"]) {
                    for (const auto& channel : animation["channels"]) {
                        const auto& target = channel["target"];
                        if (target.contains("node") && target["node"].get<uint32_t>() == nodeIndex) {
                            return true;
                        }
                    }
                }
            }
            return false;
        };

        std::function<bool(const nlohmann::json, std::string)> keyEmpty = [](const nlohmann::json elem, std::string key) {
            if (elem.contains(key)) {
                if (elem[key].size() > 0) {
                    return false;
                }
            }
            return true;
        };

        if (gltf.contains("nodes") == false) {
            throw std::runtime_error("There are no nodes to clean.");
        }

        // while loop is necessary to resolve nodes that get empty after removing an empty node
        bool done = false;
        while (!done) {
            done = true;
            if (!state["gltf"].contains("nodes")) {
                break;
            }
            nlohmann::json emptyNodes = nlohmann::json::array();
            for (auto& [key, node] : state["gltf"]["nodes"].items()) {
                int index = std::stoi(key);
                bool containsLights = false;
                // check for light extension (light is a required property and does not have to be checked seperatly)
                if (node.contains("extensions") && node["extensions"].contains("KHR_lights_punctual")) {
                    containsLights = true;
                }
                if (keyEmpty(node, "children")
                    && keyEmpty(node, "skin")
                    && keyEmpty(node, "camera")
                    && keyEmpty(node, "mesh")
                    && containsLights == false
                    && isJoint(index) == false
                    && isAnimationTarget(index) == false) {

                    emptyNodes.push_back("/gltf/nodes/" + key);
                }
            }

            if (emptyNodes.size() > 0) {

                std::string op = "remove";
                auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
                taskDescription[op]["path"] = emptyNodes;
                state = library.call(taskDescription, std::move(state), datastorage, callback);

                counter += emptyNodes.size();
                done = false;
            }
        }

        if (counter == 0) {
            throw std::runtime_error("No unused node present");
        }

        callback({ { "message", fmt::format("Cleaned up {} nodes", counter) } });

        return state;
    };

    return operation;
}

Operation gltf::cleanTexCoordAttributes()
{
    Operation operation;
    operation.name = "clean_texture_coordinate_attributes";

    // fill the operation schema header
    operation.schema = schema::operation(
        "Clean Texture Coordinate Attributes",
        "Removes all unused texture coordinate attributes.",
        schema::object({}));

    // define the operation function
    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        nlohmann::json& gltf = state["gltf"];

        // gather all unused
        if (gltf.contains("meshes") == false || gltf["meshes"].size() == 0) {
            throw std::runtime_error("No meshes present in glTF.");
        }

        // iterate over all meshes and gather texcoords for deletion
        nlohmann::json& meshes = gltf["meshes"];
        std::vector<nlohmann::json::json_pointer> deleteAttributeLater;
        for (auto& [meshKey, mesh] : meshes.items()) {
            auto& primitives = mesh["primitives"];
            for (auto& [primitiveKey, primitive] : primitives.items()) {
                std::vector<std::string> texcoords;
                nlohmann::json& attributes = primitive["attributes"];
                for (auto& [key, accessorIdx] : attributes.items()) {
                    if (key.rfind("TEXCOORD_", 0) != std::string::npos) {
                        texcoords.push_back(key);
                    }
                }

                std::sort(texcoords.begin(), texcoords.end(), std::greater());
                // delete elements
                for (std::string elem : texcoords) {
                    deleteAttributeLater.push_back("/gltf/meshes"_json_pointer / meshKey / "primitives" / primitiveKey / "attributes" / elem);
                }
            }
        }

        uint32_t count = 0u;

        // Remove the gathered attributes
        for (auto& path : deleteAttributeLater) {
            auto taskDescription = R"(
            {
                "delete_attribute": {
                    
                }
            }
            )"_json;

            taskDescription["/delete_attribute/path"_json_pointer] = path.to_string();

            try {
                state = library.call(taskDescription, std::move(state), datastorage, callback);
                count++;
            } catch (...) {
                continue;
            }
        }

        if (count > 0) {
            callback({ { "message", fmt::format("Removed {} texture coordinate {}", count, count == 1 ? "attribute" : "attributes") } });
        } else {
            throw std::runtime_error("There are no unused texture coordinate attributes");
        }

        return state;
    };

    return operation;
}

Operation gltf::createTexCoordAttribute()
{
    Operation operation;
    operation.name = "create_texture_coordinates";

    nlohmann::json allowedGenerationTypes = schema::enumeration({ "Sphere",
        "Cylinder",
        "Plane",
        "Cube" });

    // fill the operation schema header
    operation.schema = schema::operation(
        "Create Texture Coordinates",
        "Creates or recreates the specified texture coordinates for the given primitive.",
        schema::object(
            { schema::parameter::path("/gltf/meshes/\\d+/primitives/\\d+"), { "proxyShape", allowedGenerationTypes },
                { "texCoordIndex", schema::number({ 0 }) } },
            { { "normal", schema::array(schema::number(), 3, 3) } }));

    // define the operation function
    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        nlohmann::json& gltf = state["gltf"];

        mesh::generateTexCoords(params, state, datastorage, library, callback);

        return state;
    };

    return operation;
}

Operation gltf::deleteAttribute()
{
    Operation operation;
    operation.name = "delete_attribute";

    // fill the operation schema header
    operation.schema = schema::operation(
        "Delete Attribute",
        "Removes an attribute from a primitive if the removal is valid.",
        schema::object({ schema::parameter::path("/gltf/meshes/\\d+/primitives/\\d+/attributes/[A-Za-z0-9_]*") }));

    auto gltfSchemaMerged = gltf_schema::merged();

    // define the operation function
    operation.operation = [gltfSchemaMerged](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        nlohmann::json& gltf = state["gltf"];
        auto path = nlohmann::json::json_pointer(params.at("path"));
        nlohmann::json& primitive = state.at(path.parent_pointer().parent_pointer());
        nlohmann::json& attributes = state.at(path.parent_pointer());

        std::string attributeName = path.back();

        // if attribute is texcoord check if it used in the primitives material or material variants.
        // and if it is deleted in the correct order
        if (attributeName.rfind("TEXCOORD_", 0) != std::string::npos) {
            auto zippedGltf = reactor::zipSchema(gltfSchemaMerged, gltf);

            uint32_t texCoordIdx
                = stoi(attributeName.substr(9));
            // check material
            if (primitive.contains("material")) {
                uint32_t materialIdx = primitive["material"];
                nlohmann::json::json_pointer materialPointer("/materials/" + std::to_string(materialIdx));
                if (gltf.contains("materials")) {
                    auto& material = gltf.at(materialPointer);
                    if (reactor::areTexCoordsUsedInMaterial(materialPointer, material, texCoordIdx, zippedGltf, gltfSchemaMerged)) {
                        throw std::runtime_error("Cannot delete texture attribute because the primitive's material uses it for a texture.");
                    }
                }
            }

            // check material variants
            if (primitive.contains("extensions") && primitive["extensions"].contains("KHR_materials_variants")) {
                auto& materialVariantsMappings = primitive["extensions"]["KHR_materials_variants"]["mappings"];
                for (auto& [key, value] : materialVariantsMappings.items()) {
                    uint32_t materialIdx = value["material"];
                    nlohmann::json::json_pointer materialPointer("/materials/" + std::to_string(materialIdx));
                    if (gltf.contains("materials")) {
                        auto& material = gltf.at(materialPointer);
                        if (reactor::areTexCoordsUsedInMaterial(materialPointer, material, texCoordIdx, zippedGltf, gltfSchemaMerged)) {
                            throw std::runtime_error("Cannot delete texture attribute because the primitive's material uses it for a texture.");
                        }
                    }
                }
            }

            // check if tex coords are deleted in correct order
            for (auto& [attrName, accessorIdx] : attributes.items()) {
                if (attrName.rfind("TEXCOORD_", 0) != std::string::npos) {
                    std::string primAttrNumberStr = attrName.substr(9); // get everything in name after "TEXCOORD_"
                    if ((int)texCoordIdx < std::stoi(primAttrNumberStr)) {
                        throw std::runtime_error("Cannot delete texture attribute. Texture attributes have to be deleted in descending order.");
                    }
                }
            }
        }

        if (0 == attributes.erase(path.back())) {
            throw std::runtime_error("Cannot delete unknown attribute");
        }

        // remove attribute from morph targets
        if (primitive.contains("targets")) {
            for (auto& [key, morphTarget] : primitive["targets"].items()) {
                if (morphTarget.contains(attributeName)) {
                    morphTarget.erase(attributeName);
                }
            }
        }

        return state;
    };

    return operation;
}

Operation gltf::removeProperties()
{
    Operation operation;
    operation.name = "remove_properties_by_name";

    operation.schema
        = schema::operation(
            "Remove Properties by Name",
            "Removes all properties with the given name from the entire gltf.",
            schema::object({ { "property", schema::string("/extras*|/name|/animations") } }));

    // define the operation function
    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        if (params["property"].get<std::string>() == "/animations") {
            // erase animations
            {
                auto params = nlohmann::json::object();
                params["pathPattern"] = "/gltf/animations/\\d+";
                params["operation"] = "remove";
                params["parameters"] = nlohmann::json::object();
                auto taskDescription = nlohmann::json({ { "dangerous.forEach", params } });
                state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
            }
            state["gltf"].erase("animations");
            return state;
        }

        size_t count = removePropertiesFromGltf(state["gltf"], params);
        if (count == 0) {
            throw std::runtime_error(fmt::format("No property '{}' found", params["property"].get<std::string>().erase(0, 1)));
        } else {
            callback({ { "message", fmt::format("Removed {} {} {}", count, params.at("property").get<std::string>().erase(0, 1), count > 1 ? "properties" : "property") } });
        }

        return state;
    };

    return operation;
}

Operation gltf::removeKnownExtensions(const nlohmann::json& knownExtensionsArray)
{
    Operation operation;
    operation.name = "remove_extension";

    nlohmann::json allowedPopertiesSchema = schema::enumeration(knownExtensionsArray);

    auto parameters = nlohmann::json::array();
    parameters.push_back(schema::object({ { "property", allowedPopertiesSchema } }));
    parameters.push_back(schema::object({ { "properties", schema::enumerationSet(allowedPopertiesSchema) } }));

    // fill the operation schema header
    operation.schema
        = schema::operation(
            "Remove Extension",
            "Removes all occurences of the extension with the given name from the entire gltf.",
            schema::oneOf(parameters));

    // define the operation function
    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        nlohmann::json& gltf = state["gltf"];

        removePropertiesFromGltf(gltf, params, true);
        if (params.contains("property")) {
            callback({ { "message", fmt::format("Removed all occurances of {}", params.at("property").get<std::string>()) } });
        }
        if (params.contains("properties")) {
            callback({ { "message", fmt::format("Removed all occurances of {}", params.at("properties").dump()) } });
        }
        return state;
    };

    return operation;
}

Operation gltf::removeUnknownExtensions(const nlohmann::json& knownExtensionsArray)
{
    Operation operation;
    operation.name = "remove_unknown_extension";

    // fill the operation schema header
    operation.schema
        = schema::operation(
            "Remove All Unknown Extensions",
            "Removes all occurences of all extensions that are not ratified and not listed in the glTF specification.",
            schema::object({}));

    // define the operation function
    operation.operation = [knownExtensionsArray](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        nlohmann::json& gltf = state["gltf"];

        if (gltf.contains("extensionsUsed")) {
            std::vector<std::string> extensionsToRemove;
            const nlohmann::json& usedExtensions = gltf["extensionsUsed"];
            for (auto& [key, value] : usedExtensions.items()) {
                auto it = std::find(knownExtensionsArray.begin(), knownExtensionsArray.end(), value);
                if (it == knownExtensionsArray.end()) {
                    extensionsToRemove.push_back("/" + value.get<std::string>());
                }
            }
            if (extensionsToRemove.size() > 0) {
                removePropertiesHelper(gltf, extensionsToRemove, true);
            }
            // removePropertiesHelper does not remove the extension from extensionsUsed since the extensionToRemove string
            // starts witha /  but the actual value in the extensionsUsed array does not.
            // Therefore we have to remove it manually
            for (const auto& extensionToRemove : extensionsToRemove) {
                utility::removeExtensionFlag(state, extensionToRemove.substr(1));
            }
        }

        callback({ { "message", "Removed all unknown extensions" } });

        return state;
    };

    return operation;
}

size_t gltf::removePropertiesFromGltf(nlohmann::json& gltf, const nlohmann::json& params, bool removePropertyNameFromArrays)
{
    std::vector<std::string> properties;
    if (params.contains("property")) {
        std::string property = params["property"].get<std::string>();
        if (!property.rfind("/", 0) == 0) {
            property = "/" + property;
        }
        properties.push_back(property);
    }
    if (params.contains("properties")) {
        for (auto& [_, value] : params["properties"].items()) {
            std::string property = value.get<std::string>();
            if (!property.rfind("/", 0) == 0) {
                property = "/" + property;
            }
            properties.push_back(property);
        }
    }

    return removePropertiesHelper(gltf, properties, removePropertyNameFromArrays);
}

size_t gltf::removePropertiesHelper(nlohmann::json& gltf, const std::vector<std::string>& properties, bool removePropertyNameFromArrays)
{
    size_t counter = 0;
    std::vector<nlohmann::json*> worklist;
    worklist.push_back(&gltf);

    while (!worklist.empty()) {
        nlohmann::json& elem = *worklist.back();
        worklist.pop_back();

        // remove properties
        for (auto& property : properties) {
            if (elem.is_object() && elem.contains(nlohmann::json::json_pointer(property))) {
                reactor::json::erase(elem, nlohmann::json::json_pointer(property));
                ++counter;
            }
            if (elem.is_array() && removePropertyNameFromArrays) {
                auto it = std::find(elem.begin(), elem.end(), property);
                if (it != elem.end()) {
                    elem.erase(it);
                    ++counter;
                }
            }
        }

        // add child elements
        if (elem.is_array() || elem.is_object()) {
            for (auto& [key, value] : elem.items()) {
                worklist.push_back(&value);
            }
        }
    }

    return counter;
}

Operation gltf::convertPrimitivesToMeshes()
{
    Operation operation;
    operation.name = "split_up_primitives";

    operation.schema = schema::operation(
        "Split up Primitives",
        "Splits all primitives in the provided mesh into individual meshes.",
        schema::object({ schema::parameter::path("/gltf/meshes/\\d+") }));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params.at("path"));
        if (state.contains(path) == false) {
            throw std::invalid_argument("Mesh does not exist");
        }
        nlohmann::json& origMesh = state[path];
        nlohmann::json& gltfMeshesArray = state["gltf"]["meshes"];
        size_t originalMeshCount = gltfMeshesArray.size();

        std::string origMeshName = "";
        if (origMesh.contains("name")) {
            origMeshName = origMesh["name"];
        }

        // create new Meshes
        nlohmann::json& origPrimitives = origMesh["primitives"];
        if (origPrimitives.size() == 1) {
            throw std::invalid_argument("Mesh only contains one primitive. No new meshes were created.");
        }
        uint32_t counter = 0;
        for (auto [key, primitive] : origPrimitives.items()) {
            nlohmann::json newMesh;
            newMesh["name"] = origMeshName + "." + std::to_string(counter);
            newMesh["primitives"] = nlohmann::json::array();
            newMesh["primitives"].push_back(primitive);

            gltfMeshesArray.push_back(newMesh);
            counter++;
        }

        if (counter == 0) {
            throw std::runtime_error("No primitives were split up");
        }

        if (state["gltf"].contains("nodes") == false) {
            callback({ { "message", fmt::format("Converted {} primitives to {} meshes", counter, counter) } });
            return state;
        }

        nlohmann::json& nodes = state["gltf"]["nodes"];
        uint32_t origMeshIdx = std::stoi(path.back());
        uint32_t newNodeCounter = 0;
        // find all nodes using the original mesh and replace the mesh with the new ones
        uint32_t originalNodeCount = nodes.size();
        for (size_t nodeIdx = 0; nodeIdx < originalNodeCount; ++nodeIdx) {
            nlohmann::json* node = &nodes.at(nodeIdx);

            // if node has no mesh or does not contain original mesh skip it
            if (node->contains("mesh") == false || node->contains("mesh") && (*node)["mesh"] != origMeshIdx) {
                continue;
            }

            std::string nodeName = "";
            if (node->contains("name")) {
                nodeName = (*node)["name"];
            }

            // add new nodes and add new meshes to nodes
            for (size_t i = 0; i < counter; ++i) {
                nlohmann::json newNode;
                newNodeCounter++;

                // copy values
                newNode["name"] = nodeName + "." + std::to_string(i);
                newNode["mesh"] = originalMeshCount + i;
                size_t newNodeIdx = nodes.size();
                if (node->contains("skin")) {
                    newNode["skin"] = (*node)["skin"];
                }
                if (node->contains("weights")) {
                    newNode["weights"] = (*node)["weights"];
                }

                if (node->contains("children") == false) {
                    (*node)["children"] = nlohmann::json::array();
                }
                // add new node to original node
                (*node)["children"].push_back(newNodeIdx);
                nodes.push_back(newNode);
                node = &nodes.at(nodeIdx);

                // continue if there are no animations to fix
                if (state["gltf"].contains("animations") == false) {
                    continue;
                }
                // fix animations
                for (auto& [key, animation] : state["gltf"]["animations"].items()) {
                    nlohmann::json newChannels = nlohmann::json::array();
                    nlohmann::json& origChannels = animation["channels"];
                    for (auto& [key, channel] : origChannels.items()) {
                        nlohmann::json& target = channel["target"];
                        if (target["path"] == "weights" && target.contains("node") && target["node"] == nodeIdx) {
                            nlohmann::json newChannel;
                            newChannel["sampler"] = channel["sampler"];
                            newChannel["path"] = "weights";
                            newChannel["node"] = newNodeIdx;
                        }
                    }
                    origChannels.insert(origChannels.end(), newChannels.begin(), newChannels.end());
                }
            }

            node->erase("mesh");
            node->erase("skin");
            node->erase("weights");
        }
        callback({ { "message", fmt::format("Converted {} primitives to meshes in {} {}", counter, newNodeCounter, counter == 1 ? "node" : "nodes") } });
        return state;
    };
    return operation;
}
Operation gltf::joinMeshes(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "join_meshes";

    // define an array of pointers towards meshes, with minimum 2 unique pointers
    nlohmann::json meshPointerArray = R"(
    {
        "type": "array",
        "items": {
            "type": "string",
            "format": "json-pointer",
            "pattern": "^/gltf/meshes/\\d+$"
        },
        "uniqueItems": true,
        "minItems": 2
    }
    )"_json;

    operation.schema = schema::operation(
        "Join Meshes",
        "Join multiple Meshes into a new Mesh",
        schema::object({ { "meshPaths", meshPointerArray } }));

    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        if (!state.at(nlohmann::json::json_pointer("/gltf")).contains("meshes")) {
            throw std::invalid_argument("This glTF does not contain any meshes.");
        }

        nlohmann::json newMesh = nlohmann::json::object();
        std::string newName = "Joined Mesh (";

        // handle all to-be-joined meshes
        for (auto& meshPointer : params["meshPaths"]) {
            nlohmann::json& mesh = state.at(nlohmann::json::json_pointer(meshPointer));

            // use the mesh's name as part of the new mesh's name
            if (mesh.find("name") != mesh.end()) {
                newName += "\"" + mesh["name"].get<std::string>() + "\", ";
            } else {
                newName += "\"Unnamed Mesh\", ";
            }

            // copy all primitives of all to-be-joined meshes into the new mesh
            for (auto& primitive : mesh["primitives"]) {
                newMesh["primitives"].emplace_back(primitive);
            }
        }

        // remove superfluous ", " and finalize name
        newName = newName.substr(0, newName.size() - 2);
        newName += ")";
        newMesh["name"] = newName;

        // make sure that all primitives have the same number of morph targets
        int morphTargetCount = -1;
        for (auto& primitive : newMesh["primitives"]) {
            if (morphTargetCount == -1) {
                morphTargetCount = primitive["targets"].size();
            } else if (morphTargetCount != primitive["targets"].size()) {
                throw std::runtime_error("Primitives have different numbers of morph targets");
            }
        }

        // add new mesh to the glTF
        state.at(nlohmann::json::json_pointer("/gltf/meshes")).push_back(newMesh);

        return state;
    };

    return operation;
}

Operation gltf::transformNode()
{
    Operation operation;
    operation.name = "transform_node";
    operation.schema = schema::operation(
        "Transform Node",
        "Apply translation, rotation, and scale to current node",

        schema::object(
            { schema::parameter::path("/gltf/nodes/\\d+") },
            { { "translation", schema::array(schema::number(), 3, 3) },
                { "rotation", schema::array(schema::number(), 3, 3) },
                { "scale", schema::array(schema::number(), 3, 3) } }));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params["path"].get<std::string>());

        if (!state.contains(path)) {
            throw std::invalid_argument(std::string("Node not found"));
        }

        if (state.contains(path / "matrix")) {
            auto taskDescription = nlohmann::json({ { "change_transformation_format", nlohmann::json::object() } });
            taskDescription["change_transformation_format"]["path"] = params["path"];
            taskDescription["change_transformation_format"]["storageType"] = "TRS";
            state = library.call(std::move(taskDescription), std::move(state), dataStorage, callback);
        }

        auto& node = state.at(nlohmann::json::json_pointer(path));

        if (params.contains("translation")) {
            glm::dvec3 paramTranslation = params.value("translation", glm::dvec3(0.0, 0.0, 0.0));
            glm::dvec3 nodeTranslation = node.value("translation", glm::dvec3(0.0, 0.0, 0.0));
            node["translation"] = paramTranslation + nodeTranslation;
        }

        if (params.contains("rotation")) {
            glm::dvec3 paramRotation = params.value("rotation", glm::dvec3(0.0, 0.0, 0.0));
            glm::dquat nodeRotationQuat = node.value("rotation", glm::dquat(1.0, 0.0, 0.0, 0.0));
            glm::dvec3 nodeRotationEuler = utility::quaternionToEuler(nodeRotationQuat);
            glm::dquat finalRotation = utility::eulerToQuaternion(nodeRotationEuler + paramRotation);
            finalRotation = glm::normalize(finalRotation);
            node["rotation"] = finalRotation;
        }

        if (params.contains("scale")) {
            glm::dvec3 paramScale = params.value("scale", glm::dvec3(1.0));
            glm::dvec3 nodeScale = node.value("scale", glm::dvec3(1.0, 1.0, 1.0));
            node["scale"] = paramScale * nodeScale;
        }

        return state;
    };

    return operation;
}

Operation gltf::createEmpty()
{
    Operation operation;
    operation.name = "create_empty_gltf";
    operation.schema = schema::operation(
        "Create Empty glTF",
        "Create minimal and valid glTF",
        schema::object({}));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        state["gltf"] = nlohmann::json({ { "asset", { { "version", "2.0" } } } });
        state["files"] = nlohmann::json::object();
        state["embedded"] = nlohmann::json::object();
        state["glb"] = nlohmann::json::object();
        return state;
    };

    return operation;
}

Operation gltf::createLight()
{
    Operation operation;
    operation.name = "create_light";
    operation.schema = schema::operation(
        "Create Light",
        "Create light and add it to a node provided by a path",
        schema::object({}, {
                               schema::parameter::path("/gltf/nodes/\\d+"),
                           }));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        if (params.contains("path")) {
            if (!state.contains(nlohmann::json::json_pointer(params["path"]))) {
                throw std::invalid_argument("Target node does not exist");
            }
            if (state.contains(nlohmann::json::json_pointer(params["path"]) / "extensions" / "KHR_lights_punctual" / "light")) {
                throw std::invalid_argument("Node already has a light attached");
            }
        }

        auto taskDescription = nlohmann::json({ { "create_gltf_component", nlohmann::json::object() } });
        taskDescription["create_gltf_component"]["path"] = "/gltf/extensions/KHR_lights_punctual/lights";
        taskDescription["create_gltf_component"]["type"] = "point";
        state = library.call(taskDescription, std::move(state), dataStorage);

        uint32_t light_ID = state.at(nlohmann::json::json_pointer("/gltf/extensions/KHR_lights_punctual/lights")).size() - 1;

        reactor::utils::createSubElement(state, params["path"].get<std::string>() + "/extensions/KHR_lights_punctual");

        state.at(nlohmann::json::json_pointer(params["path"]) / "extensions" / "KHR_lights_punctual")["light"] = light_ID;

        return state;
    };

    return operation;
}

Operation gltf::reassignSkinnedAnimation()
{
    Operation operation;
    operation.name = "reassign_skinned_animation";

    operation.schema = schema::operation(
        "Reassign a skinned Animation",
        "Reassign an Animation from one skinned set of joints to another hierarchically identical set.",
        schema::object({ schema::parameter::path("/gltf/animations/\\d+"), schema::parameter::path("/gltf/skins/\\d+", "sourceSkin"), schema::parameter::path("/gltf/skins/\\d+", "targetSkin") }));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        const auto animation = nlohmann::json::json_pointer(params["path"]);
        const auto sourceSkin = nlohmann::json::json_pointer(params["sourceSkin"]);
        const auto targetSkin = nlohmann::json::json_pointer(params["targetSkin"]);

        if (!state["gltf"].contains("animations")) {
            throw std::invalid_argument("No animations found in this glTF");
        }
        if (!state["gltf"].contains("skins")) {
            throw std::invalid_argument("No skins found in this glTF");
        }
        if (std::stoi(animation.back()) >= state["gltf"]["animations"].size()) {
            throw std::invalid_argument("animation index is pointing out of bounds of the animations array");
        }
        if (std::stoi(sourceSkin.back()) >= state["gltf"]["skins"].size()) {
            throw std::invalid_argument("sourceSkin index is pointing out of bounds of the skins array");
        }
        if (std::stoi(targetSkin.back()) >= state["gltf"]["skins"].size()) {
            throw std::invalid_argument("targetSkin index is pointing out of bounds of the skins array");
        }

        const size_t animationChannelCount = state.at(animation)["channels"].size();
        const size_t sourceSkinJointCount = state.at(sourceSkin)["joints"].size();
        const size_t targetSkinJointCount = state.at(targetSkin)["joints"].size();
        if (sourceSkin == targetSkin) {
            throw std::invalid_argument("sourceSkin and targetSkin paths point to the same skin");
        }
        if (sourceSkinJointCount != targetSkinJointCount) {
            throw std::invalid_argument("sourceSkin and targetSkin have different numbers of joints");
        }

        // create a mapping of the indices of corresponding joints from both skins
        std::map<int, int> skinIndexMap;
        for (int i = 0; i < sourceSkinJointCount; ++i) {
            skinIndexMap.emplace(state.at(sourceSkin)["joints"][i], state.at(targetSkin)["joints"][i]);
        }

        // assign new joints as targets to existing animation
        for (int i = 0; i < animationChannelCount; ++i) {
            const int oldIndex = state.at(animation)["channels"][i]["target"]["node"];
            state.at(animation)["channels"][i]["target"]["node"] = skinIndexMap.at(oldIndex);
        }

        return state;
    };

    return operation;
}

Operation gltf::deleteBufferView(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "delete_bufferview";
    operation.schema = schema::operation(
        "Delete BufferView",
        "Deletes one or more bufferViews and the referenced buffer if no longer used",
        schema::oneOf(nlohmann::json::array({
            schema::object({ schema::parameter::path("/gltf/bufferViews/\\d+") }, { { "skipIfReferenced", schema::boolean(true) } }),
            schema::object({ { "path", schema::array(schema::pointer("/gltf/bufferViews/\\d+")) } }, { { "skipIfReferenced", schema::boolean(true) } }),
        })));

    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto cmp = [](nlohmann::json::json_pointer a, nlohmann::json::json_pointer b) -> bool { return std::stoi(a.back()) > std::stoi(b.back()); };
        std::set<nlohmann::json::json_pointer, decltype(cmp)> uniqueSortedPaths(cmp);
        std::set<nlohmann::json::json_pointer, decltype(cmp)> uniqueSortedBufferPaths(cmp);

        if (params.at("path").is_array()) {
            for (auto path : params.at("path")) {
                uniqueSortedPaths.insert(nlohmann::json::json_pointer(path));
            }
        } else {
            auto path = nlohmann::json::json_pointer(params.at("path"));
            uniqueSortedPaths.insert(path);
        }

        std::vector<nlohmann::json::json_pointer> bufferViewsToDelete;
        bufferViewsToDelete.reserve(uniqueSortedPaths.size());

        for (auto path : uniqueSortedPaths) {
            if (!state.contains(path)) {
                throw std::invalid_argument("BufferView not found");
            }

            std::vector<nlohmann::json::json_pointer> bufferViewReferences = reactor::getBackwardReferenceProperties(
                state["gltf"],
                nlohmann::json::json_pointer(path.to_string().substr(5)), // remove /gltf
                linearizedGltfSchema);

            if (!bufferViewReferences.empty() && params["skipIfReferenced"].get<bool>()) {
                continue;
            }

            // If the bufferView is not used as well, we can go on with cleaning
            if (!state.at(path).contains("buffer")) {
                throw std::invalid_argument("BufferView has no buffer");
            }
            auto bufferPath = nlohmann::json::json_pointer {} / "gltf" / "buffers" / state.at(path / "buffer").get<uint32_t>();
            uniqueSortedBufferPaths.insert(bufferPath);

            // Remove buffer view
            bufferViewsToDelete.push_back(path);
        }

        if (bufferViewsToDelete.empty()) {
            return state;
        }

        // -------------------------------
        // ----- Remove buffer views -----
        // -------------------------------
        // we do not use the regular remove operation here.
        // state = library.call({ { "remove", { { "path", bufferViewsToDelete } } } }, std::move(state), datastorage, callback);
        // the regular operation is quite slow resulting in veeery long wait times (> 15 minutes) for all meshes
        // once a gltf has a few hundred buffer views/ accessors

        auto analyzeFun = [](auto path, std::vector<uint32_t>& indexShift) -> void {
            int removalIndex = std::stoi(path.back());
            indexShift[removalIndex]++;
        };

        auto removeRefencesFun = [&state](auto path) -> void {
            int removalIndex = std::stoi(path.back());
            auto& gltf = state["gltf"];
            // iterate all accessors and fix references
            if (gltf.contains("accessors")) {
                for (auto& [key, accessor] : gltf["accessors"].items()) {
                    if (accessor.contains("bufferView") && accessor["bufferView"] == removalIndex) {
                        accessor.erase("bufferView");
                    }
                    if (accessor.contains("sparse")) {
                        if (accessor["sparse"].contains("indices") && accessor["sparse"]["indices"].contains("bufferView") && accessor["sparse"]["indices"]["bufferView"] == removalIndex) {
                            accessor["sparse"]["indices"].erase("bufferView");
                        }
                        if (accessor["sparse"].contains("values") && accessor["sparse"]["values"].contains("bufferView") && accessor["sparse"]["values"]["bufferView"] == removalIndex) {
                            accessor["sparse"]["indices"].erase("bufferView");
                        }
                    }
                }
            }
            // iterate all images and fix references
            if (gltf.contains("images")) {
                for (auto& [key, image] : gltf["images"].items()) {
                    if (image.contains("bufferView") && image["bufferView"] == removalIndex) {
                        image["bufferView"].erase("bufferView");
                    }
                }
            }

            // iterate all meshes -> primitives and fix references
            if (gltf.contains("meshes")) {
                for (auto& [key, mesh] : gltf["meshes"].items()) {
                    if (mesh.contains("primitives")) {
                        for (auto& primitive : mesh["primitives"]) {
                            if (primitive.contains("extensions") && primitive["extensions"].contains("KHR_draco_mesh_compression") && primitive["extensions"]["KHR_draco_mesh_compression"].contains("bufferView") && primitive["extensions"]["KHR_draco_mesh_compression"]["bufferView"] == removalIndex) {
                                primitive["extensions"]["KHR_draco_mesh_compression"].erase("bufferView");

                                if (primitive["extensions"]["KHR_draco_mesh_compression"].empty()) {
                                    primitive["extensions"].erase("KHR_draco_mesh_compression");
                                }
                            }
                        }
                    }
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

        auto fixReferencesFun = [&state](auto path, auto indexShiftFun) -> void {
            auto arraypath = path.parent_pointer();
            if (!state.contains(arraypath)) {
                return;
            }
            for (int i = 0; i < state.at(arraypath).size(); i++) {
                if (indexShiftFun(i) == 0) {
                    continue;
                }
                auto& gltf = state["gltf"];
                // iterate all accessors and fix references
                if (gltf.contains("accessors")) {
                    for (auto& [key, accessor] : gltf["accessors"].items()) {
                        if (accessor.contains("bufferView") && accessor["bufferView"] == i) {
                            accessor["bufferView"] = i - indexShiftFun(i);
                        }
                        if (accessor.contains("sparse")) {
                            if (accessor["sparse"].contains("indices") && accessor["sparse"]["indices"].contains("bufferView") && accessor["sparse"]["indices"]["bufferView"] == i) {
                                accessor["sparse"]["indices"]["bufferView"] = i - indexShiftFun(i);
                            }
                            if (accessor["sparse"].contains("values") && accessor["sparse"]["values"].contains("bufferView") && accessor["sparse"]["values"]["bufferView"] == i) {
                                accessor["sparse"]["indices"]["bufferView"] = i - indexShiftFun(i);
                            }
                        }
                    }
                }
                // iterate all images and fix references
                if (gltf.contains("images")) {
                    for (auto& [key, image] : gltf["images"].items()) {
                        if (image.contains("bufferView") && image["bufferView"] == i) {
                            image["bufferView"] = i - indexShiftFun(i);
                        }
                    }
                }

                // iterate all meshes -> primitives and fix references
                if (gltf.contains("meshes")) {
                    for (auto& [key, mesh] : gltf["meshes"].items()) {
                        if (mesh.contains("primitives")) {
                            for (auto& primitive : mesh["primitives"]) {
                                if (primitive.contains("extensions") && primitive["extensions"].contains("KHR_draco_mesh_compression") && primitive["extensions"]["KHR_draco_mesh_compression"].contains("bufferView") && primitive["extensions"]["KHR_draco_mesh_compression"]["bufferView"] == i) {
                                    primitive["extensions"]["KHR_draco_mesh_compression"]["bufferView"] = i - indexShiftFun(i);
                                }
                            }
                        }
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

        std::vector<nlohmann::json::json_pointer> pathVector;
        for (auto path : bufferViewsToDelete) {
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

        if (!extensions.empty()) {
            auto taskDescription = nlohmann::json({ { "update_extension_flags", nlohmann::json::object() } });
            taskDescription["update_extension_flags"]["extensions"] = extensions;
            state = library.call(taskDescription, std::move(state), datastorage, callback);
        }

        for (auto path : uniqueSortedBufferPaths) {
            // Remove unreferenced data from the buffer
            state = library.call({ { "trim_buffer", { { "path", path.to_string() } } } }, std::move(state), datastorage, callback);
        }

        return state;
    };
    return operation;
}

Operation gltf::deleteAccessor(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "delete_accessor";

    auto parameters = nlohmann::json::array();
    parameters.push_back(schema::object({ schema::parameter::path("/gltf/accessors/\\d+") }, { { "skipIfReferenced", schema::boolean(true) } }));
    parameters.push_back(schema::object({ { "path", schema::array(schema::pointer("/gltf/accessors/\\d+")) } }, { { "skipIfReferenced", schema::boolean(true) } }));

    // fill the operation schema header
    operation.schema = schema::operation(
        "Delete Accessor",
        "Deletes one or more accessors and the referenced bufferViews and data of the buffer if no longer used",
        schema::oneOf(parameters));

    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto deleteAccessor = [&](nlohmann::json::json_pointer path) -> void {
            if (!state.contains(path)) {
                throw std::invalid_argument("Accessor not found");
            }
            nlohmann::json::json_pointer bufferViewPath = nlohmann::json::json_pointer();
            if (state.at(path).contains("bufferView")) {
                bufferViewPath = nlohmann::json::json_pointer("/gltf/bufferViews") / state.at(path / "bufferView").get<uint32_t>();
            }

            { // Delete accessor

                // remove /gltf from path
                std::string tmp = path.to_string();
                if (tmp.starts_with("/gltf")) {
                    tmp = tmp.substr(5);
                }
                std::vector<nlohmann::json::json_pointer> accessorReferences = reactor::getBackwardReferenceProperties(state.at("gltf"), nlohmann::json::json_pointer(tmp), linearizedGltfSchema);
                if (accessorReferences.size() != 0 && params["skipIfReferenced"].get<bool>() == true) {
                    return;
                }
                std::string op = "remove";
                auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
                taskDescription[op]["path"] = path.to_string();
                state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
            }

            if (!bufferViewPath.empty()) { // Delete bufferView
                std::string op = "delete_bufferview";
                auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
                taskDescription[op]["path"] = bufferViewPath.to_string();
                state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
            }
        };

        if (params.at("path").is_array()) {
            auto cmp = [](nlohmann::json::json_pointer a, nlohmann::json::json_pointer b) -> bool { return std::stoi(a.back()) > std::stoi(b.back()); };
            std::set<nlohmann::json::json_pointer, decltype(cmp)> uniqueSortedPaths(cmp);

            for (auto path : params.at("path")) {
                uniqueSortedPaths.insert(nlohmann::json::json_pointer(path));
            }

            for (auto path : uniqueSortedPaths) {
                deleteAccessor(path);
            }

        } else {
            auto path = nlohmann::json::json_pointer(params.at("path"));
            deleteAccessor(path);
        }

        return state;
    };
    return operation;
}

Operation gltf::updateExtensionFlags(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "update_extension_flags";
    operation.schema = schema::operation(
        "Update Extension Flags",
        "Set extensions used and extensions required flags with respect to extension occurrence in the glTF",
        schema::object({}, { { "extensions", schema::array(schema::string()) } }));

    operation.operation = [gltfSchema, linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        // define helper function
        auto has_object = [](const nlohmann::json& json, std::string objectName) -> bool {
            std::vector<const nlohmann::json*> workList;
            workList.push_back(&json);

            while (!workList.empty()) {
                const nlohmann::json& elem = *workList.back();
                workList.pop_back();

                if (elem.contains(objectName)) {
                    return true;
                }

                // add child elements
                if (elem.is_array() || elem.is_object()) {
                    for (auto& [key, value] : elem.items()) {
                        workList.push_back(&value);
                    }
                }
            }
            return false;
        };

        std::vector<std::string> knownExtensions {
            "KHR_draco_mesh_compression",
            "KHR_lights_punctual",
            "KHR_materials_clearcoat",
            "KHR_materials_emissive_strength",
            "KHR_materials_ior",
            "KHR_materials_iridescence",
            "KHR_materials_sheen",
            "KHR_materials_specular",
            "KHR_materials_transmission",
            "KHR_materials_unlit",
            "KHR_materials_variants",
            "KHR_materials_volume",
            //"KHR_mesh_quantization", // TODO we have to handle this extension separately
            "KHR_texture_basisu",
            "KHR_texture_transform",
            "KHR_xmp_json_ld"
        };

        std::vector<std::string> unkownExtensions;

        auto extensionsExists = [has_object](nlohmann::json& state, std::vector<nlohmann::json::json_pointer> paths, std::string extension) -> bool {
            for (auto& path : paths) {
                if (state.contains(path) && has_object(state.at(path), extension)) {
                    return true;
                }
            }
            return false;
        };

        auto update_flag = [extensionsExists](nlohmann::json& state, std::vector<nlohmann::json::json_pointer> paths, std::string extension, bool setRequired = false) -> void {
            if (extensionsExists(state, paths, extension)) {
                operations::utility::useExtension(state, extension, setRequired);
            } else {
                operations::utility::removeExtensionFlag(state, extension);
            }
        };

        if (params.contains("extensions")) {
            std::vector<std::string> selectedExtensions;
            for (std::string extension : params.at("extensions")) {
                if (std::find(knownExtensions.begin(), knownExtensions.end(), extension) == knownExtensions.end()) {
                    unkownExtensions.push_back(extension);
                } else {
                    selectedExtensions.push_back(extension);
                }
            }
            knownExtensions = selectedExtensions;
        }

        for (auto extension : knownExtensions) {
            if (extension == "KHR_materials_variants") {
                update_flag(state, { "/gltf/meshes"_json_pointer, "/gltf/extensions"_json_pointer }, extension);
            } else if (extension.find("materials") != std::string::npos) {
                update_flag(state, { nlohmann::json::json_pointer("/gltf/materials") }, extension);
            } else if (extension.find("KHR_draco_mesh_compression") != std::string::npos) {
                update_flag(state, { nlohmann::json::json_pointer("/gltf/meshes") }, extension, true);
            } else if (extension.find("KHR_texture_basisu") != std::string::npos) {
                bool foundExtension = extensionsExists(state, { nlohmann::json::json_pointer("/gltf/textures") }, extension);
                if (!foundExtension) {
                    if (state.contains("/gltf/images"_json_pointer)) {
                        for (auto& image : state.at("/gltf/images"_json_pointer)) {
                            if (image.value("mimeType", "") == "image/ktx2") {
                                foundExtension = true;
                                break;
                            }
                        }
                    }
                }
                if (foundExtension) {
                    operations::utility::useExtension(state, extension, true);
                } else {
                    operations::utility::removeExtensionFlag(state, extension);
                }
            } else if (extension.find("KHR_texture_transform") != std::string::npos) {
                update_flag(state, { nlohmann::json::json_pointer("/gltf/materials") }, extension);
            } else if (extension == "KHR_lights_punctual") {
                update_flag(state, { "/gltf/nodes"_json_pointer, "/gltf/extensions"_json_pointer }, extension);
            } else {
                update_flag(state, { nlohmann::json::json_pointer("/gltf") }, extension);
            }
        }

        for (auto extension : unkownExtensions) {
            update_flag(state, { nlohmann::json::json_pointer("/gltf") }, extension);
        }

        return state;
    };
    return operation;
}

const Operation gltf::resolveSparseAccessor {
    .name = "resolve_sparse_accessor",

    .schema = schema::operation(
        "Resolve sparse accessor",
        "Convert a sparse accessor to a regular one by eliminating the indirection through indices",
        schema::object({
            schema::parameter::path("/gltf/accessors/\\d+"),
        })),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        nlohmann::json::json_pointer path { params["path"] };

        auto& accessor = state[path];
        if (!accessor.contains("sparse")) {
            throw std::runtime_error { "Accessor has no sparse property" };
        }
        auto usedBufferViews = nlohmann::json::array();

        int sparseCount = accessor["sparse"]["count"];
        reactor::gltf::ComponentType indexType = accessor["sparse"]["indices"]["componentType"];

        size_t indexBytesOffset = accessor["sparse"]["indices"].value("byteOffset", 0);
        std::string indexBV = "/gltf/bufferViews/" + std::to_string(accessor["sparse"]["indices"]["bufferView"].get<int>());
        usedBufferViews.push_back(indexBV);
        std::vector<uint8_t> indexBytes = reactor::utils::getBufferViewData(nlohmann::json::json_pointer(indexBV), state, dataStorage);

        size_t valueBytesOffset = accessor["sparse"]["values"].value("byteOffset", 0);
        std::string valueBV = "/gltf/bufferViews/" + std::to_string(accessor["sparse"]["values"]["bufferView"].get<int>());
        usedBufferViews.push_back(valueBV);
        std::vector<uint8_t> valueBytes = reactor::utils::getBufferViewData(nlohmann::json::json_pointer(valueBV), state, dataStorage);

        std::vector<uint8_t> accessorData = reactor::utils::getAccessorByteDataPacked(path, state, dataStorage);
        size_t accessorElementSize = ux3d::reactor::gltf::elementSize(accessor);

        for (size_t i = 0; i < sparseCount; ++i) {
            size_t index = 0;
            if (indexType == reactor::gltf::ComponentType::UnsignedByte) {
                index = reinterpret_cast<uint8_t*>(indexBytes.data() + indexBytesOffset)[i];
            } else if (indexType == reactor::gltf::ComponentType::UnsignedShort) {
                index = reinterpret_cast<uint16_t*>(indexBytes.data() + indexBytesOffset)[i];
            } else {
                index = reinterpret_cast<uint32_t*>(indexBytes.data() + indexBytesOffset)[i];
            }

            // replace accessor value per byte
            memcpy(&accessorData[accessorElementSize * index], &valueBytes[accessorElementSize * i + valueBytesOffset], accessorElementSize);
        }

        if (accessor.contains("bufferView")) {
            usedBufferViews.push_back("/gltf/bufferViews/" + std::to_string(accessor["bufferView"].get<uint32_t>()));
            auto bufferPath = nlohmann::json::json_pointer("/gltf/buffers") / accessor["bufferView"].get<uint32_t>();
            accessor["bufferView"] = std::stoi(reactor::utils::appendBufferView(bufferPath, accessorData, state, dataStorage).back());
        } else {
            auto buffer = reactor::utils::createBuffer(state, dataStorage, accessorData);
            auto view = reactor::utils::createBufferView(state, buffer);
            state[path]["bufferView"] = std::stoi(view.back());
        }
        state[path].erase("sparse");

        {
            auto taskDescription = nlohmann::json({ { "delete_bufferview", nlohmann::json::object() } });
            taskDescription["delete_bufferview"]["path"] = usedBufferViews;
            state = library.call(std::move(taskDescription), std::move(state), dataStorage, callback);
        }

        return state;
    }
};

const Operation gltf::changeCameraType = {
    .name = "change_camera_type",
    .schema = schema::operation(
        "Changes the type of the camera",
        "Changes the type of the camera. If no type is specified the type is toggled or set to perspective if no type is already defined.",
        schema::object({ schema::parameter::path("/gltf/cameras/\\d+") }, { { "type", schema::string("perspective|orthographic") } })),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        std::string type = "perspective";
        const auto cameraPath = nlohmann::json::json_pointer(params["path"]);
        nlohmann::json& camera = state.at(cameraPath);
        if (camera.contains("type")) {
            // Check if new camera type needs to be switched from perspective to orthographic
            if (camera.at("type") == "perspective") {
                type = "orthographic";
            }
            // If params type is not the same as the only applicable type, throw.
            if (params.contains("type") && params.at("type") != type) {
                throw std::runtime_error("Type is already set.");
            }
        } else if (params.contains("type")) {
            // If type is not set, set it to the type of params or perspective if not provided
            type = params.at("type");
        }

        camera.at("type") = type;
        if (type == "perspective") {
            nlohmann::json perspective = {};
            double znear = 0.1;
            double yFoV = 1.0;
            if (camera.contains("orthographic")) {
                const nlohmann::json& orthographic = camera.at("orthographic");
                if (orthographic.contains("zfar")) {
                    perspective["zfar"] = orthographic.at("zfar");
                }
                if (orthographic.contains("znear")) {
                    znear = orthographic.at("znear");
                }
                double xmag = 1.0;
                double ymag = 1.0;
                bool calculateAspect = false;
                if (orthographic.contains("xmag")) {
                    xmag = orthographic.at("xmag");
                    calculateAspect = true;
                }
                if (orthographic.contains("ymag")) {
                    ymag = orthographic.at("ymag");
                    calculateAspect = true;
                }
                if (calculateAspect) {
                    perspective["aspectRatio"] = xmag / ymag;
                    yFoV = tan(ymag) * 2.0;
                }
                camera.erase("orthographic");
            }
            perspective["yfov"] = yFoV;
            perspective["znear"] = znear;
            camera["perspective"] = perspective;
        } else {
            double zfar = 100.0;
            double znear = 0.1;
            double xmag = 1.0;
            double ymag = 1.0;
            if (camera.contains("perspective")) {
                const nlohmann::json& perspective = camera.at("perspective");
                if (perspective.contains("zfar")) {
                    zfar = perspective.at("zfar");
                }
                if (perspective.contains("znear")) {
                    znear = perspective.at("znear");
                }
                double yFoV = 1.0;
                double aspectRatio = 1.0;
                if (perspective.contains("yfov")) {
                    yFoV = perspective.at("yfov");
                }
                if (perspective.contains("aspectRatio")) {
                    aspectRatio = perspective.at("aspectRatio");
                }
                double ratio = atan(yFoV / 2.0);
                xmag = aspectRatio * ratio;
                ymag = ratio;
                camera.erase("perspective");
            }
            const nlohmann::json defaultOrthographic = {
                { "xmag", xmag },
                { "ymag", ymag },
                { "zfar", zfar },
                { "znear", znear }
            };
            camera["orthographic"] = defaultOrthographic;
        }
        return state;
    }
};

Operation gltf::resetGltf(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "reset_gltf";
    operation.schema = schema::operation(
        "Reset glTF",
        "Resets the glTF to a minimal valid version",
        schema::object({ schema::parameter::path("/gltf") }));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        state = nlohmann::json({ { "gltf", nlohmann::json::object() },
            { "files", nlohmann::json::object() },
            { "embedded", nlohmann::json::object() },
            { "glb", nlohmann::json::object() } });

        state["gltf"] = nlohmann::json::object();
        state["gltf"]["asset"] = nlohmann::json::object();
        state["gltf"]["asset"]["version"] = "2.0";

        return state;
    };
    return operation;
};

Operation gltf::createCameraFromView()
{
    Operation operation;
    operation.name = "create_camera_from_view";
    operation.schema = schema::operation(
        "Create camera from view",
        "Creates a perspective camera with the specified properties, attaches it to a new node, and attaches that to the provided scene. sceneIdx can be set to -1 to avoid adding the new node to a scene.",
        schema::object({ { "zNear", schema::number() },
            { "zFar", schema::number() }, { "yFov", schema::number() },
            { "translation", schema::array(schema::number(), 3, 3) }, { "rotation", schema::array(schema::number(), 4, 4) },
            { "sceneIdx", schema::number() } }));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        const float zNear = params["zNear"].get<float>();
        const float zFar = params["zFar"].get<float>();
        const float yFov = params["yFov"].get<float>();
        const int sceneIdx = params["sceneIdx"].get<int>();

        auto taskDescription = nlohmann::json({ { "create_gltf_component", nlohmann::json::object() } });
        taskDescription["create_gltf_component"]["path"] = "/gltf/cameras";
        taskDescription["create_gltf_component"]["type"] = "perspective";
        taskDescription["create_gltf_component"]["name"] = "Generated Camera";
        taskDescription["create_gltf_component"]["perspective"] = nlohmann::json::object();
        taskDescription["create_gltf_component"]["perspective"]["znear"] = zNear;
        taskDescription["create_gltf_component"]["perspective"]["zfar"] = zFar;
        taskDescription["create_gltf_component"]["perspective"]["yfov"] = yFov;
        state = library.call(std::move(taskDescription), std::move(state), dataStorage, callback);

        taskDescription = nlohmann::json({ { "create_gltf_component", nlohmann::json::object() } });
        taskDescription["create_gltf_component"]["path"] = "/gltf/nodes";
        taskDescription["create_gltf_component"]["name"] = "Generated Node";
        taskDescription["create_gltf_component"]["camera"] = state["gltf"]["cameras"].size() - 1;
        taskDescription["create_gltf_component"]["translation"] = params["translation"];
        taskDescription["create_gltf_component"]["rotation"] = params["rotation"];
        state = library.call(std::move(taskDescription), std::move(state), dataStorage, callback);

        if (sceneIdx > -1) {
            taskDescription = nlohmann::json({ { "insert_gltf_pointer", nlohmann::json::object() } });
            taskDescription["insert_gltf_pointer"]["path"] = "/gltf/scenes/" + std::to_string(sceneIdx) + "/nodes";
            taskDescription["insert_gltf_pointer"]["pointer"] = "/gltf/nodes/" + std::to_string(state["gltf"]["nodes"].size() - 1);
            state = library.call(std::move(taskDescription), std::move(state), dataStorage, callback);
        }

        return state;
    };
    return operation;
};

Operation gltf::createTangents(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "create_tangents";
    operation.schema = schema::operation(
        "Create Tangents",
        "Create tangents for the specified primitive",
        schema::object({ schema::parameter::path("/gltf/meshes/\\d+/primitives/\\d+") }));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        // only create tangents for primitives in triangle mode
        if (state.at(nlohmann::json::json_pointer(params["path"].get<std::string>())).contains("mode")
            && state.at(nlohmann::json::json_pointer(params["path"].get<std::string>()) / "mode").get<uint32_t>() != 4) {
            // mesh.primitive.mode: 4 = TRIANGLES
            throw std::invalid_argument("Primitive mode has to be set to TRIANGLES");
        }

        tangent::create(params, state, dataStorage);
        return state;
    };
    return operation;
};

Operation gltf::addMaterialVariantToPrimitive(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "add_material_variant_to_primitive";
    operation.schema = schema::operation(
        "Add material variant to primitive mapping",
        "Adds a material variant/ material pair to the variants mapping of a primitive. Also ensures the extension is properly set up if it does not yet exist on the primitive. (Material and variant index are respective to their gltf level arrays). Does not handle removal.",
        schema::object({
            schema::parameter::path("/gltf/meshes/\\d+/primitives/\\d+"),
            { "materialIDX", schema::pointer("/gltf/materials/\\d+") },
            { "variantIDX", schema::pointer("/gltf/extensions/KHR_materials_variants/variants/\\d+") },
        }));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        const nlohmann::json::json_pointer primitivePath { params["path"] };
        const nlohmann::json::json_pointer materialPath { params["materialIDX"] };
        const nlohmann::json::json_pointer variantPath { params["variantIDX"] };
        const uint32_t materialIDX = std::stoi(materialPath.back());
        const uint32_t variantIDX = std::stoi(variantPath.back());

        if (!state.contains(nlohmann::json::json_pointer("/gltf/extensions/KHR_materials_variants/variants"))) {
            throw std::invalid_argument("Material variants extension not initialized. No variants found.");
        }
        if (!reactor::json::arrayContains(state["gltf"]["extensionsUsed"], "KHR_materials_variants")) {
            throw std::invalid_argument("Material variants extension not initialized. (Missing in extensionsUsed.)");
        }
        if (!state.contains(primitivePath)) {
            throw std::invalid_argument("Primitive not found.");
        }
        if (!state.contains(materialPath)) {
            throw std::invalid_argument("Material not found.");
        }
        if (!state.contains(variantPath)) {
            throw std::invalid_argument("Variant not found.");
        }

        // remove variant from current mapping
        auto taskDescription = nlohmann::json({ { "remove_material_variant_from_primitive", nlohmann::json::object() } });
        taskDescription["remove_material_variant_from_primitive"]["path"] = params["path"];
        taskDescription["remove_material_variant_from_primitive"]["variantIDX"] = params["variantIDX"];
        state = library.call(std::move(taskDescription), std::move(state), dataStorage, callback);

        // setup extension in case it is missing (e.g because the removal above removed the last mapping)
        if (!state.at(primitivePath).contains("extensions")) {
            state.at(primitivePath)["extensions"] = nlohmann::json::object();
        }
        if (!state.at(primitivePath / "extensions").contains("KHR_materials_variants")) {
            state.at(primitivePath / "extensions")["KHR_materials_variants"] = nlohmann::json::object();
        }
        if (!state.at(primitivePath / "extensions" / "KHR_materials_variants").contains("mappings")) {
            state.at(primitivePath / "extensions" / "KHR_materials_variants")["mappings"] = nlohmann::json::array();
        }

        const nlohmann::json::json_pointer mappingsPath = primitivePath / "extensions" / "KHR_materials_variants" / "mappings";
        auto& materialVariantsMappings = state.at(mappingsPath);

        // first do a cleanup step to prevent wrong mappings
        nlohmann::json cleanedMappings = nlohmann::json::array();
        for (auto& [key, mapping] : materialVariantsMappings.items()) {
            if (!mapping.contains("material") || !mapping.contains("variants")) {
                continue;
            }
            if (!mapping["variants"].is_array() && mapping["variants"].size() == 0) {
                continue;
            }
            cleanedMappings.push_back(mapping);
        }
        state.at(mappingsPath) = cleanedMappings;

        bool mappingFound = false;
        for (auto& [key, mapping] : materialVariantsMappings.items()) {
            if (!mapping.contains("material") && !mapping["material"].is_null()) {
                uint32_t materialIdxFromMapping = mapping["material"];
                if (materialIdxFromMapping != materialIDX) {
                    continue;
                }
            } else {
                continue;
            }

            mappingFound = true;
            if (!mapping.contains("variants") || !mapping["variants"].is_array()) {
                mapping["variants"] = nlohmann::json::array();
            }
            if (reactor::json::arrayContains(mapping["variants"], variantIDX)) {
                throw std::invalid_argument("Variant already in mapping.");
            } else {
                mapping["variants"].push_back(variantIDX);
                break;
            }
        }

        if (!mappingFound) {
            // add mapping if not found
            state.at(mappingsPath).push_back(nlohmann::json::object({ { "material", materialIDX }, { "variants", nlohmann::json::array({ variantIDX }) } }));
        }
        return state;
    };
    return operation;
};

Operation gltf::removeMaterialVaraiantFromPrimitive(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "remove_material_variant_from_primitive";
    operation.schema = schema::operation(
        "Remove material variant from primitive mapping",
        "Removes a material variant/ material pair from the variants mapping of a primitive. (Material and variant index are respective to their gltf level arrays). Does not handle removal.",
        schema::object({
            schema::parameter::path("/gltf/meshes/\\d+/primitives/\\d+"),
            { "variantIDX", schema::pointer("/gltf/extensions/KHR_materials_variants/variants/\\d+") },
        }));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        const nlohmann::json::json_pointer primitivePath { params["path"] };
        const nlohmann::json::json_pointer variantPath { params["variantIDX"] };
        const uint32_t variantIDX = std::stoi(variantPath.back());

        if (!state.contains(primitivePath)) {
            throw std::invalid_argument("Primitive not found.");
        }

        if (!state.at(primitivePath).contains("extensions") || !state.at(primitivePath / "extensions").contains("KHR_materials_variants") || !state.at(primitivePath / "extensions" / "KHR_materials_variants").contains("mappings")) {
            return state;
        }

        const nlohmann::json::json_pointer mappingsPath = primitivePath / "extensions" / "KHR_materials_variants" / "mappings";
        for (auto it = state.at(mappingsPath).begin(); it != state.at(mappingsPath).end(); ++it) {
            auto& mapping = *it;
            if (reactor::json::arrayContains(mapping["variants"], variantIDX)) {
                mapping["variants"].erase(std::find(mapping["variants"].begin(), mapping["variants"].end(), variantIDX));
                if (mapping["variants"].empty()) {
                    state.at(mappingsPath).erase(it);
                }
                break;
            }
        }

        // erase material variants extension if no mappings are left
        if (state.at(mappingsPath).empty()) {
            state.at(primitivePath / "extensions").erase("KHR_materials_variants");
            // erase extensions if no extensions are left
            if (state.at(primitivePath / "extensions").empty()) {
                state.at(primitivePath).erase("extensions");
            }
        }

        return state;
    };
    return operation;
};

Operation gltf::replaceMaterial(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "replace_material";
    operation.schema = schema::operation(
        "Replaces a material with another and deletes original material",
        "Replaces a material with another one and deletes the original material.",
        schema::object({
            schema::parameter::path("/gltf/materials/\\d+"),
            { "replacement", schema::pointer("/gltf/materials/\\d+") },
        }));

    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        const nlohmann::json::json_pointer originalPath { params["path"] };
        const nlohmann::json::json_pointer replacementPath { params["replacement"] };

        nlohmann::json::json_pointer originalPathWithoutGltf("/materials/" + originalPath.back());
        auto pointerArrayMaterial = reactor::getBackwardReferenceProperties(state.at("gltf"), originalPathWithoutGltf, linearizedGltfSchema);
        if (pointerArrayMaterial.size() == 0) {
            throw std::invalid_argument("Material can not be replaced. It is not referenced in the glTF.");
        }

        for (auto& pointer : pointerArrayMaterial) {
            const nlohmann::json::json_pointer pointerWithGltf = "/gltf"_json_pointer / pointer;
            state.at(pointerWithGltf) = std::stoi(replacementPath.back());
        }

        auto taskDescription = nlohmann::json({ { "remove", nlohmann::json::object() } });
        taskDescription["remove"]["path"] = params["path"];
        state = library.call(std::move(taskDescription), std::move(state), dataStorage, callback);

        return state;
    };
    return operation;
};