// Copyright © 2022 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include "datastorage/DataStorage.h"
#include "reactor/GltfEnums.h"
#include "reactor/ImageOperations.h"
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <variant>
#include <vec2.hpp>
#include <vec3.hpp>
#include <vec4.hpp>
#include <vector>

namespace ux3d::reactor::utils {

// maps carrying attribute names and all combinations of component types and normalization that is allowed by KHR_mesh_quantization
const std::map<std::string, std::vector<std::tuple<gltf::ComponentType, bool>>> meshQuantizationTypes = {
    { "POSITION", { { gltf::ComponentType::Byte, false }, { gltf::ComponentType::Byte, true }, { gltf::ComponentType::UnsignedByte, false }, { gltf::ComponentType::UnsignedByte, true }, { gltf::ComponentType::Short, false }, { gltf::ComponentType::Short, true }, { gltf::ComponentType::UnsignedShort, false }, { gltf::ComponentType::UnsignedShort, true } } },
    { "NORMAL", { { gltf::ComponentType::Byte, true }, { gltf::ComponentType::Short, true } } },
    { "TANGENT", { { gltf::ComponentType::Byte, true }, { gltf::ComponentType::Short, true } } },
    { "TEXCOORD", { { gltf::ComponentType::Byte, false }, { gltf::ComponentType::Byte, true }, { gltf::ComponentType::UnsignedByte, false }, { gltf::ComponentType::Short, false }, { gltf::ComponentType::Short, true }, { gltf::ComponentType::UnsignedShort, false } } }
};
const std::map<std::string, std::vector<std::tuple<gltf::ComponentType, bool>>> meshQuantizationTypesMorph = {
    { "POSITION", { { gltf::ComponentType::Byte, false }, { gltf::ComponentType::Byte, true }, { gltf::ComponentType::Short, false }, { gltf::ComponentType::Short, true } } },
    { "NORMAL", { { gltf::ComponentType::Byte, true }, { gltf::ComponentType::Short, true } } },
    { "TANGENT", { { gltf::ComponentType::Byte, true }, { gltf::ComponentType::Short, true } } },
    { "TEXCOORD", { { gltf::ComponentType::Byte, false }, { gltf::ComponentType::Short, false } } }
};

// String helper functions
std::vector<std::string> splitString(std::string s, const std::string& delimiter);
std::string mergeString(const std::vector<std::string>& tokens, const std::string& separator);
void searchAndReplace(std::string& str, const std::string& search, const std::string& replace);

// Json helper functions
bool hasElement(const nlohmann::json& json, std::vector<std::variant<std::string, int>> path);
void createSubElement(nlohmann::json& json, std::vector<std::variant<std::string, int>> path);
void createSubElement(nlohmann::json& json, std::string path);

// Data handling helper functions
bool isHashedDataUri(std::string uri);
bool isDataUri(std::string uri);
std::tuple<std::string, std::string> getUUIDLocalPath(nlohmann::json::json_pointer pointer, const nlohmann::json& stateWithUUIDs);
nlohmann::json::json_pointer getDataIDPath(nlohmann::json::json_pointer propertyPath, const nlohmann::json& state);
nlohmann::json::json_pointer getDataIDPath(const nlohmann::json& property, const nlohmann::json& state);
void updateBufferData(nlohmann::json::json_pointer bufferPath, std::vector<uint8_t>& data, nlohmann::json& state, datastorage::DataStorage& datastorage);
std::vector<uint8_t> getBufferViewData(nlohmann::json::json_pointer bufferViewPath, const nlohmann::json& state, const datastorage::DataStorage& datastorage);
std::vector<uint8_t> getAccessorByteDataPacked(nlohmann::json::json_pointer accessorPath, const nlohmann::json& state, const datastorage::DataStorage& datastorage);
std::vector<uint8_t> getImageData(nlohmann::json::json_pointer imagePath, const nlohmann::json& state, const datastorage::DataStorage& datastorage);
image::ImageData decodeImageData(nlohmann::json::json_pointer imagePath, const nlohmann::json& state, const datastorage::DataStorage& datastorage);
void setImageData(nlohmann::json::json_pointer imagePath, const image::ImageData& imageData, nlohmann::json& state, datastorage::DataStorage& datastorage);

nlohmann::json::json_pointer createBuffer(nlohmann::json& state, datastorage::DataStorage& datastorage, const std::vector<uint8_t>& data, bool createEmbedded = false, std::string uri = "");
nlohmann::json::json_pointer createBufferView(nlohmann::json& state, nlohmann::json::json_pointer bufferPath);
nlohmann::json::json_pointer appendBufferView(nlohmann::json::json_pointer bufferPath, const std::vector<uint8_t>& data, nlohmann::json& state, datastorage::DataStorage& datastorage);

std::string getRandomString();
bool isURIUsed(std::string uri, const nlohmann::json& state);
bool isURIStemUsed(std::string uriStem, const nlohmann::json& state);
std::string createURIStem(const nlohmann::json& state);

size_t getAccessorByteSize(nlohmann::json::json_pointer accessorPath, const nlohmann::json& state);

template <typename T>
std::vector<T> getAccessorData(
    nlohmann::json::json_pointer accessorPath,
    const nlohmann::json& state,
    const datastorage::DataStorage& datastorage);
typedef std::variant<
    std::monostate,
    std::vector<uint8_t>,
    std::vector<uint16_t>,
    std::vector<uint32_t>,
    std::vector<float>,
    std::vector<glm::vec2>,
    std::vector<glm::vec3>,
    std::vector<glm::vec4>>
    AccessorTypeVariant;
utils::AccessorTypeVariant getAccessorData(nlohmann::json::json_pointer accessorPath, const nlohmann::json& state, const datastorage::DataStorage& datastorage);

// This function creates an accessor that matches the data format (vec3<float>,..).
// To properly store the data, bufferView and buffer are created and referenced accordingly
template <typename T>
nlohmann::json::json_pointer createAccessorFromData(const std::vector<T>& data, nlohmann::json& state, datastorage::DataStorage& datastorage);
nlohmann::json::json_pointer createAccessorFromData(
    std::string attributeType,
    gltf::ComponentType componentType,
    uint32_t count,
    const std::vector<uint8_t>& data,
    nlohmann::json& state, datastorage::DataStorage& datastorage);

template <typename T>
inline nlohmann::json::json_pointer createAccessorFromData(const std::vector<T>& data, nlohmann::json& state, datastorage::DataStorage& datastorage)
{
    if (data.empty()) {
        throw std::invalid_argument("No data given");
    }
    std::string attributeType = "";
    gltf::ComponentType componentType;
    if constexpr (std::is_same_v<T, float>) {
        attributeType = "SCALAR";
        componentType = gltf::ComponentType::Float;
    } else if (std::is_same_v<T, glm::vec2>) {
        attributeType = "VEC2";
        componentType = gltf::ComponentType::Float;
    } else if (std::is_same_v<T, glm::vec3>) {
        attributeType = "VEC3";
        componentType = gltf::ComponentType::Float;
    } else if (std::is_same_v<T, glm::vec4>) {
        attributeType = "VEC4";
        componentType = gltf::ComponentType::Float;
    } else if (std::is_same_v<T, uint32_t>) {
        attributeType = "SCALAR";
        componentType = gltf::ComponentType::UnsignedInt;
    } else if (std::is_same_v<T, uint16_t>) {
        attributeType = "SCALAR";
        componentType = gltf::ComponentType::UnsignedShort;
    } else if (std::is_same_v<T, uint8_t>) {
        attributeType = "SCALAR";
        componentType = gltf::ComponentType::UnsignedByte;
    } else {
        throw std::invalid_argument("Unsupported type");
    }
    uint32_t count = data.size();
    std::vector<uint8_t> byteData;
    byteData.resize(count * sizeof(T));
    memcpy(&byteData[0], data.data(), byteData.size());
    return reactor::utils::createAccessorFromData(attributeType, componentType, count, byteData, state, datastorage);
}

template <typename T>
inline std::vector<T> getAccessorData(nlohmann::json::json_pointer accessorPath, const nlohmann::json& state, const datastorage::DataStorage& datastorage)
{
    std::vector<uint8_t> byteData = getAccessorByteDataPacked(accessorPath, state, datastorage);
    std::vector<T> typedVector;
    typedVector.resize(byteData.size() / sizeof(T));
    memcpy(typedVector.data(), byteData.data(), byteData.size());
    return typedVector;
}

std::vector<nlohmann::json::json_pointer> getReferencedAccessors(const nlohmann::json& primitive);
/**
 * @brief Searches through the prmitive index and attribute accessors and replaces the oldAccessorPath with the newAccessorPath.
 * This function does not check if the provided paths are accessors.
 * This function is only interested in the last pointer in the accessor path (i.e. the 8 in /accessors/8).
 * Therefore, both paths can either be /gltf/accessors/8 or /accessors/8.
 *
 * @param oldAccessorPath THIS HAS TO BE AN ACCESSOR PATH
 * @param newAccessorPath THIS HAS TO BE AN ACCESSOR PATH
 * @param primitive
 */
void replaceAccessorInPrimitive(const int oldAccessorIDX, const int newAccessorIDX, nlohmann::json& state, const nlohmann::json::json_pointer& primitivePath);
std::vector<std::tuple<nlohmann::json::json_pointer, std::string>> getMeshQuantizationAccessors(const nlohmann::json& state);

/**
 * @brief Extracts the image datastorage uri for the given image pointer. (also works for textures, if the texture has a source)
 * returns an empty string if no image data is found or on error.
 *
 * @param pointer pointer to either an image or a texture eg. /images/0 or /textures/0
 * @param data
 * @param headPtr
 * @param datastorage
 * @return std::string image datastorage uri (or empty on error)
 */
nlohmann::json extractImagePreviewFromData(const nlohmann::json::json_pointer& pointer, const std::shared_ptr<const nlohmann::json>& headPtr, const datastorage::DataStorage& datastorage);
}
