// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "HelperFunctions.h"

#include "PropertyOperations.h"
#include "StorageOperations.h"
#include "datastorage/DataStorage.h"
#include "operations/Library.h"
#include "operations/Operations.h"
#include "operations/Schema.h"
#include "reactor/DataUtils.h"
#include "reactor/Gltf.h"
#include "reactor/Reactor.h"
#include "reactor/Utils.h"

#include "imageformats/imageformats.h"
#include "utils/Base64.h"

#include <gltf_schema/gltf_single_json.h>

#include <exception>
#include <filesystem>
#include <fstream>
#include <gtx/matrix_decompose.hpp>
#include <sstream>
#include <string>

using namespace ux3d;
using namespace ux3d::operations;
using namespace std::string_literals;

std::string file::normalizeURI(std::string uri)
{
    // See https://github.com/KhronosGroup/glTF/issues/1449
    reactor::utils::searchAndReplace(uri, "%20", " ");
    uri = std::filesystem::path(uri).generic_string();
    reactor::utils::searchAndReplace(uri, "\\", "/");
    return uri;
}

std::vector<uint8_t> file::readFile(std::filesystem::path filepath)
{
    std::ifstream file(filepath, std::ios::binary);
    if (file.fail()) {
        throw std::invalid_argument("Could not open file");
    }
    file.unsetf(std::ios::skipws);

    std::streampos byteLength;
    file.seekg(0, std::ios::end);
    byteLength = file.tellg();
    file.seekg(0, std::ios::beg);
    if (file.fail()) {
        throw std::invalid_argument("Could not read file");
    }
    std::vector<uint8_t> data;
    data.reserve(byteLength);
    data.insert(data.begin(), std::istream_iterator<uint8_t>(file), std::istream_iterator<uint8_t>());
    file.close();

    return data;
}

void file::loadBufferDataToDataStorage(datastorage::DataStorage& datastorage, nlohmann::json& state, std::filesystem::path gltfDir, std::optional<std::filesystem::path> resourceDir)
{
    if (!reactor::utils::hasElement(state, { "gltf", "buffers" })) {
        return;
    }

    auto& buffers = state["gltf"]["buffers"];
    for (uint32_t i = 0; i < buffers.size(); i++) {
        auto& buffer = buffers[i];

        if (!buffer.contains("uri") || buffer.at("uri").size() == 0) {
            continue;
        }

        std::string uri = buffer["uri"].get<std::string>();
        std::vector<uint8_t> data;
        if (reactor::utils::isDataUri(uri)) {
            if (uri.find(";base64,") == std::string::npos) {
                continue;
            }

            size_t pos = uri.find(";base64,");
            pos += strlen(";base64,");
            std::string dataString = uri.substr(pos);
            utils::base64::decode(dataString, data);

            std::size_t hash = std::hash<std::string> {}(uri);
            buffer["uri"] = "base64_embedded_data_" + std::to_string(hash);
        } else {
            uri = file::normalizeURI(uri);
            buffer["uri"] = uri;

            std::filesystem::path bufferPath = gltfDir / uri;

            if (!std::filesystem::exists(bufferPath) && resourceDir.has_value()) {
                bufferPath = resourceDir.value() / uri; // Use resource dir as alternative search path
            }
            if (!std::filesystem::exists(bufferPath)) {
                // ToDo: if we introduce a debug log, we should report missing resources
                continue;
            }
            try {
                data = readFile(bufferPath);
            } catch (std::exception e) {
                continue;
            }
        }

        auto dataIDPath = reactor::utils::getDataIDPath(nlohmann::json::json_pointer("/gltf/buffers") / i, state);

        datastorage::DataStorage::ID dataStorageID = datastorage.insert(data);
        state[dataIDPath] = dataStorageID;
    }
}

void file::loadImageDataToDataStorage(datastorage::DataStorage& datastorage, nlohmann::json& state, std::filesystem::path gltfDir, std::optional<std::filesystem::path> resourceDir)
{
    if (!reactor::utils::hasElement(state, { "gltf", "images" })) {
        return;
    }

    auto& images = state["gltf"]["images"];
    for (uint32_t i = 0; i < images.size(); i++) {
        auto& image = images[i];

        if (!image.contains("uri") || image.at("uri").size() == 0) {
            continue;
        }
        std::string uri = image["uri"].get<std::string>();
        std::vector<uint8_t> data;

        if (reactor::utils::isDataUri(uri)) {
            if (uri.find(";base64,") == std::string::npos) {
                continue;
            }

            size_t pos = uri.find(";base64,");
            pos += strlen(";base64,");
            std::string dataString = uri.substr(pos);
            utils::base64::decode(dataString, data);
            std::size_t hash = std::hash<std::string> {}(uri);
            image["uri"] = "base64_embedded_data_" + std::to_string(hash);

        } else {
            uri = file::normalizeURI(uri);
            image["uri"] = uri;
            std::filesystem::path imagePath = gltfDir / uri;

            if (!std::filesystem::exists(imagePath) && resourceDir.has_value()) {
                imagePath = resourceDir.value() / uri; // Use resource dir as alternative search path
            }
            if (!std::filesystem::exists(imagePath)) {
                // ToDo: if we introduce a debug log, we should report missing resources
                // For example add a placeholder image as embedded string to the uri
                continue;
            }
            try {
                data = readFile(imagePath);
            } catch (std::exception e) {
                continue;
            }
            // If somehow the loaded uri uses the same prefix we use for embedded
            if (reactor::utils::isHashedDataUri(uri)) {
                image.at("uri") = reactor::utils::createURIStem(state);
            }
        }

        auto dataIDPath = reactor::utils::getDataIDPath(nlohmann::json::json_pointer("/gltf/images") / i, state);
        datastorage::DataStorage::ID dataStorageID = datastorage.insert(data);
        state[dataIDPath] = dataStorageID;
    }
}

