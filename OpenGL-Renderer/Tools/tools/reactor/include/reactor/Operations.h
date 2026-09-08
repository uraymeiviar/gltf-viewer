// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include <nlohmann/json.hpp>

namespace ux3d::reactor::operations {

/**
 * @brief Transfom a library schema to an object with operation name
 * as key and an array as value with the possible parameter schemas as entries 
 * 
 * @param librarySchema Operation Library Schema
 * @return nlohmann::json Parameter schemas accessible by operation name
 */
nlohmann::json byName(const nlohmann::json& librarySchema);

/**
 * @brief Transform a library schema to an object mapping json pointer
 * path patterns to operation names. The path patterns correspond to
 * the json pointers that are accepted as "path" parameter in some operations.
 * Only operations that have such an input appear in the output of this function.
 * 
 * @param operationsByName Operations by name as returned by "byName(...)"
 * @return nlohmann::json Object mapping json pointer paths to operation names
 */
nlohmann::json byPathPattern(const nlohmann::json& operationsByName);

/**
 * @brief Returns a dict of operations that have no path
 *
 * @param operationsByPathPattern Operations by path pattern as returned by "operationsByPathPattern(...)"
 * @return nlohmann::json Object of all global operations
 */
nlohmann::json global(const nlohmann::json& operationsByName);

/**
 * @brief Returns a dict of operations that are available for this specific glTF path
 *
 * @param operationsByPathPattern Operations by path pattern as returned by "operationsByPathPattern(...)"
 * @return nlohmann::json Object of all operations
 */
nlohmann::json::object_t byPath(std::string path, const nlohmann::json& operationsByPathPattern);
}