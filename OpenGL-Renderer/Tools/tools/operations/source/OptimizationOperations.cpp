// Copyright © 2022 UX3D GmbH. All rights reserved.

#include "OptimizationOperations.h"
#include "AnimationTools.h"
#include "DracoDecoder.h"
#include "DracoEncoder.h"
#include "HelperFunctions.h"
#include "MeshQuantizationResolver.h"
#include "StorageOperations.h"
#include "VertexClustering.h"
#include "datastorage/DataStorage.h"
#include "operations/Library.h"
#include "operations/Operations.h"
#include "operations/Schema.h"
#include "reactor/GltfEnums.h"
#include "reactor/ImageOperations.h"
#include "reactor/Reactor.h"
#include "reactor/Schema.h"
#include "reactor/Utils.h"

#include <gltf_schema/gltf_single_json.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <string>

#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include <gtx/matrix_decompose.hpp>
#include <gtx/string_cast.hpp>

#include "fast_quadric_mesh_simplification/Simplify.h"

#include <fmt/format.h>

using namespace ux3d;
using namespace ux3d::operations;
using namespace std::string_literals;

namespace ux3d::operations::optimization {

bool isInterleaved(nlohmann::json& state, nlohmann::json::json_pointer path);

}

bool optimization::isInterleaved(nlohmann::json& state, nlohmann::json::json_pointer path)
{
    if (!state.contains(path)) {
        return false;
    }

    auto& accessor = state.at(path);

    if (!accessor.contains("bufferView")) {
        return false;
    }
    nlohmann::json::json_pointer bufferViewpath("/gltf/bufferViews");
    bufferViewpath = bufferViewpath / accessor.at("bufferView").get<uint32_t>();

    if (!state.contains(bufferViewpath)) {
        return false;
    }

    auto& bufferView = state.at(bufferViewpath);

    if (!bufferView.contains("byteStride")) {
        return false;
    }

    uint32_t bufferViewStride = bufferView.at("byteStride").get<uint32_t>();
    size_t expectedStride = reactor::gltf::elementSizePadded(accessor);

    if (bufferViewStride == static_cast<uint32_t>(expectedStride)) {
        return false;
    }
    return true;
}

Operation optimization::deinterleaveAccessor(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "deinterleave_accessor";

    operation.schema = schema::operation(
        "Deinterleave Accessor",
        "The data of the Accessor is reordered so that it is tightly packed",
        schema::object({ schema::parameter::path("/gltf/accessors/\\d+") }));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params["path"].get<std::string>());
        if (!isInterleaved(state, path)) {
            return state;
        }

        auto& accessor = state.at(path);

        nlohmann::json::json_pointer bufferViewpath("/gltf/bufferViews");
        bufferViewpath = bufferViewpath / accessor.at("bufferView").get<uint32_t>();

        if (!state.contains(bufferViewpath)) {
            throw std::invalid_argument(std::string("bufferView not found"));
        }

        auto& oldBufferView = state.at(bufferViewpath);
        if (!oldBufferView.contains("buffer")) {
            throw std::invalid_argument(std::string("buffer not found"));
        }

        auto bufferPath = nlohmann::json::json_pointer("/gltf/buffers") / oldBufferView.at("buffer").get<uint32_t>();
        if (!state.contains(bufferPath)) {
            throw std::invalid_argument(std::string("buffer not found"));
        }

        auto& oldBuffer = state.at(bufferPath);

        uint32_t bufferViewStride = oldBufferView["byteStride"];
        uint32_t accessorStride = reactor::gltf::elementSizePadded(accessor);
        uint32_t accessorOffset = 0;
        if (accessor.contains("byteOffset")) {
            accessorOffset = accessor.at("byteOffset").get<uint32_t>();
        }

        if (bufferViewStride < accessorStride) {
            throw std::invalid_argument(std::string("Invalid stride"));
        }

        std::vector<uint8_t> newDataVector;
        newDataVector.resize(accessor.at("count").get<uint32_t>() * accessorStride);

        std::vector<uint8_t> bufferViewData = reactor::utils::getBufferViewData(bufferViewpath, state, datastorage);

        for (size_t i = 0; i < accessor.at("count").get<uint32_t>(); ++i) {
            for (size_t j = 0; j < accessorStride; ++j) {
                size_t bufferViewIndex = accessorOffset + i * bufferViewStride + j;
                size_t accessorIndex = i * accessorStride + j;
                newDataVector[accessorIndex] = bufferViewData[bufferViewIndex];
            }
        }
        auto newBufferPath = reactor::utils::createBuffer(state, datastorage, newDataVector);
        auto newBufferViewPath = reactor::utils::createBufferView(state, newBufferPath);
        state.at(newBufferViewPath)["byteStride"] = accessorStride;
        accessor["bufferView"] = std::stoi(newBufferViewPath.back());
        accessor["byteOffset"] = 0;

        return state;
    };

    return operation;
}

std::map<std::string, nlohmann::json> compressionSettingsSignature = {
    { "keepUncompressedData", schema::boolean(false) },
    { "targetCompressionLevel", schema::integer(7, 0, 10) },
    { "sequentialEncoding", schema::boolean(false) },
    { "quantizationBitsPosition", schema::integer(11, 1) },
    { "quantizationBitsNormal", schema::integer(8, 1) },
    { "quantizationBitsUv", schema::integer(10, 1) },
    { "quantizationBitsColor", schema::integer(8, 1) },
    { "quantizationBitsGeneric", schema::integer(8, 1) },
};

Operation optimization::applyDraco(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    auto childOfRoots = reactor::gltf::schema::filterChildOfRootProperties(linearizedGltfSchema);

    Operation operation;
    operation.name = "apply_draco";

    operation.schema = schema::operation(
        "Apply Draco Mesh Compression",
        "Apply draco mesh compression on a primitive with given compression settings",
        schema::object({ schema::parameter::path("/gltf/meshes/\\d+/primitives/\\d+") },
            compressionSettingsSignature // optional
            ));

    operation.operation = [childOfRoots, linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params["path"].get<std::string>());
        auto primitive = state.at(path);
        std::vector<nlohmann::json::json_pointer> accessors = reactor::utils::getReferencedAccessors(primitive);

        for (auto accessorPath : accessors) {
            auto& accessor = state.at(accessorPath);
            if (accessor.contains("sparse")) {
                throw std::invalid_argument("Unable to handle sparse accessors");
            }
        }

        for (auto accessorPath : accessors) {
            auto accessorSubPath = accessorPath.to_string();
            accessorSubPath.erase(accessorSubPath.begin(), accessorSubPath.begin() + std::string("/gltf").length());

            auto backReferences = reactor::getBackwardReferenceProperties(state.at("gltf"), nlohmann::json::json_pointer(accessorSubPath), linearizedGltfSchema);
            if (backReferences.size() > 1) {
                auto taskDescription = nlohmann::json({ { "clone_property", nlohmann::json::object() } });
                taskDescription["clone_property"]["path"] = accessorPath;
                taskDescription["clone_property"]["cloneModeShallow"] = true;
                taskDescription["clone_property"]["target"] = "parent_optional";
                state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);

                const int tmp = state["gltf"]["accessors"].size() - 1;
                const nlohmann::json::json_pointer clonedAccessorPath = nlohmann::json::json_pointer("/accessors") / tmp;

                reactor::utils::replaceAccessorInPrimitive(std::stoi(accessorPath.back()), std::stoi(clonedAccessorPath.back()), state, path);

                auto backReferences
                    = reactor::getBackwardReferenceProperties(state.at("gltf"), nlohmann::json::json_pointer(clonedAccessorPath), linearizedGltfSchema);
                if (backReferences.size() > 1) {
                    throw std::invalid_argument("Accessor used by other properties");
                }
            }
        }

        for (auto accessorPath : accessors) {
            auto taskDescription = nlohmann::json({ { "deinterleave_accessor", nlohmann::json::object() } });
            taskDescription["deinterleave_accessor"]["path"] = accessorPath.to_string();
            state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
        }

        encoder::draco(params, state, datastorage, library, callback);

        // Recalculating min/max of accessors has to be done as draco compression changes values
        // For this, we need to decode the newly compressed data and perform the check on that data
        auto stateCopy = state;
        auto decodeParameters = nlohmann::json::object();
        decodeParameters["path"] = path;
        decodeParameters["deleteEncodedData"] = false;

        stateCopy = decoder::draco(decodeParameters, std::move(stateCopy), datastorage, library, callback);

        auto accessorPathCopy = nlohmann::json::json_pointer("/gltf/accessors") / stateCopy.at(path)["attributes"]["POSITION"].get<uint32_t>();

        auto taskDescription = nlohmann::json({ { "calculate_min_max", nlohmann::json::object() } });
        taskDescription["calculate_min_max"]["path"] = accessorPathCopy.to_string();

        stateCopy = library.call(std::move(taskDescription), std::move(stateCopy), datastorage, callback);

        auto accessorPath = nlohmann::json::json_pointer("/gltf/accessors") / state.at(path)["attributes"]["POSITION"].get<uint32_t>();
        state.at(accessorPath)["min"] = stateCopy.at(accessorPathCopy)["min"];
        state.at(accessorPath)["max"] = stateCopy.at(accessorPathCopy)["max"];

        // In case we want to keep uncompressed data, we just use the state that includes both
        if (params.contains("keepUncompressedData") && params["keepUncompressedData"].get<bool>() == true) {
            state = stateCopy;
        }

        // Add extension to used and required:
        if (!state.at("gltf").contains("extensionsUsed")) {
            state.at("gltf")["extensionsUsed"] = nlohmann::json::array();
        }
        if (!state.at("gltf").contains("extensionsRequired")) {
            state.at("gltf")["extensionsRequired"] = nlohmann::json::array();
        }

        auto containsValue = [&](const auto& jsonArray, const std::string value) -> bool {
            bool containsValue = false;
            for (auto& elm : jsonArray) {
                if (elm.template get<std::string>() == value) {
                    containsValue = true;
                }
            }
            return containsValue;
        };
        if (!containsValue(state.at("gltf")["extensionsRequired"], "KHR_draco_mesh_compression")) {
            state.at("gltf")["extensionsRequired"].push_back("KHR_draco_mesh_compression");
        }
        if (!containsValue(state.at("gltf")["extensionsUsed"], "KHR_draco_mesh_compression")) {
            state.at("gltf")["extensionsUsed"].push_back("KHR_draco_mesh_compression");
        }

        return state;
    };

    return operation;
}

