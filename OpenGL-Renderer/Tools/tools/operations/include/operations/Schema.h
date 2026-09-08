// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝
#pragma once

#include <nlohmann/json.hpp>

#include <map>
#include <optional>

// The schema namespace contains convenience templates to create JSON Schemas in
// C++ for use in the definition of the operations signature. You can combine schema templates
// to complex signature definitions.
// E.g.
// signature({ {"dividend", number() }, {"divisor", number() } })
// could be used to define the signature of a "divide" operation.

namespace ux3d::operations::schema {

nlohmann::json anyOf(const nlohmann::json schemaArray);
nlohmann::json oneOf(const nlohmann::json schemaArray);
nlohmann::json pointer(const std::string& pattern);
nlohmann::json enumeration(const nlohmann::json enumeration, const std::optional<nlohmann::json> defaultValue = std::nullopt);
nlohmann::json enumerationSet(const nlohmann::json& set);
nlohmann::json uuid();
nlohmann::json string(const std::string& pattern = "", const std::optional<std::string> defaultValue = std::nullopt);
nlohmann::json number(const std::optional<double> defaultValue = std::nullopt, const std::optional<double> minimum = std::nullopt, const std::optional<double> maximum = std::nullopt, bool exclusiveMinimum = false, bool exclusiveMaximum = false);
nlohmann::json integer(const std::optional<int> defaultValue = std::nullopt, const std::optional<int> minimum = std::nullopt, const std::optional<int> maximum = std::nullopt);
nlohmann::json boolean(const std::optional<bool> defaultValue = std::nullopt);
nlohmann::json any();
nlohmann::json array(const nlohmann::json& items, std::optional<uint32_t> minItems = 1, std::optional<uint32_t> maxItems = std::nullopt);
nlohmann::json set(const nlohmann::json& items);
nlohmann::json object(const std::map<std::string, nlohmann::json>& requiredKeys, const std::map<std::string, nlohmann::json>& optionalKeys = {}, bool additionalProperties = false);
nlohmann::json operation(const std::string& title, const std::string& description, const nlohmann::json& schema);
nlohmann::json properties(const nlohmann::json& properties);

namespace parameter {
    /**
     * @brief JSON key by which the path parameter can be accessed.
     *
     */
    constexpr auto pathKey = "path";

    /**
     * @brief Create a path parameter schema. This parameter is used to identify
     * the "self" path of this operation, i.e. the operation could be considered
     * a member function of any json property at the json pointer path pattern.
     * There must only be one path parameter in an operation signature.
     *
     * @param pattern json pointer pattern of the path parameter
     * @return nlohmann::json schma of the path parameter. Should be passed into a "signature" schema of an operation.
     */
    nlohmann::json path(const std::string& pattern, const std::string& key = pathKey);

    nlohmann::json uuid();
}

}
