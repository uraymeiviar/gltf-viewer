// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include "operations/Operations.h"

namespace ux3d::operations::dangerous {

extern const Operation setValue;
extern const Operation removeValue;
Operation forEach(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation forEachElement(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation forEachElementInArray(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation combineOperations(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);

}