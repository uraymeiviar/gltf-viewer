// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "reactor/Reactor.h"
#include "reactor/Gltf.h"
#include "reactor/Utils.h"
#include <nlohmann/json-schema.hpp>

#include <array>
#include <atomic>
#include <mutex>
#include <regex>
#include <string>
#include <thread>

using namespace ux3d;

// nlohmann::json reactor::validate(const nlohmann::json& schema, const nlohmann::json& json)
//{
//     auto schemaLoader = [&schema](const nlohmann::json_uri& uri, nlohmann::json& json) {
//         json = schema.at(uri.pointer());
//     };
//
//     nlohmann::json_schema::json_validator validator(schemaLoader);
//
//     validator.set_root_schema(schema.at("glTF.schema.json")); // insert root-schema
//
//     return validator.validate(json);
// }

nlohmann::json reactor::zipSchema(const nlohmann::json& schema, const nlohmann::json& json)
{
    std::function<nlohmann::json(const nlohmann::json::json_pointer&, const nlohmann::json::json_pointer&)> traverse;
    traverse = [&](const nlohmann::json::json_pointer& subSchemaPtr, const nlohmann::json::json_pointer& subJsonPtr) -> nlohmann::json {
        nlohmann::json zipped;
        nlohmann::json subJson = json.at(subJsonPtr);

        // follow $ref references
        if (schema.at(subSchemaPtr).contains("$ref")) {
            auto childZipped = traverse(nlohmann::json::json_pointer("") / schema.at(subSchemaPtr / "$ref").get<std::string>(), subJsonPtr);
            zipped.merge_patch(childZipped);
        }

        // traverse into sub schemas
        if (schema.at(subSchemaPtr).contains("allOf")) {
            size_t numAllOfs = schema.at(subSchemaPtr / "allOf").size();
            for (int i = 0; i < numAllOfs; ++i) {
                auto childZipped = traverse(subSchemaPtr / "allOf" / std::to_string(i), subJsonPtr);
                zipped.merge_patch(childZipped);
            }
        }
        // glTF doesn't use "any of" (OR) properties
        // glTF doesn't use "one of" (XOR) properties

        // objects require traversal into the key value pairs (and matching them with the schema)
        if (subJson.is_object()) {
            for (auto& [key, value] : subJson.items()) {
                // look for the key in the properties
                if (schema.at(subSchemaPtr).contains("properties")) {
                    auto properties = schema.at(subSchemaPtr / "properties");
                    if (auto property = properties.find(key); property != properties.end()) {
                        auto childZipped = traverse(subSchemaPtr / "properties" / key, subJsonPtr / key);
                        zipped.merge_patch(childZipped);
                    }
                }

                // glTF doesn't use "pattern properties"
            }
        }

        // schemas for array items are available under the "items" property in the schema
        if (subJson.is_array()) {
            if (schema.at(subSchemaPtr).contains("items")) {
                size_t numItems = subJson.size();
                for (int i = 0; i < subJson.size(); ++i) {
                    auto childZipped = traverse(subSchemaPtr / "items", subJsonPtr / std::to_string(i));
                    zipped.merge_patch(childZipped);
                }
            }
        }

        // add the current path pair to the zipped dictionary
        zipped[subJsonPtr.to_string()] = subSchemaPtr.to_string();

        return zipped;
    };

    return traverse(nlohmann::json::json_pointer("/glTF.schema.json"), nlohmann::json::json_pointer(""));
}

