#include "VertexClustering.h"

#include "BucketGridStructure.h"
#include "HelperFunctions.h"
#include "operations/Library.h"
#include "reactor/Utils.h"

#include <algorithm>
#include <fmt/format.h>
#include <glm.hpp>
#include <gtx/component_wise.hpp>
#include <utility>

using namespace ux3d;
using namespace ux3d::operations;

nlohmann::json mesh::clusterVertices(nlohmann::json params, nlohmann::json& state, datastorage::DataStorage& datastorage, const Library& library, std::function<void(const nlohmann::json&)> callback)
{
    auto primitivePath = nlohmann::json::json_pointer(params["path"]);
    auto& primitive = state.at(primitivePath);

    std::vector<mesh::ClusterVertex> vertices;

    if (!mesh::resolveIndexBuffer(primitivePath, state, datastorage, vertices)) // fill m_vertices
    {
        throw std::runtime_error("Unable to resolve index buffer");
    }
    nlohmann::json meshInfo = {};
    nlohmann::json positionAccessor = state.at(getAccessor(primitive["attributes"]["POSITION"]));
    mesh::getNormalizationValues(positionAccessor, meshInfo); // get scaling and minimal values from the mesh

    mesh::FailCount failCount;
    if (!mesh::performClustering(meshInfo, params, vertices)) // search for similar vertices and mark them
    {
        throw std::runtime_error("Unable to perform clustering ");
    }

    mesh::fillAttributeData(primitivePath, state, datastorage, library, callback, vertices); // recreate accessors based on found similarities

    {
        std::string op = "calculate_min_max";
        uint32_t accessorIndex = state.at(primitivePath)["attributes"]["POSITION"].get<uint32_t>();
        auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
        taskDescription[op]["path"] = (nlohmann::json::json_pointer("/gltf/accessors") / accessorIndex).to_string();
        state = library.call(std::move(taskDescription), std::move(state), datastorage, callback);
    }

    printFailureReason(failCount, callback);
    return state;
}

bool mesh::resolveIndexBuffer(
    nlohmann::json::json_pointer prmitivePath,
    nlohmann::json& state,
    datastorage::DataStorage& dataStorage,
    std::vector<mesh::ClusterVertex>& outVertices)
{
    auto& primitive = state.at(prmitivePath);
    auto getAttributeData = [&state, &dataStorage ](auto primitive, std::string attribute) -> auto
    {
        auto accessorPath = mesh::getAccessor(primitive["attributes"][attribute]);
        return reactor::utils::getAccessorData(accessorPath, state, dataStorage);
    };

    // find out vertex count dependent on the availability of an indices buffer
    uint32_t vertexCount = 0u;
    reactor::utils::AccessorTypeVariant indicesData = std::monostate();
    if (primitive.contains("indices")) {
        auto accessorPath = mesh::getAccessor(primitive["indices"]);
        indicesData = reactor::utils::getAccessorData(accessorPath, state, dataStorage);
        vertexCount = state.at(accessorPath)["count"].get<uint32_t>();
    }

    std::map<std::string, reactor::utils::AccessorTypeVariant> attributeMap;
    for (auto& [key, value] : primitive["attributes"].items()) {
        attributeMap[key] = getAttributeData(primitive, key);
    }

    if (vertexCount == 0) {
        vertexCount = std::get<std::vector<glm::vec3>>(attributeMap["POSITION"]).size();
    }

    for (uint32_t i = 0; i < vertexCount; i++) {
        // if we have no indices buffer, we use sequential indexing
        uint32_t vertexIndex = i;
        if (std::holds_alternative<std::vector<uint32_t>>(indicesData)) {
            vertexIndex = std::get<std::vector<uint32_t>>(indicesData)[i];
        }
        if (std::holds_alternative<std::vector<uint16_t>>(indicesData)) {
            vertexIndex = std::get<std::vector<uint16_t>>(indicesData)[i];
        }
        if (std::holds_alternative<std::vector<uint8_t>>(indicesData)) {
            vertexIndex = std::get<std::vector<uint8_t>>(indicesData)[i];
        }

        mesh::ClusterVertex clusterVertex;

        if (attributeMap.contains("POSITION")) {

            clusterVertex.position = std::get<std::vector<glm::vec3>>(attributeMap["POSITION"])[vertexIndex];
        }
        if (attributeMap.contains("TEXCOORD_0")) {

            clusterVertex.texCoord0 = std::get<std::vector<glm::vec2>>(attributeMap["TEXCOORD_0"])[vertexIndex];
        }
        if (attributeMap.contains("TEXCOORD_1")) {

            clusterVertex.texCoord1 = std::get<std::vector<glm::vec2>>(attributeMap["TEXCOORD_1"])[vertexIndex];
        }
        if (attributeMap.contains("NORMAL")) {

            clusterVertex.normal = std::get<std::vector<glm::vec3>>(attributeMap["NORMAL"])[vertexIndex];
        }
        if (attributeMap.contains("TANGENT")) {

            clusterVertex.tangent = std::get<std::vector<glm::vec4>>(attributeMap["TANGENT"])[vertexIndex];
        }
        if (attributeMap.contains("COLOR_0")) {
            if (std::holds_alternative<std::vector<glm::vec3>>(attributeMap["COLOR_0"])) {
                clusterVertex.color0 = std::get<std::vector<glm::vec3>>(attributeMap["COLOR_0"])[vertexIndex];
            }
            if (std::holds_alternative<std::vector<glm::vec4>>(attributeMap["COLOR_0"])) {
                clusterVertex.color0 = std::get<std::vector<glm::vec4>>(attributeMap["COLOR_0"])[vertexIndex];
            }
        }

        outVertices.emplace_back(clusterVertex);
    }

    return true;
}