Operation optimization::applyDracoForEach(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "apply_draco_for_each";

    operation.schema = schema::operation(
        "Apply Draco Mesh Compression for Each Primitive",
        "Apply draco mesh compression to each primitive with given compression settings",
        schema::object({}, { compressionSettingsSignature }));

    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto taskDescription = nlohmann::json({ { "dangerous.forEach", nlohmann::json::object() } });
        taskDescription["dangerous.forEach"]["operation"] = "apply_draco";
        taskDescription["dangerous.forEach"]["pathPattern"] = "/gltf/meshes/\\d+/primitives/\\d+";
        taskDescription["dangerous.forEach"]["parameters"] = params;

        return library.call(std::move(taskDescription), std::move(state), datastorage, callback);
    };

    return operation;
}

Operation optimization::resolveDraco(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "resolve_draco";

    operation.schema = schema::operation(
        "Resolve Draco Mesh Compression",
        "Decodes the compressed primitive and removes the draco mesh compression extension from it",
        schema::object({ schema::parameter::path("/gltf/meshes/\\d+/primitives/\\d+") }, { { "deleteEncodedData", schema::boolean(true) }, { "cleanExtensionsUsed", schema::boolean(false) } }));

    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params["path"].get<std::string>());

        if (!state.contains(path)) {
            throw std::invalid_argument(std::string("primitive not found"));
        }

        state = decoder::draco(params, std::move(state), datastorage, library, callback);

        if (params.contains("cleanExtensionsUsed") && params["cleanExtensionsUsed"].get<bool>() == true) {
            auto taskDescription = nlohmann::json({ { "update_extension_flags", { { "extensions", { "KHR_draco_mesh_compression" } } } } });
            state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
        }

        return state;
    };

    return operation;
}

Operation optimization::resolveDracoForEach(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "resolve_draco_for_each";

    operation.schema = schema::operation(
        "Resolve Draco Mesh Compression for Each Primitive",
        "Decodes all compressed primitives and removes the draco mesh compression extension from them",
        schema::object({}, { { "deleteEncodedData", schema::boolean(true) }, { "cleanExtensionsUsed", schema::boolean(true) } }));

    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        nlohmann::json paramsCopy = params;
        // do not clean extensions after each mesh
        paramsCopy["cleanExtensionsUsed"] = false;

        auto taskDescription = nlohmann::json({ { "dangerous.forEach", nlohmann::json::object() } });
        taskDescription["dangerous.forEach"]["operation"] = "resolve_draco";
        taskDescription["dangerous.forEach"]["pathPattern"] = "/gltf/meshes/\\d+/primitives/\\d+";
        taskDescription["dangerous.forEach"]["parameters"] = paramsCopy;

        state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);

        if (params.contains("cleanExtensionsUsed") && params["cleanExtensionsUsed"].get<bool>() == true) {
            auto cleanupTaskDescription = nlohmann::json({ { "update_extension_flags", { { "extensions", { "KHR_draco_mesh_compression" } } } } });
            state = library.call(std::move(cleanupTaskDescription), std::move(state), datastorage, callback);
        }
        return state;
    };

    return operation;
}

Operation optimization::removeRedundantDraco(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "remove_redundant_draco";

    operation.schema = schema::operation(
        "Remove Redundant Draco",
        "Remove decoded primitive data if draco encoded data is redundantly stored",
        schema::object({}));

    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        if (!state["gltf"].contains("meshes")) {
            return state;
        }

        auto usedBufferViews = nlohmann::json::array();
        for (auto& mesh : state["gltf"].at("meshes")) {
            for (auto& primitive : mesh.at("primitives")) {
                if (!primitive.contains(nlohmann::json::json_pointer("/extensions/KHR_draco_mesh_compression"))) {
                    continue;
                }
                // check attributes
                for (auto& [attributeName, attribute] : primitive.at("attributes").items()) {
                    nlohmann::json::json_pointer accessorPath("/gltf/accessors");
                    uint32_t idx = attribute.get<uint32_t>();
                    auto& accessor = state.at(accessorPath / idx);
                    if (accessor.contains("bufferView")) {
                        usedBufferViews.push_back(nlohmann::json::json_pointer("/gltf/bufferViews") / accessor["bufferView"].get<uint64_t>());
                        accessor.erase("bufferView");
                    }
                    if (accessor.contains("byteOffset")) {
                        accessor.erase("byteOffset");
                    }
                }
                // check index buffer
                if (primitive.contains("indices")) {
                    nlohmann::json::json_pointer accessorPath("/gltf/accessors");
                    uint32_t idx = primitive.at("indices").get<uint32_t>();
                    auto& accessor = state.at(accessorPath / idx);
                    if (accessor.contains("bufferView")) {
                        usedBufferViews.push_back(nlohmann::json::json_pointer("/gltf/bufferViews") / accessor["bufferView"].get<uint64_t>());
                        accessor.erase("bufferView");
                    }
                    if (accessor.contains("byteOffset")) {
                        accessor.erase("byteOffset");
                    }
                }
            }
        }
        if (!usedBufferViews.empty()) { // Remove redundant uncompressed bufferViews
            std::string op = "delete_bufferview";
            auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
            taskDescription[op]["path"] = usedBufferViews;
            state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
        }
        return state;
    };

    return operation;
}

Operation optimization::clusterVertices(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "apply_vertex_clustering";

    operation.schema = schema::operation(
        "Apply Vertex Clustering",
        "Cluster vertices of a given primitive with deviation thresholds given as parameter",
        schema::object({ { "path", schema::pointer("/gltf/meshes/\\d+/primitives/\\d+") } },
            { {
                { "positionThreshold", schema::number(0.0) },
                { "normalThreshold", schema::number(0.05) },
                { "tangentThreshold", schema::number(0.05) },
                { "texCoordThreshold", schema::number(0.0) },
                { "colorThreshold", schema::number(0.01) },
            } }));

    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto& primitive = state[nlohmann::json::json_pointer(params["path"])];
        if (primitive.contains(nlohmann::json::json_pointer("/extensions/KHR_draco_mesh_compression"))) {
            throw std::invalid_argument("primitive has the following extension: KHR_draco_mesh_compression");
        }
        mesh::clusterVertices(params, state, dataStorage, library, callback);

        return state;
    };

    return operation;
}

Operation optimization::normalizeWeightsAccessor(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "normalize_weights";

    operation.schema = schema::operation(
        "Normalize Weights Accessor",
        "Some attributes require normalized accessors (e.g. weights). This operation ensures that the sum of all components is 1.0",
        schema::object({ schema::parameter::path("/gltf/accessors/\\d+") }));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params["path"].get<std::string>());
        auto byteData = reactor::utils::getAccessorByteDataPacked(path, state, datastorage);
        auto& accessor = state.at(path);

        auto componentType = accessor["componentType"].get<reactor::gltf::ComponentType>();
        auto attributeType = accessor["type"].get<std::string>();

        if (componentType != reactor::gltf::ComponentType::Float && attributeType != "VEC4") {
            throw std::invalid_argument("Unsupported type: " + attributeType);
        }

        std::vector<glm::vec4> vecData;
        vecData.resize(byteData.size() / sizeof(glm::vec4));
        std::memcpy(vecData.data(), byteData.data(), byteData.size());

        for (auto& vec : vecData) {
            float sum = 0.0f;
            for (int i = 0; i < vec.length(); i++) {
                sum += vec[i];
            }
            for (int i = 0; i < vec.length(); i++) {
                vec[i] /= sum;
            }
            if (std::isnan(vec[0])) { // Fix weight sum of zero
                vec = glm::vec4(1.0, 0.0, 0.0, 0.0);
            }
        }
        std::memcpy(byteData.data(), vecData.data(), byteData.size());
        auto newBuffer = reactor::utils::createBuffer(state, datastorage, byteData);
        auto newBufferView = reactor::utils::createBufferView(state, newBuffer);
        accessor["bufferView"] = std::stoi(newBufferView.back());
        accessor["byteOffset"] = 0;

        {
            auto taskDescription = nlohmann::json({ { "calculate_min_max", nlohmann::json::object() } });
            taskDescription["calculate_min_max"]["path"] = path.to_string();
            state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
        }

        return state;
    };

    return operation;
}

