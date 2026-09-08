// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include <nlohmann/json.hpp>

namespace ux3d::reactor::gltf {

/**
 * @brief validate node hierarchy against the glTF specification.
 * see https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#nodes-and-hierarchy
 * @param gltf the gltf for which all node hierarchies are validated
 * @returns nlohmann::json containing a validation report if there are validation errors,
 * an empty json object otherwise
 */
nlohmann::json validateNodeHierarchies(const nlohmann::json& gltf);

/**
 * @brief Generate a mapping between properties and a UUID that can remain constant for
 * the lifetime of the property.
 *
 * @param uuids the json returned by previous calls to propertyUUIDs
 * @param gltfDiff the diff between the current gltf and the gltf that was used to generate
 * @param recursePath into which property the recursion should go. If empty uuids for all properties are created.
 * the previous UUIDs
 * @return nlohmann::json a mapping from JSON Path to UUID
 */
nlohmann::json propertyUUIDs(nlohmann::json&& uuids, const nlohmann::json& minimalDiff, const std::string& recursePath = "");

/**
 * @brief Substitutes all indices in a path with //d+
 *
 * @param path the path to correct.
 *
 * @return corrected path.
 */
std::string substituteIndices(const std::string& path);

/**
 * @brief Substitutes the last index in a path with //d+
 *
 * @param path the path to correct.
 *
 * @return corrected path.
 */
std::string substituteLastIndex(const std::string& path);

/**
 * @brief Uses the indicies of the parent for all indicies but the last one
 *
 * @param parentPath to use for indicies
 * @param path with //d+.
 *
 * @return corrected path.
 */
std::string applyParentToReference(const std::string& parentPath, const std::string& path);

/**
 * @brief Returns the gltf property referenced under path.
 *
 * @param uuids the path to the gltf property
 * @param gltf the gltf from which to query the value
 * @param linearizedSchema Schema linearized with schema::linearize
 *
 * @return nlohmann::json the json object.
 */
const nlohmann::json* getGltfProperty(const nlohmann::json::json_pointer& path, const nlohmann::json& gltf, const nlohmann::json& linearizedSchema);

/**
 * @brief Fills outCandidates and outArrayCandidates with all property paths which can insertCandidate can be inserted into
 *
 * @param parentPath Where to insert the item (without /gltf)
 * @param insertCandidate What to insert (without /gltf)
 * @param linearizedSchema
 * @param outCandidates result vector for reference properties
 * @param outArrayCandidates result vector for reference arrays
 */
void getSettableProperties(const std::string& parentPath, const std::string& insertCandidate, const nlohmann::json& linearizedSchema,
    std::vector<std::string>& outCandidates, std::vector<std::string>& outArrayCandidates);

/**
 * @brief Gather a list of nodes that are not used in any scene under any other property.
 * @param gltf the gltf from which to gather the unused nodes. It is expected that the references in the glTF are resolved.
 * @param backwardReferences the backward references as produced by gltf::schema::backwardReferences. It is sufficient to
 * provide the backward references of the nodes property e.g. { "/nodes/\\d+" : ["/nodes/\\d+/children", "/scenes/\\d+/nodes ] }
 *
 * @return std::vector<nlohmann::json::json_pointer> a vector containing json pointers to all unused nodes
 */
std::vector<nlohmann::json::json_pointer> gatherUnusedNodes(const nlohmann::json& gltf, const nlohmann::json& backwardReferences);

namespace schema {

    /**
     * @brief Gather referencing properties for each property in the linearized schema.
     * The output is a json like
     * {
     *      "/meshes/\\d+" : ["/nodes/\\d+/mesh"],
     *      "/images/\\d+" : ["/textures/\\d+/source"]
     * }
     *
     * @param linearizedSchema Schema linearized with schema::linearize
     * @return nlohmann::json JSON containing the backward reference mappings
     */
    nlohmann::json backwardReferences(const nlohmann::json& linearizedSchema);

    /**
     * @brief Gather referencing properties for each property in the linearized schema.
     * The output is a json like
     * [
     *      "/meshes/\\d+",
     *      "/images/\\d+"
     * ]
     *
     * @param linearizedSchema Schema linearized with schema::linearize
     * @return nlohmann::json JSON array containing the top level items (mostly child of root)
     */
    nlohmann::json topLevelProperties(const nlohmann::json& linearizedSchema);

    /**
     * @brief Filter linearized json schema to only contain child of root properties
     *
     * @param linearizedSchema
     * @return nlohmann::json
     */
    nlohmann::json filterChildOfRootProperties(const nlohmann::json& linearizedSchema);

    /**
     * @brief Filter linearized json schema to only contain properties containing a uri (e.g. buffers, images)
     *
     * @param linearizedSchema
     * @return nlohmann::json
     */
    nlohmann::json filterUriProperties(const nlohmann::json& linearizedSchema);

    /**
     *  @brief  /gltf/nodes/1/name -> /gltf/nodes/1
     *  @param linearizedSchemaResolved has to be a linearizedSchema with AllOf entries resolved via reactor::json::resolveAllOf
     */
    std::string getGlobalPath(const nlohmann::json& linearizedSchemaResolved, std::string path);

    /**
     *  @brief  /gltf/nodes/1/name -> /name
     *  @param linearizedSchemaResolved has to be a linearizedSchema with AllOf entries resolved via reactor::json::resolveAllOf
     */
    std::string getLocalPath(const nlohmann::json& linearizedSchemaResolved, std::string path);

    /**
     * @brief Resolves additionalProperties in linearizedSchema according to specific glTF
     * e.g. key "/meshes/\\d+/primitives/\\d+/attributes/NORMAL" is added if one attribute in glTF contains NORMAL
     *
     * @param linearizedSchema
     * @param gltf
     * @return linearizedSchema with resolved additionalProperties
     */
    nlohmann::json resolveAdditionalProperties(const nlohmann::json& linearizedSchema, const nlohmann::json& gltf);
}

}
