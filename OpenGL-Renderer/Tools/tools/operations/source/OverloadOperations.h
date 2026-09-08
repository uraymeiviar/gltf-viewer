// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include "operations/Operations.h"

namespace ux3d::operations::overload {
Operation setTRSValue(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation joinPrimitives(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation resolveDraco(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation setTexCoord(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation setCameraOrthographicProperties(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation setCameraPerspectiveProperties(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation setMaterialExtensionsWithUnlitConflict(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema, const std::string& description, const std::string& detailedDescription);
Operation setMaterialUnlit(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema, const std::string& description, const std::string& detailedDescription);
Operation setMaterialExtensionsWithSpecGlossConflict(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema, const std::string& description, const std::string& detailedDescription);
Operation setMaterialSpecGloss(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema, const std::string& description, const std::string& detailedDescription);
Operation setAnimationSamplerReference(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation setExtensionUsed(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema, const std::string& description, const std::string& detailedDescription);
Operation applyOperationToArrayProperty(const std::string& overloadOperation, const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema, const std::string& propertyPatternPath, const std::string& description, const std::string& detailedDescription);
Operation setImageURI(const std::string& overloadOperation, const nlohmann::json& glTFSchema, const nlohmann::json& linearizedGltfSchema);
}
