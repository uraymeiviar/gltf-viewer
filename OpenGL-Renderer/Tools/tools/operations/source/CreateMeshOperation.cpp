#include "PropertyOperations.h"
#include "StorageOperations.h"
#include "operations/Library.h"
#include "operations/Schema.h"
#include "reactor/GltfEnums.h"
#include "reactor/Utils.h"

#include <vector>

#include <gtc/constants.hpp>

using namespace ux3d::operations;

const Operation gltf::createMesh = {
    .name = "create_mesh",

    .schema = schema::operation(
        "Add New Mesh",
        "Adds a newly generated mesh",
        schema::object(
            {
                schema::parameter::path("/gltf/nodes/\\d+"),
                { "shape", schema::enumeration({ "plane", "cube", "disk", "sphere", "torus" }) },
            },
            {
                { "scale", schema::number() },
                { "sectors", schema::number() },
                { "outerRadius", schema::number() },
                { "innerRadius", schema::number() },
                { "segments", schema::number() },
            })),

    .operation = [](const nlohmann::json& params, nlohmann::json&& state, datastorage::DataStorage& dataStorage, const Library& library, std::function<void(const nlohmann::json&)> callback) -> nlohmann::json {
        if (!state.contains(nlohmann::json::json_pointer(params["path"]))) {
            throw std::invalid_argument("Node does not exist");
        }

        std::vector<float> positions;
        std::vector<float> normals;
        std::vector<float> texCoords;

        std::vector<uint16_t> indices;
        bool isDoubleSided = false;

        if (params["shape"] == "plane") {
            positions = {
                -1.0f, 0.0f, -1.0f,
                -1.0f, 0.0f, +1.0f,
                +1.0f, 0.0f, -1.0f,
                +1.0f, 0.0f, +1.0f
            };

            normals = {
                0.0f, +1.0f, 0.0f,
                0.0f, +1.0f, 0.0f,
                0.0f, +1.0f, 0.0f,
                0.0f, +1.0f, 0.0f
            };

            texCoords = {
                0.0f, 0.0f,
                0.0f, 1.0f,
                1.0f, 0.0f,
                1.0f, 1.0f
            };

            indices = {
                0, 1, 2, 3, 2, 1
            };

            isDoubleSided = true;
        } else if (params["shape"] == "cube") {
            positions = {
                -1.0f, -1.0f, -1.0f,
                -1.0f, -1.0f, +1.0f,
                +1.0f, -1.0f, +1.0f,
                +1.0f, -1.0f, -1.0f,

                -1.0f, +1.0f, -1.0f,
                -1.0f, +1.0f, +1.0f,
                +1.0f, +1.0f, +1.0f,
                +1.0f, +1.0f, -1.0f,

                -1.0f, -1.0f, -1.0f,
                -1.0f, +1.0f, -1.0f,
                +1.0f, +1.0f, -1.0f,
                +1.0f, -1.0f, -1.0f,

                -1.0f, -1.0f, +1.0f,
                -1.0f, +1.0f, +1.0f,
                +1.0f, +1.0f, +1.0f,
                +1.0f, -1.0f, +1.0f,

                -1.0f, -1.0f, -1.0f,
                -1.0f, -1.0f, +1.0f,
                -1.0f, +1.0f, +1.0f,
                -1.0f, +1.0f, -1.0f,

                +1.0f, -1.0f, -1.0f,
                +1.0f, -1.0f, +1.0f,
                +1.0f, +1.0f, +1.0f,
                +1.0f, +1.0f, -1.0f
            };

            normals = {
                0.0f, -1.0f, 0.0f,
                0.0f, -1.0f, 0.0f,
                0.0f, -1.0f, 0.0f,
                0.0f, -1.0f, 0.0f,

                0.0f, +1.0f, 0.0f,
                0.0f, +1.0f, 0.0f,
                0.0f, +1.0f, 0.0f,
                0.0f, +1.0f, 0.0f,

                0.0f, 0.0f, -1.0f,
                0.0f, 0.0f, -1.0f,
                0.0f, 0.0f, -1.0f,
                0.0f, 0.0f, -1.0f,

                0.0f, 0.0f, +1.0f,
                0.0f, 0.0f, +1.0f,
                0.0f, 0.0f, +1.0f,
                0.0f, 0.0f, +1.0f,

                -1.0f, 0.0f, 0.0f,
                -1.0f, 0.0f, 0.0f,
                -1.0f, 0.0f, 0.0f,
                -1.0f, 0.0f, 0.0f,

                +1.0f, 0.0f, 0.0f,
                +1.0f, 0.0f, 0.0f,
                +1.0f, 0.0f, 0.0f,
                +1.0f, 0.0f, 0.0f
            };

            texCoords = {
                0.0f, 1.0f,
                0.0f, 0.0f,
                1.0f, 0.0f,
                1.0f, 1.0f,

                0.0f, 0.0f,
                0.0f, 1.0f,
                1.0f, 1.0f,
                1.0f, 0.0f,

                1.0f, 1.0f,
                1.0f, 0.0f,
                0.0f, 0.0f,
                0.0f, 1.0f,

                0.0f, 1.0f,
                0.0f, 0.0f,
                1.0f, 0.0f,
                1.0f, 1.0f,

                0.0f, 1.0f,
                1.0f, 1.0f,
                1.0f, 0.0f,
                0.0f, 0.0f,

                1.0f, 1.0f,
                0.0f, 1.0f,
                0.0f, 0.0f,
                1.0f, 0.0f
            };

            indices = {
                0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11, 12, 15, 14, 12, 14, 13, 16, 17, 18, 16, 18, 19, 20, 23, 22, 20, 22, 21
            };
        } else if (params["shape"] == "disk") {
            size_t numberSectors = params.value("sectors", 64);
            float angleStep = (2.0f * glm::pi<float>()) / (float)numberSectors;

            size_t count = numberSectors + 2;
            positions.resize(3 * count);
            normals.resize(3 * count);
            texCoords.resize(2 * count);
            indices.resize(numberSectors * 3);

            size_t vertexCounter = 0;

            positions[vertexCounter * 3 + 0] = 0.0f;
            positions[vertexCounter * 3 + 1] = 0.0f;
            positions[vertexCounter * 3 + 2] = 0.0f;

            normals[vertexCounter * 3 + 0] = 0.0f;
            normals[vertexCounter * 3 + 1] = 1.0f;
            normals[vertexCounter * 3 + 2] = 0.0f;

            texCoords[vertexCounter * 2 + 0] = 0.5f;
            texCoords[vertexCounter * 2 + 1] = 0.5f;

            vertexCounter++;

            for (size_t i = 0; i < numberSectors + 1; i++) {
                float angle = angleStep * (float)i;

                positions[vertexCounter * 3 + 0] = cosf(angle);
                positions[vertexCounter * 3 + 1] = 0.0f;
                positions[vertexCounter * 3 + 2] = sinf(angle);

                normals[vertexCounter * 3 + 0] = 0.0f;
                normals[vertexCounter * 3 + 1] = 1.0f;
                normals[vertexCounter * 3 + 2] = 0.0f;

                texCoords[vertexCounter * 2 + 0] = 0.5f - 0.5f * sinf(angle);
                texCoords[vertexCounter * 2 + 1] = 0.5f + 0.5f * cosf(angle);

                vertexCounter++;
            }

            size_t indexIndices = 0;
            size_t indexCounter = 1;

            for (size_t i = 0; i < numberSectors; i++) {
                indices[indexIndices++] = 0;
                indices[indexIndices++] = indexCounter + 1;
                indices[indexIndices++] = indexCounter;

                indexCounter++;
            }

            isDoubleSided = true;
        } else if (params["shape"] == "sphere") {
            size_t numberSectors = params.value("sectors", 64);
            size_t numberSlices = numberSectors / 2;
            size_t count = (numberSlices + 1) * (numberSectors + 1);
            size_t indicesCount = numberSlices * numberSectors * 6;

            float angleStep = (2.0f * glm::pi<float>()) / (float)numberSectors;

            positions.resize(3 * count);
            normals.resize(3 * count);
            texCoords.resize(2 * count);
            indices.resize(indicesCount);

            size_t indexIndices = 0;

            for (size_t i = 0; i < numberSlices + 1; i++) {
                for (size_t j = 0; j < numberSectors + 1; j++) {
                    size_t vertexIndex = (i * (numberSectors + 1) + j) * 3;
                    size_t normalIndex = (i * (numberSectors + 1) + j) * 3;
                    size_t texCoordsIndex = (i * (numberSectors + 1) + j) * 2;

                    positions[vertexIndex + 0] = sinf(angleStep * (float)i) * sinf(angleStep * (float)j);
                    positions[vertexIndex + 1] = cosf(angleStep * (float)i);
                    positions[vertexIndex + 2] = sinf(angleStep * (float)i) * cosf(angleStep * (float)j);

                    normals[normalIndex + 0] = positions[vertexIndex + 0];
                    normals[normalIndex + 1] = positions[vertexIndex + 1];
                    normals[normalIndex + 2] = positions[vertexIndex + 2];

                    texCoords[texCoordsIndex + 0] = (float)i / (float)numberSlices;
                    texCoords[texCoordsIndex + 1] = 1.0f - (float)j / (float)numberSectors;

                    if (i < numberSlices && j < numberSectors) {
                        indices[indexIndices++] = i * (numberSectors + 1) + j;
                        indices[indexIndices++] = (i + 1) * (numberSectors + 1) + j;
                        indices[indexIndices++] = (i + 1) * (numberSectors + 1) + (j + 1);

                        indices[indexIndices++] = i * (numberSectors + 1) + j;
                        indices[indexIndices++] = (i + 1) * (numberSectors + 1) + (j + 1);
                        indices[indexIndices++] = i * (numberSectors + 1) + (j + 1);
                    }
                }
            }
        } else if (params["shape"] == "torus") {
            float outerRadius = params.value("outerRadius", 1.0f);
            float innerRadius = params.value("innerRadius", 0.25f * outerRadius);
            float radius = (outerRadius - innerRadius) / 2.0f;
            float centerRadius = 1 - radius;

            size_t numberSectors = params.value("sectors", 64);
            size_t numberSegments = params.value("segments", 64);

            size_t count = (numberSectors + 1) * (numberSegments + 1);
            positions.resize(3 * count);
            normals.resize(3 * count);
            texCoords.resize(2 * count);
            indices.resize(numberSectors * numberSegments * 2 * 3);

            size_t indexm_vertexPositions = 0;
            size_t indexNormals = 0;
            size_t indexTexCoords = 0;

            float sIncr = 1.0f / (float)numberSegments;
            float tIncr = 1.0f / (float)numberSectors;

            float s = 0.0f;
            for (size_t sideCount = 0; sideCount <= numberSegments; ++sideCount, s += sIncr) {
                float cos2PIs = cosf(2.0f * glm::pi<float>() * s);
                float sin2PIs = sinf(2.0f * glm::pi<float>() * s);

                float t = 0.0f;
                for (size_t faceCount = 0; faceCount <= numberSectors; ++faceCount, t += tIncr) {
                    float cos2PIt = cosf(2.0f * glm::pi<float>() * t);
                    float sin2PIt = sinf(2.0f * glm::pi<float>() * t);

                    indexm_vertexPositions = ((sideCount * (numberSectors + 1)) + faceCount) * 3;
                    positions[indexm_vertexPositions + 0] = (centerRadius + radius * cos2PIt) * sin2PIs;
                    positions[indexm_vertexPositions + 1] = radius * sin2PIt;
                    positions[indexm_vertexPositions + 2] = (centerRadius + radius * cos2PIt) * cos2PIs;

                    indexNormals = ((sideCount * (numberSectors + 1)) + faceCount) * 3;
                    normals[indexNormals + 0] = sin2PIs * cos2PIt;
                    normals[indexNormals + 1] = sin2PIt;
                    normals[indexNormals + 2] = cos2PIs * cos2PIt;

                    indexTexCoords = ((sideCount * (numberSectors + 1)) + faceCount) * 2;
                    texCoords[indexTexCoords + 0] = t;
                    texCoords[indexTexCoords + 1] = s;
                }
            }

            size_t v0, v1, v2, v3;
            size_t indexIndices = 0;
            for (size_t sideCount = 0; sideCount < numberSegments; ++sideCount) {
                for (size_t faceCount = 0; faceCount < numberSectors; ++faceCount) {
                    v0 = ((sideCount * (numberSectors + 1)) + faceCount);
                    v1 = (((sideCount + 1) * (numberSectors + 1)) + faceCount);
                    v2 = (((sideCount + 1) * (numberSectors + 1)) + (faceCount + 1));
                    v3 = ((sideCount * (numberSectors + 1)) + (faceCount + 1));

                    indices[indexIndices++] = v0;
                    indices[indexIndices++] = v1;
                    indices[indexIndices++] = v2;

                    indices[indexIndices++] = v0;
                    indices[indexIndices++] = v2;
                    indices[indexIndices++] = v3;
                }
            }
        }

        if (params.contains("scale")) {
            float scale = params["scale"];
            for (float& position : positions) {
                position = position * scale;
            }
        }

        // Renormalize normals
        for (size_t i = 0; i < normals.size(); i += 3) {
            float length = std::sqrt(normals[i] * normals[i] + normals[i + 1] * normals[i + 1] + normals[i + 2] * normals[i + 2]);
            if (length != 0.0f) {
                normals[i] *= length;
                normals[i + 1] *= length;
                normals[i + 2] *= length;
            }
        }

        nlohmann::json mesh;
        nlohmann::json primitive;

        {
            std::vector<uint8_t> bytes;
            bytes.resize(positions.size() * sizeof(float));
            memcpy(bytes.data(), positions.data(), bytes.size());

            auto bufferView = reactor::utils::createBufferView(state, reactor::utils::createBuffer(state, dataStorage, bytes));
            state["gltf"]["accessors"].emplace_back(nlohmann::json({
                { "type", reactor::gltf::Type::VEC3 },
                { "componentType", reactor::gltf::ComponentType::Float },
                { "count", positions.size() / 3 },
                { "bufferView", std::stoi(bufferView.back()) },
            }));
            primitive["attributes"]["POSITION"] = state["gltf"]["accessors"].size() - 1;
        }

        {
            std::vector<uint8_t> bytes;
            bytes.resize(normals.size() * sizeof(float));
            memcpy(bytes.data(), normals.data(), bytes.size());

            auto bufferView = reactor::utils::createBufferView(state, reactor::utils::createBuffer(state, dataStorage, bytes));
            state["gltf"]["accessors"].emplace_back(nlohmann::json({
                { "type", reactor::gltf::Type::VEC3 },
                { "componentType", reactor::gltf::ComponentType::Float },
                { "count", normals.size() / 3 },
                { "bufferView", std::stoi(bufferView.back()) },
            }));
            primitive["attributes"]["NORMAL"] = state["gltf"]["accessors"].size() - 1;
        }

        {
            std::vector<uint8_t> bytes;
            bytes.resize(texCoords.size() * sizeof(float));
            memcpy(bytes.data(), texCoords.data(), bytes.size());

            auto bufferView = reactor::utils::createBufferView(state, reactor::utils::createBuffer(state, dataStorage, bytes));
            state["gltf"]["accessors"].emplace_back(nlohmann::json({
                { "type", reactor::gltf::Type::VEC2 },
                { "componentType", reactor::gltf::ComponentType::Float },
                { "count", texCoords.size() / 2 },
                { "bufferView", std::stoi(bufferView.back()) },
            }));
            primitive["attributes"]["TEXCOORD_0"] = state["gltf"]["accessors"].size() - 1;
        }

        {
            std::vector<uint8_t> bytes;
            bytes.resize(indices.size() * sizeof(uint16_t));
            memcpy(bytes.data(), indices.data(), bytes.size());

            auto bufferView = reactor::utils::createBufferView(state, reactor::utils::createBuffer(state, dataStorage, bytes));

            state["gltf"]["accessors"].emplace_back(nlohmann::json({
                { "type", "SCALAR" },
                { "componentType", reactor::gltf::ComponentType::UnsignedShort },
                { "count", indices.size() },
                { "bufferView", std::stoi(bufferView.back()) },
            }));
            primitive["indices"] = state["gltf"]["accessors"].size() - 1;
        }

        auto material = nlohmann::json::object();
        if (isDoubleSided) {
            material["doubleSided"] = true;
        }
        state["gltf"]["materials"].emplace_back(material);
        primitive["material"] = state["gltf"]["materials"].size() - 1;

        mesh["primitives"] = nlohmann::json::array({ primitive });
        state["gltf"]["meshes"].emplace_back(mesh);

        state[nlohmann::json::json_pointer(params["path"])]["mesh"] = state["gltf"]["meshes"].size() - 1;

        state = library.call({ { "calculate_min_max", { { "path", (nlohmann::json::json_pointer() / "gltf" / "accessors" / primitive["attributes"]["POSITION"].get<int>()).to_string() } } } }, std::move(state), dataStorage, callback);

        return state;
    }
};
