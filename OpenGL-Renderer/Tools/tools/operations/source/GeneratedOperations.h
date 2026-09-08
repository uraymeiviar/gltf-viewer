// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include "operations/Operations.h"

namespace ux3d::operations::generated {

Operation setGltfValue(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation setGltfPointer(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation createGltfObject(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation createGltfComponent(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation removeGltfPointer(const nlohmann::json& glTFSchema, const nlohmann::json& linearizedGltfSchema);
Operation moveGltfPointer(const nlohmann::json& glTFSchema, const nlohmann::json& linearizedGltfSchema);
Operation insertGltfPointer(const nlohmann::json& glTFSchema, const nlohmann::json& linearizedGltfSchema);
Operation removeGltfPointerFromArray(const nlohmann::json& glTFSchema, const nlohmann::json& linearizedGltfSchema);
Operation moveGltfPointerFromArray(const nlohmann::json& glTFSchema, const nlohmann::json& linearizedGltfSchema);
Operation remove(const nlohmann::json& glTFSchema, const nlohmann::json& linearizedGltfSchema);
Operation reset(const nlohmann::json& glTFSchema, const nlohmann::json& linearizedGltfSchema);

}