Operation optimization::clampAccessor(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "clamp_accessor_values";

    operation.schema = schema::operation(
        "Clamp Accessor Values",
        "Clamp accessor values to specified range",
        schema::object({ schema::parameter::path("/gltf/accessors/\\d+"), { "min", schema::number(0.0) } }, { { "max", schema::number() } }));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params["path"].get<std::string>());
        auto byteData = reactor::utils::getAccessorByteDataPacked(path, state, datastorage);
        auto& accessor = state.at(path);

        auto componentType = accessor["componentType"].get<reactor::gltf::ComponentType>();
        auto attributeType = accessor["type"].get<reactor::gltf::Type>();

        if (componentType != reactor::gltf::ComponentType::Float && attributeType != reactor::gltf::Type::SCALAR) {
            throw std::invalid_argument("Unsupported type");
        }

        std::vector<float> vecData;
        vecData.resize(byteData.size() / sizeof(float));
        std::memcpy(vecData.data(), byteData.data(), byteData.size());

        for (auto& vec : vecData) {
            if (vec < params["min"].get<float>()) {
                vec = params["min"].get<float>();
            }
            if (params.contains("max") && vec > params["max"].get<float>()) {
                vec = params["max"].get<float>();
            }
        }

        std::memcpy(byteData.data(), vecData.data(), byteData.size());
        auto newBuffer = reactor::utils::createBuffer(state, datastorage, byteData);
        auto newBufferView = reactor::utils::createBufferView(state, newBuffer);
        accessor["bufferView"] = std::stoi(newBufferView.back());
        accessor["byteOffset"] = 0;

        {
            auto taskDescription = nlohmann::json({ { "calculate_min_max", nlohmann::json::object() } });
            taskDescription["calculate_min_max"]["path"] = path.to_string();
            state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
        }

        return state;
    };

    return operation;
}

Operation optimization::convertImage()
{
    Operation operation;
    operation.name = "convert_image";

    auto parameters = nlohmann::json::array();
    // Define individual parameter sets for png, jpeg and ktx2
    parameters.push_back(
        schema::object({ schema::parameter::path("/gltf/images/\\d+"),
                           { "mimeType", schema::string("image/png") } },
            {
                // optional parameters:
                { "width", schema::number() },
                { "height", schema::number() },
                { "scale", schema::number(1) },
            }));
    parameters.push_back(
        schema::object({ schema::parameter::path("/gltf/images/\\d+"),
                           { "mimeType", schema::string("image/jpeg") } },
            {
                // optional parameters:
                { "quality", schema::number(90) },
                { "width", schema::number() },
                { "height", schema::number() },
                { "scale", schema::number(1) },
            }));
    parameters.push_back(
        schema::object({ schema::parameter::path("/gltf/images/\\d+"),
                           { "mimeType", schema::string("image/webp") } },
            {
                // optional parameters:
                { "quality", schema::number(90) },
                { "width", schema::number() },
                { "height", schema::number() },
                { "scale", schema::number(1) },
            }));
    parameters.push_back(
        schema::object({ schema::parameter::path("/gltf/images/\\d+"),
                           { "mimeType", schema::string("image/ktx2") } },
            {
                // optional parameters:
                { "width", schema::number() },
                { "height", schema::number() },
                { "scale", schema::number(1) },
                { "compression", schema::enumeration({ "ETC1S", "UASTC" }, "ETC1S") },
                { "transferFunction", schema::number() },
                // ETC1S
                { "etc1sQualityLevel", schema::number(128) },
                { "etc1sCompressionLevel", schema::number(1) },
                // UASTC:
                { "uastcQualityLevel", schema::number(2) },
                { "uastcRDO", schema::number(1.0) },
                { "superCompression", schema::enumeration({ "ZSTANDARD", "NONE" }, "NONE") },
                { "zstdCompressionLevel", schema::number(3) },
            }));

    operation.schema = schema::operation(
        "Convert Image",
        "Change resolution and compression of an image",
        schema::oneOf(parameters));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        std::string storageType = "File";
        auto path = nlohmann::json::json_pointer(params["path"].get<std::string>());

        if (!state.contains(path)) {
            throw std::invalid_argument("Image not found");
        }

        const std::string newMimeType = params.at("mimeType");

        auto source = std::stoi(path.back());

        std::vector<uint8_t> rawData = reactor::utils::getImageData(path, state, datastorage);
        auto formerMimeType = reactor::image::getMimeType(rawData);
        auto imagedata = reactor::image::decode(rawData);

        // Optional scale step
        if (params.contains("width")) {
            if (!params.contains("height")) {
                throw std::invalid_argument("Width given but height is not defined");
            }
            imagedata = reactor::image::scaleImage(imagedata, params["width"], params["height"]);
        } else if (params.contains("height")) {
            throw std::invalid_argument("Height given but width is not defined");
        }
        if (params.contains("scale")) {
            double scale = params["scale"];
            double width = imagedata.info["width"];
            double height = imagedata.info["height"];
            if (scale <= 0.0) {
                throw std::invalid_argument("Image resizing scale must be positive");
            }
            if (std::abs(1.0 - scale) > 1.0e-05) {
                int newWidth = scale * width;
                int newHeight = scale * height;
                if (newMimeType == "image/ktx2") {
                    int correctedWidth = newWidth / 4 * 4;
                    if (correctedWidth == 0) {
                        correctedWidth = 4;
                    }
                    int correctedHeight = newHeight / 4 * 4;
                    if (correctedHeight == 0) {
                        correctedHeight = 4;
                    }
                    newWidth = correctedWidth;
                    newHeight = correctedHeight;
                }
                imagedata = reactor::image::scaleImage(imagedata, newWidth, newHeight);
            }
        }

        rawData = reactor::image::encode(imagedata, params);

        std::string usedBufferView = "";
        // For now: always store as file
        if (state.at(path).contains("bufferView")) {
            usedBufferView = "/gltf/bufferViews/" + std::to_string(state.at(path / "bufferView").get<uint64_t>());
            state.at(path).erase("bufferView");
            storageType = "BufferView";
        }

        auto mimeType = reactor::image::getMimeType(rawData);
        std::string fileExtension = reactor::image::getFileExtension(mimeType);
        if (fileExtension.empty()) {
            fileExtension = ".bin";
        }

        // Substitute existing file extension with new file extension
        std::string newUri = state.at(path).value("uri", reactor::utils::createURIStem(state));
        if (reactor::utils::isHashedDataUri(newUri)) {
            storageType = "Embedded";
            newUri = reactor::utils::createURIStem(state);
        }
        if (newUri.find_last_of(".") != std::string::npos) {
            newUri = newUri.substr(0, newUri.find_last_of("."));
        }
        newUri = newUri + fileExtension;

        state.at(path)["uri"] = newUri;
        state.at(path)["mimeType"] = newMimeType;

        auto dataStorageID = datastorage.insert(rawData);
        auto dataIDPath = reactor::utils::getDataIDPath(path, state);
        state[dataIDPath] = dataStorageID;

        if (!state.contains(nlohmann::json::json_pointer("/gltf/textures"))) {
            return state; // no textures to fix
        }

        // Check texture reference: add/remove basisU/ webp extension
        for (auto& texture : state["gltf"]["textures"]) {
            bool hasCurrentImage = false;
            bool hasKTX2 = false;
            bool hasWEBP = false;

            // check source
            if (texture.contains("source") && texture.at("source").get<uint32_t>() == source) {
                hasCurrentImage = true;
            }
            // check webp extension
            if (texture.contains(nlohmann::json::json_pointer("/extensions/EXT_texture_webp/source")) && texture["extensions"]["EXT_texture_webp"]["source"].get<uint32_t>() == source) {
                hasWEBP = true;
                hasCurrentImage = true;
            }
            // check basisU extension
            if (texture.contains(nlohmann::json::json_pointer("/extensions/KHR_texture_basisu/source")) && texture["extensions"]["KHR_texture_basisu"]["source"].get<uint32_t>() == source) {
                hasKTX2 = true;
                hasCurrentImage = true;
            }

            if (!hasCurrentImage) {
                continue;
            }

            // remove ktx/webp extension if not needed
            if (hasKTX2 && mimeType.compare("image/ktx2") != 0) {
                texture["extensions"].erase("KHR_texture_basisu");
                if (texture["extensions"].empty()) {
                    texture.erase("extensions");
                }
            }

            // remove webp extension if not needed
            if (hasWEBP && mimeType.compare("image/webp") != 0) {
                texture["extensions"].erase("EXT_texture_webp");
                if (texture["extensions"].empty()) {
                    texture.erase("extensions");
                }
            }

            // set source if not set and mimeType is neither ktx2 nor webp
            if (mimeType.compare("image/ktx2") != 0 && mimeType.compare("image/webp") != 0) {
                texture["source"] = source;
            }

            // add basisU (ktx) extension
            if (mimeType.compare("image/ktx2") == 0) {
                if (texture.contains("source") && texture.at("source").get<uint32_t>() == source) {
                    texture.erase("source");
                }
                reactor::utils::createSubElement(texture, "/extensions/KHR_texture_basisu");
                texture["extensions"]["KHR_texture_basisu"]["source"] = source;

            } else if (mimeType.compare("image/webp") == 0) {
                // add webP extension
                if (texture.contains("source") && texture.at("source").get<uint32_t>() == source) {
                    texture.erase("source");
                }
                reactor::utils::createSubElement(texture, "/extensions/EXT_texture_webp");
                texture["extensions"]["EXT_texture_webp"]["source"] = source;
            }
        }

        if (mimeType.compare("image/ktx2") == 0) {
            utility::useExtension(state, "KHR_texture_basisu", true);
        }

        if (mimeType.compare("image/webp") == 0) {
            utility::useExtension(state, "EXT_texture_webp", true);
        }

        if (formerMimeType.compare("image/ktx2") == 0) {
            auto taskDescription = nlohmann::json({ { "update_extension_flags", { { "extensions", { "KHR_texture_basisu" } } } } });
            state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
        }

        if (formerMimeType.compare("image/webp") == 0) {
            auto taskDescription = nlohmann::json({ { "update_extension_flags", { { "extensions", { "EXT_texture_webp" } } } } });
            state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
        }

        if (!usedBufferView.empty()) {
            // Remove no longer used bufferViews
            std::string op = "delete_bufferview";
            auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
            taskDescription[op]["path"] = usedBufferView;
            state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
        }

        if (storageType != "File") {
            nlohmann::json taskDescription = { { "convert_storage_type", { { "path", params["path"] }, { "storageType", storageType } } } };
            state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
        }

        return state;
    };

    return operation;
}