nlohmann::json file::loadGltf(const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library)
{
    std::filesystem::path filepath(params["filepath"].get<std::string>());
    std::ifstream filestream(filepath, std::ios::in);
    if (filestream.bad()) {
        throw std::invalid_argument("File unreadable");
    }

    std::ostringstream stringStream {};
    filestream >> stringStream.rdbuf();

    if (filestream.fail() && !filestream.eof()) {
        throw std::invalid_argument("File unreadable");
    }
    filestream.close();

    nlohmann::json gltf = nlohmann::json::parse(stringStream.str(), nullptr, true);
    state["gltf"] = gltf;

    std::optional<std::filesystem::path> resourceDir = std::nullopt;
    if (params.contains("resourceDirectory")) {
        resourceDir = params.at("resourceDirectory").get<std::string>();
    }
    loadBufferDataToDataStorage(datastorage, state, filepath.parent_path(), resourceDir);
    loadImageDataToDataStorage(datastorage, state, filepath.parent_path(), resourceDir);

    return state;
}

void file::writeFile(std::filesystem::path filepath, const std::vector<uint8_t>& data, bool overwrite)
{
    if (std::filesystem::exists(filepath) && !overwrite) {
        throw std::invalid_argument("File exists and \"overwrite\" flag is not set");
    }

    if (filepath.has_parent_path() && !std::filesystem::exists(filepath.parent_path())) {
        if (!std::filesystem::create_directories(filepath.parent_path())) {
            throw std::invalid_argument("Unable to create directories");
        }
    }

    std::ofstream file(filepath, std::ios::binary);
    if (file.fail()) {
        throw std::runtime_error("Could not open file");
    }
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    if (file.fail()) {
        file.close();
        throw std::runtime_error("Could not write file");
    }
    file.close();
}

void file::writeFile(std::filesystem::path filepath, const std::string& data, bool overwrite)
{
    if (std::filesystem::exists(filepath) && !overwrite) {
        throw std::invalid_argument("File exists and \"overwrite\" flag is not set");
    }

    if (filepath.has_parent_path() && !std::filesystem::exists(filepath.parent_path())) {
        if (!std::filesystem::create_directories(filepath.parent_path())) {
            throw std::invalid_argument("Unable to create directories");
        }
    }

    std::ofstream filestream(filepath);
    if (filestream.fail()) {
        throw std::runtime_error("Could not open file");
    }
    filestream << data;
    if (filestream.fail()) {
        filestream.close();
        throw std::runtime_error("Could not write file");
    }
    filestream.close();
}

nlohmann::json file::writeBufferDataToFile(datastorage::DataStorage& datastorage, nlohmann::json& state, std::filesystem::path gltfDir, bool overwrite)
{
    nlohmann::json result = nlohmann::json::array();
    if (!state["gltf"].contains("buffers")) {
        return result;
    }

    auto& buffers = state["gltf"]["buffers"];
    for (uint32_t i = 0; i < buffers.size(); i++) {
        auto& buffer = buffers[i];

        if (!buffer.contains("uri") || buffer.at("uri").size() == 0) {
            continue;
        }
        std::string uri = buffer["uri"].get<std::string>();
        if (reactor::utils::isDataUri(uri)) {
            continue;
        }

        nlohmann::json::json_pointer dataIDPath = reactor::utils::getDataIDPath(nlohmann::json::json_pointer("/gltf/buffers") / i, state);
        if (!state.contains(dataIDPath)) {
            result.push_back("Could not find data for /buffers/" + std::to_string(i));
            continue;
        }
        uint64_t dataStorageID = state.at(dataIDPath);
        if (!datastorage.contains(dataStorageID)) {
            result.push_back("Could not find data for /buffers/" + std::to_string(i));
            continue;
        }
        const std::vector<uint8_t>& data = datastorage.at(dataStorageID);

        std::filesystem::path filePath = gltfDir / uri;

        try {
            writeFile(filePath, data, overwrite);
        } catch (std::exception e) {
            result.push_back(filePath.string() + ": " + e.what());
        }
    }
    return result;
}

