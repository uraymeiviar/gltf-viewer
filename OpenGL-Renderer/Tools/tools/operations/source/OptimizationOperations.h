// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include "operations/Operations.h"

namespace ux3d::operations::optimization {

Operation applyDraco(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation applyDracoForEach(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation resolveDraco(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation resolveDracoForEach(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation removeRedundantDraco(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation clusterVertices(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation deinterleaveAccessor(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation normalizeWeightsAccessor(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation clampAccessor(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation convertImage();
Operation convertImageForEach();
Operation changeTRS(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation changeTRSForAll(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation joinPrimitives(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation narrowIndexType(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation convertSpecularGlossinessToMetallicRoughness(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation reduceTriangleCount();
Operation resolveMeshQuantizationForAll(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation resolveMeshQuantization(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation optimizeAnimation(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
}