Operation optimization::convertImageForEach()
{
    Operation operation;
    operation.name = "convert_image_for_each";

    auto parameters = nlohmann::json::array();
    // Define individual parameter sets for png, jpeg and ktx2
    parameters.push_back(
        schema::object({ { "mimeType", schema::string("image/png") } },
            {
                // optional parameters:
                { "width", schema::number() },
                { "height", schema::number() },
            }));
    parameters.push_back(
        schema::object({ { "mimeType", schema::string("image/jpeg") } },
            {
                // optional parameters:
                { "quality", schema::number() },
                { "width", schema::number() },
                { "height", schema::number() },
            }));
    parameters.push_back(
        schema::object({ { "mimeType", schema::string("image/ktx2") } },
            {
                // optional parameters:
                { "width", schema::number() },
                { "height", schema::number() },
                { "compression", schema::enumeration({ "ETC1S", "UASTC" }) },
                // ETC1S
                { "etc1sQualityLevel", schema::number() },
                { "etc1sCompressionLevel", schema::number() },
                // UASTC:
                { "uastcQualityLevel", schema::number() },
                { "uastcRDO", schema::number() },
                { "superCompression", schema::enumeration({ "ZSTANDARD", "NONE" }) },
                { "zstdCompressionLevel", schema::number() },
            }));

    operation.schema = schema::operation(
        "Convert Image for Each",
        "Change resolution and compression of all images",
        schema::oneOf(parameters));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto taskDescription = nlohmann::json({ { "dangerous.forEach", nlohmann::json::object() } });
        taskDescription["dangerous.forEach"]["operation"] = "convert_image";
        taskDescription["dangerous.forEach"]["pathPattern"] = "/gltf/images/\\d+";
        taskDescription["dangerous.forEach"]["parameters"] = params;

        return library.call(std::move(taskDescription), std::move(state), datastorage, callback);
    };

    return operation;
}

Operation optimization::changeTRS(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "change_transformation_format";

    const std::map<std::string, nlohmann::json> requiredKeys = { { schema::parameter::path("/gltf/nodes/\\d+") } };
    const std::map<std::string, nlohmann::json> optionalKeys = { { "storageType", schema::string("TRS|Matrix") } };

    operation.schema = schema::operation(
        "Change Transformation Format",
        "Change Translation, Rotation, Scale format of the node to individual components (TRS) or to a single transformation matrix",
        schema::object(requiredKeys, optionalKeys));

    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params["path"].get<std::string>());

        if (!state.contains(path)) {
            throw std::invalid_argument(std::string("Node not found"));
        }

        nlohmann::json& node = state.at(path);

        if (!node.contains("matrix") && !node.contains("translation") && !node.contains("scale") && !node.contains("rotation")) {
            throw std::runtime_error("No value in node to convert");
        }

        std::string storageType;
        if (params.contains("storageType")) {
            storageType = params["storageType"].get<std::string>();
        } else {
            storageType = node.contains("matrix") ? "TRS" : "Matrix";
        }

        if (node.contains("matrix") && storageType == "Matrix") {
            throw std::invalid_argument(std::string("Matrix is already set"));
        }

        if (!node.contains("matrix") && storageType == "TRS") {
            throw std::invalid_argument(std::string("TRS is already set"));
        }

        if (storageType.compare("Matrix") == 0) {

            // scale
            glm::dvec3 scale = glm::dvec3(1.0, 1.0, 1.0);

            if (node.contains("scale") && node.at("scale").is_array() && node.at("scale").size() == 3) {
                scale[0] = node.at("scale")[0].get<double>();
                scale[1] = node.at("scale")[1].get<double>();
                scale[2] = node.at("scale")[2].get<double>();
            }

            constexpr glm::dmat4 identity = glm::identity<glm::dmat4>();

            glm::dmat4 scaleMatrix = glm::scale(identity, scale);

            // rotation
            glm::dquat rotation = glm::dquat(1, 0, 0, 0);

            if (node.contains("rotation") && node.at("rotation").is_array() && node.at("rotation").size() == 4) {
                rotation[0] = node.at("rotation")[0].get<double>();
                rotation[1] = node.at("rotation")[1].get<double>();
                rotation[2] = node.at("rotation")[2].get<double>();
                rotation[3] = node.at("rotation")[3].get<double>();
            }

            glm::dmat4 rotationMatrix = glm::mat4_cast(rotation);

            // translation
            glm::dvec3 translation = glm::dvec3(0.0, 0.0, 0.0);
            if (node.contains("translation") && node.at("translation").is_array() && node.at("translation").size() == 3) {
                translation[0] = node.at("translation")[0].get<double>();
                translation[1] = node.at("translation")[1].get<double>();
                translation[2] = node.at("translation")[2].get<double>();
            }

            glm::dmat4 translationMatrix = glm::translate(identity, translation);

            glm::dmat4 transformationMatrix = translationMatrix * rotationMatrix * scaleMatrix;

            node["matrix"] = nlohmann::json::array();
            for (uint32_t i = 0; i < 4 * 4; i++) {
                uint32_t col = i / 4;
                uint32_t row = i % 4;
                double element = transformationMatrix[col][row];
                node["matrix"].push_back(element);
            }
            node.erase("translation");
            node.erase("rotation");
            node.erase("scale");

        } else if (storageType.compare("TRS") == 0) {
            glm::dvec3 translation;
            glm::dvec3 scale;
            glm::dquat rotation;
            glm::dvec4 perspective;
            glm::dvec3 skew;
            glm::dmat4 matrix;

            if (node.contains("matrix") && node.at("matrix").is_array() && node.at("matrix").size() == 16) {
                for (uint32_t i = 0; i < 16; i++) {
                    uint32_t col = i / 4;
                    uint32_t row = i % 4;
                    matrix[col][row] = node.at("matrix")[i].get<double>();
                }
                if (glm::decompose(matrix, scale, rotation, translation, skew, perspective)) {
                    node["translation"] = nlohmann::json::array();
                    node["scale"] = nlohmann::json::array();
                    node["rotation"] = nlohmann::json::array();
                    for (uint32_t i = 0; i < 3; i++) {
                        node["translation"].push_back(translation[i]);
                        node["scale"].push_back(scale[i]);
                        node["rotation"].push_back(rotation[i]);
                    }
                    node["rotation"].push_back(rotation[3]);
                    node.erase("matrix");
                } else {
                    throw std::invalid_argument(std::string("Transformation matrix is not decomposable. No changes were applied"));
                }
            }

        } else {
            throw std::invalid_argument(std::string("Invalid parameter, valid parameters are: 'TRS' or 'Matrix'"));
        }

        return state;
    };

    return operation;
}

Operation optimization::changeTRSForAll(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "change_transformation_format_for_each";

    operation.schema = schema::operation(
        "Change Transformation Format for Each",
        "Change transformation format (TRS or Matrix) for each node in the glTF",
        schema::object({ { "storageType", schema::string("TRS|Matrix") } }));

    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto taskDescription = nlohmann::json({ { "dangerous.forEach", nlohmann::json::object() } });
        taskDescription["dangerous.forEach"]["operation"] = "change_transformation_format";
        taskDescription["dangerous.forEach"]["pathPattern"] = "/gltf/nodes/\\d+";
        taskDescription["dangerous.forEach"]["parameters"] = params;

        return library.call(std::move(taskDescription), std::move(state), datastorage, callback);
    };

    return operation;
}

