// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include "datastorage/DataStorage.h"
#include "operations/Operations.h"
#include <functional>
#include <nlohmann/json-schema.hpp>
#include <rxcpp/rx.hpp>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace ux3d::operations {

class Library {
public:
    /**
     * @brief Construct the library and load the operations in it. This Constructor is quite heavy,
     * threfore ideally create a library once and use it throughout the existance of your program.
     *
     */
    Library();

    /**
     * @brief Execute an operation and return the modified state. This function will throw an exception if the
     * taskDescription is invalid (check the "schema" for what is expected) and if the operation encounters
     * a runtime error
     *
     * @param taskDescription for the operation that should be run on the state
     * @param state the state the operation should act upon. Will be modified inside the operation and returned.
     * @param dataStorage data storage containing for large buffers in the state. Might be modified by the operation
     * as a side effect.
     * @return nlohmann::json The state as modified by the operation.
     */
    nlohmann::json call(
        const nlohmann::json& taskDescription, nlohmann::json&& state, datastorage::DataStorage& dataStorage, std::function<void(const nlohmann::json&)> callback = [](auto&&...) {}, bool useGeneric = false) const;

    /**
     * @brief Load additional operations from outside of the operations module. This can for example be helpful to allow
     * definining editor operations with dependencies that don't suit the operations module.
     *
     * @param pluginOperations A vector of operations that will be added to the library. Overload operations need to be initialized before the generic version either by a previous loadPlugin call or by sorting the vector correctly.
     * The library schema will be extended with these operations as well.
     */
    void loadPlugin(const std::vector<Operation>& pluginOperations);

    /**
     * @brief A schema for the task description parameter of the "call" function.
     * This schema can be used to dynamically access the libraries' contents.
     */
    nlohmann::json schema;

    /**
     * @brief Reset library
     */
    void reset();

private:
    std::unordered_map<std::string, std::vector<Operation>> m_operationsMap;
    std::unordered_map<std::string, std::vector<nlohmann::json_schema::json_validator>> m_operationSchemas;
};

}
