// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include "operations/Operations.h"

namespace ux3d::operations::gltf {

extern const Operation addNode;
extern const Operation addNewNode;
extern const Operation changeCameraType;
extern const Operation createScene;
Operation createCameraFromView();
extern const Operation removeNodeReference;
extern const Operation createMesh;
extern const Operation createAnimation;
Operation resetGltf(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation mergeBuffers(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation trimBuffer(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation clone(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation cleanAll(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation calculateMinMax(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation cleanTexCoordAttributes();
Operation createTexCoordAttribute();
Operation deleteAttribute();
Operation cleanNodes();
Operation removeProperties();
Operation removeKnownExtensions(const nlohmann::json& knownExtensionsArray);
Operation removeUnknownExtensions(const nlohmann::json& knownExtensionsArray);
Operation convertPrimitivesToMeshes();
Operation joinMeshes(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation transformNode();
Operation createEmpty();
Operation createLight();
Operation reassignSkinnedAnimation();
Operation deleteAccessor(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
extern const Operation resolveSparseAccessor;
Operation deleteBufferView(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation updateExtensionFlags(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation createTangents(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation addMaterialVariantToPrimitive(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation removeMaterialVaraiantFromPrimitive(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);
Operation replaceMaterial(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema);

// helper functions:
const std::vector<uint8_t>& getBufferData(std::string fileID, const nlohmann::json& state, const datastorage::DataStorage& datastorage);
nlohmann::json::json_pointer cloneProperty(const nlohmann::json::json_pointer path, nlohmann::json& state, datastorage::DataStorage& datastorage, const Library& library, bool clonemodeShallow, const nlohmann::json& linearizedGltfSchema, std::function<void(const nlohmann::json&)> callback);
void addNodeHelper(nlohmann::json& state, nlohmann::json::json_pointer pathTarget, uint32_t nodeID);
nlohmann::json::json_pointer getParent(nlohmann::json::json_pointer propertyPath, const nlohmann::json& state, bool resultIsRequired = true);

/**
 * @brief removes all properties provided in the params from the gltf.
 *
 * @param gltf
 * @param params
 * @param removePropertyNameFromArrays If set to true every occurance of all provided property names will be removed from arrays.
 * (necessary for extension removal to remove extension name from extensionsUsed array.)
 */
size_t removePropertiesFromGltf(nlohmann::json& gltf, const nlohmann::json& params, bool removePropertyNameFromArrays = false);
/**
 * @brief removes all provided properties from the gltf.
 *
 * @param gltf
 * @param properties
 * @param removePropertyNameFromArrays If set to true every occurance of all provided property names will be removed from arrays.
 * (necessary for extension removal to remove extension name from extensionsUsed array.)
 */
size_t removePropertiesHelper(nlohmann::json& gltf, const std::vector<std::string>& properties, bool removePropertyNameFromArrays = false);
}