Operation optimization::joinPrimitives(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "join_primitives";

    operation.schema = schema::operation(
        "Join Primitives",
        "Join primitives of a mesh",
        schema::object({ { "paths", schema::array(schema::pointer("/gltf/meshes/\\d+/primitives/\\d+"), 2) } }));

    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        // All paths together must form a subset of primitives of a single mesh.
        size_t meshIndex = -1;
        std::vector<size_t> primitiveIndices;
        const nlohmann::json& paths = params["paths"];
        for (const auto& path : paths) {
            std::regex re { "/gltf/meshes/(\\d+)/primitives/(\\d+)" };
            std::smatch matches;
            auto string = path.get<std::string>();
            std::regex_match(string, matches, re);
            if (matches.size() != 3) {
                throw std::invalid_argument("Invalid primitive path");
            }
            size_t currentMeshIndex = std::stoi(matches[1].str());
            if (meshIndex == -1) {
                meshIndex = currentMeshIndex;
            } else if (meshIndex != currentMeshIndex) {
                throw std::invalid_argument("All primitives must be owned by the same mesh");
            }
            primitiveIndices.push_back(std::stoi(matches[2].str()));
        }
        auto& mesh = state["gltf"]["meshes"][meshIndex];

        std::vector<nlohmann::json> primitives;
        for (size_t i : primitiveIndices) {
            primitives.emplace_back(mesh["primitives"][i]);
        }

        // Restrict use of extensions.
        for (const auto& path : paths) {
            const auto& primitive = state[nlohmann::json::json_pointer(path)];

            if (!primitive.contains("extensions")) {
                continue;
            }
            if (primitive["extensions"].contains("KHR_draco_mesh_compression")) {
                throw std::invalid_argument("Primitives are compressed");
            } else if ((primitive["extensions"].contains("KHR_materials_variants") != primitives.front()["extensions"].contains("KHR_materials_variants")) || (primitive["extensions"].contains("KHR_materials_variants") && primitive["extensions"]["KHR_materials_variants"] != primitives.front()["extensions"]["KHR_materials_variants"])) {
                throw std::invalid_argument("Either no primitives are allowed to use material variants, or all must use the same material variant");
            } else if (primitive["extensions"].size() > 0) {
                throw std::invalid_argument("Primitives use extensions");
            }
        }

        nlohmann::json targetPrimitive = {
            { "attributes", nlohmann::json::object() }
        };

        // Check that primitives are compatible.
        for (const auto& prim : primitives) {
            auto check = [&](const char* key) {
                if (primitives.front().contains(key) != prim.contains(key)) {
                    return false;
                }
                if (primitives.front().contains(key)) {
                    return primitives.front()[key] == prim[key];
                }
                return true;
            };
            if (!check("material") || !check("mode") || !check("targets")) {
                throw std::invalid_argument("Primitives are not compatible");
            }
            if (primitives.front().contains("indices") != prim.contains("indices")) {
                throw std::invalid_argument("Primitives are inconsistently indexed");
            }
            if (prim.contains("mode") && prim["mode"] != reactor::gltf::Topology::TRIANGLES) {
                throw std::invalid_argument("Some primitive modes are not triangles");
            }
        }

        // Check that all primitives have the same set of attributes.
        {
            std::set<std::string> attrs;
            for (const auto& [attr, _] : primitives.front()["attributes"].items()) {
                attrs.emplace(attr);
            }
            for (const auto& prim : primitives) {
                std::set<std::string> otherAttrs;
                for (const auto& [attr, _] : prim["attributes"].items()) {
                    otherAttrs.emplace(attr);
                }
                if (attrs != otherAttrs) {
                    throw std::invalid_argument("Cannot join primitives with inconsistent attributes");
                }
            }
        }

        if (primitives.front().contains("material")) {
            targetPrimitive["material"] = primitives.front()["material"];
        }
        if (primitives.front().contains("targets")) {
            targetPrimitive["targets"] = primitives.front()["targets"];
        }

        // Join indices.
        // We join indices using the largest available index component type UNSIGNED_INT,
        // as choosing the most efficient index bit-length is a separate problem and implemented by another operation.
        if (primitives.front().contains("indices")) {
            std::vector<uint8_t> targetIndexBytes;
            uint64_t totalIndexCount = 0;
            uint64_t indexShift = 0;
            uint32_t maxIndex = 0;
            uint32_t minIndex = std::numeric_limits<uint32_t>::max();

            for (const auto& primitive : primitives) {
                const auto& accessor = state["gltf"]["accessors"][primitive["indices"].get<int>()];
                reactor::gltf::ComponentType componentType = accessor["componentType"];

                auto bufferViewPointer = nlohmann::json::json_pointer("/gltf/bufferViews") / accessor.at("bufferView").get<int>();
                std::vector<uint8_t> bytes = reactor::utils::getBufferViewData(bufferViewPointer, state, dataStorage);

                auto count = accessor["count"].get<size_t>();
                targetIndexBytes.resize(targetIndexBytes.size() + count * sizeof(uint32_t));
                for (size_t i = 0; i < count; ++i) {
                    uint32_t index = 0;

                    switch (componentType) {
                    case reactor::gltf::ComponentType::UnsignedInt:
                        index = reinterpret_cast<uint32_t*>(bytes.data())[i];
                        break;
                    case reactor::gltf::ComponentType::UnsignedShort:
                        index = reinterpret_cast<uint16_t*>(bytes.data())[i];
                        break;
                    case reactor::gltf::ComponentType::UnsignedByte:
                        index = reinterpret_cast<uint8_t*>(bytes.data())[i];
                        break;
                    default:
                        throw std::runtime_error("Invalid index component type");
                    }

                    // Shift index to account for preceding indexed primitives.
                    index += indexShift;

                    maxIndex = std::max(maxIndex, index);
                    minIndex = std::min(minIndex, index);

                    reinterpret_cast<uint32_t*>(targetIndexBytes.data())[totalIndexCount + i] = index;
                }

                totalIndexCount += count;

                // Check that maximal index value is not exceeded.
                indexShift += state["gltf"]["accessors"][primitive["attributes"]["POSITION"].get<size_t>()]["count"].get<uint32_t>();
                if (indexShift > std::numeric_limits<uint32_t>::max() - 1) {
                    throw std::invalid_argument("Joining primitives would exceed the maximal index value range");
                }
            }

            auto bufferPath = reactor::utils::createBuffer(state, dataStorage, targetIndexBytes);
            auto bufferViewPath = reactor::utils::createBufferView(state, bufferPath);
            state["gltf"]["accessors"].emplace_back(nlohmann::json({
                { "type", "SCALAR" },
                { "componentType", reactor::gltf::ComponentType::UnsignedInt },
                { "count", totalIndexCount },
                { "min", nlohmann::json::array({ minIndex }) },
                { "max", nlohmann::json::array({ maxIndex }) },
                { "bufferView", std::stoi(bufferViewPath.back()) },
            }));
            targetPrimitive["indices"] = state["gltf"]["accessors"].size() - 1;
        }

        // Join attributes
        for (const auto& [attr, _] : primitives.front()["attributes"].items()) {
            std::vector<uint8_t> targetAttrBytes;

            std::array<float, 3> min, max;
            min.fill(std::numeric_limits<float>::infinity());
            max.fill(-std::numeric_limits<float>::infinity());

            for (const auto& primitive : primitives) {
                const auto& accessor = state["gltf"]["accessors"][primitive["attributes"][attr].get<int>()];

                auto bufferViewPointer = nlohmann::json::json_pointer() / "gltf" / "bufferViews" / accessor["bufferView"].get<int>();
                std::vector<uint8_t> attrBytes = reactor::utils::getBufferViewData(bufferViewPointer, state, dataStorage);
                targetAttrBytes.insert(targetAttrBytes.end(), attrBytes.begin(), attrBytes.end());

                if (attr == "POSITION") {
                    min[0] = std::min(min[0], accessor["min"][0].get<float>());
                    min[1] = std::min(min[1], accessor["min"][1].get<float>());
                    min[2] = std::min(min[2], accessor["min"][2].get<float>());
                    max[0] = std::max(max[0], accessor["max"][0].get<float>());
                    max[1] = std::max(max[1], accessor["max"][1].get<float>());
                    max[2] = std::max(max[2], accessor["max"][2].get<float>());
                }
            }

            targetPrimitive["attributes"][attr] = state["gltf"]["accessors"].size();
            auto bufferPath = reactor::utils::createBuffer(state, dataStorage, targetAttrBytes);
            auto bufferViewPath = reactor::utils::createBufferView(state, bufferPath);

            const auto& accessor = state["gltf"]["accessors"][primitives.front()["attributes"][attr].get<int>()];
            reactor::gltf::Type type = accessor["type"];
            reactor::gltf::ComponentType componentType = accessor["componentType"];
            nlohmann::json targetAccessor = {
                { "type", type },
                { "componentType", reactor::gltf::ComponentType::Float },
                { "count", targetAttrBytes.size() / byteLengthPadded(type, componentType) },
                { "bufferView", std::stoi(bufferViewPath.back()) },
            };
            if (attr == "POSITION") {
                targetAccessor["min"] = min;
                targetAccessor["max"] = max;
            }
            state["gltf"]["accessors"].emplace_back(std::move(targetAccessor));
            targetPrimitive["attributes"][attr] = state["gltf"]["accessors"].size() - 1;
        }

        // Remove primitives
        std::vector<size_t> maybeUnusedAccessorsIdxs;
        std::sort(primitiveIndices.begin(), primitiveIndices.end(), std::greater<>());
        for (size_t index : primitiveIndices) {
            maybeUnusedAccessorsIdxs.push_back(mesh["primitives"][index]["indices"]);
            for (const auto& [_, idx] : mesh["primitives"][index]["attributes"].items()) {
                maybeUnusedAccessorsIdxs.push_back(idx);
            }
            mesh["primitives"].erase(index);
        }

        // Add new primitive
        mesh["primitives"].push_back(targetPrimitive);

        // Try to shrink index type
        state = library.call({ { "optimize_index_type", "/gltf/accessors/" + std::to_string(targetPrimitive["indices"].get<size_t>()) } }, std::move(state), dataStorage, callback);

        // Cleanup possibly unused accessors, buffer views and buffers
        std::vector<std::string> cleanupFilter;
        for (size_t acc : maybeUnusedAccessorsIdxs) {
            cleanupFilter.push_back("/gltf/accessors/" + std::to_string(acc));

            size_t buffViewIdx = state["gltf"]["accessors"][acc]["bufferView"];
            cleanupFilter.push_back("/gltf/bufferViews/" + std::to_string(buffViewIdx));

            size_t buffIdx = state["gltf"]["bufferViews"][buffViewIdx]["buffer"];
            cleanupFilter.push_back("/gltf/buffers/" + std::to_string(buffIdx));
        }
        state = library.call({ { "clean_gltf", { { "filter", cleanupFilter } } } }, std::move(state), dataStorage, callback);

        return state;
    };

    return operation;
}