void mesh::getNormalizationValues(const nlohmann::json& accessor, nlohmann::json& meshInfo)
{
    glm::dvec3 minimum = accessor.at("min").get<glm::dvec3>();
    glm::dvec3 maximum = accessor.at("max").get<glm::dvec3>();

    meshInfo["minimum"] = minimum;

    double maxDiff = 0.0;
    for (uint32_t i = 0; i < 3; i++) {
        maxDiff = std::max(maxDiff, maximum[i] - minimum[i]);
    }
    meshInfo["scale"] = maxDiff;
}

bool mesh::performClustering(
    const nlohmann::json& meshInfo,
    const nlohmann::json& params,
    std::vector<mesh::ClusterVertex>& vertices)
{
    mesh::SimilarityThreshold threshold;
    threshold.positionDistance = params["positionThreshold"];
    threshold.normalDeviation = params["normalThreshold"];
    threshold.tangentDeviation = params["tangentThreshold"];
    threshold.texCoordDistance = params["texCoordThreshold"];
    threshold.colorDistance = params["colorThreshold"];

    mesh::BucketGridStructure bucketGridStructure(threshold.positionDistance);

    for (uint32_t i = 0; i < vertices.size(); i++) {

        bucketGridStructure.addElement(i, (vertices[i].position - meshInfo["minimum"].get<glm::vec3>()) / meshInfo["scale"].get<float>());
    }

    for (uint32_t i = 0; i < vertices.size(); i++) {
        ClusterVertex& vertexA = vertices[i];
        if (vertexA.deleted) {
            continue;
        }
        vertexA.indices.emplace_back(i); // vertex is used by itself

        std::list<uint32_t> elementsNearby = bucketGridStructure.getElements((vertexA.position - meshInfo["minimum"].get<glm::vec3>()) / meshInfo["scale"].get<float>());

        for (uint32_t j : elementsNearby) {
            if (i == j) {
                continue;
            }

            ClusterVertex& vertexB = vertices[j];
            if (vertexB.deleted) {
                continue;
            }

            if (vertexSimilarity(vertexA, vertexB, threshold, meshInfo)) {
                vertexB.indices.emplace_back(i); // vertexA is used by vertexB...
                vertexB.deleted = true; // ...so we can delete vertexB
            }
        }
    }

    return true;
}

