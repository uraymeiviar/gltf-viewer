#include "DracoEncoder.h"

#include "StorageOperations.h"
#include "reactor/GltfEnums.h"
#include "reactor/Utils.h"

// Gestaltor\OpenGL-Renderer\Tools\Runtime\runtime\core\include\core\Platform.h
// UX3D_BEGIN_IGNORE_WARNINGS
#include <draco/compression/encode.h>
// UX3D_END_IGNORE_WARNINGS

#include <fmt/format.h>

using namespace ux3d;
using namespace ux3d::operations;

nlohmann::json::json_pointer getAccessor(nlohmann::json& attribute)
{
    nlohmann::json::json_pointer accessorPath("/gltf/accessors");
    uint32_t idx = attribute.get<uint32_t>();
    return accessorPath / idx;
}

nlohmann::json::json_pointer getBuffer(nlohmann::json::json_pointer accessorPath, const nlohmann::json& state)
{
    auto bvIndex = state.at(accessorPath / "bufferView").get<uint64_t>();
    auto bIndex = state.at(nlohmann::json::json_pointer("/gltf/bufferViews") / bvIndex / "buffer").get<uint64_t>();
    return nlohmann::json::json_pointer("/gltf/buffers") / bIndex;
}

void encoder::draco(const nlohmann::json& params, nlohmann::json& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback)
{
    auto path = nlohmann::json::json_pointer(params["path"].get<std::string>());
    auto& primitive = state.at(path);
    if (reactor::utils::hasElement(primitive, { "extensions", "KHR_draco_mesh_compression" })) {
        return;
    }
    if (primitive.contains("mode") && primitive["mode"] != 4) {
        return; // PrimitiveMode is not Triangles
    }

    auto usedBufferViews = nlohmann::json::array();
    draco::Mesh dracoMesh;

    std::vector<draco::DataBuffer> dracoAttributeDataBuffers;
    auto KHR_draco_mesh_compression = nlohmann::json::object();
    KHR_draco_mesh_compression["attributes"] = nlohmann::json::object();

    nlohmann::json::json_pointer usedBuffer;
    for (auto& [attributeName, attribute] : primitive.at("attributes").items()) {
        draco::DataType dataType = draco::DT_INVALID;
        const auto accessorPath = getAccessor(attribute);
        const auto accessor = state.at(accessorPath);
        usedBuffer = getBuffer(accessorPath, state);

        if (accessor.contains("bufferView")) {
            usedBufferViews.push_back((nlohmann::json::json_pointer("/gltf/bufferViews") / accessor["bufferView"].get<uint64_t>()).to_string());
        }

        size_t attributeElementCount = accessor.at("count");

        switch (accessor.at("componentType").get<uint32_t>()) {
        case 5120:
            dataType = draco::DT_INT8;
            break;
        case 5121:
            dataType = draco::DT_UINT8;
            break;
        case 5122:
            dataType = draco::DT_INT16;
            break;
        case 5123:
            dataType = draco::DT_UINT16;
            break;
        case 5125:
            dataType = draco::DT_UINT32;
            break;
        case 5126:
            dataType = draco::DT_FLOAT32;
            break;
        default:
            throw ::std::invalid_argument("Invalid ComponentType");
        }

        int8_t componentCount = reactor::gltf::componentCount(accessor["type"]);

        std::vector<uint8_t> attributeBytes = reactor::utils::getAccessorByteDataPacked(accessorPath, state, datastorage);

        size_t stride = reactor::gltf::elementSize(accessor);

        draco::DataBuffer& dracoBuffer = dracoAttributeDataBuffers.emplace_back();
        dracoBuffer.Resize(static_cast<int64_t>(attributeBytes.size()));

        draco::GeometryAttribute dracoAttribute;

        draco::GeometryAttribute::Type dracoAttributeType;
        if (attributeName.compare("POSITION") == 0) {
            dracoAttributeType = draco::GeometryAttribute::POSITION;
            dracoMesh.set_num_points(accessor.at("count").get<uint32_t>());
        } else if (attributeName.compare("NORMAL") == 0) {
            dracoAttributeType = draco::GeometryAttribute::NORMAL;
        } else if (attributeName.compare("COLOR") == 0) {
            dracoAttributeType = draco::GeometryAttribute::COLOR;
        } else if (strncmp(attributeName.c_str(), "TEXCOORD_", sizeof("TEXCOORD_"))) {
            dracoAttributeType = draco::GeometryAttribute::TEX_COORD;
        } else {
            dracoAttributeType = draco::GeometryAttribute::GENERIC;
        }

        bool normalized = false;
        if (accessor.contains("normalized")) {
            normalized = accessor.at("normalized").get<bool>();
        }

        dracoAttribute.Init(dracoAttributeType, &dracoBuffer, componentCount, dataType, normalized, static_cast<int64_t>(stride), 0);

        uint32_t dracoAttributeId;
        if (int id = dracoMesh.AddAttribute(dracoAttribute, true, static_cast<draco::AttributeValueIndex::ValueType>(attributeElementCount));
            id >= 0) {
            dracoAttributeId = static_cast<uint32_t>(id);
        } else {
            throw ::std::invalid_argument("Draco Encoding Error");
        }

        for (draco::AttributeValueIndex::ValueType i = 0; i < attributeElementCount; ++i) {
            const uint8_t* bytes = &attributeBytes[i * stride];
            dracoMesh.attribute(static_cast<int32_t>(dracoAttributeId))->SetAttributeValue(draco::AttributeValueIndex(i), bytes);
        }

        KHR_draco_mesh_compression["attributes"][attributeName] = dracoAttributeId;
    }

    // Indices
    if (primitive.contains("indices")) {

        const auto accessorPath = getAccessor(primitive.at("indices"));
        auto& indicesAccessor = state.at(accessorPath);

        if (indicesAccessor.contains("bufferView")) {
            usedBufferViews.push_back((nlohmann::json::json_pointer("/gltf/bufferViews") / indicesAccessor["bufferView"].get<uint64_t>()).to_string());
        }

        auto byteData = reactor::utils::getAccessorByteDataPacked(accessorPath, state, datastorage);

        size_t componentSize = reactor::gltf::byteLength(indicesAccessor["componentType"].get<reactor::gltf::ComponentType>());
        size_t stride = reactor::gltf::elementSize(indicesAccessor);

        const size_t indicesCount = indicesAccessor.at("count").get<uint32_t>();
        for (size_t i = 0; i < indicesCount; i += 3) {
            draco::Mesh::Face face;

            if (componentSize == 4) {
                const uint32_t* ints = reinterpret_cast<const uint32_t*>(&byteData[i * stride]);
                face[0] = ints[0];
                face[1] = ints[1];
                face[2] = ints[2];
            } else if (componentSize == 2) {
                const uint16_t* shorts = reinterpret_cast<const uint16_t*>(&byteData[i * stride]);
                face[0] = shorts[0];
                face[1] = shorts[1];
                face[2] = shorts[2];
            } else if (componentSize == 1) {
                const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&byteData[i * stride]);
                face[0] = bytes[0];
                face[1] = bytes[1];
                face[2] = bytes[2];
            } else {
                throw std::runtime_error(fmt::format("Draco Encoding Error: unknown component size: {}", componentSize));
            }

            dracoMesh.AddFace(face);
        }

        // Cleanup Accessor (original data is no longer valid):
        indicesAccessor.erase("bufferView");
        indicesAccessor.erase("byteOffset");

    } else {
        const uint32_t numFaces = dracoMesh.num_points() / 3;
        for (uint32_t i = 0; i < numFaces; i++) {
            draco::Mesh::Face face;
            face[0] = 3 * i;
            face[1] = 3 * i + 1;
            face[2] = 3 * i + 2;
            dracoMesh.AddFace(face);
        }
    }

    // Cleanup Accessor (original data is no longer valid):
    for (auto& [attributeName, attribute] : primitive.at("attributes").items()) {
        const auto accessorPath = getAccessor(attribute);
        auto& accessor = state.at(accessorPath);
        accessor.erase("bufferView");
        accessor.erase("byteOffset");
    }

    draco::EncoderBuffer dracoEncoderBuffer;
    draco::Encoder dracoEncoder;

    if (params.contains("targetCompressionLevel")) {
        dracoEncoder.SetSpeedOptions(params.at("targetCompressionLevel").get<uint32_t>(), params.at("targetCompressionLevel").get<uint32_t>());
    }

    dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::POSITION, params.at("quantizationBitsPosition").get<uint32_t>());

    dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::NORMAL, params.at("quantizationBitsNormal").get<uint32_t>());

    dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::TEX_COORD, params.at("quantizationBitsUv").get<uint32_t>());

    dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::COLOR, params.at("quantizationBitsColor").get<uint32_t>());

    dracoEncoder.SetAttributeQuantization(draco::GeometryAttribute::GENERIC, params.at("quantizationBitsGeneric").get<uint32_t>());

    bool useSequentialEncoding = false;
    if (primitive.contains("targets")) {
        useSequentialEncoding = true;
    } else if (params.contains("sequentialEncoding")) {
        useSequentialEncoding = params.at("sequentialEncoding").get<bool>();
    }

    dracoEncoder.SetEncodingMethod(useSequentialEncoding ? draco::MESH_SEQUENTIAL_ENCODING : draco::MESH_EDGEBREAKER_ENCODING);

    dracoEncoder.SetTrackEncodedProperties(true);

    bool successfulEncoding = dracoEncoder.EncodeMeshToBuffer(dracoMesh, &dracoEncoderBuffer).ok();
    if (!successfulEncoding) {
        throw std::runtime_error("Draco Encoding Error");
    }

    // Draco can change the number of vertices:
    if (!useSequentialEncoding) {
        for (auto& [attributeName, attribute] : primitive.at("attributes").items()) {
            const auto accessorPath = getAccessor(attribute);
            auto& accessor = state.at(accessorPath);
            accessor.at("count") = dracoEncoder.num_encoded_points();
        }
        if (primitive.contains("indices")) {
            const auto accessorPath = getAccessor(primitive.at("indices"));
            auto& indicesAccessor = state.at(accessorPath);
            indicesAccessor.at("count") = 3 * dracoEncoder.num_encoded_faces();
        }
    }

    // Add encoded data to glTF via buffers and bufferViews:
    std::vector<uint8_t> data;
    data.assign(dracoEncoderBuffer.buffer()->data(), dracoEncoderBuffer.buffer()->data() + dracoEncoderBuffer.buffer()->size());

    auto bufferViewPath = reactor::utils::appendBufferView(usedBuffer, data, state, datastorage);

    KHR_draco_mesh_compression["bufferView"] = std::stoi(bufferViewPath.back());

    reactor::utils::createSubElement(primitive, "/extensions/KHR_draco_mesh_compression");
    primitive["extensions"]["KHR_draco_mesh_compression"] = KHR_draco_mesh_compression;

    { // Remove no longer used bufferViews
        std::string op = "delete_bufferview";
        auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
        taskDescription[op]["path"] = usedBufferViews;
        state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
    }
    // core::debugLog(core::LogLevel::Info, "Mesh Compression: encoded %zu points", dracoEncoder.num_encoded_points());
    // core::debugLog(core::LogLevel::Info, "Mesh Compression: encoded %zu triangles", dracoEncoder.num_encoded_faces());
    // core::logInfo("Compressed mesh size: %zu bytes", dracoEncoderBuffer.buffer()->size());
}