Operation optimization::narrowIndexType(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "optimize_index_type";

    operation.schema = schema::operation(
        "Optimize Index Type",
        "Minimize byte length of indices",
        schema::pointer("/gltf/accessors/\\d+"));

    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto& accessor = state[nlohmann::json::json_pointer(params)];

        if (accessor["componentType"] == reactor::gltf::ComponentType::UnsignedByte) {
            // No smaller index type available.
            return state;
        }
        if (!accessor.contains("bufferView")) {
            return state;
        }

        if (accessor["type"] != "SCALAR" || (accessor["componentType"] != reactor::gltf::ComponentType::UnsignedShort && accessor["componentType"] != reactor::gltf::ComponentType::UnsignedInt)) {
            throw std::invalid_argument("Not a valid index accessor");
        }

        auto count = accessor["count"].get<size_t>();
        auto componentType = accessor["componentType"].get<reactor::gltf::ComponentType>();
        auto bufferViewPointer = nlohmann::json::json_pointer() / "gltf" / "bufferViews" / accessor["bufferView"].get<size_t>();
        auto bytes = reactor::utils::getBufferViewData(bufferViewPointer, state, dataStorage);

        uint32_t max = 0;
        if (accessor.contains("max")) {
            max = accessor["max"][0];
        } else {
            for (size_t i = 0; i < count; ++i) {
                uint32_t index;
                switch (componentType) {
                case reactor::gltf::ComponentType::UnsignedInt:
                    index = reinterpret_cast<uint32_t*>(bytes.data())[i];
                    break;
                case reactor::gltf::ComponentType::UnsignedShort:
                    index = reinterpret_cast<uint16_t*>(bytes.data())[i];
                    break;
                case reactor::gltf::ComponentType::UnsignedByte:
                    break;
                default:
                    throw std::runtime_error("Invalid index component type");
                }
                max = std::max(max, index);
            }
        }

        reactor::gltf::ComponentType targetComponentType;
        if (max <= std::numeric_limits<uint8_t>::max() - 1) {
            targetComponentType = reactor::gltf::ComponentType::UnsignedByte;
        } else if (max <= std::numeric_limits<uint16_t>::max() - 1) {
            targetComponentType = reactor::gltf::ComponentType::UnsignedShort;
        } else {
            // Index type cannot be narrowed.
            return state;
        }

        {
            std::vector<uint8_t> targetIndexBytes;
            targetIndexBytes.resize(byteLength(targetComponentType) * count);
            for (size_t i = 0; i < count; ++i) {
                memcpy(&targetIndexBytes[i * byteLength(targetComponentType)], &bytes[i * byteLength(componentType)], byteLength(targetComponentType));
            }

            auto bufferPath = reactor::utils::createBuffer(state, dataStorage, targetIndexBytes);
            auto bufferViewPath = reactor::utils::createBufferView(state, bufferPath);
            accessor["bufferView"] = std::stoi(bufferViewPath.back());
            accessor["max"] = nlohmann::json::array({ max });
            accessor["componentType"] = targetComponentType;
        }

        // Clean up possibly unused buffer view and it's buffer
        state = library.call({ { "clean_gltf", { { "filter", nlohmann::json::array({ bufferViewPointer.to_string(), (nlohmann::json::json_pointer() / "gltf" / "buffers" / state[bufferViewPointer]["buffer"].get<int>()).to_string() }) } } } }, std::move(state), dataStorage, callback);

        return state;
    };

    return operation;
}

Operation optimization::convertSpecularGlossinessToMetallicRoughness(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;

    operation.name = "convert_specular_glossiness_to_metallic_roughness",

    operation.schema = schema::operation(
        "Convert Specular Glossiness to Metallic Roughness",
        "Approximate a specular-glossiness based workflow with a metallic-roughness based one",
        schema::object(
            {
                schema::parameter::path("(/gltf/materials/\\d+)"),
            })),

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        nlohmann::json::json_pointer materialPointer { params["path"] };
        auto& material = state[materialPointer];

        if (!material.contains("extensions") || !material["extensions"].contains("KHR_materials_pbrSpecularGlossiness")) {
            throw std::invalid_argument("Material does not contain KHR_materials_pbrSpecularGlossiness");
        }

        const auto& specularGlossiness = material["extensions"]["KHR_materials_pbrSpecularGlossiness"];

        auto diffuseFactor = specularGlossiness.value("diffuseFactor", glm::dvec4(1.0));
        auto specularColorFactor = specularGlossiness.value("specularFactor", glm::dvec3(1.0));
        auto glossinessFactor = specularGlossiness.value("glossinessFactor", 1.0);

        bool diffuseHasAlpha = false;
        std::optional<reactor::image::ImageData> diffuseImageData;
        std::optional<reactor::image::ImageData> specularImageData;

        if (specularGlossiness.contains("diffuseTexture")) {
            const auto& diffuseTexture = state["gltf"]["textures"][specularGlossiness["diffuseTexture"]["index"].get<size_t>()];

            if (diffuseTexture.contains("source")) {
                auto imagePointer = nlohmann::json::json_pointer {} / "gltf" / "images" / diffuseTexture["source"].get<size_t>();

                const auto& diffuseImage = state[imagePointer];
                if (diffuseImage.contains("extensions") && diffuseImage["extensions"].contains("KHR_texture_transform")) {
                    throw std::runtime_error { "Diffuse texture uses KHR_texture_transform" };
                }

                auto imageData = reactor::image::decode(reactor::utils::getImageData(imagePointer, state, dataStorage));

                if (imageData.info["channels"] == 3) {
                    diffuseHasAlpha = false;
                } else if (imageData.info["channels"] == 4) {
                    diffuseHasAlpha = true;
                } else {
                    throw std::runtime_error {
                        fmt::format("Diffuse texture must have 3 or 4 channels but has {}", imageData.info["channels"])
                    };
                }

                diffuseImageData = imageData;
            }
        }

        if (specularGlossiness.contains("specularGlossinessTexture")) {
            const auto& specularTexture = state["gltf"]["textures"][specularGlossiness["specularGlossinessTexture"]["index"].get<size_t>()];

            if (specularTexture.contains("source")) {
                auto imagePointer = nlohmann::json::json_pointer {} / "gltf" / "images" / specularTexture["source"].get<size_t>();

                const auto& specularImage = state[imagePointer];
                if (specularImage.contains("extensions") && specularImage["extensions"].contains("KHR_texture_transform")) {
                    throw std::runtime_error { "Specular texture uses KHR_texture_transform" };
                }

                auto imageData = reactor::image::decode(reactor::utils::getImageData(imagePointer, state, dataStorage));

                if (imageData.info["channels"] != 4) {
                    throw std::runtime_error {
                        fmt::format("Specular texture must have 4 channels but has {}", imageData.info["channels"])
                    };
                }

                specularImageData = imageData;
            }
        }

        // Check if texture conversion is necessary and possible
        bool textureConversionNeeded = false;
        size_t imageWidth = 0;
        size_t imageHeight = 0;
        size_t metallicRoughnessTextureIndex = ~0;
        size_t baseColorTextureIndex = ~0;
        size_t specularTextureIndex = ~0;
        if (diffuseImageData) {
            if (specularImageData) {
                if (diffuseImageData->info["width"] != specularImageData->info["width"]
                    || diffuseImageData->info["height"] != specularImageData->info["height"]) {
                    throw std::runtime_error { "Diffuse and specular texture must have the same size" };
                }
            }
            textureConversionNeeded = true;
            imageWidth = diffuseImageData->info["width"];
            imageHeight = diffuseImageData->info["height"];

        } else if (specularImageData) {
            textureConversionNeeded = true;
            imageWidth = specularImageData->info["width"];
            imageHeight = specularImageData->info["height"];
        }

        auto conversion = reactor::image::convertSpecularGlossinessToMetallicRoughness(diffuseFactor, specularColorFactor, glossinessFactor);

        if (textureConversionNeeded) {
            size_t imageSize = imageWidth * imageHeight;

            std::vector<uint8_t> baseColorPixels;
            baseColorPixels.resize(imageSize * 4);
            std::vector<uint8_t> metallicRoughnessPixels;
            metallicRoughnessPixels.resize(imageSize * 4);
            std::vector<uint8_t> specularColorPixels;
            specularColorPixels.resize(imageSize * 3);

            for (size_t k = 0; k < imageSize; ++k) {

                glm::dvec4 diffuse;
                if (diffuseImageData) {
                    if (diffuseHasAlpha) {
                        diffuse = {
                            diffuseImageData->data[4 * k],
                            diffuseImageData->data[4 * k + 1],
                            diffuseImageData->data[4 * k + 2],
                            diffuseImageData->data[4 * k + 3],
                        };
                    } else {
                        diffuse = {
                            diffuseImageData->data[3 * k],
                            diffuseImageData->data[3 * k + 1],
                            diffuseImageData->data[3 * k + 2],
                            255.0,
                        };
                    }
                    diffuse /= 255.0;
                    diffuse = reactor::image::toLinear(diffuse);
                } else {
                    diffuse = diffuseFactor;
                }

                glm::dvec3 specularColor;
                double glossiness;
                if (specularImageData) {
                    specularColor = {
                        specularImageData->data[4 * k],
                        specularImageData->data[4 * k + 1],
                        specularImageData->data[4 * k + 2],
                    };
                    specularColor /= 255.0;
                    specularColor = reactor::image::toLinear(specularColor);
                    glossiness = (specularImageData->data[4 * k + 3] / 255.0) * glossinessFactor;
                } else {
                    specularColor = specularColorFactor;
                    glossiness = glossinessFactor;
                }

                auto conversion = reactor::image::convertSpecularGlossinessToMetallicRoughness(diffuse, specularColor, glossiness);

                conversion.baseColor = reactor::image::toSrgb(conversion.baseColor);
                conversion.baseColor *= 255.0;
                baseColorPixels[4 * k + 0] = static_cast<uint8_t>(conversion.baseColor.r);
                baseColorPixels[4 * k + 1] = static_cast<uint8_t>(conversion.baseColor.g);
                baseColorPixels[4 * k + 2] = static_cast<uint8_t>(conversion.baseColor.b);
                baseColorPixels[4 * k + 3] = static_cast<uint8_t>(conversion.baseColor.a);

                conversion.roughness *= 255.0;
                conversion.metallic *= 255.0;
                metallicRoughnessPixels[4 * k + 0] = 0;
                metallicRoughnessPixels[4 * k + 1] = static_cast<uint8_t>(conversion.roughness);
                metallicRoughnessPixels[4 * k + 2] = static_cast<uint8_t>(conversion.metallic);
                metallicRoughnessPixels[4 * k + 3] = 255;

                specularColor = reactor::image::toSrgb(specularColor);
                specularColor *= 255.0;
                specularColorPixels[3 * k + 0] = static_cast<uint8_t>(specularColor.r);
                specularColorPixels[3 * k + 1] = static_cast<uint8_t>(specularColor.g);
                specularColorPixels[3 * k + 2] = static_cast<uint8_t>(specularColor.b);
            }

            // RoughnessFactor = 1.0, because in this case we multiply the glossinessFactor into the texture,
            // to fix the issue of e.g. GlossinessFactor = 1.0, GlossinessTexture is present => RoughnessFactor = 0.0, RoughnessTexture is present (but ignored)
            conversion.roughness = 1.0;

            reactor::image::ImageData imageData;
            imageData.info["width"] = imageWidth;
            imageData.info["height"] = imageHeight;

            imageData.info["pixelByteSize"] = 4;
            imageData.info["channels"] = 4;
            imageData.data = std::move(baseColorPixels);
            nlohmann::json baseColorImage { { "uri", reactor::utils::createURIStem(state) + ".png" } };
            state["gltf"]["images"].emplace_back(baseColorImage);
            state[reactor::utils::getDataIDPath(baseColorImage, state)]
                = dataStorage.insert(reactor::image::encode(imageData, { { "mimeType", "image/png" } }));

            state["gltf"]["textures"].push_back({ { "source", state["gltf"]["images"].size() - 1 } });
            baseColorTextureIndex = state["gltf"]["textures"].size() - 1;

            imageData.data = std::move(metallicRoughnessPixels);
            nlohmann::json metallicRoughnessImage { { "uri", reactor::utils::createURIStem(state) + ".png" } };
            state["gltf"]["images"].emplace_back(metallicRoughnessImage);
            state[reactor::utils::getDataIDPath(metallicRoughnessImage, state)]
                = dataStorage.insert(reactor::image::encode(imageData, { { "mimeType", "image/png" } }));

            state["gltf"]["textures"].push_back({ { "source", state["gltf"]["images"].size() - 1 } });
            metallicRoughnessTextureIndex = state["gltf"]["textures"].size() - 1;

            imageData.info["pixelByteSize"] = 3;
            imageData.info["channels"] = 3;
            imageData.data = std::move(specularColorPixels);
            nlohmann::json specularImage { { "uri", reactor::utils::createURIStem(state) + ".png" } };
            state["gltf"]["images"].emplace_back(specularImage);
            state[reactor::utils::getDataIDPath(specularImage, state)]
                = dataStorage.insert(reactor::image::encode(imageData, { { "mimeType", "image/png" } }));

            state["gltf"]["textures"].push_back({ { "source", state["gltf"]["images"].size() - 1 } });
            specularTextureIndex = state["gltf"]["textures"].size() - 1;
        }

        material["pbrMetallicRoughness"]["metallicFactor"] = conversion.metallic;
        material["pbrMetallicRoughness"]["roughnessFactor"] = conversion.roughness;
        material["pbrMetallicRoughness"]["baseColorFactor"] = conversion.baseColor;
        material["extensions"]["KHR_materials_specular"]["specularColorFactor"] = specularColorFactor;
        material["extensions"]["KHR_materials_ior"]["ior"] = 0.0;

        if (textureConversionNeeded) {
            material["pbrMetallicRoughness"]["baseColorTexture"] = { { "index", baseColorTextureIndex } };
            material["pbrMetallicRoughness"]["metallicRoughnessTexture"] = { { "index", metallicRoughnessTextureIndex } };
            material["extensions"]["KHR_materials_specular"]["specularColorTexture"] = { { "index", specularTextureIndex } };
        }

        material["extensions"].erase("KHR_materials_pbrSpecularGlossiness");

        utility::useExtension(state, "KHR_materials_specular", true);
        utility::useExtension(state, "KHR_materials_ior", true);

        return state;
    };
    return operation;
}

