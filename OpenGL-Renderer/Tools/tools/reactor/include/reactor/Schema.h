#pragma once

#include <nlohmann/json.hpp>

// 🥽🧪🥼🧪🥽
// Reactor transforms that can be run on a json schema

namespace ux3d::reactor::schema {

/**
 * @brief Resolve all $ref s in a schema by placing the referenced schema at the location of the $ref 
 * @param schema Contains all schemas with the filename as key and the content of the file as value
 * @param rootKey Name of the root schema. Must be one of the keys in the "schema" parameter
 * @return nlohmann::json schema with all references resolved
 */
nlohmann::json resolveRefs(const nlohmann::json& schema, const std::string& rootKey);

/**
 * @brief Linearize a json schema into a mapping of json pointer pattern to json
 * schema that applies to properties at the json pointer pattern in a document that is
 * described by the schema. The mapped schemas will not contain any references.
 * @code{.json}
 *      {
 *          "/nodes/\d+/name": { 
 *             "type": "string" 
 *          },
 *          "/nodes/\d+/children" : { 
 *              "type": "array",
 *              "items": {
 *                  "$id": "glTFid.schema.json",
 *                  "title": "glTF Id",
 *                  "type": "integer",
 *                  "minimum": 0
 *              }
 *          },
 *          ...
 *      }
 * 
 * @endcode
 * @param schema JSON schema following JSON schema draft 7. All references ($ref) in the schema must
 * be resolved or they will also appear in the output json.
 * @return nlohmann::json A json object with json pointer patterns as keys and schemas
 * for the property at the json pointer path as values.
 */
nlohmann::json linearize(const nlohmann::json& schema);

/**
 * @brief Iterate through all properties in a json file that match the pattern.
 * 
 * @param pattern Pattern to match against.
 * @param json Json from which properties are matched against the pattern.
 */
void patternForEach(const std::string& pattern, const nlohmann::json& json, std::function<void(const nlohmann::json::json_pointer& pointer)> f);

/**
 * @brief Map valid pointers of a JSON structure to pointer patterns.
 * 
 * @param linearizedSchema A schema linearized with schema::linearize
 * @param json JSON structure to map pointers for
 * @return std::map<std::string, std::string> Mapping between pointers and their pattern
 */
std::map<std::string, std::string> mapPointersToPatterns(const nlohmann::json& linearizedSchema, const nlohmann::json& json);

/**
 * @brief Map pointer patterns to valid pointers in a json structure
 * 
 * @param linearizedSchema A schema linearized with schema::linearize
 * @param json JSON structure to map patterns to 
 * @return nlohmann::json JSON structure containing a mapping from pattern to a json array containing
 * the valid pointers in th json that fit the pattern
 */
nlohmann::json mapPatternsToPointers(const nlohmann::json& linearizedSchema, const nlohmann::json& json);

/**
 * @brief Map pointer of a JSON to the corresponding pointer pattern in the schema.
 * 
 * @param linearizedSchema A schema linearized with schema::linearize
 * @param path JSON pointer that should be matched with a pattern in linearizedSchema
 * @return std::string pointer pattern for the pointer
 */
std::string mapToPattern(const nlohmann::json& linearizedSchema, const std::string& pointer);
}