nlohmann::json reactor::resolveGltfReferences(const nlohmann::json& linearizedSchema, const nlohmann::json& gltf)
{
    // There is room for optimization, e.g. by passing a reference to the json object and
    // only emplacing the path prefix there
    std::function<nlohmann::json(const nlohmann::json::json_pointer&, const nlohmann::json&)> traverse;
    traverse = [&](const nlohmann::json::json_pointer& subJsonPtr, const nlohmann::json& additionalPropertiesSchema) -> nlohmann::json {
        auto subJson = gltf.at(subJsonPtr);
        auto regexPath = gltf::substituteIndices(subJsonPtr.to_string());

        // Traverse into json objects with children (non primitives)
        if (false == subJson.is_primitive()) {
            nlohmann::json additionalSchema;
            if (linearizedSchema.contains(regexPath) && linearizedSchema.at(regexPath).contains("additionalProperties")) {
                additionalSchema = linearizedSchema.at(regexPath).at("additionalProperties");
            }
            for (auto& [key, value] : subJson.items()) {
                // write resolved subJson into place of a potential reference
                value = traverse(subJsonPtr / key, additionalSchema);
            }
            return subJson;
        }

        // Resolve primitive values that are a reference to a json pointer
        if (linearizedSchema.contains(regexPath) && linearizedSchema.at(regexPath).contains("ux3d_pathPrefix")) {
            std::string resolvedPrefix = reactor::gltf::applyParentToReference(subJsonPtr.to_string(), linearizedSchema.at(regexPath).at("ux3d_pathPrefix"));
            return (nlohmann::json::json_pointer(resolvedPrefix) / gltf.at(subJsonPtr).get<int>()).to_string();
        }

        if (additionalPropertiesSchema.contains("ux3d_pathPrefix") && subJson.is_number_integer()) {
            std::string resolvedPrefix = reactor::gltf::applyParentToReference(subJsonPtr.to_string(), additionalPropertiesSchema.at("ux3d_pathPrefix"));
            return (nlohmann::json::json_pointer(resolvedPrefix) / gltf.at(subJsonPtr).get<int>()).to_string();
        }

        // Just return the value if there is nothing to do
        return subJson;
    };

    return traverse(nlohmann::json::json_pointer(""), nlohmann::json());
}

nlohmann::json reactor::resolveGltfHierarchy(const nlohmann::json& schemaMapping, const nlohmann::json& linearizedSchema, const nlohmann::json& gltf)
{
    int threadLimit = 8;
    std::atomic<int> threadCount(0);
    std::function<nlohmann::json(const nlohmann::json::json_pointer&)> traverse;
    traverse = [&](const nlohmann::json::json_pointer& subJsonPtr) -> nlohmann::json {
        auto regexPath = gltf::substituteIndices(subJsonPtr.to_string());

        if (!gltf.contains(subJsonPtr)) {
            return nlohmann::json::array();
        }
        const auto& subJson = gltf.at(subJsonPtr);

        bool shouldAdd = linearizedSchema.contains(regexPath) && linearizedSchema.at(regexPath).contains("ux3d_hierarchy");

        if (shouldAdd && schemaMapping.contains(regexPath)) {
            nlohmann::json hierarchyNode = nlohmann::json::object();
            nlohmann::json array = nlohmann::json::array();
            auto pointer = nlohmann::json::json_pointer(subJson);
            bool has_only_digits = (pointer.back().find_first_not_of("0123456789") == std::string::npos);
            if (has_only_digits) {
                hierarchyNode["id"] = subJsonPtr.to_string();
                auto resolvedtuple = utils::getUUIDLocalPath(pointer, gltf);
                auto pathTuple = utils::getUUIDLocalPath(subJsonPtr, gltf);
                hierarchyNode["id_info"]["uuid"] = std::move(std::get<0>(pathTuple));
                hierarchyNode["id_info"]["local_path"] = std::move(std::get<1>(pathTuple));
                hierarchyNode["id_resolved"]["uuid"] = std::move(std::get<0>(resolvedtuple));
                hierarchyNode["id_resolved"]["path"] = pointer.to_string();
                hierarchyNode["children"] = traverse(pointer);
                array.emplace_back(hierarchyNode);
            }
            return array;
        }

        // traverse into json objects with children (non primitives) and collect
        // them in a flat array (we only want to create hierarchy nodes for child
        // of root objects)
        if (subJsonPtr.back() != "skin") {
            nlohmann::json children = nlohmann::json::array();

            const auto& subSchemaIter = linearizedSchema.find(regexPath);

            // if custom ordering is found
            if (subSchemaIter != linearizedSchema.end() && subSchemaIter->contains("ux3d_propertiesOrdering")) {
                const auto& propertiesOrdering = subSchemaIter->at("ux3d_propertiesOrdering");
                // go over all items specified by ordering (in that order)
                for (const std::string& key : propertiesOrdering) {
                    if (subJson.contains(key)) {
                        auto child = traverse(subJsonPtr / key);
                        if (false == child.empty()) {
                            children.insert(children.end(), child.begin(), child.end());
                        }
                    }
                }

                // iterate everything else
                for (auto& [key, value] : subJson.items()) {
                    if (std::find(propertiesOrdering.begin(), propertiesOrdering.end(), key) != propertiesOrdering.end()) {
                        continue;
                    }
                    auto child = traverse(subJsonPtr / key);
                    if (false == child.empty()) {
                        children.insert(children.end(), child.begin(), child.end());
                    }
                }
            } else {
                for (auto& [key, value] : subJson.items()) {
                    auto child = traverse(subJsonPtr / key);
                    if (false == child.empty()) {
                        children.insert(children.end(), child.begin(), child.end());
                    }
                }
            }

            // if (subJson.size() <= 1 || threadCount >= threadLimit) {
            //     // no need for thread overhead in this case
            //     for (auto& [key, value] : subJson.items()) {
            //         auto result = traverse(subJsonPtr / key);
            //         children.insert(children.end(), result.begin(), result.end());
            //     }
            // } else {
            //     std::mutex childrenMutex;
            //     std::vector<std::thread> workers(subJson.size());
            //     int idx = 0;
            //     threadCount += workers.size();
            //     for (auto& [key, value] : subJson.items()) {
            //         workers[idx] = std::thread { [idx, subJsonPtr, key = key, &traverse, &childrenMutex, &children] {
            //             auto result = traverse(subJsonPtr / key);
            //             childrenMutex.lock();
            //             children.insert(children.end(), result.begin(), result.end());
            //             childrenMutex.unlock();
            //         } };
            //         idx++;
            //     }
            //     for (auto& worker : workers) {
            //         if (worker.joinable()) {
            //             worker.join();
            //         }
            //     }
            //     threadCount -= workers.size();
            // }

            // as scenes are the root objects, return a hierarchy node for them as well
            // also match primitives to show them in Hierarchy
            if (shouldAdd) {
                nlohmann::json hierarchyNode = nlohmann::json::object();

                hierarchyNode["id"] = subJsonPtr.to_string();
                auto pathTuple = utils::getUUIDLocalPath(subJsonPtr, gltf);
                hierarchyNode["id_info"]["uuid"] = std::move(std::get<0>(pathTuple));
                hierarchyNode["id_info"]["local_path"] = std::move(std::get<1>(pathTuple));
                hierarchyNode["id_resolved"]["uuid"] = hierarchyNode["id_info"]["uuid"];
                hierarchyNode["id_resolved"]["path"] = hierarchyNode["id"];
                hierarchyNode["children"] = children;

                nlohmann::json array = nlohmann::json::array();
                array.emplace_back(hierarchyNode);
                return array;
            }

            // otherwise just return the children as array, they will be packed
            // into a hierarchy node in higher layers of the recursion
            return children;
        }

        return nlohmann::json::array();
    };

    // wrap the Hierarchy into the root glTF object's children array
    nlohmann::json glTFObject = { { "children", traverse(nlohmann::json::json_pointer("/scenes")) }, { "id", "" } };
    glTFObject["id_info"]["uuid"] = gltf.at("uuid");
    glTFObject["id_info"]["local_path"] = "";
    glTFObject["id_resolved"]["uuid"] = glTFObject["id_info"]["uuid"];
    glTFObject["id_resolved"]["path"] = "";
    nlohmann::json glTFChildArray = nlohmann::json::array();
    glTFChildArray.emplace_back(glTFObject);
    // wrap the output array in another "children" property
    return { { "children", glTFChildArray } };
}

