// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#pragma once

#include <filesystem>
#include <optional>

#include <gtc/quaternion.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vec3.hpp>

namespace ux3d::datastorage {
class DataStorage;
}

namespace ux3d::operations {
class Library;
}

namespace ux3d::operations::file {

nlohmann::json loadGltf(const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library);
nlohmann::json loadGlb(const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library);
nlohmann::json saveGltf(const nlohmann::json& params, nlohmann::json state, datastorage::DataStorage& datastorage, const Library& library);
nlohmann::json saveGlb(const nlohmann::json& params, nlohmann::json state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback);

std::vector<uint8_t> readFile(std::filesystem::path filepath);
void loadBufferDataToDataStorage(datastorage::DataStorage& datastorage, nlohmann::json& state, std::filesystem::path gltfDir, std::optional<std::filesystem::path> resourceDir = std::nullopt);
void loadImageDataToDataStorage(datastorage::DataStorage& datastorage, nlohmann::json& state, std::filesystem::path gltfDir, std::optional<std::filesystem::path> resourceDir = std::nullopt);
void writeFile(std::filesystem::path filepath, const std::vector<uint8_t>& data, bool overwrite);
void writeFile(std::filesystem::path filepath, const std::string& data, bool overwrite);
uint64_t getDataSize(datastorage::DataStorage& datastorage, nlohmann::json& state);
nlohmann::json writeBufferDataToFile(datastorage::DataStorage& datastorage, nlohmann::json& state, std::filesystem::path gltfDir, bool overwrite);
nlohmann::json writeImageDataToFile(datastorage::DataStorage& datastorage, nlohmann::json& state, std::filesystem::path gltfDir, bool overwrite);
void convertGlbBuffer(nlohmann::json& state, datastorage::DataStorage& datastorage);
void substituteEmbeddedMarkers(nlohmann::json& state, datastorage::DataStorage& datastorage);
void markSubtreeObjects(const nlohmann::json& linearizedSchema, nlohmann::json& gltf, nlohmann::json::json_pointer subtreePointer, bool isRootObject = false);
nlohmann::json reduceGltfToSubtreeObjects(nlohmann::json&& state, datastorage::DataStorage& datastorage, const nlohmann::json& linearizedGltfSchema, const Library& library, std::function<void(const nlohmann::json&)> callback);
bool pointsToChildOfRoot(const std::string& pathToArray, const nlohmann::json& linearizedGltfSchema);

std::string normalizeURI(std::string uri);
}

namespace ux3d::operations::utility {
glm::dquat eulerToQuaternion(glm::dvec3 euler);
glm::dvec3 quaternionToEuler(glm::dquat quaternion);

/// Pushes `extension` to `extensionsUsed` and if required, also pushes to `extensionsRequired`.
/// Does not push if these arrays already include the extension.
void useExtension(nlohmann::json& state, const std::string& extension, bool required);
void removeExtensionFlag(nlohmann::json& state, const std::string& extension);
void getExtensionsAtPath(nlohmann::json::json_pointer path, const nlohmann::json& state, std::vector<std::string>& foundExtensions, std::map<std::string, std::vector<nlohmann::json::json_pointer>>* extensionPaths = nullptr);
}

namespace ux3d::operations::mesh {
nlohmann::json::json_pointer getAccessor(nlohmann::json& attribute);
}

namespace glm {

// Implement implicit conversions from and to `nlohmann::json` for `glm` vectors, matrices, and quaternions
// generic across their dimensionality, scalar type, and precision.

// The function signatures must be the way they are and they must reside inside the `glm` namespace,
// otherwise `nlohmann::json` will not find them.

// https://github.com/nlohmann/json#arbitrary-types-conversions

template <glm::length_t L, typename T, glm::qualifier Q>
void to_json(nlohmann::json& j, const glm::vec<L, T, Q>& v)
{
    std::vector<T> array;
    array.resize(L);
    for (glm::length_t i = 0; i < L; ++i) {
        array[i] = v[i];
    }
    j = std::move(array);
}

template <glm::length_t L, typename T, glm::qualifier Q>
void from_json(const nlohmann::json& j, glm::vec<L, T, Q>& v)
{
    for (glm::length_t i = 0; i < L; ++i) {
        v[i] = j[i];
    }
}

template <glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
void to_json(nlohmann::json& j, const glm::mat<C, R, T, Q>& m)
{
    std::vector<T> array;
    array.resize(C * R);
    for (glm::length_t c = 0; c < C; ++c) {
        for (glm::length_t r = 0; r < R; ++r) {
            array[c * R + r] = m[c][r];
        }
    }
    j = std::move(array);
}

template <glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
void from_json(const nlohmann::json& j, glm::mat<C, R, T, Q>& m)
{
    for (glm::length_t c = 0; c < C; ++c) {
        for (glm::length_t r = 0; r < R; ++r) {
            m[c][r] = j[c * R + r];
        }
    }
}

template <typename T, glm::qualifier Q>
void to_json(nlohmann::json& j, const glm::qua<T, Q>& q)
{
    j = nlohmann::json::array({ q.x, q.y, q.z, q.w });
}

template <typename T, glm::qualifier Q>
void from_json(const nlohmann::json& j, glm::qua<T, Q>& q)
{
    q.x = j[0];
    q.y = j[1];
    q.z = j[2];
    q.w = j[3];
}

}
