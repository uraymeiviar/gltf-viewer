#include "DracoDecoder.h"

#include "StorageOperations.h"
#include "datastorage/DataStorage.h"
#include "operations/Library.h"
#include "reactor/GltfEnums.h"
#include "reactor/Utils.h"

// Gestaltor\OpenGL-Renderer\Tools\Runtime\runtime\core\include\core\Platform.h
// UX3D_BEGIN_IGNORE_WARNINGS
#include <draco/compression/decode.h>
// UX3D_END_IGNORE_WARNINGS

using namespace ux3d;
using namespace ux3d::operations;

nlohmann::json decoder::draco(const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback)
{
    auto primitivePath = nlohmann::json::json_pointer(params["path"].get<std::string>());

    // define local helper function
    auto getAccessor = [](nlohmann::json& attribute) -> nlohmann::json::json_pointer {
        nlohmann::json::json_pointer accessorPath("/gltf/accessors");
        uint32_t idx = attribute.get<uint32_t>();
        return accessorPath / idx;
    };

    auto& primitive = state.at(primitivePath);

    if (!reactor::utils::hasElement(primitive, { "extensions", "KHR_draco_mesh_compression" })) {
        return state; // nothing to do
    }

    auto KHR_draco_mesh_compression = primitive["extensions"]["KHR_draco_mesh_compression"];
    if (!KHR_draco_mesh_compression.contains("bufferView")) {
        throw std::invalid_argument(std::string("KHR_draco_mesh_compression has no bufferView"));
    }

    auto dracoBufferViewPath = nlohmann::json::json_pointer("/gltf/bufferViews") / KHR_draco_mesh_compression.at("bufferView").get<uint32_t>();

    std::vector<uint8_t> compressedData = reactor::utils::getBufferViewData(dracoBufferViewPath, state, dataStorage);

    draco::DecoderBuffer dracoBuffer;
    dracoBuffer.Init((const char*)compressedData.data(), compressedData.size());

    if (auto statType = draco::Decoder::GetEncodedGeometryType(&dracoBuffer); statType.ok() == false || statType.value() != draco::TRIANGULAR_MESH) {
        throw std::invalid_argument(std::string("Draco buffer geometry type not supported (glTF requires TRIANGULAR_MESH)"));
    }

    draco::Decoder dracoDec;
    std::vector<uint8_t> stagingBuffer;
    auto decMesh = dracoDec.DecodeMeshFromBuffer(&dracoBuffer);

    if (!decMesh.ok()) {
        throw std::runtime_error(std::string("Draco decoding failed"));
    }

    const auto& dracoMesh = decMesh.value();

    uint32_t faces = dracoMesh->num_faces();
    uint32_t indices = faces * 3u;

    uint32_t indexElementSize = 0;
    uint32_t indexBufferSize = 0;

    if (primitive.contains("indices")) {
        auto& indexAccessor = state.at(getAccessor(primitive.at("indices")));

        indexElementSize = reactor::gltf::elementSize(indexAccessor);
        indexBufferSize = indices * indexElementSize;
    }

    uint32_t vertexCount = dracoMesh->num_points();

    // index data is prepended to the front of the buffer
    uint32_t primitiveSize = indexBufferSize;

    // accumulate attribute sizes
    for (const auto& [key, value] : KHR_draco_mesh_compression.at("attributes").items()) {

        if (!primitive.at("attributes").contains(key)) {
            throw std::invalid_argument("glTF attribute not found for draco id");
        }
        auto accessorPath = getAccessor(primitive["attributes"][key]);
        if (!state.contains(accessorPath)) {
            throw std::invalid_argument("Invalid accessor");
        }
        auto& accessor = state.at(accessorPath);
        uint32_t stride = reactor::gltf::elementSize(accessor);

        primitiveSize += vertexCount * stride;
    }

    auto decodedBufferPath = reactor::utils::createBuffer(state, dataStorage, stagingBuffer);
    // we need to update the stagingBuffer data later

    stagingBuffer.resize(primitiveSize);

    // copy indices
    if (primitive.contains("indices")) {
        auto& indexAccessor = state.at(getAccessor(primitive.at("indices")));
        if (indexElementSize == 4u) {
            uint32_t* pIndices = reinterpret_cast<uint32_t*>(stagingBuffer.data());
            for (draco::FaceIndex f(0u); f < faces; ++f) {
                const auto& face = dracoMesh->face(f);
                *(pIndices++) = face[0].value();
                *(pIndices++) = face[1].value();
                *(pIndices++) = face[2].value();
            }
        } else if (indexElementSize == 2u) {
            uint16_t* pIndices = reinterpret_cast<uint16_t*>(stagingBuffer.data());
            for (draco::FaceIndex f(0u); f < faces; ++f) {
                const auto& face = dracoMesh->face(f);
                *(pIndices++) = static_cast<uint16_t>(face[0].value());
                *(pIndices++) = static_cast<uint16_t>(face[1].value());
                *(pIndices++) = static_cast<uint16_t>(face[2].value());
            }
        } else if (indexElementSize == 1u) {
            uint8_t* pIndices = reinterpret_cast<uint8_t*>(stagingBuffer.data());
            for (draco::FaceIndex f(0u); f < faces; ++f) {
                const auto& face = dracoMesh->face(f);
                *(pIndices++) = static_cast<uint8_t>(face[0].value());
                *(pIndices++) = static_cast<uint8_t>(face[1].value());
                *(pIndices++) = static_cast<uint8_t>(face[2].value());
            }
        } else {
            throw std::invalid_argument("Index format not supported");
        }

        // create bufferView and link it to accessor
        auto indexBufferView = nlohmann::json::object(); // m_gltf.create<BufferView>();
        indexBufferView["buffer"] = std::stoi(decodedBufferPath.back());
        indexBufferView["byteOffset"] = 0;
        indexBufferView["byteLength"] = indexBufferSize;
        auto bufferViewPath = nlohmann::json::json_pointer("/gltf/bufferViews");
        state.at(bufferViewPath).push_back(indexBufferView);
        indexAccessor["bufferView"] = (state.at(bufferViewPath).size() - 1);
    }

    // copy vertex attributes
    uint32_t attributeOffset = indexBufferSize;
    for (const auto& [key, attrib] : KHR_draco_mesh_compression.at("attributes").items()) {

        if (!primitive.at("attributes").contains(key)) {
            throw std::invalid_argument("glTF attribute not found for draco id");
        }
        if (!state.contains(getAccessor(primitive["attributes"][key]))) {
            throw std::invalid_argument("Invalid accessor");
        }

        auto& pAccessor = state.at(getAccessor(primitive["attributes"][key]));

        uint32_t stride = reactor::gltf::elementSize(pAccessor);

        const draco::PointAttribute* pointAttribute = dracoMesh->GetAttributeByUniqueId(attrib.get<uint32_t>());

        uint8_t* data = &stagingBuffer.data()[attributeOffset];
        for (uint32_t i = 0; i < vertexCount; ++i) {
            auto idx = pointAttribute->mapped_index(draco::PointIndex(i));

            uint8_t* value = data + static_cast<size_t>(i) * stride;

            // draco attribute size and type can differ from gltf accessor type, cant mem copy
            // convert value from draco attribute type to gltf attribute type
            bool converted = false;
            reactor::gltf::ComponentType componentType = pAccessor["componentType"];
            switch (componentType) {
            case reactor::gltf::ComponentType::Byte:
                converted = pointAttribute->ConvertValue(idx, reinterpret_cast<int8_t*>(value));
                break;
            case reactor::gltf::ComponentType::UnsignedByte:
                converted = pointAttribute->ConvertValue(idx, reinterpret_cast<uint8_t*>(value));
                break;
            case reactor::gltf::ComponentType::Short:
                converted = pointAttribute->ConvertValue(idx, reinterpret_cast<short*>(value));
                break;
            case reactor::gltf::ComponentType::UnsignedShort:
                converted = pointAttribute->ConvertValue(idx, reinterpret_cast<unsigned short*>(value));
                break;
            case reactor::gltf::ComponentType::UnsignedInt:
                converted = pointAttribute->ConvertValue(idx, reinterpret_cast<unsigned int*>(value));
                break;
            case reactor::gltf::ComponentType::Float:
                converted = pointAttribute->ConvertValue(idx, reinterpret_cast<float*>(value));
                break;
            default:
                break;
            }

            if (!converted) {
                throw std::runtime_error("Decoding error");
            }
        }

        // create bufferView and link it to accessor
        auto bufferView = nlohmann::json::object();
        bufferView["buffer"] = std::stoi(decodedBufferPath.back());
        bufferView["byteOffset"] = attributeOffset;
        bufferView["byteLength"] = vertexCount * stride;
        bufferView["byteStride"] = stride;
        auto bufferViewPath = nlohmann::json::json_pointer("/gltf/bufferViews");
        state.at(bufferViewPath).push_back(bufferView);

        pAccessor["bufferView"] = (state.at(bufferViewPath).size() - 1);
        pAccessor["byteOffset"] = 0;
        if (pAccessor.contains("normalized") || pointAttribute->normalized()) {
            pAccessor.at("normalized") = pointAttribute->normalized();
        }
        attributeOffset += bufferView["byteLength"].get<uint32_t>();
    }

    reactor::utils::updateBufferData(decodedBufferPath, stagingBuffer, state, dataStorage);

    if (state["gltf"]["buffers"].size() > 1) {
        auto taskDescription = nlohmann::json({ { "merge_buffers", nlohmann::json::object() } });
        taskDescription["merge_buffers"]["bufferPaths"] = nlohmann::json::array();

        taskDescription["merge_buffers"]["bufferPaths"].push_back("/gltf/buffers/0");
        taskDescription["merge_buffers"]["bufferPaths"].push_back(decodedBufferPath.to_string());

        state = library.call(std::move(taskDescription), std::move(state), dataStorage, callback);
    }

    if (params.contains("deleteEncodedData") && params["deleteEncodedData"].get<bool>() == true) {
        // Cleanup
        // do not use original primitive reference, since reference might now be invalid
        state.at(primitivePath)["extensions"].erase("KHR_draco_mesh_compression");
        if (state.at(primitivePath)["extensions"].empty()) {
            state.at(primitivePath).erase("extensions");
        }
        {
            auto taskDescription = nlohmann::json({ { "delete_bufferview", nlohmann::json::object() } });
            taskDescription["delete_bufferview"]["path"] = dracoBufferViewPath.to_string();
            state = library.call(std::move(taskDescription), std::move(state), dataStorage, callback);
        }
    }

    return state;
}
