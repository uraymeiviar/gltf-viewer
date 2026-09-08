#include "reactor/Gltf.h"
#include <algorithm>
#include <fstream>
#include <gsl/span>
#include <regex>
#include <set>
#include <string>
#include <uuid.h>

#include "reactor/Json.h"
#include "reactor/Schema.h"
#include "reactor/Utils.h"

using namespace ux3d;
using namespace ux3d::reactor;
using namespace std::string_literals;

nlohmann::json gltf::validateNodeHierarchies(const nlohmann::json& gltf)
{
    auto validationErrors = nlohmann::json::array();

    // Copy the nodes array so that we can extend it with info about parent nodes.
    // It should also be an object not to break references to elements in the nodes array
    // after removing elements.
    auto nodes = nlohmann::json::object();
    const auto& gltfNodesArray = gltf.value("nodes", nlohmann::json::array());
    for (int i = 0; i < gltfNodesArray.size(); ++i) {
        nodes[std::to_string(i)] = gltfNodesArray[i];
    }
    auto scenes = gltf.value("scenes", nlohmann::json::array());

    // Fill in parent pointer into node children
    for (auto& [parentId, parent] : nodes.items()) {
        for (int childId : parent.value("children", nlohmann::json::array())) {
            auto& child = nodes[std::to_string(childId)];
            if (!child.contains("parents")) {
                child["parents"] = nlohmann::json::array();
            }
            child["parents"].emplace_back(std::stoi(parentId));
        }
    }

    // Search for children with more than one parent
    for (auto& [nodeId, node] : nodes.items()) {
        if (node.value("parents", nlohmann::json::array()).size() > 1) {
            validationErrors.emplace_back(nlohmann::json::object({ { "path", "/nodes/" + nodeId },
                { "what", "Node has more than one parent." } }));
        }
    }

    for (auto& [sceneId, scene] : scenes.items()) {
        for (int nodeId : scene.value("nodes", nlohmann::json::array())) {
            if (nodes[std::to_string(nodeId)].contains("parents")) {
                validationErrors.emplace_back(nlohmann::json::object({ { "path", "/nodes/"s + std::to_string(nodeId) },
                    { "what", "RootNode has parent." } }));
            }
        }
    }

    // Test wether the node hierarchy is an acyclic graph
    // Algorithm based on iteratively removing all leaf nodes
    // until either no node or the cycle nodes are left.
    // See https://www.cs.hmc.edu/~keller/courses/cs60/s98/examples/acyclic/
    bool hasLeaf = true;
    while (hasLeaf) {
        hasLeaf = false;
        for (auto& [leafId, leaf] : nodes.items()) {
            if (leaf.value("children", nlohmann::json::array()).empty()) {
                // remove leaf
                // remove from parents "children" list
                for (int parentId : leaf.value("parents", nlohmann::json::array())) {
                    // the parent is guaranteed to have children
                    auto& children = nodes[std::to_string(parentId)].at("children");
                    auto leafIt = std::find(children.begin(), children.end(), std::stoi(leafId));
                    children.erase(leafIt);
                }
                hasLeaf = true;
                nodes.erase(leafId);
                // break loop as it would be invalid after erase
                break;
            }
        }
    }

    // We are left with the nodes forming a cycle in the graph
    if (!nodes.empty()) {
        auto nodesCycle = nlohmann::json::array();
        for (auto& [nodeId, node] : nodes.items()) {
            nodesCycle.emplace_back(nodeId);
        }

        for (auto& [nodeId, node] : nodes.items()) {
            validationErrors.emplace_back(nlohmann::json::object({ { "path", "/nodes/" + nodeId },
                { "what", "Node forms a node cycle with " + nodesCycle.dump() } }));
        }
    }

    return validationErrors;
}

