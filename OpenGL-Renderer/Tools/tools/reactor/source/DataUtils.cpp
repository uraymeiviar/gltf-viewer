#include "reactor/DataUtils.h"
#include "datastorage/DataStorage.h"
#include "reactor/Utils.h"

using namespace ux3d;
using namespace ux3d::reactor;

uint64_t getAccessorPackedSize(const nlohmann::json& accessor)
{
    uint64_t count = accessor["count"];
    uint64_t elementSize = gltf::elementSize(accessor);
    uint64_t packedSize = count * elementSize;
    return packedSize;
}

uint64_t data::getDecodedDracoSize(const nlohmann::json& state, const datastorage::DataStorage& datastorage)
{
    uint64_t dataSize = 0;

    if (!state["gltf"].contains("meshes")) {
        return dataSize;
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
                dataSize += getAccessorPackedSize(accessor);
            }
            // check index buffer
            if (primitive.contains("indices")) {
                nlohmann::json::json_pointer accessorPath("/gltf/accessors");
                uint32_t idx = primitive.at("indices").get<uint32_t>();
                auto& accessor = state.at(accessorPath / idx);
                dataSize += getAccessorPackedSize(accessor);
            }
        }
    }
    return dataSize;
}

uint64_t data::getDataSize(const nlohmann::json& state, const datastorage::DataStorage& datastorage)
{
    uint64_t dataSize = 0;

    if (state["gltf"].contains("buffers")) {
        auto& buffers = state["gltf"]["buffers"];
        for (uint32_t i = 0; i < buffers.size(); i++) {
            auto& buffer = buffers[i];

            if (!buffer.contains("uri") || buffer.at("uri").size() == 0) {
                // continue;
            } else {
                std::string uri = buffer["uri"].get<std::string>();
                if (reactor::utils::isDataUri(uri)) {
                    continue;
                }
            }

            nlohmann::json::json_pointer dataIDPath = reactor::utils::getDataIDPath(nlohmann::json::json_pointer("/gltf/buffers") / i, state);
            if (!state.contains(dataIDPath)) {
                continue;
            }
            uint64_t dataStorageID = state.at(dataIDPath);
            const std::vector<uint8_t>& data = datastorage.at(dataStorageID);
            dataSize += data.size();
        }
    }

    if (state["gltf"].contains("images")) {
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
                continue;
            }
            uint64_t dataStorageID = state.at(dataIDPath);
            const std::vector<uint8_t>& data = datastorage.at(dataStorageID);
            dataSize += data.size();
        }
    }
    dataSize -= getDecodedDracoSize(state, datastorage);

    return dataSize;
}
