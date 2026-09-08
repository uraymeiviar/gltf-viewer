// Copyright © 2022 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ux3d::reactor::json {
/**
 * @brief Iterate through all properties in a json file that match the pattern.
 *
 * @param pattern Pattern to match against.
 * @param json Json from which properties are matched against the pattern.
 */
void for_each_match(const std::string& pointerRegex, const nlohmann::json& json, std::function<void(const nlohmann::json::json_pointer& pointer)> f);
nlohmann::json keys(const nlohmann::json& json);
nlohmann::json values(const nlohmann::json& json);
nlohmann::json unzip(const nlohmann::json& json);
nlohmann::json minimal_diff(const nlohmann::json& left, const nlohmann::json& right, const std::string& path = "");
nlohmann::json filter(nlohmann::json&& json, const std::function<bool(const nlohmann::json&)> predicate);
nlohmann::json flatten_on(const nlohmann::json& json, const std::string& pointerRegex);

// If values are given for both, jsonBase and jsonMerge, the value of jsonBase is used
nlohmann::json merge(const nlohmann::json& jsonBase, const nlohmann::json& jsonMerge, bool ignoreConflicts = true);
nlohmann::json resolveAllOf(nlohmann::json json);

size_t findNth(const std::string& string, const std::string& searchString, size_t nth, size_t pos);
size_t pointerLength(const nlohmann::json::json_pointer& pointer);
nlohmann::json::json_pointer getPointerPrefix(const nlohmann::json::json_pointer& pointer, size_t numberOfElements);

// Return array of paths
std::vector<std::string> findValue(const nlohmann::json& json, nlohmann::json value);
bool arrayContains(const nlohmann::json& json, const nlohmann::json& value);
void erase(nlohmann::json& json, nlohmann::json::json_pointer path);
nlohmann::json tree_diff(const nlohmann::json& source, const nlohmann::json& target, const std::string& childrenKey = "children");
}