nlohmann::json gltf::propertyUUIDs(nlohmann::json&& uuids, const nlohmann::json& minimalDiff, const std::string& recursePath)
{
    // Initialize UUID random number generator
    std::random_device rd;
    auto seed_data = std::array<int, std::mt19937::state_size> {};
    std::generate(std::begin(seed_data), std::end(seed_data), std::ref(rd));
    std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
    std::mt19937 generator(seq);
    uuids::uuid_random_generator generateUUID { generator };

    // Traverse function that generates UUIDs for children of newly added properties
    std::function<void(const nlohmann::json&, const nlohmann::json::json_pointer&)> assignUUIDsRecursively;
    std::function<void(const nlohmann::json&, const nlohmann::json::json_pointer&)> assignUUIDsIterateJson;
    // traversal function for newly created elements
    std::function<void(const nlohmann::json&, const nlohmann::json::json_pointer&)> assignUUIDsAdd;

    assignUUIDsIterateJson = [&](const nlohmann::json& valueJson, const nlohmann::json::json_pointer& path) {
        // Traverse into items
        for (auto& [key, value] : valueJson.items()) {
            // skip primitive properties
            if (key.empty()) {
                continue;
            }
            // json::at(...) doesn't allow strings as keys for arrays, therefore
            // convert it to a json pointer which is universal for both object and array
            auto keyPointer = nlohmann::json::json_pointer("/" + key);
            assignUUIDsRecursively(valueJson.at(keyPointer), path / key);
        }
    };

    assignUUIDsRecursively = [&](const nlohmann::json& valueJson, const nlohmann::json::json_pointer& path) {
        if (valueJson.is_object()) {
            uuids[path]["uuid"] = uuids::to_string(generateUUID());
        }
        assignUUIDsIterateJson(valueJson, path);
    };

    assignUUIDsAdd = [&](const nlohmann::json& valueJson, const nlohmann::json::json_pointer& path) {
        if (valueJson.is_object()) {
            nlohmann::json uuidObject = { { "uuid", uuids::to_string(generateUUID()) } };
            if (uuids[path.parent_pointer()].is_array()) {
                auto it = uuids[path.parent_pointer()].cbegin() + std::stoi(path.back());
                uuids[path.parent_pointer()].insert(it, uuidObject);
            } else {
                uuids[path] = uuidObject;
            }
        }
        assignUUIDsIterateJson(valueJson, path);
    };

    for (auto& diff : minimalDiff) {
        if (diff["path"].get<std::string>().rfind(recursePath, 0) == std::string::npos) {
            continue;
        }
        auto path = nlohmann::json::json_pointer(diff["path"]);
        if (diff["op"] == "add") {
            assignUUIDsAdd(diff["value"], path);
        } else if (diff["op"] == "remove") {
            // if the property is not part of UUIDs nothing has to be done
            if (!uuids.contains(path.parent_pointer())) {
                continue;
            }
            // erase with the correct key (json_pointer is not supported by erase())
            auto& parent = uuids.at(path.parent_pointer());
            if (parent.is_object()) {
                parent.erase(path.back());
            } else if (parent.is_array()) {
                parent.erase(std::stoi(path.back()));
            }

            uuids.erase(path);
        } else if (diff["op"] == "replace") {
            // If the value is replaced this essentially is the same as remove + add so just
            // replace the UUID with a new one
            assignUUIDsRecursively(diff["value"], path);
        }
    }
    return uuids;
}

std::string gltf::substituteIndices(const std::string& path)
{
    std::vector<std::string> tokens = reactor::utils::splitString(path, "/");
    for (auto& token : tokens) {
        if (token.length() > 0 && token.find_first_not_of("0123456789") == std::string::npos) {
            token = "\\d+";
        }
    }
    return reactor::utils::mergeString(tokens, "/");
}

std::string gltf::substituteLastIndex(const std::string& path)
{
    std::vector<std::string> tokens = reactor::utils::splitString(path, "/");
    for (auto it = tokens.rbegin(); it != tokens.rend(); ++it) {
        if (it->length() > 0 && it->find_first_not_of("0123456789") == std::string::npos) {
            *it = "\\d+";
            break;
        }
    }
    return reactor::utils::mergeString(tokens, "/");
}

std::string gltf::applyParentToReference(const std::string& parentPath, const std::string& path)
{
    size_t index = path.rfind("\\d+");
    if (index == std::string::npos) {
        return path;
    }
    std::vector<std::string> tokens = reactor::utils::splitString(path, "/");
    if (tokens.size() > 1 && tokens[1] == "gltf") {
        tokens.erase(tokens.begin() + 1);
    }
    std::vector<std::string> tail;
    for (int i = tokens.size() - 1; i >= 0; --i) {
        if (tokens[i] == "\\d+") {
            std::vector<std::string> parentTokens = reactor::utils::splitString(parentPath, "/");
            if (parentTokens.size() > 1 && parentTokens[1] == "gltf") {
                parentTokens.erase(parentTokens.begin() + 1);
            }
            parentTokens.resize(static_cast<size_t>(i) + 1);
            parentTokens.insert(parentTokens.end(), tail.begin(), tail.end());
            return reactor::utils::mergeString(parentTokens, "/");
        }
        tail.emplace_back(tokens[i]);
    }
    return path;
}

