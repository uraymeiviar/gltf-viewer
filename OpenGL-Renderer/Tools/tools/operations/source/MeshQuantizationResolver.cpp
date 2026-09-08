#include "MeshQuantizationResolver.h"
#include "operations/Library.h"
#include "reactor/GltfEnums.h"
#include "reactor/Utils.h"

using namespace ux3d;
using namespace ux3d::operations;

nlohmann::json meshQuantization::resolve(nlohmann::json::json_pointer accessorPath, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback)
{
    // gather accessor data
    const int jsonComponentType = state.at(accessorPath)["componentType"].get<int>();
    const reactor::gltf::ComponentType componentType = static_cast<reactor::gltf::ComponentType>(jsonComponentType);

    reactor::gltf::Type type;
    reactor::gltf::from_json(state.at(accessorPath)["type"], type);

    std::vector<uint8_t> byteDataPacked = reactor::utils::getAccessorByteDataPacked(accessorPath, state, dataStorage);

    bool normalized = false;
    if (state.at(accessorPath).contains("normalized")) {
        normalized = state.at(accessorPath)["normalized"].get<bool>();
    }

    // define conversion functions
    auto convertByteNormalized = [](int8_t value) -> float {
        return std::max(value / 127.0f, -1.0f);
    };
    auto convertUByteNormalized = [](uint8_t value) -> float {
        return value / 255.0f;
    };
    auto convertShortNormalized = [](int16_t value) -> float {
        return std::max(value / 32767.0f, -1.0f);
    };
    auto convertUShortNormalized = [](uint16_t value) -> float {
        return value / 65535.0f;
    };
    auto convertIdentity = [](auto value) -> float {
        return static_cast<float>(value);
    };

    // transform to float using the fitting conversion function and save as byte vector
    std::vector<uint8_t> newData;

    if (normalized) {
        switch (componentType) {
        case reactor::gltf::ComponentType::Byte:
            newData = convert<int8_t>(byteDataPacked, &convertByteNormalized);
            break;
        case reactor::gltf::ComponentType::UnsignedByte:
            newData = convert<uint8_t>(byteDataPacked, &convertUByteNormalized);
            break;
        case reactor::gltf::ComponentType::Short:
            newData = convert<int16_t>(byteDataPacked, &convertShortNormalized);
            break;
        case reactor::gltf::ComponentType::UnsignedShort:
            newData = convert<uint16_t>(byteDataPacked, &convertUShortNormalized);
            break;
        default:
            throw std::invalid_argument("Accessor does not use KHR_mesh_quantization");
        }
    } else {
        switch (componentType) {
        case reactor::gltf::ComponentType::Byte:
            newData = convert<int8_t>(byteDataPacked, &convertIdentity);
            break;
        case reactor::gltf::ComponentType::UnsignedByte:
            newData = convert<uint8_t>(byteDataPacked, &convertIdentity);
            break;
        case reactor::gltf::ComponentType::Short:
            newData = convert<int16_t>(byteDataPacked, &convertIdentity);
            break;
        case reactor::gltf::ComponentType::UnsignedShort:
            newData = convert<uint16_t>(byteDataPacked, &convertIdentity);
            break;
        default:
            throw std::invalid_argument("Accessor does not use KHR_mesh_quantization");
        }
    }

    auto& accessor = state.at(accessorPath);

    nlohmann::json_pointer oldBufferViewPath = nlohmann::json::json_pointer("/gltf/bufferViews") / accessor["bufferView"].get<int>();
    nlohmann::json_pointer bufferPath = nlohmann::json::json_pointer("/gltf/buffers") / state.at(oldBufferViewPath)["buffer"].get<int>();

    // create bufferView, add data to existing buffer and set corresponding values
    auto newBufferViewPath = reactor::utils::appendBufferView(bufferPath, newData, state, dataStorage);

    accessor["bufferView"] = std::stoi(newBufferViewPath.back());
    accessor["componentType"] = reactor::gltf::ComponentType::Float;
    accessor["byteOffset"] = 0;
    accessor["normalized"] = false;
    accessor.erase("min");
    accessor.erase("max");

    // cleanup old bufferView
    nlohmann::json removeBufferView;
    removeBufferView["delete_bufferview"]["path"] = oldBufferViewPath.to_string();
    state = library.call(removeBufferView, std::move(state), dataStorage, callback);

    return state;
}
