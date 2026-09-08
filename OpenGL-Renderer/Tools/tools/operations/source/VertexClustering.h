#pragma once

//#include "gltf/AccessorCreation.h"
//#include "glm/VectorTypes.h"
#include <glm.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <variant>
#include <vector>

namespace ux3d::operations {
class Library;
}
namespace ux3d::datastorage {
class DataStorage;
}
namespace ux3d::operations::mesh {

struct ClusterVertex {
    glm::vec3 position = glm::vec3(0.0);
    glm::vec3 normal = glm::vec3(1.0, 0.0, 0.0);
    glm::vec4 tangent = glm::vec4(1.0, 0.0, 0.0, 1.0);
    glm::vec2 texCoord0 = glm::vec2(0.0);
    glm::vec2 texCoord1 = glm::vec2(0.0);
    std::variant<glm::vec3, glm::vec4> color0 = glm::vec4(0.0);

    std::vector<uint32_t> indices; // this vertex is used at these indices.
    uint32_t indexOffset = 0u;

    bool deleted = false;
};

struct SimilarityThreshold {
    double positionDistance = 0.0; // [0-1] percent of mesh extent
    double normalDeviation = 0.0; // [0-1] // geometric operations are involved that introduce an error
    double tangentDeviation = 0.0; // [0-1]
    double texCoordDistance = 0.0; // [0-1] // be careful with these
    double colorDistance = 0.0; // [0-1]
};

struct FailCount {
    uint32_t position = 0;
    uint32_t normal = 0;
    uint32_t tangent = 0;
    uint32_t texCoord = 0;
    uint32_t color = 0;
};

nlohmann::json clusterVertices(
    nlohmann::json params,
    nlohmann::json& state,
    datastorage::DataStorage& dataStorage,
    const Library& library,
    std::function<void(const nlohmann::json&)> callback);

void printFailureReason(mesh::FailCount& failCount, std::function<void(const nlohmann::json&)> callback);
void getNormalizationValues(const nlohmann::json& accessor, nlohmann::json& meshInfo);

bool performClustering(
    const nlohmann::json& meshInfo,
    const nlohmann::json& params,
    std::vector<mesh::ClusterVertex>& vertices);

bool resolveIndexBuffer(
    nlohmann::json::json_pointer primitivePath,
    nlohmann::json& state,
    datastorage::DataStorage& dataStorage,
    std::vector<ClusterVertex>& outVertices);

uint32_t getMaxIndexValue(const std::vector<ClusterVertex>& vertices);
bool calculateIndexOffset(std::vector<ClusterVertex>& vertices);

void createIndices(
    nlohmann::json::json_pointer primitivePath,
    nlohmann::json& state,
    datastorage::DataStorage& dataStorage,
    std::vector<mesh::ClusterVertex>& vertices);

bool fillAttributeData(
    nlohmann::json::json_pointer primitivePath,
    nlohmann::json& state,
    datastorage::DataStorage& dataStorage,
    const operations::Library& library,
    std::function<void(const nlohmann::json&)> callback,
    std::vector<mesh::ClusterVertex>& vertices);

bool vertexSimilarity(
    const ClusterVertex& _vertexA,
    const ClusterVertex& _vertexB,
    const SimilarityThreshold& _threshold,
    const nlohmann::json& meshInfo);

// Generic Template to access elements of a struct
template <typename TObject, typename TMember, typename TValue>
void getValue(TObject* _object, TMember _member, TValue* _value)
{
    *_value = (*_object).*_member;
}

template <typename TMember, typename TValue>
void extractData(TMember _member, std::vector<ClusterVertex>& inData, std::vector<TValue>& outData)
{
    for (ClusterVertex& vertex : inData) {
        if (vertex.deleted) {
            continue;
        }
        TValue v;
        getValue(&vertex, _member, &v);
        outData.emplace_back(v);
    }
}
}
