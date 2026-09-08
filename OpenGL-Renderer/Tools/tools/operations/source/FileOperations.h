// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include "filesystem/FileSystem.h"
#include "operations/Operations.h"

namespace ux3d::operations::file {
Operation save();
Operation saveSelection(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation exportAccessor(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation importImage(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation importTexture(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation exportImage(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation reloadFile(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
extern Operation exportUsdz;
}