nlohmann::json file::writeImageDataToFile(datastorage::DataStorage& datastorage, nlohmann::json& state, std::filesystem::path gltfDir, bool overwrite)
{
    nlohmann::json result = nlohmann::json::array();
    if (!state["gltf"].contains("images")) {
        return result;
    }

    auto& images = state["gltf"]["images"];
    for (uint32_t i = 0; i < images.size(); i++) {
        auto& image = images[i];

        if (!image.contains("uri") || image.at("uri").size() == 0) {
            continue;
        }
        std::string uri = image["uri"].get<std::string>();
        if (reactor::utils::isDataUri(uri)) {
            continue; // embedded data is not stored as file
        }

        auto dataIDPath = reactor::utils::getDataIDPath(nlohmann::json::json_pointer("/gltf/images") / i, state);
        if (!state.contains(dataIDPath)) {
            result.push_back("Could not find data for /images/" + std::to_string(i));
            continue;
        }
        uint64_t dataStorageID = state.at(dataIDPath);
        if (!datastorage.contains(dataStorageID)) {
            result.push_back("Could not find data for /images/" + std::to_string(i));
            continue;
        }
        const std::vector<uint8_t>& data = datastorage.at(dataStorageID);

        std::filesystem::path filePath = gltfDir / uri;
        try {
            writeFile(filePath, data, overwrite);
        } catch (std::exception e) {
            result.push_back(filePath.string() + ": " + e.what());
        }
    }
    return result;
}

void file::substituteEmbeddedMarkers(nlohmann::json& state, datastorage::DataStorage& datastorage)
{
    if (state["gltf"].contains("images")) {
        auto& images = state["gltf"]["images"];
        for (uint32_t i = 0; i < images.size(); i++) {
            auto& image = images[i];

            if (!image.contains("uri") || image.at("uri").size() == 0) {
                continue;
            }
            std::string uri = image["uri"].get<std::string>();
            if (!reactor::utils::isHashedDataUri(uri)) {
                continue;
            }

            nlohmann::json::json_pointer dataIDPath = reactor::utils::getDataIDPath(nlohmann::json::json_pointer("/gltf/images") / i, state);
            if (!state.contains(dataIDPath)) {
                continue;
            }
            uint64_t dataStorageID = state.at(dataIDPath);
            const std::vector<uint8_t>& data = datastorage.at(dataStorageID);

            auto encoding = imageformats::getEncodingFromBuffer(data);
            std::string mimeType = imageformats::imageEncodingToString(encoding);
            image["uri"] = "data:" + mimeType + ";base64," + utils::base64::encode(data);
        }
    }
    if (state["gltf"].contains("buffers")) {
        auto& buffers = state["gltf"]["buffers"];
        for (uint32_t i = 0; i < buffers.size(); i++) {
            auto& buffer = buffers[i];

            if (!buffer.contains("uri") || buffer.at("uri").size() == 0) {
                continue;
            }
            std::string uri = buffer["uri"].get<std::string>();
            if (!reactor::utils::isHashedDataUri(uri)) {
                continue;
            }

            nlohmann::json::json_pointer dataIDPath = reactor::utils::getDataIDPath(nlohmann::json::json_pointer("/gltf/buffers") / i, state);
            if (!state.contains(dataIDPath)) {
                continue;
            }
            uint64_t dataStorageID = state.at(dataIDPath);
            const std::vector<uint8_t>& data = datastorage.at(dataStorageID);

            auto encoding = imageformats::getEncodingFromBuffer(data);
            std::string mimeType = "application/octet-stream";
            buffer["uri"] = "data:" + mimeType + ";base64," + utils::base64::encode(data);
        }
    }
}

void file::convertGlbBuffer(nlohmann::json& state, datastorage::DataStorage& datastorage)
{
    if (!state["gltf"].contains("buffers")) {
        return;
    }
    if (state["gltf"].at("buffers").size() == 0) {
        return;
    }

    auto& buffer = state["gltf"].at("buffers").at(0); // only first buffer can be a glb buffer
    if (!buffer.contains("uri") || buffer.at("uri").get<std::string>().size() == 0) {
        nlohmann::json::json_pointer dataIDPath = reactor::utils::getDataIDPath(buffer, state);
        uint64_t dataStorageID = state.at(dataIDPath);
        state.erase(dataIDPath);
        buffer["uri"] = reactor::utils::createURIStem(state) + ".bin";
        state[reactor::utils::getDataIDPath(buffer, state)] = dataStorageID;
    }
}

nlohmann::json file::saveGltf(const nlohmann::json& params, nlohmann::json state, datastorage::DataStorage& datastorage, const Library& library)
{
    std::filesystem::path filepath(params["filepath"].get<std::string>());
    if (!state.contains("gltf")) {
        throw std::invalid_argument("Invalid glTF");
    }

    bool overwrite = false;
    if (params.contains("overwrite")) {
        overwrite = params["overwrite"].get<bool>();
    }

    if (std::filesystem::exists(filepath) && !overwrite) {
        throw std::invalid_argument("File exists and \"overwrite\" flag is not set");
    }

    convertGlbBuffer(state, datastorage);
    substituteEmbeddedMarkers(state, datastorage);

    std::string gltfString = state["gltf"].dump(1, '\t', false, nlohmann::json::error_handler_t::ignore);

    uint64_t fileSize = gltfString.length();
    fileSize += reactor::data::getDataSize(state, datastorage);

    nlohmann::json result = nlohmann::json::array();
    try {
        writeFile(filepath, gltfString, overwrite);
    } catch (std::exception e) {
        result.push_back(filepath.string() + ": " + e.what());
    }
    auto result2 = writeBufferDataToFile(datastorage, state, filepath.parent_path(), overwrite);
    auto result3 = writeImageDataToFile(datastorage, state, filepath.parent_path(), overwrite);

    result.insert(result.end(), result2.begin(), result2.end());
    result.insert(result.end(), result3.begin(), result3.end());

    return result;
}

