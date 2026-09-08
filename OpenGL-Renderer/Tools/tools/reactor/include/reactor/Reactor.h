// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include <nlohmann/json.hpp>
#include <set>

// 🥽🧪🥼🧪🥽
// Reactor is a library that provides functions to transform
// the base data model (e.g. raw glTF) to more helpful representations.
// An example for this is the hierarchy representation that resolves
// all glTF references between glTF child-of-root properties into a
// transform hierarchy, another would be a validator that generates
// a validation report from the base glTF data.

#include "reactor/Gltf.h"
#include "reactor/Operations.h"
#include "reactor/Schema.h"

namespace ux3d::reactor {

///// validate a json against a specified schema.
///// @param schema to validate agains. According to JSON Scema draft 7.
///// @param json the json data to be validate
///// @returns a validation report containing all issues in the json
// nlohmann::json validate(const nlohmann::json& schema, const nlohmann::json& json);

/// zipSchema combines properties of a json file with their corresponding json schema descriptions. The output is a
/// JSON mapping a json path to a schema property.
/// @param schema  for the json
/// @param json  to zip with the schema
/// @returns a json mapping between the json and its schema
nlohmann::json zipSchema(const nlohmann::json& schema, const nlohmann::json& json);

/// resolveGltfReferences converts all glTF child of root references
/// to json paths.
/// Example: a node glTFid reference of "3" would be resolved to the
/// json path "/nodes/3"
/// @param linearized schema as created with schema::linearize
/// @param gltf the glTF json file
nlohmann::json resolveGltfReferences(const nlohmann::json& linearizedSchema, const nlohmann::json& gltf);

/// resolveGltfHierarchy constructs a datastructure with a resolved glTF
/// node hierarchy. Example:
/// [{
///      path: "/scenes/0",
///      children: [{
///          path: "/nodes/3",
///          children: [{
///              path: "/meshes/2",
///              children: [ ... ],
///          }],
///      }],
/// }]
/// @param schemaMapping mapping from gltf properties to their schemas (see zipSchema function)
/// @param schema the glTF schema
/// @param gltf the glTF json file
nlohmann::json resolveGltfHierarchy(const nlohmann::json& schemaMapping, const nlohmann::json& linearizedSchema, const nlohmann::json& gltf);

/**
 * @brief Old version of resolveGltfHierarchy which is still used for clone
 * @param schemaMapping
 * @param linearizedSchema
 * @param gltf
 * @return
 */
nlohmann::json getCloneHierarchy(const nlohmann::json& schemaMapping, const nlohmann::json& linearizedSchema, const nlohmann::json& gltf, const nlohmann::json::json_pointer& path);

nlohmann::json resolveGltfResources(const std::vector<nlohmann::json::json_pointer>& resources, const nlohmann::json& gltf);

nlohmann::json translateIDsToUUIDs(const nlohmann::json& gltfHierarchy, const nlohmann::json& uuids);

/**
 * @brief Extracts a subtree from the given gltf starting at the provided element.
 *
 * @param gltf
 * @param from starting element
 * @param linearizedSchema linearized gltf schema
 * @param includeHierarchyElements if non child of root elements visible in the hierarchy schould be included in the subtree
 *              (this includes primitives and scenes (as of the writing of this text 05.04.2022)).
 *
 * @return std::vector<nlohmann::json::json_pointer>
 */
std::set<nlohmann::json::json_pointer> extractSubtree(const nlohmann::json& gltf, nlohmann::json::json_pointer from, const nlohmann::json& linearizedSchema, bool includeHierarchyElements = true);

std::vector<nlohmann::json> cleanSubTreeElementsFromArray(const nlohmann::json& inputArray, const nlohmann::json& gltf, const nlohmann::json& linearizedSchema);

/**
 * @brief checks if a texCoord is used in a material. This function recursively iterates all items in a provided json
 * and searches for occurances of a texture info element.
 * In the first iteration you should provide a material to this function.
 *
 * @param subPointer pointer to material (or parts of material)
 * @param subJson json for material (or material part)
 * @param texCoordIndex index of tested texcoord
 * @param gltfToSchemaMap insert mapping from gltf properties to schema here (create with zipSchema from Reactor.h)
 * @param schema
 * @return true
 * @return false
 */
bool areTexCoordsUsedInMaterial(nlohmann::json::json_pointer subPointer, const nlohmann::json& subJson, uint32_t texCoordIndex, const nlohmann::json& gltfToSchemaMap, const nlohmann::json& schema);

std::vector<nlohmann::json::json_pointer> getBackwardReferenceProperties(const nlohmann::json& gltf, const nlohmann::json::json_pointer& propertyPath, const nlohmann::json& linearizedSchema);

/**
 * @brief returns the size of an array in the gltf
 *
 * @param gltf
 * @param path to an array in the gltf
 * @return size of the array
 */
size_t getArraySize(const nlohmann::json& gltf, const nlohmann::json::json_pointer& path);

/**
 * @brief returns a list of tuple which contain the names and pointers to all references which could be selected for a property (e.g. camera in node).
 * This returns all items from acceptable types and does not do any advance checking such as an invalid node hierarchy check.
 *
 * @param gltf
 * @param path to the property which can have references
 * @return Vector of Tuples with path to reference and name of reference
 */
std::vector<std::tuple<std::string, std::string>> getReferenceCandidates(const nlohmann::json& gltf, nlohmann::json::json_pointer path, const std::string& pathToCalledProperty = "");
nlohmann::json getReferenceCandidatesJson(const nlohmann::json& gltf, nlohmann::json::json_pointer path);

/**
 * @brief returns a displayable name, either the properties gltf name or a generated name from its type and index
 * @param gltf the full gltf object
 * @param path to the property with a name property
 * @return A string with either the glTF name or a constructed name, e.g. "Scene 0"
 */
std::string getDisplayName(const nlohmann::json& gltf, nlohmann::json::json_pointer path);

}