const nlohmann::json* gltf::getGltfProperty(const nlohmann::json::json_pointer& path, const nlohmann::json& gltf, const nlohmann::json& linearizedSchema)
{
    std::string generalizedPath = substituteIndices(path.to_string());
    if (linearizedSchema.contains(generalizedPath) == false) {
        return nullptr;
    }
    const nlohmann::json& referenceSchema = linearizedSchema[generalizedPath];
    if (referenceSchema.contains("ux3d_pathPrefix")) {
        std::string pathPrefix = referenceSchema["ux3d_pathPrefix"];
        if (gltf.contains(path)) {
            std::string pathPrefix = reactor::gltf::applyParentToReference(path.to_string(), pathPrefix);
            const int idx = gltf[path];
            std::string finalPath = pathPrefix + "/" + std::to_string(idx);
            return &gltf[nlohmann::json::json_pointer(finalPath)];
        }
    }
    return nullptr;
}

void gltf::getSettableProperties(const std::string& parentPath, const std::string& insertCandidate, const nlohmann::json& linearizedSchema,
    std::vector<std::string>& outCandidates, std::vector<std::string>& outArrayCandidates)
{
    std::string substitudedPathSelected = gltf::substituteIndices(parentPath);
    for (auto& [key, value] : linearizedSchema[substitudedPathSelected]["properties"].items()) {
        if (value.contains("ux3d_pathPrefix") && insertCandidate.rfind(value["ux3d_pathPrefix"], 0) == 0) {
            outCandidates.push_back("/" + key);
        } else if (value.contains("/items/ux3d_pathPrefix"_json_pointer) && insertCandidate.rfind(value["items"]["ux3d_pathPrefix"], 0) == 0) {
            outArrayCandidates.push_back("/" + key);
        }
    }
    if (linearizedSchema[substitudedPathSelected].contains("/properties/extensions/properties"_json_pointer)) {
        for (auto& [key, value] : linearizedSchema[substitudedPathSelected]["properties"]["extensions"]["properties"].items()) {
            for (auto& [extensionKey, extensionValue] : value["properties"].items()) {
                if (extensionValue.contains("ux3d_pathPrefix") && insertCandidate.rfind(extensionValue["ux3d_pathPrefix"], 0) == 0) {
                    outCandidates.push_back("/extensions/" + key + "/" + extensionKey);
                } else if (extensionValue.contains("/items/ux3d_pathPrefix"_json_pointer) && insertCandidate.rfind(extensionValue["items"]["ux3d_pathPrefix"], 0) == 0) {
                    outArrayCandidates.push_back("/extensions/" + key + "/" + extensionKey);
                }
            }
        }
    }
}

std::vector<nlohmann::json::json_pointer> gltf::gatherUnusedNodes(const nlohmann::json& gltf, const nlohmann::json& backwardReferences)
{
    std::vector<nlohmann::json::json_pointer> unusedNodes;
    if (!gltf.contains("nodes")) {
        return unusedNodes;
    }

    if (!gltf.contains("scenes")) {
        return unusedNodes;
    }

    std::set<std::string> usedNodes;

    // gather all root nodes
    for (const auto& scene : gltf.at("scenes")) {
        if (!scene.contains("nodes")) {
            continue;
        }
        for (const auto& nodeId : scene.at("nodes")) {
            usedNodes.insert(nodeId);
        }
    }

    // gather all child nodes (child nodes are not allowed to be root nodes anyways)
    for (const auto& node : gltf.at("nodes")) {
        if (node.contains("children") == false) {
            continue;
        }

        for (const auto& childId : node.at("children")) {
            std::string childIdStr = childId.get<std::string>();
            usedNodes.insert(childIdStr);
        }
    }

    for (const auto& [key, node] : gltf.at("nodes").items()) {
        if (usedNodes.contains("/nodes/" + key) == false) {
            unusedNodes.emplace_back("/nodes/" + key);
        }
    }

    return unusedNodes;
}

nlohmann::json gltf::schema::backwardReferences(const nlohmann::json& linearizedSchema)
{
    nlohmann::json refs;

    for (const auto& [pattern, schema] : linearizedSchema.items()) {
        std::string pathPrefix;
        bool additionalProperty = false;
        if (schema.contains("ux3d_pathPrefix")) {
            pathPrefix = schema.at("ux3d_pathPrefix").get<std::string>() + "/\\d+";
        } else if (schema.contains("/additionalProperties/ux3d_pathPrefix"_json_pointer)) {
            pathPrefix = schema.at("/additionalProperties/ux3d_pathPrefix"_json_pointer).get<std::string>() + "/\\d+";
            additionalProperty = true;
        } else {
            continue;
        }
        // Push the current pattern to the backwardReferences
        if (!refs.contains(pathPrefix)) {
            refs[pathPrefix] = nlohmann::json::array();
        }
        if (additionalProperty) {
            // Add regex for any next property (\\x2f -> /)
            refs.at(pathPrefix).push_back(pattern + "/[^\\x2f]*");
        } else {
            refs.at(pathPrefix).push_back(pattern);
        }
    }

    return refs;
}

