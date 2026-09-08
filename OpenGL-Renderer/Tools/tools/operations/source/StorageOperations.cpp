// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "StorageOperations.h"

#include "datastorage/DataStorage.h"
#include "operations/Library.h"
#include "operations/Operations.h"
#include "operations/Schema.h"
#include "reactor/Reactor.h"
#include "reactor/Schema.h"
#include "reactor/Utils.h"
#include "utils/Base64.h"

#include "imageformats/imageformats.h"

#include <gltf_schema/gltf_single_json.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <locale>
#include <map>
#include <string>

using namespace ux3d;
using namespace ux3d::operations;
using namespace std::string_literals;

Operation storage::convertStorageType()
{
    Operation operation;
    operation.name = "convert_storage_type";

    auto parameterSchemas = nlohmann::json::array();
    parameterSchemas.push_back(
        schema::object({
            schema::parameter::path("(/gltf/images/\\d+)"),
            { "storageType", schema::string("Embedded|BufferView|File") } // Embedded, BufferView (only images), File
        }));
    parameterSchemas.push_back(
        schema::object({ schema::parameter::path("(/gltf/buffers/\\d+)"),
            { "storageType", schema::string("Embedded|File") } }));

    // fill the operation schema header
    operation.schema = schema::operation(
        "Convert Storage Type",
        "Convert storage type of an image (Embedded, BufferView, File) or buffer (Embedded, File)",
        schema::oneOf(parameterSchemas));

    // define the operation function
    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params.at("path"));
        std::string storageType = params["storageType"].get<std::string>();
        bool isImage = path.parent_pointer().back().compare("images") == 0;
        int bufferView = state.value(path / "bufferView", -1);

        if (storageType.compare("BufferView") == 0 && isImage) {
            // Store image data as BufferView
            auto& image = state.at(path);
            if (!image.contains("uri") || image.at("uri").size() == 0) {
                return state;
            }

            nlohmann::json::json_pointer dataIDPath = reactor::utils::getDataIDPath(path, state);
            uint64_t dataStorageID = state.at(dataIDPath);
            const std::vector<uint8_t>& data = datastorage.at(dataStorageID);

            auto bufferPath = reactor::utils::createBuffer(state, datastorage, data);
            auto bufferViewPath = reactor::utils::createBufferView(state, bufferPath);
            image.erase("uri");
            image["bufferView"] = stoi(bufferViewPath.back());

            auto encoding = imageformats::getEncodingFromBuffer(data);
            image["mimeType"] = imageformats::imageEncodingToString(encoding);

        } else if (storageType.compare("Embedded") == 0) {
            auto& gltfProperty = state.at(path);

            if (gltfProperty.contains("uri") && reactor::utils::isHashedDataUri(gltfProperty["uri"].get<std::string>())) {
                // Nothing to do here
                return state;
            }

            std::vector<uint8_t> data;
            nlohmann::json::json_pointer dataIDPath = reactor::utils::getDataIDPath(path, state);
            uint64_t dataStorageID = state.at(dataIDPath);
            if (isImage) {
                data = reactor::utils::getImageData(path, state, datastorage);
            } else {
                data = datastorage.at(dataStorageID);
            }

            std::string mimeType = "application/octet-stream";

            if (isImage) {
                auto encoding = imageformats::getEncodingFromBuffer(data);
                mimeType = imageformats::imageEncodingToString(encoding);
            }

            std::string uri = "data:" + mimeType + ";base64," + utils::base64::encode(data);
            std::size_t hash = std::hash<std::string> {}(uri);
            // This confuses the legacy system:
            // gltfProperty["uri"] = "data:" + mimeType + ";base64," + std::to_string(hash);
            gltfProperty["uri"] = "base64_embedded_data_" + std::to_string(hash);

            // Update files entry with new URI:
            if (bufferView != -1) {
                gltfProperty.erase("bufferView");
                dataIDPath = reactor::utils::getDataIDPath(path, state);
                state[dataIDPath] = datastorage.insert(data);
                auto taskDescription = nlohmann::json({ { "delete_bufferview", nlohmann::json::object() } });
                taskDescription["delete_bufferview"]["path"] = "/gltf/bufferViews/" + std::to_string(bufferView);
                state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
            } else {
                dataIDPath = reactor::utils::getDataIDPath(path, state);
                state[dataIDPath] = dataStorageID;
            }

        } else if (storageType.compare("File") == 0) {

            if (state.at(path).contains("uri") && !reactor::utils::isHashedDataUri(state.at(path)["uri"].get<std::string>())) {
                // Already stored as file ->  Nothing to do here
                return state;
            }
            if (state.at(path).contains("uri") && state.at(path).contains("bufferView")) {
                // Invalid
                throw std::invalid_argument("Property has uri and bufferView at the same time");
            }

            std::vector<uint8_t> data;
            nlohmann::json::json_pointer dataIDPath = reactor::utils::getDataIDPath(path, state);
            uint64_t dataStorageID = state.at(dataIDPath);
            if (isImage) {
                data = reactor::utils::getImageData(path, state, datastorage);
            } else {
                data = datastorage.at(dataStorageID);
            }

            auto encoding = imageformats::getEncodingFromBuffer(data);
            std::string fileExtension = imageformats::getFileExtension(encoding);
            if (fileExtension.empty() || !isImage) {
                fileExtension = ".bin";
            }

            state.at(path)["uri"] = reactor::utils::createURIStem(state) + fileExtension;

            // Update files entry with new URI:
            if (bufferView != -1) {
                state.at(path).erase("bufferView");
                dataIDPath = reactor::utils::getDataIDPath(path, state);
                state[dataIDPath] = datastorage.insert(data);
                auto taskDescription = nlohmann::json({ { "delete_bufferview", nlohmann::json::object() } });
                taskDescription["delete_bufferview"]["path"] = "/gltf/bufferViews/" + std::to_string(bufferView);
                state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
            } else {
                dataIDPath = reactor::utils::getDataIDPath(path, state);
                state[dataIDPath] = dataStorageID;
            }

        } else {
            throw std::invalid_argument("Invalid parameter");
        }

        return state;
    };

    return operation;
}