struct GlbHeader {
    uint32_t magic = 0;
    uint32_t version = 0;
    uint32_t length = 0;
};

struct ChunkHeader {
    uint32_t length = 0;
    uint32_t type = 0;
};

nlohmann::json file::loadGlb(const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& datastorage, const Library& library)
{
    std::filesystem::path filepath(params["filepath"].get<std::string>());

    if (!std::filesystem::exists(filepath)) {
        return state;
    }

    std::fstream glbStream;
    glbStream.open(filepath, std::fstream::in | std::fstream::binary);
    if (glbStream.fail()) {
        throw std::invalid_argument("Cannot open glb file");
    }
    glbStream.unsetf(std::ios::skipws);
    glbStream.seekg(0, glbStream.end);
    size_t fileSize = glbStream.tellg();

    // Read and verify Header
    {
        glbStream.seekg(0, glbStream.beg);
        if (fileSize < sizeof(GlbHeader) + sizeof(ChunkHeader)) {
            throw std::invalid_argument("Invalid glb file");
        }
        GlbHeader glbHeader;
        glbStream.read((char*)&glbHeader, sizeof(GlbHeader));
        std::string typeString(reinterpret_cast<char*>(&glbHeader.magic), 4);
        if (typeString.compare("glTF") != 0 || glbHeader.length != fileSize) {
            throw std::invalid_argument("Invalid glb file");
        }
    }

    // Read Json
    {

        // glbStream.seekg(sizeof(GlbHeader), std::ios::beg);
        ChunkHeader jsonChunkHeader;
        glbStream.read((char*)&jsonChunkHeader, sizeof(ChunkHeader));

        std::string typeString(reinterpret_cast<char*>(&jsonChunkHeader.type), 4);
        if (typeString.compare("JSON") != 0) {
            throw std::invalid_argument("Invalid glb file");
        }
        if (static_cast<size_t>(glbStream.tellg()) + jsonChunkHeader.length > fileSize) {
            throw std::invalid_argument("Invalid glb file");
        }

        std::vector<uint8_t> jsonData(jsonChunkHeader.length);
        glbStream.read((char*)jsonData.data(), jsonChunkHeader.length);

        std::string jsonString(jsonData.begin(), jsonData.end());
        nlohmann::json gltf = nlohmann::json::parse(jsonString, nullptr, true);
        state["gltf"] = gltf;
    }

    auto readBufferChunk = [&glbStream, &state](uint32_t bufferIdx) -> std::vector<uint8_t> {
        ChunkHeader chunkHeader;
        glbStream.read((char*)&chunkHeader, sizeof(ChunkHeader));

        std::string typeString(reinterpret_cast<char*>(&chunkHeader.type), 3);
        if (typeString.compare("BIN") != 0) {
            throw std::invalid_argument("Invalid glb file");
        }

        uint64_t bufferByteLength = state.at(nlohmann::json::json_pointer("/gltf/buffers") / bufferIdx).at("byteLength").get<uint64_t>();
        if (bufferByteLength != chunkHeader.length
            && bufferByteLength + (4 - (bufferByteLength % 4)) != chunkHeader.length) { // consider 4 byte alignment
            throw std::invalid_argument("Invalid glb file");
        }

        std::vector<uint8_t> data(bufferByteLength);
        glbStream.read((char*)data.data(), bufferByteLength);

        if (!reactor::utils::hasElement(state, { "gltf", "buffers", 0 })) {
            throw std::invalid_argument("Invalid glb file");
        }
        return data;
    };

    // Read optional buffer
    if (state["gltf"].contains("buffers")) {
        for (int i = 0; i < state.at(nlohmann::json::json_pointer("/gltf/buffers")).size(); i++) {
            if (static_cast<size_t>(glbStream.tellg()) + sizeof(ChunkHeader) < fileSize) {

                auto data = readBufferChunk(i);

                if (i > 0) {
                    // this is no longer a standard glb file
                    // we have to adapt so that all buffers are stored correctly
                    state.at(nlohmann::json::json_pointer("/gltf/buffers") / i)["uri"] = "glb_buffer_" + std::to_string(i);
                }

                // Insert into datastorage
                nlohmann::json::json_pointer dataIDPath
                    = reactor::utils::getDataIDPath(nlohmann::json::json_pointer("/gltf/buffers") / i, state);
                datastorage::DataStorage::ID dataStorageID = datastorage.insert(data);
                state[dataIDPath] = dataStorageID;

            } else {
                throw std::invalid_argument("Invalid glb file");
            }
        }
    }

    if (filepath.extension() == ".gestalt") {
        gltf::removePropertiesFromGltf(state.at("gltf"), { { "property", "ux3d_gestaltor" } }, true);
    }

    return state;
}