Operation optimization::resolveMeshQuantizationForAll(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "resolve_mesh_quantization_for_each";

    operation.schema = schema::operation(
        "Resolve KHR_mesh_quantization for Each",
        "Resolve the KHR_mesh_quantization extension for each accessor",
        schema::object({}));

    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto accessors = reactor::utils::getMeshQuantizationAccessors(state);
        if (accessors.empty()) {
            throw std::invalid_argument("glTF does not have any Accessors using KHR_mesh_quantization");
        }

        for (auto& [accessorPath, attributeName] : accessors) {
            state = meshQuantization::resolve(accessorPath, std::move(state), dataStorage, library, callback);

            if (attributeName == "POSITION") {
                nlohmann::json calculateMinMax;
                calculateMinMax["calculate_min_max"]["path"] = accessorPath.to_string();

                state = library.call(calculateMinMax, std::move(state), dataStorage, callback);
            }
        }

        utility::removeExtensionFlag(state, "KHR_mesh_quantization");

        return state;
    };

    return operation;
}

Operation optimization::resolveMeshQuantization(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "resolve_mesh_quantization";

    operation.schema = schema::operation(
        "Resolve KHR_mesh_quantization",
        "Resolve the KHR_mesh_quantization extension in a specific Accessor",
        schema::object(
            {
                schema::parameter::path("/gltf/accessors/\\d+"),
            }));

    operation.operation = [linearizedGltfSchema](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        nlohmann::json::json_pointer accessorPath = nlohmann::json::json_pointer(params.at("path"));
        auto meshQuantAccessors = reactor::utils::getMeshQuantizationAccessors(state);
        std::string thisAttributeName;
        bool usesMeshQuantization = false;

        for (auto& [meshQuantPath, attributeName] : meshQuantAccessors) {
            if (meshQuantPath == accessorPath) {
                thisAttributeName = attributeName;
                usesMeshQuantization = true;
                break;
            }
        }

        if (!usesMeshQuantization) {
            throw std::invalid_argument("Accessor does not use KHR_mesh_quantization");
        }

        state = meshQuantization::resolve(accessorPath, std::move(state), dataStorage, library, callback);

        if (thisAttributeName == "POSITION") {
            nlohmann::json calculateMinMax;
            calculateMinMax["calculate_min_max"]["path"] = accessorPath.to_string();

            state = library.call(calculateMinMax, std::move(state), dataStorage, callback);
        }

        if (meshQuantAccessors.size() == 1) {
            utility::removeExtensionFlag(state, "KHR_mesh_quantization");
        }

        return state;
    };

    return operation;
}