nlohmann::json ux3d::reactor::getCloneHierarchy(const nlohmann::json& schemaMapping, const nlohmann::json& linearizedSchema, const nlohmann::json& gltf, const nlohmann::json::json_pointer& path)
{
    std::function<nlohmann::json(const nlohmann::json::json_pointer&)> traverse;
    traverse = [&](const nlohmann::json::json_pointer& subJsonPtr) -> nlohmann::json {
        auto regexPath = gltf::substituteIndices(subJsonPtr.to_string());

        if (!gltf.contains(subJsonPtr)) {
            return nlohmann::json::array();
        }
        const auto& subJson = gltf.at(subJsonPtr);

        // handle references inside the glTF, e.g. regexPath == "/nodes/children/\\d+"
        if (schemaMapping.contains(regexPath)) {
            auto& subSchema = schemaMapping.at(regexPath);
            std::string resolvedPrefix = reactor::gltf::applyParentToReference(subJsonPtr.to_string(), subSchema.at("ux3d_pathPrefix"));
            auto pointer = nlohmann::json::json_pointer(resolvedPrefix) / subJson.get<int>();
            nlohmann::json hierarchyNode = nlohmann::json::object();
            nlohmann::json array = nlohmann::json::array();

            const bool has_only_digits = (pointer.back().find_first_not_of("0123456789") == std::string::npos);
            if (has_only_digits) {
                nlohmann::json data = nlohmann::json::object();
                data["path"] = pointer.to_string();
                data["source"] = subJsonPtr.to_string();
                if (subJsonPtr.back() != "skin") {
                    hierarchyNode["children"] = traverse(pointer);
                }
                hierarchyNode["data"] = data;

                array.emplace_back(hierarchyNode);
            }

            return array;
        }

        // traverse into json objects with children (non primitives) and collect
        // them in a flat array (we only want to create hierarchy nodes for child
        // of root objects)
        if (false == subJson.is_primitive() && subJsonPtr.back() != "skin") {
            nlohmann::json children = nlohmann::json::array();

            // iterate over all items
            for (auto& [key, value] : subJson.items()) {
                auto child = traverse(subJsonPtr / key);
                if (false == child.empty()) {
                    children.insert(children.end(), child.begin(), child.end());
                }
            }

            return children;
        }

        return nlohmann::json::array();
    };

    // add root object to hierarchy
    if (!gltf.contains(path)) {
        return nlohmann::json();
    }
    const auto& subJson = gltf.at(path);

    nlohmann::json data = nlohmann::json::object();
    data["path"] = path.to_string();
    data["source"] = path.to_string();

    nlohmann::json hierarchyNode = nlohmann::json::object();
    hierarchyNode["data"] = data;
    hierarchyNode["children"] = traverse(path);

    nlohmann::json array = nlohmann::json::array();
    array.emplace_back(hierarchyNode);

    return { { "children", array } };
}