Operation storage::convertStorageTypeForEach()
{
    Operation operation;
    operation.name = "convert_storage_type_for_each";

    auto parameterSchemas = nlohmann::json::array();
    parameterSchemas.push_back(
        schema::object({
            schema::parameter::path("(/gltf/images)"),
            { "storageType", schema::string("Embedded|BufferView|File") } // Embedded, BufferView (only images), File
        }));
    parameterSchemas.push_back(
        schema::object({ schema::parameter::path("(/gltf/buffers)"),
            { "storageType", schema::string("Embedded|File") } }));

    // fill the operation schema header
    operation.schema = schema::operation(
        "Convert Storage Type for All Images or Buffers",
        "Convert storage type for all images (Embedded, BufferView, File) or buffers (Embedded, File)",
        schema::oneOf(parameterSchemas));

    // define the operation function
    operation.operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        auto path = nlohmann::json::json_pointer(params.at("path"));
        std::string pathPattern = "";
        if (path.to_string() == "/gltf/images") {
            pathPattern = "/gltf/images/\\d+";
            if (state["gltf"].contains("images") == false) {
                return state;
            }
        }
        if (path.to_string() == "/gltf/buffers") {
            pathPattern = "/gltf/buffers/\\d+";
            if (state["gltf"].contains("buffers") == false) {
                return state;
            }
        }

        auto taskDescription = nlohmann::json({ { "dangerous.forEach", nlohmann::json::object() } });
        taskDescription["dangerous.forEach"]["operation"] = "convert_storage_type";
        taskDescription["dangerous.forEach"]["pathPattern"] = pathPattern;
        taskDescription["dangerous.forEach"]["parameters"] = params;

        state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);

        return state;
    };

    return operation;
}