bool mesh::vertexSimilarity(const ClusterVertex& _vertexA, const ClusterVertex& _vertexB, const SimilarityThreshold& _threshold, const nlohmann::json& meshInfo)
{
    double positionDistance = glm::distance(_vertexA.position, _vertexB.position);
    if (positionDistance > _threshold.positionDistance * meshInfo["scale"].get<float>()) {
        // m_failCount.position++;
        return false;
    }

    double normalDeviation = 1.0 - glm::abs(glm::dot(_vertexA.normal, _vertexB.normal));
    if (normalDeviation > _threshold.normalDeviation + 0.01) // includes angular deviation offset
    {
        // m_failCount.normal++;
        return false;
    }

    double tangentDeviation = 1.0 - glm::abs(glm::dot(glm::normalize(glm::vec3(_vertexA.tangent)), glm::normalize(glm::vec3(_vertexB.tangent))));
    if (tangentDeviation > _threshold.tangentDeviation + 0.01 // includes angular deviation offset
        || _vertexA.tangent.w != _vertexB.tangent.w) {
        // m_failCount.tangent++;
        return false;
    }

    double texCoord0Distance = glm::compMax(glm::abs(_vertexA.texCoord0 - _vertexB.texCoord0));
    if (texCoord0Distance > _threshold.texCoordDistance) {
        // m_failCount.texCoord++;
        return false;
    }

    double texCoord1Distance = glm::compMax(glm::abs(_vertexA.texCoord1 - _vertexB.texCoord1));
    if (texCoord1Distance > _threshold.texCoordDistance) {
        // m_failCount.texCoord++;
        return false;
    }

    double colorDistance = 0.0;
    // index 0 corresponds to vec3
    if (_vertexA.color0.index() == 0 && _vertexA.color0.index() == _vertexB.color0.index()) {
        colorDistance = glm::compMax(glm::abs(std::get<glm::vec3>(_vertexA.color0) - std::get<glm::vec3>(_vertexB.color0)));
    } else {
        colorDistance = glm::compMax(glm::abs(std::get<glm::vec4>(_vertexA.color0) - std::get<glm::vec4>(_vertexB.color0)));
    }
    if (colorDistance > _threshold.colorDistance) {
        // m_failCount.color++;
        return false;
    }

    return true;
}

bool mesh::calculateIndexOffset(std::vector<mesh::ClusterVertex>& inOutVertices)
{
    uint32_t deletedVertices = 0u;
    for (mesh::ClusterVertex& vertex : inOutVertices) {
        if (vertex.deleted) {
            deletedVertices++;
        }
        vertex.indexOffset = deletedVertices;
    }
    return true;
}

uint32_t mesh::getMaxIndexValue(const std::vector<mesh::ClusterVertex>& vertices)
{
    uint32_t maxValue = 0;
    for (const mesh::ClusterVertex& vertex : vertices) {
        if (vertex.deleted) {
            continue;
        }
        maxValue++;
    }
    return maxValue;
}

void mesh::createIndices(
    nlohmann::json::json_pointer primitivePath,
    nlohmann::json& state,
    datastorage::DataStorage& dataStorage,
    std::vector<mesh::ClusterVertex>& vertices)
{
    auto createIndicesByType = [&](auto indices) {
        for (mesh::ClusterVertex& vertex : vertices) {
            for (uint32_t index : vertex.indices) {
                uint32_t shiftedIndex = index - vertices[index].indexOffset;
                indices.emplace_back(shiftedIndex);
            }
        }
        auto accessorPath = reactor::utils::createAccessorFromData(indices, state, dataStorage);
        state.at(primitivePath)["indices"] = std::stoi(accessorPath.back());
    };

    mesh::calculateIndexOffset(vertices);
    uint32_t maxIndex = mesh::getMaxIndexValue(vertices);

    if (maxIndex >= std::numeric_limits<uint16_t>::max()) {
        std::vector<uint32_t> indices;
        createIndicesByType(indices);
    } else if (maxIndex >= std::numeric_limits<uint8_t>::max()) {
        std::vector<uint16_t> indices;
        createIndicesByType(indices);
    } else {
        std::vector<uint8_t> indices;
        createIndicesByType(indices);
    }
}