nlohmann::json reactor::resolveGltfResources(const std::vector<nlohmann::json::json_pointer>& resources, const nlohmann::json& gltf)
{
    nlohmann::json result = nlohmann::json::object();
    auto children = nlohmann::json::array();

    for (const auto& resource : resources) {
        auto category = nlohmann::json::object();
        category["id"] = resource.to_string();
        auto categoryChildren = nlohmann::json::array();
        if (gltf.contains(resource)) {
            auto pathTuple = utils::getUUIDLocalPath(resource, gltf);
            category["id_info"]["uuid"] = ""; // To make categories selectable: std::move(std::get<0>(pathTuple)); (this will break isHeading)
            category["id_info"]["local_path"] = std::move(std::get<1>(pathTuple));
            category["id_resolved"]["uuid"] = category["id_info"]["uuid"];
            category["id_resolved"]["path"] = category["id"];
            const auto& value = gltf[resource];
            for (auto& [childKey, childValue] : value.items()) {
                auto categoryChild = nlohmann::json::object();
                categoryChild["children"] = nlohmann::json::array();
                nlohmann::json::json_pointer pointerToChild = resource;
                pointerToChild.push_back(childKey);
                categoryChild["id"] = pointerToChild.to_string();
                pathTuple = utils::getUUIDLocalPath(pointerToChild, gltf);
                categoryChild["id_info"]["uuid"] = std::move(std::get<0>(pathTuple));
                categoryChild["id_info"]["local_path"] = std::move(std::get<1>(pathTuple));
                categoryChild["id_resolved"]["uuid"] = categoryChild["id_info"]["uuid"];
                categoryChild["id_resolved"]["path"] = categoryChild["id"];
                categoryChildren.emplace_back(std::move(categoryChild));
            }
        } else {
            category["id_info"]["uuid"] = "";
            category["id_info"]["local_path"] = "";
            category["id_resolved"]["uuid"] = "";
            category["id_resolved"]["path"] = resource;
        }
        category["children"] = categoryChildren;
        children.emplace_back(std::move(category));
    }
    result["children"] = children;
    return result;
}

nlohmann::json reactor::translateIDsToUUIDs(const nlohmann::json& gltfHierarchy, const nlohmann::json& uuids)
{
    nlohmann::json resolvedHierarchy = gltfHierarchy;
    std::function<void(const nlohmann::json::json_pointer&)> IDtoUUID;
    IDtoUUID = [&](const nlohmann::json::json_pointer& subJsonPtr) {
        if (resolvedHierarchy.contains(subJsonPtr / "id")) {
            nlohmann::json::json_pointer tmpPointer(resolvedHierarchy.at(subJsonPtr / "id"));
            std::string localPath = "";
            while (!tmpPointer.empty() && !uuids.contains(tmpPointer / "uuid")) {
                // Back will throw an exceptions if the pointer is empty. This should never happen but it stops the loop
                localPath = "/" + tmpPointer.back() + localPath;
                tmpPointer.pop_back();
            }
            resolvedHierarchy[subJsonPtr / "uuid"] = uuids.at(tmpPointer / "uuid").get<std::string>() + localPath;
        }
        if (resolvedHierarchy.contains(subJsonPtr / "children")) {
            if (size_t childrenCount = resolvedHierarchy.at(subJsonPtr / "children").size();
                childrenCount > 0) {
                for (int i = 0; i < childrenCount; ++i) {
                    IDtoUUID(subJsonPtr / "children" / i);
                }
            }
        }
    };

    IDtoUUID(nlohmann::json::json_pointer(""));

    return resolvedHierarchy;
}

