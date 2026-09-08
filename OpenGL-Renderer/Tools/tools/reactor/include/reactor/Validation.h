// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include <nlohmann/json.hpp>

#include "datastorage/DataStorage.h"

namespace ux3d::reactor::validation {

/**
 * @brief Validate a glTF
 *
 * @param state A gltf json
 * @return nlohmann::json A validation report containing errors, warnings and hints
 */
nlohmann::json validate(const nlohmann::json& gltf);
nlohmann::json validate(const std::string& path, bool checkResources = true);

void sanitize(nlohmann::json& input);
}