nlohmann::json file::saveGlb(const nlohmann::json& params, nlohmann::json state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback)
{
    nlohmann::json result = nlohmann::json::array();
    std::filesystem::path filepath(params["filepath"].get<std::string>());
    if (!state.contains("gltf")) {
        throw std::invalid_argument("Invalid glTF");
    }

    bool overwrite = false;
    if (params.contains("overwrite")) {
        overwrite = params["overwrite"].get<bool>();
    }

    if (std::filesystem::exists(filepath) && !overwrite) {
        throw std::invalid_argument("File exists and \"overwrite\" flag is not set");
    }

    // We need to prepare the gltf in order to save it as a glb file
    // All images have to be stored as bufferviews and we want to have one big buffer
    // For this, we operate on a copy of our state and datastorage

    // Store all images in buffers
    if (state["gltf"].contains("images")) {
        auto taskDescription = nlohmann::json({ { "convert_storage_type_for_each", nlohmann::json::object() } });
        taskDescription["convert_storage_type_for_each"]["path"] = "/gltf/images";
        taskDescription["convert_storage_type_for_each"]["storageType"] = "BufferView";
        state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
    }

    // avoid storing buffers as embedded uri
    auto taskDescription = nlohmann::json({ { "convert_storage_type_for_each", nlohmann::json::object() } });
    taskDescription["convert_storage_type_for_each"]["path"] = "/gltf/buffers";
    taskDescription["convert_storage_type_for_each"]["storageType"] = "File";
    state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);

    // Merge all buffers into one big buffer
    if (state["gltf"].contains("buffers")) {
        int32_t bufferCount = state["gltf"]["buffers"].size();
        for (int32_t i = bufferCount - 1; i >= 0; i--) {
            // trim in reverse order as they can be removed when empty
            auto taskDescription = nlohmann::json({ { "trim_buffer", nlohmann::json::object() } });
            taskDescription["trim_buffer"]["path"] = "/gltf/buffers/" + std::to_string(i);
            state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
        }
        if (state["gltf"]["buffers"].size() > 1) {
            auto taskDescription = nlohmann::json({ { "merge_buffers", nlohmann::json::object() } });
            taskDescription["merge_buffers"]["bufferPaths"] = nlohmann::json::array();
            // Add all buffers
            for (int i = 0; i < state["gltf"]["buffers"].size(); i++) {
                taskDescription["merge_buffers"]["bufferPaths"].push_back("/gltf/buffers/" + std::to_string(i));
            }
            state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
        }
    }

    std::vector<uint8_t> binaryData;

    if (state["gltf"].contains("buffers")) {
        // prepare binary data:
        nlohmann::json::json_pointer dataIDPath = reactor::utils::getDataIDPath(nlohmann::json::json_pointer("/gltf/buffers/0"), state);
        uint64_t dataStorageID = state.at(dataIDPath);
        binaryData = datastorage.at(dataStorageID);
        while (binaryData.size() % 4 != 0) { // ensure 4-byte alignment:
            binaryData.emplace_back(uint8_t(0));
        }

        if (state["gltf"].contains("buffers") && state["gltf"]["buffers"][0].contains("uri")) {
            state["gltf"]["buffers"][0].erase("uri");
        }
    }

    // prepare json data
    std::string gltfString = "";
    gltfString = state["gltf"].dump(-1, ' ', false, nlohmann::json::error_handler_t::ignore);
    while (gltfString.length() % 4 != 0) { // ensure 4-byte alignment:
        gltfString += " ";
    }

    // calculate sizes
    size_t jsonChunkSize = sizeof(ChunkHeader) + sizeof(char) * gltfString.length();
    size_t bufferChunkSize = sizeof(ChunkHeader) + sizeof(uint8_t) * binaryData.size();
    size_t glbSize = sizeof(GlbHeader) + jsonChunkSize;
    if (!binaryData.empty()) {
        glbSize += bufferChunkSize;
    }

    // Start writing
    std::ofstream file(filepath, std::ios::binary);
    if (file.fail()) {
        throw std::invalid_argument("Could not open file");
    }

    {
        GlbHeader glbHeader;
        glbHeader.length = uint32_t(glbSize);
        glbHeader.version = 2;
        glbHeader.magic = 0x46546C67;
        file.write(reinterpret_cast<const char*>(&glbHeader), sizeof(GlbHeader));
        if (file.fail()) {
            throw std::runtime_error("Could not write glb header");
        }
    }
    {
        ChunkHeader chunkHeader;
        chunkHeader.length = uint32_t(sizeof(char) * gltfString.length());
        chunkHeader.type = 0x4E4F534A;

        file.write(reinterpret_cast<const char*>(&chunkHeader), sizeof(ChunkHeader));
        file.write(reinterpret_cast<const char*>(gltfString.c_str()), chunkHeader.length);
        if (file.fail()) {
            result.push_back("Could not write gltf");
        }
    }
    if (!binaryData.empty()) {
        ChunkHeader chunkHeader;
        chunkHeader.length = uint32_t(sizeof(uint8_t) * binaryData.size());
        chunkHeader.type = 0x004E4942;

        file.write(reinterpret_cast<const char*>(&chunkHeader), sizeof(ChunkHeader));
        file.write(reinterpret_cast<const char*>(binaryData.data()), chunkHeader.length);
        if (file.fail()) {
            result.push_back("Could not write binary data");
        }
    }
    file.close();
    return result;
}