std::set<nlohmann::json::json_pointer> reactor::extractSubtree(const nlohmann::json& gltf, nlohmann::json::json_pointer from,
    const nlohmann::json& linearizedSchema, bool includeHierarchyElements)
{
    std::set<nlohmann::json::json_pointer> subtree;

    // There is room for optimization, e.g. by passing a reference to the json object and
    // only emplacing the path prefix there
    std::function<void(const nlohmann::json::json_pointer)> recursiveTraversal =
        [&](const nlohmann::json::json_pointer subJsonPtr) {
            const auto& subJson = gltf.at(subJsonPtr);
            std::string regexPath = reactor::gltf::substituteIndices(subJsonPtr.to_string());
            if (linearizedSchema.contains(regexPath) == false) {
                return;
            }
            const auto& subSchema = linearizedSchema.at(regexPath);

            // traverse into json objects with children (non primitives)
            if (false == subJson.is_primitive()) {
                // traverse all parameters
                for (const auto& [key, value] : subJson.items()) {
                    recursiveTraversal(subJsonPtr / key);
                }
            }

            // this should mainly cover primitives
            if (includeHierarchyElements && subSchema.contains("ux3d_hierarchy") && subSchema.contains("ux3d_pathPrefix") == false) {
                subtree.insert(subJsonPtr);
            }

            // replace the context less reference (glTFid) with a full json pointer
            // by reading the ux3d_pathPrefix from the schema
            if (subSchema.contains("ux3d_pathPrefix")) {
                std::string resolvedPrefix = reactor::gltf::applyParentToReference(subJsonPtr.to_string(), subSchema.at("ux3d_pathPrefix"));
                auto resolvedSubJsonPointer = nlohmann::json::json_pointer(resolvedPrefix) / gltf.at(subJsonPtr).get<int>();
                subtree.insert(resolvedSubJsonPointer);
                recursiveTraversal(resolvedSubJsonPointer);
            }
        };

    recursiveTraversal(from);

    return subtree;
}

std::vector<nlohmann::json> reactor::cleanSubTreeElementsFromArray(const nlohmann::json& inputArray, const nlohmann::json& gltf, const nlohmann::json& linearizedSchema)
{
    // gather all subtree elements in one big set.
    std::set<nlohmann::json::json_pointer> combinedSubtreeElements;
    for (auto& [key, elem] : inputArray.items()) {
        std::string tmp = elem["absolutePath"];
        // remove /gltf from path
        if (tmp.starts_with("/gltf")) {
            tmp = tmp.substr(5);
        }
        nlohmann::json::json_pointer pointer(tmp);
        std::set<nlohmann::json::json_pointer> subtree = extractSubtree(gltf, pointer, linearizedSchema);
        combinedSubtreeElements.merge(subtree);
    }

    std::vector<nlohmann::json> elementsFinal;
    elementsFinal.reserve(inputArray.size());
    // for each element check if it is in any of the subtrees. if not add it to the final list
    for (auto [key, elem] : inputArray.items()) {
        std::string tmp = elem["absolutePath"];
        // remove /gltf from path
        if (tmp.starts_with("/gltf")) {
            tmp = tmp.substr(5);
        }
        nlohmann::json::json_pointer pointer(tmp);
        if (combinedSubtreeElements.contains(pointer) == false) {
            elementsFinal.push_back(elem);
        }
    }

    return elementsFinal;
}