nlohmann::json gltf::schema::topLevelProperties(const nlohmann::json& linearizedSchema)
{
    nlohmann::json topLevelItems = nlohmann::json::array();

    for (const auto& [pattern, schema] : linearizedSchema.items()) {
        if (!schema.contains("ux3d_pathPrefix")) {
            continue;
        }
        // We know that "ux3d_pathPrefix" points to an array
        auto pathPrefix = schema.at("ux3d_pathPrefix").get<std::string>() + "/\\d+";
        // Push the current pattern to the backwardReferences
        auto contained = std::find(topLevelItems.begin(), topLevelItems.end(), pathPrefix);
        if (contained == topLevelItems.end()) {
            topLevelItems.push_back(pathPrefix);
        }
    }

    return topLevelItems;
}

nlohmann::json gltf::schema::filterChildOfRootProperties(const nlohmann::json& linearizedSchema)
{
    auto childOfRoots = nlohmann::json::object();
    for (const auto& [pattern, schema] : linearizedSchema.items()) {
        if (schema.contains("allOf")
            && schema.at("allOf").at(0).contains("title")
            && schema.at("allOf").at(0).at("title") == "glTF Child of Root Property") {
            childOfRoots[pattern] = schema;
        }
    }
    return childOfRoots;
}

nlohmann::json gltf::schema::filterUriProperties(const nlohmann::json& linearizedSchema)
{
    auto uriProperties = nlohmann::json::object();
    auto parameters = nlohmann::json::array();
    for (const auto& [pattern, schema] : linearizedSchema.items()) {
        if (schema.contains("properties")
            && schema.at("properties").contains("uri")) {
            uriProperties[pattern] = schema;
        }
    }
    return uriProperties;
}

std::string gltf::schema::getGlobalPath(const nlohmann::json& linearizedSchemaResolved, std::string path)
{
    nlohmann::json::json_pointer jsonPointer(path);

    while (!jsonPointer.empty()) {
        for (const auto& [pattern, schema] : linearizedSchemaResolved.items()) {
            if (!schema.contains("type")) {
                continue;
            }
            if (schema.at("type") != "object") {
                continue;
            }

            std::regex regex { "^/gltf"s + pattern + "$"s };
            std::smatch match;
            std::string stringPointer = jsonPointer.to_string();
            if (std::regex_search(stringPointer, match, regex)) {
                return stringPointer;
            }
        }
        jsonPointer.pop_back();
    }

    return "";
}

std::string gltf::schema::getLocalPath(const nlohmann::json& linearizedSchemaResolved, std::string path)
{
    nlohmann::json::json_pointer globalPath(path);
    nlohmann::json::json_pointer localPath;

    while (!globalPath.empty()) {
        for (const auto& [pattern, schema] : linearizedSchemaResolved.items()) {
            if (!schema.contains("type")) {
                continue;
            }
            if (schema.at("type") != "object") {
                continue;
            }

            std::regex regex { "^/gltf"s + pattern + "$"s };
            std::smatch match;
            std::string stringPointer = globalPath.to_string();
            if (std::regex_search(stringPointer, match, regex)) {
                return localPath.to_string();
            }
        }
        localPath = nlohmann::json::json_pointer("/" + globalPath.back()) / localPath;
        globalPath.pop_back();
    }

    return "";
}

nlohmann::json gltf::schema::resolveAdditionalProperties(const nlohmann::json& linearizedSchema, const nlohmann::json& gltf)
{
    nlohmann::json resolvedSchema = linearizedSchema;
    for (const auto& item : linearizedSchema.items()) {
        const auto& regexPath = item.key();
        const auto& schemaContent = item.value();
        if (schemaContent.contains("additionalProperties")) {
            reactor::json::for_each_match(regexPath, gltf, [&](const nlohmann::json::json_pointer& pointer) {
                for (auto& gltfItem : gltf.at(pointer).items()) {
                    auto gltfProperty = gltfItem.key();
                    if (!schemaContent.contains("properties") || !schemaContent["properties"].contains(gltfProperty)) {

                        resolvedSchema[regexPath][gltfProperty] = schemaContent["additionalProperties"];
                        if (schemaContent["additionalProperties"].contains("ux3d_pathPrefix")) {
                            resolvedSchema[regexPath + "/" + gltfProperty] = schemaContent["additionalProperties"];
                        }
                    }
                }
            });
        }
    }
    return resolvedSchema;
}