// markSubtreeObjects adds a "ux3d_subtreeObject" object to all child of root objects that belong to the subtreeRoot's subtree
void file::markSubtreeObjects(const nlohmann::json& linearizedSchema, nlohmann::json& gltf, nlohmann::json::json_pointer subtreePointer, bool isRootObject)
{
    // throws if pointer is invalid
    nlohmann::json& subJson = gltf.at(subtreePointer);
    if (isRootObject) {
        subJson["ux3d_subtreeObject"] = true;
    }

    std::string regexPath = reactor::gltf::substituteIndices(subtreePointer.to_string());
    if (linearizedSchema.contains(regexPath) == false) {
        return;
    }
    const auto& subSchema = linearizedSchema.at(regexPath);

    // traverse into json objects with children (non primitives)
    if (false == subJson.is_primitive()) {
        // traverse all parameters
        for (const auto& [key, value] : subJson.items()) {
            markSubtreeObjects(linearizedSchema, gltf, subtreePointer / key);
        }
    }

    if (subSchema.contains("ux3d_pathPrefix")) {
        std::string resolvedPrefix = reactor::gltf::applyParentToReference(subtreePointer.to_string(), subSchema.at("ux3d_pathPrefix"));
        auto resolvedSubJsonPointer = nlohmann::json::json_pointer(resolvedPrefix) / gltf.at(subtreePointer).get<int>();
        if (gltf.contains(resolvedSubJsonPointer)) {
            gltf[resolvedSubJsonPointer]["ux3d_subtreeObject"] = true;
            markSubtreeObjects(linearizedSchema, gltf, resolvedSubJsonPointer);
        }
    }
}

