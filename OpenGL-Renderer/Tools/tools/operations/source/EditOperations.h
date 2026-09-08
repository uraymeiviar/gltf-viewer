// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include "operations/Operations.h"

namespace ux3d::operations::edit {

Operation mergeGltf(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation importAsset(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation blendImages(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation applyNodeTransform(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);

/**
 * @brief This function is only here due to an import bug caused by windows.h
 * assimp can't be used in FileOperations.cpp.
 * The bug: https://github.com/assimp/assimp/issues/2271
 * the fix: https://github.com/assimp/assimp/pull/4846
 * If you find this function and assimp has been updated to a version newer than 5.3.0 you can move this code back into
 * the load operation in FileOperations.cpp.
 *
 */
Operation load();
}