Operation optimization::reduceTriangleCount()
{
    Operation operation;
    operation.name = "reduce_triangle_count",
    operation.schema = schema::operation(
        "Reduce Triangle Count",
        "Reduce the triangle count by approximating the shape",
        schema::object(
            { schema::parameter::path("/gltf/meshes/\\d+/primitives/\\d+") },
            {
                { "targetSize", schema::number(1) },
                { "quality", schema::number(7) },
            })),

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer { params["path"] };
        double targetSize = params["targetSize"];
        double quality = params["quality"];

        auto primitive = state[path];

        if (!primitive.contains("indices")) {
            throw std::runtime_error { "Primitive is missing an index accessor" };
        }

        mesh_simplification::vertices.clear();
        mesh_simplification::triangles.clear();

        auto indexAccessor = state["gltf"]["accessors"][primitive["indices"].get<int>()];

        std::vector<int> indices;
        indices.resize(indexAccessor["count"]);
        if (indexAccessor["componentType"] == reactor::gltf::ComponentType::UnsignedShort) {
            auto shorts = reactor::utils::getAccessorData<uint16_t>(
                nlohmann::json::json_pointer {} / "gltf" / "accessors" / primitive["indices"].get<int>(),
                state, dataStorage);
            std::transform(shorts.begin(), shorts.end(), indices.begin(), [](uint16_t i) { return i; });
        } else {
            auto ints = reactor::utils::getAccessorData<uint32_t>(
                nlohmann::json::json_pointer {} / "gltf" / "accessors" / primitive["indices"].get<int>(),
                state, dataStorage);
            std::transform(ints.begin(), ints.end(), indices.begin(), [](uint32_t i) { return i; });
        }

        {
            std::string unsupportedAttributes;
            int count = 0;
            for (const auto& [attribute, _] : primitive["attributes"].items()) {
                if (attribute != "POSITION" && attribute != "NORMAL" && attribute != "TANGENT" && attribute != "TEXCOORD_0" && attribute != "TEXCOORD_1" && attribute != "COLOR_0") {
                    if (count > 0) {
                        unsupportedAttributes += ", ";
                    }
                    unsupportedAttributes += attribute;
                    count += 1;
                }
            }
            if (count == 1) {
                throw std::runtime_error { fmt::format("Unsupported attribute {}", unsupportedAttributes) };
            } else if (count > 1) {
                throw std::runtime_error { fmt::format("Unsupported attributes {}", unsupportedAttributes) };
            }
        }

        auto positions = reactor::utils::getAccessorData<glm::vec3>(
            nlohmann::json::json_pointer {} / "gltf" / "accessors" / primitive["attributes"]["POSITION"].get<int>(),
            state, dataStorage);

        std::vector<glm::vec2> texCoord0;
        if (primitive["attributes"].contains("TEXCOORD_0")) {
            texCoord0 = reactor::utils::getAccessorData<glm::vec2>(
                nlohmann::json::json_pointer {} / "gltf" / "accessors" / primitive["attributes"]["TEXCOORD_0"].get<int>(),
                state, dataStorage);
        }

        std::vector<glm::vec2> texCoord1;
        if (primitive["attributes"].contains("TEXCOORD_1")) {
            texCoord1 = reactor::utils::getAccessorData<glm::vec2>(
                nlohmann::json::json_pointer {} / "gltf" / "accessors" / primitive["attributes"]["TEXCOORD_1"].get<int>(),
                state, dataStorage);
        }

        std::vector<glm::vec3> normal;
        if (primitive["attributes"].contains("NORMAL")) {
            normal = reactor::utils::getAccessorData<glm::vec3>(
                nlohmann::json::json_pointer {} / "gltf" / "accessors" / primitive["attributes"]["NORMAL"].get<int>(),
                state, dataStorage);
        }

        std::vector<glm::vec4> color0;
        if (primitive["attributes"].contains("COLOR_0")) {
            auto pointer = nlohmann::json::json_pointer {} / "gltf" / "accessors" / primitive["attributes"]["COLOR_0"].get<int>();

            if (state[pointer]["type"] == "VEC4") {
                color0 = reactor::utils::getAccessorData<glm::vec4>(
                    nlohmann::json::json_pointer {} / "gltf" / "accessors" / primitive["attributes"]["COLOR_0"].get<int>(),
                    state, dataStorage);
            } else {
                auto rgb = reactor::utils::getAccessorData<glm::vec3>(
                    nlohmann::json::json_pointer {} / "gltf" / "accessors" / primitive["attributes"]["COLOR_0"].get<int>(),
                    state, dataStorage);
                color0.resize(rgb.size());
                std::transform(rgb.begin(), rgb.end(), color0.begin(), [](glm::vec3 c) { return glm::vec4 { c, 0.0 }; });
            }
        }

        std::vector<glm::vec4> tangent;
        if (primitive["attributes"].contains("TANGENT")) {
            tangent = reactor::utils::getAccessorData<glm::vec4>(
                nlohmann::json::json_pointer {} / "gltf" / "accessors" / primitive["attributes"]["TANGENT"].get<int>(),
                state, dataStorage);
            primitive["attributes"].erase("TANGENT");

            float tangentHandedness = tangent[0].w;
            for (auto& tangent : tangent) {
                if (tangent.w != tangentHandedness) {
                    tangent *= -1.0;
                }
            }
        }

        for (uint32_t i = 0; i < indices.size(); i += 3) {
            mesh_simplification::Triangle triangle {
                .v = { indices[i], indices[i + 1], indices[i + 2] },
                .attr = 0,
            };

            if (!texCoord0.empty()) {
                triangle.texCoord0[0] = texCoord0[indices[i]];
                triangle.texCoord0[1] = texCoord0[indices[i + 1]];
                triangle.texCoord0[2] = texCoord0[indices[i + 2]];
            }

            if (!texCoord1.empty()) {
                triangle.texCoord0[0] = texCoord1[indices[i]];
                triangle.texCoord0[1] = texCoord1[indices[i + 1]];
                triangle.texCoord0[2] = texCoord1[indices[i + 2]];
            }

            if (!tangent.empty()) {
                triangle.tangent[0] = tangent[indices[i]];
                triangle.tangent[1] = tangent[indices[i + 1]];
                triangle.tangent[2] = tangent[indices[i + 2]];
            }

            if (!normal.empty()) {
                triangle.normal[0] = normal[indices[i]];
                triangle.normal[1] = normal[indices[i + 1]];
                triangle.normal[2] = normal[indices[i + 2]];
            }

            if (!color0.empty()) {
                triangle.color0[0] = color0[indices[i]];
                triangle.color0[1] = color0[indices[i + 1]];
                triangle.color0[2] = color0[indices[i + 2]];
            }

            mesh_simplification::triangles.push_back(triangle);
        }

        mesh_simplification::vertices.resize(positions.size());
        std::transform(
            positions.begin(), positions.end(), mesh_simplification::vertices.begin(),
            [](glm::vec3 position) { return mesh_simplification::Vertex { .position = position }; });

        size_t initialTriangleCount = mesh_simplification::triangles.size();
        mesh_simplification::simplify_mesh(static_cast<int>(static_cast<float>(initialTriangleCount) * targetSize), quality, false);

        positions.clear();

        positions.resize(mesh_simplification::vertices.size());
        std::transform(
            mesh_simplification::vertices.begin(), mesh_simplification::vertices.end(), positions.begin(),
            [](mesh_simplification::Vertex vertex) { return vertex.position; });

        if (!texCoord0.empty()) {
            texCoord0.clear();
            texCoord0.resize(mesh_simplification::vertices.size());
        }

        if (!texCoord1.empty()) {
            texCoord1.clear();
            texCoord1.resize(mesh_simplification::vertices.size());
        }

        if (!normal.empty()) {
            normal.clear();
            normal.resize(mesh_simplification::vertices.size());
        }

        if (!tangent.empty()) {
            tangent.clear();
            tangent.resize(mesh_simplification::vertices.size());
        }

        if (!color0.empty()) {
            color0.clear();
            color0.resize(mesh_simplification::vertices.size());
        }

        std::vector<uint16_t> indices16;
        std::vector<uint32_t> indices32;

        for (const auto& triangle : mesh_simplification::triangles) {
            if (mesh_simplification::vertices.size() >= std::numeric_limits<uint16_t>::max()) {
                indices32.push_back(triangle.v[0]);
                indices32.push_back(triangle.v[1]);
                indices32.push_back(triangle.v[2]);
            } else {
                indices16.push_back(triangle.v[0]);
                indices16.push_back(triangle.v[1]);
                indices16.push_back(triangle.v[2]);
            }

            if (!texCoord0.empty()) {
                texCoord0[triangle.v[0]] = triangle.texCoord0[0];
                texCoord0[triangle.v[1]] = triangle.texCoord0[1];
                texCoord0[triangle.v[2]] = triangle.texCoord0[2];
            }

            if (!texCoord1.empty()) {
                texCoord1[triangle.v[0]] = triangle.texCoord1[0];
                texCoord1[triangle.v[1]] = triangle.texCoord1[1];
                texCoord1[triangle.v[2]] = triangle.texCoord1[2];
            }

            if (!normal.empty()) {
                normal[triangle.v[0]] = triangle.normal[0];
                normal[triangle.v[1]] = triangle.normal[1];
                normal[triangle.v[2]] = triangle.normal[2];
            }

            if (!tangent.empty()) {
                tangent[triangle.v[0]] = triangle.tangent[0];
                tangent[triangle.v[1]] = triangle.tangent[1];
                tangent[triangle.v[2]] = triangle.tangent[2];
            }

            if (!color0.empty()) {
                color0[triangle.v[0]] = triangle.color0[0];
                color0[triangle.v[1]] = triangle.color0[1];
                color0[triangle.v[2]] = triangle.color0[2];
            }
        }

        state[path]["indices"] = state["gltf"]["accessors"].size();
        if (mesh_simplification::vertices.size() >= std::numeric_limits<uint16_t>::max()) {
            reactor::utils::createAccessorFromData(indices32, state, dataStorage);
        } else {
            reactor::utils::createAccessorFromData(indices16, state, dataStorage);
        }

        state[path]["attributes"]["POSITION"] = state["gltf"]["accessors"].size();
        reactor::utils::createAccessorFromData(positions, state, dataStorage);

        if (!texCoord0.empty()) {
            state[path]["attributes"]["TEXCOORD_0"] = state["gltf"]["accessors"].size();
            reactor::utils::createAccessorFromData(texCoord0, state, dataStorage);
        }

        if (!texCoord1.empty()) {
            state[path]["attributes"]["TEXCOORD_1"] = state["gltf"]["accessors"].size();
            reactor::utils::createAccessorFromData(texCoord1, state, dataStorage);
        }

        if (!normal.empty()) {
            state[path]["attributes"]["NORMAL"] = state["gltf"]["accessors"].size();
            reactor::utils::createAccessorFromData(normal, state, dataStorage);
        }

        if (!tangent.empty()) {
            state[path]["attributes"]["TANGENT"] = state["gltf"]["accessors"].size();
            reactor::utils::createAccessorFromData(tangent, state, dataStorage);
        }

        if (!color0.empty()) {
            state[path]["attributes"]["COLOR_0"] = state["gltf"]["accessors"].size();
            reactor::utils::createAccessorFromData(color0, state, dataStorage);
        }

        state = library.call(
            { {
                "calculate_min_max",
                { { "path", (nlohmann::json::json_pointer {} / "gltf" / "accessors" / state[path]["attributes"]["POSITION"].get<int>()).to_string() } },
            } },
            std::move(state), dataStorage, callback);

        callback({ { "message", fmt::format("Reduced triangle count from {} to {}", initialTriangleCount, mesh_simplification::triangles.size()) } });

        return state;
    };
    return operation;
}

Operation optimization::optimizeAnimation(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    Operation operation;
    operation.name = "optimize_keyframes",
    operation.schema = schema::operation(
        "Optimize Keyframes",
        "Optimize animation by removing redundant keyframe information",
        schema::object(
            { schema::parameter::path("/gltf/animations/\\d+") }));

    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        nlohmann::json result = animation::optimizeAnimation(params, state, dataStorage);

        // std::cout << "Removed keyframes: " << result["info"]["removed_keyframes"].get<uint32_t>() << std::endl;
        // std::cout << "Size before: " << result["info"]["byte_size_before"].get<uint32_t>() <<" bytes" << std::endl;
        // std::cout << "Size after: " << result["info"]["byte_size_after"].get<uint32_t>() <<" bytes"<< std::endl;

        for (auto path : result["info"]["added_accessors"]) {
            auto params = nlohmann::json::object();
            params["path"] = path;
            auto taskDescription = nlohmann::json({ { "calculate_min_max", params } });
            state = library.call(std::move(taskDescription), std::move(state), dataStorage, callback);
        }

        if (result["info"]["removed_accessors"].size() > 0) {
            auto params = nlohmann::json::object();
            params["path"] = result["info"]["removed_accessors"];
            auto taskDescription = nlohmann::json({ { "delete_accessor", params } });
            state = library.call(std::move(taskDescription), std::move(state), dataStorage, callback);
        }

        return state;
    };
    return operation;
}