// reduceGltfToSubtreeObjects deletes all child of root objects that do not have the "ux3d_subtreeObject" object
nlohmann::json file::reduceGltfToSubtreeObjects(nlohmann::json&& state, datastorage::DataStorage& datastorage, const nlohmann::json& linearizedGltfSchema, const Library& library, std::function<void(const nlohmann::json&)> callback)
{
    // deleteLater contains arrays and objects that are empty after these operations. These are deleted from the glTF afterwards
    std::vector<std::tuple<nlohmann::json::json_pointer, std::string>> deleteLater;
    // removeLater contains pointers that should be removed
    std::vector<nlohmann::json::json_pointer> removeLater;

    // traverse all regular child of root objects
    for (const auto& [key, value] : state["gltf"].items()) {
        if (!pointsToChildOfRoot(key, linearizedGltfSchema)) {
            continue;
        }

        for (int i = value.size() - 1; i >= 0; --i) {
            nlohmann::json& childOfRoot = value.at(i);

            if (childOfRoot.contains("ux3d_subtreeObject")) {
                // remove the helper entry, keep the object
                childOfRoot.erase("ux3d_subtreeObject");
            } else {
                // this object is not contained in the subtree and should be removed
                auto removeAtPointer = nlohmann::json::json_pointer("/gltf") / key / std::to_string(i);
                removeLater.push_back(removeAtPointer);
            }
        }
    }

    // traverse all extension child of root objects
    if (state["gltf"].contains("extensions")) {
        std::vector<std::string> extensionsRemoved;

        for (auto& [key, value] : state["gltf"]["extensions"].items()) {
            int childDeleteCount = 0;

            for (const auto& [extensionArrayKey, extensionArrayValue] : value.items()) {
                for (int i = extensionArrayValue.size() - 1; i >= 0; --i) { // auto& [extensionKey, extensionValue] : extensionArrayValue.items()) {
                    auto& extensionValue = extensionArrayValue[i];
                    if (!pointsToChildOfRoot("extensions/" + key + "/" + extensionArrayKey, linearizedGltfSchema) && key != "KHR_xmp_json_ld") {
                        continue;
                    }

                    if (extensionValue.contains("ux3d_subtreeObject")) {
                        // remove the helper entry, keep the object
                        extensionValue.erase("ux3d_subtreeObject");
                    } else {
                        // this object is not contained in the subtree and should be removed
                        if (key == "KHR_xmp_json_ld") {
                            // handle xmp explicitly because it does not use child of root objects
                            // xmp does not have any references to other objects itself, so it can be deleted in a basic fashion
                            state[nlohmann::json::json_pointer("/gltf/extensions/" + key + "/" + extensionArrayKey)].erase(i);

                        } else {
                            auto removeAtPointer = nlohmann::json::json_pointer("/gltf") / "extensions" / key / extensionArrayKey / std::to_string(i);
                            removeLater.push_back(removeAtPointer);
                        }
                    }
                }
            }
        }
    }

    // Remove all objects that are not part of the subtree. This has to happen after gathering
    // the removeLater candidates to avoid breaking the iterators
    for (const auto& removeAtPointer : removeLater) {
        auto removeObjectDescription = nlohmann::json({ { "remove", nlohmann::json::object() } });
        removeObjectDescription["remove"]["path"] = removeAtPointer.to_string();
        state = library.call(std::move(removeObjectDescription), std::move(state), datastorage, callback);
    }
    removeLater.clear();
    // Cleanup empty arrays
    for (const auto& [key, value] : state["gltf"].items()) {
        if (!pointsToChildOfRoot(key, linearizedGltfSchema)) {
            continue;
        }
        if (value.size() == 0) {
            deleteLater.push_back({ nlohmann::json::json_pointer("/gltf"), key });
        }
    }
    // Cleanup empty extension arrays
    if (state["gltf"].contains("extensions")) {
        std::vector<std::string> extensionsRemoved;
        // delete extensions from the extension arrays
        for (auto& [key, value] : state["gltf"]["extensions"].items()) {
            int childDeleteCount = 0;

            for (const auto& [extensionArrayKey, extensionArrayValue] : value.items()) {
                if (extensionArrayValue.size() == 0) {
                    deleteLater.push_back({ nlohmann::json::json_pointer("/gltf/extensions") / key, extensionArrayKey });
                    childDeleteCount++;
                }
            }

            if (childDeleteCount == value.size()) {
                deleteLater.push_back({ nlohmann::json::json_pointer("/gltf/extensions"), key });
                extensionsRemoved.push_back(key);
            }
        }
        // delete entries in extensionsUsed and extensionsRequired
        for (const std::string& extensionKey : extensionsRemoved) {
            if (state["gltf"].contains("extensionsUsed")) {
                nlohmann::json& extensionsUsed = state["gltf"]["extensionsUsed"];
                auto iterator = std::find(extensionsUsed.begin(), extensionsUsed.end(), extensionKey);
                if (iterator != extensionsUsed.end()) {
                    state["gltf"]["extensionsUsed"].erase(iterator);
                }
            }
            if (state["gltf"].contains("extensionsRequired")) {
                nlohmann::json& extensionsRequired = state["gltf"]["extensionsRequired"];
                auto iterator = std::find(extensionsRequired.begin(), extensionsRequired.end(), extensionKey);
                if (iterator != extensionsRequired.end()) {
                    state["gltf"]["extensionsRequired"].erase(iterator);
                }
            }
        }
    }

    // Finally delete all gathered objects
    for (auto& [pointer, objectKey] : deleteLater) {
        if (state.contains(pointer / objectKey)) {
            state[pointer].erase(objectKey);
        }
    }

    // Ensure the output doesn't contain empty "extensions", "extensionsUsed" and "extensionsRequired" arrays
    if (state["gltf"].contains("extensions") && state["gltf"]["extensions"].empty()) {
        state["gltf"].erase("extensions");
    }
    if (state["gltf"].contains("extensionsUsed") && state["gltf"]["extensionsUsed"].empty()) {
        state["gltf"].erase("extensionsUsed");
    }
    if (state["gltf"].contains("extensionsRequired") && state["gltf"]["extensionsRequired"].empty()) {
        state["gltf"].erase("extensionsRequired");
    }

    return state;
}

void utility::getExtensionsAtPath(nlohmann::json::json_pointer path, const nlohmann::json& state, std::vector<std::string>& foundExtensions, std::map<std::string, std::vector<nlohmann::json::json_pointer>>* extensionPaths)
{
    // Check if we are currently in an extension object and add it to the vector
    if (!state.contains(path)) {
        return;
    }
    if (!path.parent_pointer().empty() && path.parent_pointer().back() == "extensions") {
        if (std::find(foundExtensions.begin(), foundExtensions.end(), path.back()) == foundExtensions.end()) {
            foundExtensions.push_back(path.back());
        }
        if (extensionPaths != nullptr) {
            // Try to emplace a vector with current path
            std::vector<nlohmann::json::json_pointer> test = { path };
            auto result = extensionPaths->try_emplace(path.back(), test);
            // If a vector already exists, push back path
            if (!result.second) {
                result.first->second.push_back(path);
            }
        }
    }
    if (state.at(path).is_object() || state.at(path).is_array()) {
        // Go into properties recursivly
        for (auto& [key, value] : state.at(path).items()) {
            if (value.is_object() || value.is_array()) {
                for (auto& [innerKey, innerValue] : state.at(path / key).items()) {
                    getExtensionsAtPath(path / key / innerKey, state, foundExtensions, extensionPaths);
                }
            }
        }
    }
}