bool mesh::fillAttributeData(
    nlohmann::json::json_pointer primitivePath,
    nlohmann::json& state,
    datastorage::DataStorage& dataStorage,
    const operations::Library& library,
    std::function<void(const nlohmann::json&)> callback,
    std::vector<mesh::ClusterVertex>& vertices)
{
    auto& primitive = state.at(primitivePath);
    auto accessorPath = nlohmann::json::json_pointer("/gltf/accessors");

    nlohmann::json unusedAccessors = nlohmann::json::array();
    if (primitive.contains("indices")) {
        unusedAccessors.push_back((accessorPath / primitive.at("indices").get<uint32_t>()).to_string());
    }

    mesh::createIndices(primitivePath, state, dataStorage, vertices);

    for (auto& [attribute, accessor] : primitive.at("attributes").items()) {
        unusedAccessors.push_back((accessorPath / accessor.get<uint32_t>()).to_string());

        if (attribute == "POSITION") {
            std::vector<glm::vec3> data;
            extractData(&ClusterVertex::position, vertices, data);
            auto accessorPath = reactor::utils::createAccessorFromData(data, state, dataStorage);

            primitive["attributes"][attribute] = std::stoi(accessorPath.back());
        }
        if (attribute == "NORMAL") {
            std::vector<glm::vec3> data;
            extractData(&ClusterVertex::normal, vertices, data);
            auto accessorPath = reactor::utils::createAccessorFromData(data, state, dataStorage);
            primitive["attributes"][attribute] = std::stoi(accessorPath.back());
        }
        if (attribute == "TANGENT") {
            std::vector<glm::vec4> data;
            extractData(&ClusterVertex::tangent, vertices, data);
            auto accessorPath = reactor::utils::createAccessorFromData(data, state, dataStorage);
            primitive["attributes"][attribute] = std::stoi(accessorPath.back());
        }
        if (attribute == "TEXCOORD_0") {
            std::vector<glm::vec2> data;
            extractData(&ClusterVertex::texCoord0, vertices, data);
            auto accessorPath = reactor::utils::createAccessorFromData(data, state, dataStorage);
            primitive["attributes"][attribute] = std::stoi(accessorPath.back());
        }
        if (attribute == "TEXCOORD_1") {
            std::vector<glm::vec2> data;
            extractData(&ClusterVertex::texCoord1, vertices, data);
            auto accessorPath = reactor::utils::createAccessorFromData(data, state, dataStorage);
            primitive["attributes"][attribute] = std::stoi(accessorPath.back());
        }
        if (attribute == "COLOR_0") {

            std::vector<std::variant<glm::vec3, glm::vec4>> dataVariant;
            extractData(&ClusterVertex::color0, vertices, dataVariant);

            if (!dataVariant.empty() && std::holds_alternative<glm::vec3>(dataVariant[0])) {
                std::vector<glm::vec3> vec3Buffer;
                vec3Buffer.reserve(dataVariant.size());
                for (std::variant<glm::vec3, glm::vec4>& var : dataVariant) {
                    vec3Buffer.push_back(std::get<glm::vec3>(var));
                }
                auto accessorPath = reactor::utils::createAccessorFromData(vec3Buffer, state, dataStorage);
                primitive["attributes"][attribute] = std::stoi(accessorPath.back());
            } else {
                std::vector<glm::vec4> vec4Buffer;
                vec4Buffer.reserve(dataVariant.size());
                for (std::variant<glm::vec3, glm::vec4>& var : dataVariant) {
                    vec4Buffer.push_back(std::get<glm::vec4>(var));
                }
                auto accessorPath = reactor::utils::createAccessorFromData(vec4Buffer, state, dataStorage);
                primitive["attributes"][attribute] = std::stoi(accessorPath.back());
            }
        }
    }

    { // Remove no longer used accessors
        std::string op = "delete_accessor";
        auto taskDescription = nlohmann::json({ { op, nlohmann::json::object() } });
        taskDescription[op]["path"] = unusedAccessors;
        state = library.call(std::move(taskDescription), std::move(state), dataStorage, callback);
    }
    return true;
}

void mesh::printFailureReason(mesh::FailCount& failCount, std::function<void(const nlohmann::json&)> callback)
{
    uint32_t maxFailureCount = 1;
    // Exclude positions as this is in most cases the main reason (obviously)
    // maxFailureCount = std::max(maxFailureCount, m_failCount.position);
    maxFailureCount = std::max(maxFailureCount, failCount.normal);
    maxFailureCount = std::max(maxFailureCount, failCount.tangent);
    maxFailureCount = std::max(maxFailureCount, failCount.texCoord);
    maxFailureCount = std::max(maxFailureCount, failCount.color);

    if (maxFailureCount == failCount.position) {
        callback({ { "message", "Vertex Clustering: unable to merge more vertices because of position differences" } });
    }
    if (maxFailureCount == failCount.normal) {
        callback({ { "message", fmt::format("Vertex Clustering: unable to merge more vertices because of normal differences ({} candidates)", failCount.normal) } });
    }
    if (maxFailureCount == failCount.tangent) {
        callback({ { "message", fmt::format("Vertex Clustering: unable to merge more vertices because of tangent differences ({} candidates)", failCount.tangent) } });
    }
    if (maxFailureCount == failCount.texCoord) {
        callback({ { "message", fmt::format("Vertex Clustering: unable to merge more vertices because of texCoord differences ({} candidates)", failCount.texCoord) } });
    }
    if (maxFailureCount == failCount.color) {
        callback({ { "message", fmt::format("Vertex Clustering: unable to merge more vertices because of color differences ({} candidates)", failCount.color) } });
    }
}