bool reactor::areTexCoordsUsedInMaterial(nlohmann::json::json_pointer subPointer, const nlohmann::json& subJson,
    uint32_t texCoordIndex, const nlohmann::json& gltfToSchemaMap, const nlohmann::json& schema)
{
    std::string subPointerStr = subPointer.to_string();
    if (gltfToSchemaMap.contains(subPointerStr)) {
        const nlohmann::json::json_pointer schemaPath(gltfToSchemaMap[subPointerStr]);
        auto& subSchema = schema[schemaPath];
        nlohmann::json flattenedSubschema = subSchema.flatten();
        for (auto& [innerKey, innerValue] : flattenedSubschema.items()) {
            if (innerValue == "textureInfo.schema.json") {
                uint32_t texCoordInMaterial = 0; // default value
                if (subJson.contains("texCoord")) {
                    texCoordInMaterial = (uint32_t)subJson["texCoord"];
                }
                if (texCoordInMaterial == texCoordIndex) {
                    return true;
                }
            }
        }
    }
    if (subJson.is_array() || subJson.is_object()) {
        for (auto& [key, value] : subJson.items()) {
            // if recursive call finds a match return true, otherwise continue search
            if (areTexCoordsUsedInMaterial(nlohmann::json::json_pointer(subPointerStr + "/" + key), value, texCoordIndex, gltfToSchemaMap, schema)) {
                return true;
            }
        }
    }
    return false;
}

std::vector<nlohmann::json::json_pointer> reactor::getBackwardReferenceProperties(const nlohmann::json& gltf, const nlohmann::json::json_pointer& propertyPath, const nlohmann::json& linearizedSchema)
{
    auto childOfRoots = reactor::gltf::schema::filterChildOfRootProperties(linearizedSchema);
    auto backwardReferences = reactor::gltf::schema::backwardReferences(linearizedSchema);
    std::vector<nlohmann::json::json_pointer> propertyPaths;
    uint64_t targetIndex = std::stoi(propertyPath.back());
    auto pattern = reactor::schema::mapToPattern(childOfRoots, propertyPath);
    if (backwardReferences.contains(pattern)) {
        for (const auto& referencePattern : backwardReferences[pattern]) {
            reactor::schema::patternForEach(referencePattern, gltf, [&](const nlohmann::json::json_pointer& pointer) {
                if (gltf.at(pointer).template get<uint64_t>() == targetIndex) {
                    propertyPaths.push_back(pointer);
                }
            });
        }
    }
    return propertyPaths;
}

size_t reactor::getArraySize(const nlohmann::json& gltf, const nlohmann::json::json_pointer& path)
{
    if (gltf.contains(path)) {
        if (gltf.at(path).is_array()) {
            return gltf.at(path).size();
        }
    }
    return 0;
}

std::vector<std::tuple<std::string, std::string>> reactor::getReferenceCandidates(const nlohmann::json& gltf, nlohmann::json::json_pointer path, const std::string& pathToCalledProperty)
{
    path = nlohmann::json::json_pointer(reactor::gltf::applyParentToReference(pathToCalledProperty, path.to_string()));
    size_t arraySize = getArraySize(gltf, path);
    std::vector<std::tuple<std::string, std::string>> result;
    result.reserve(arraySize);
    for (size_t i = 0; i < arraySize; ++i) {
        nlohmann::json::json_pointer pathToElement = (path / i);
        std::string name = getDisplayName(gltf, pathToElement);
        result.push_back(std::make_tuple(pathToElement.to_string(), name));
    }
    return result;
}

nlohmann::json reactor::getReferenceCandidatesJson(const nlohmann::json& gltf, nlohmann::json::json_pointer path)
{
    const auto pathToArray = nlohmann::json::json_pointer(gltf.at(path)).parent_pointer();

    size_t arraySize = getArraySize(gltf, pathToArray);
    auto referenceCandidatesJson = nlohmann::json::array();
    for (size_t i = 0; i < arraySize; ++i) {
        nlohmann::json::json_pointer pathToElement = (pathToArray / i);
        std::string name = getDisplayName(gltf, pathToElement);
        referenceCandidatesJson.push_back(nlohmann::json { { "path", pathToElement.to_string() }, { "name", name } });
    }

    return referenceCandidatesJson;
}

std::string reactor::getDisplayName(const nlohmann::json& gltf, nlohmann::json::json_pointer path)
{

    if (gltf.at(path).contains("name")) {
        return gltf.at((path / "name")).get<std::string>();
    }

    if (path.empty() || path.parent_pointer().empty()) {
        // If there is no name available, fall back to the path
        return path.to_string();
    }

    // Construct the name from the type and index
    const std::string index = path.back();
    path.pop_back();

    auto name = path.back();
    if (name.empty()) {
        // This should never happen as a valid json pointer always
        // has a non empty string in each segment
        return index;
    }

    name[0] = std::toupper(name[0]);

    // Remove trailing 's' character
    if (name.back() == 's') {
        name.pop_back();
    }

    return name + " " + index;
}