// pointsToChildOfRoot return true if the array with the given path contains child of root objects
bool file::pointsToChildOfRoot(const std::string& pathToArray, const nlohmann::json& linearizedGltfSchema)
{
    const std::string regexPath = "/" + pathToArray + "/\\d+";
    if (linearizedGltfSchema.contains(regexPath)) {
        const auto& subSchema = linearizedGltfSchema.at(regexPath);
        if (subSchema.contains("allOf")) {
            for (const auto& [key, value] : subSchema["allOf"].items()) {
                if (value.contains("title") && value["title"].get<std::string>() == "glTF Child of Root Property") {
                    return true;
                }
            }
        }
    }

    return false;
}

glm::dquat utility::eulerToQuaternion(glm::dvec3 angle)
{
    glm::vec3 radians = glm::radians(angle);
    // code from http://www.euclideanspace.com/maths/geometry/rotations/conversions/eulerToQuaternion/index.htm
    // Assuming the angles are in radians.
    // the math uses a different arangement for x,y,z than we do. Therefor the axis have to be reordered
    double c1 = cos(radians.y / 2);
    double s1 = sin(radians.y / 2);
    double c2 = cos(radians.z / 2);
    double s2 = sin(radians.z / 2);
    double c3 = cos(radians.x / 2);
    double s3 = sin(radians.x / 2);
    double c1c2 = c1 * c2;
    double s1s2 = s1 * s2;

    double w = c1c2 * c3 - s1s2 * s3;
    double x = c1c2 * s3 + s1s2 * c3;
    double y = s1 * c2 * c3 + c1 * s2 * s3;
    double z = c1 * s2 * c3 - s1 * c2 * s3;

    return glm::quat(w, x, y, z);
}

glm::dvec3 utility::quaternionToEuler(glm::dquat q)
{
    // code taken from http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToEuler/
    // the math uses a different arangement for x,y,z than we do. Therefor the axis have to be reordered
    double sqw = q.w * q.w;
    double sqx = q.x * q.x;
    double sqy = q.y * q.y;
    double sqz = q.z * q.z;
    double unit = sqx + sqy + sqz + sqw; // if normalised is one, otherwise is correction factor
    double test = q.x * q.y + q.z * q.w;
    double heading = atan2(2 * q.y * q.w - 2 * q.x * q.z, sqx - sqy - sqz + sqw);
    double attitude = asin(2 * test / unit);
    double bank = atan2(2 * q.x * q.w - 2 * q.y * q.z, -sqx + sqy - sqz + sqw);
    if (test > 0.499 * unit) { // singularity at north pole
        heading = 2 * atan2(q.x, q.w);
        attitude = math::pi<double>() / 2;
        bank = 0;
    } else if (test < -0.499 * unit) { // singularity at south pole
        heading = -2 * atan2(q.x, q.w);
        attitude = -math::pi<double>() / 2;
        bank = 0;
    }

    glm::dvec3 euler(bank, heading, attitude);
    return glm::degrees(euler);
}

void utility::useExtension(nlohmann::json& state, const std::string& extension, bool required)
{
    if (required && !state["gltf"].contains("extensionsRequired")) {
        state["gltf"]["extensionsRequired"] = nlohmann::json::array();
    }
    if (!state["gltf"].contains("extensionsUsed")) {
        state["gltf"]["extensionsUsed"] = nlohmann::json::array();
    }

    if (required && std::find(state["gltf"]["extensionsRequired"].begin(), state["gltf"]["extensionsRequired"].end(), extension) == state["gltf"]["extensionsRequired"].end()) {
        state["gltf"]["extensionsRequired"].emplace_back(extension);
    }
    if (std::find(state["gltf"]["extensionsUsed"].begin(), state["gltf"]["extensionsUsed"].end(), extension) == state["gltf"]["extensionsUsed"].end()) {
        state["gltf"]["extensionsUsed"].emplace_back(extension);
    }
}

void utility::removeExtensionFlag(nlohmann::json& state, const std::string& extensionName)
{
    if (state.contains("/gltf/extensionsUsed"_json_pointer)) {
        auto& extensionsUsed = state.at("/gltf/extensionsUsed"_json_pointer);
        if (auto iter = std::find(extensionsUsed.begin(), extensionsUsed.end(), extensionName);
            iter != extensionsUsed.end()) {
            extensionsUsed.erase(iter);
        }
        if (state.at("/gltf/extensionsUsed"_json_pointer).empty()) {
            state.at("gltf").erase("extensionsUsed");
        }
    }

    if (state.contains("/gltf/extensionsRequired"_json_pointer)) {
        auto& extensionsUsed = state.at("/gltf/extensionsRequired"_json_pointer);
        if (auto iter = std::find(extensionsUsed.begin(), extensionsUsed.end(), extensionName);
            iter != extensionsUsed.end()) {
            extensionsUsed.erase(iter);
        }
        if (state.at("/gltf/extensionsRequired"_json_pointer).empty()) {
            state.at("gltf").erase("extensionsRequired");
        }
    }
}

nlohmann::json::json_pointer mesh::getAccessor(nlohmann::json& attribute)
{
    nlohmann::json::json_pointer accessorPath("/gltf/accessors");
    uint32_t idx = attribute.get<uint64_t>();
    return accessorPath / idx;
};
