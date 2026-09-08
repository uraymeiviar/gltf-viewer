#include "gltf/VertexClustering.h"

#include "core/Logger.h"
#include "gltf/Accessor.h"
#include "gltf/Gltf.h"
#include "gltf/GltfUtils.h"
#include "gltf/mesh/Primitive.h"
#include "utils/BucketGridStructure.h"

#include <gtx/component_wise.hpp>
#include <string>
#include <type_traits>
#include <utility>

using namespace ux3d;
using namespace ux3d::gltf;

VertexClustering::VertexClustering(Gltf& _gltf, Primitive& _primitive)
    : m_gltf(_gltf)
    , m_primitive(_primitive)
{
}

bool VertexClustering::execute(const SimilarityThreshold& _threshold)
{
    // copy accessors to intermediate representation
    if (!resolveIndexBuffer()) // fill m_vertices
    {
        return false;
    }

    if (!setNormalizationValues()) // get scaling and minimal values from the mesh
    {
        return false;
    }

    if (!performClustering(_threshold)) // search for similar vertices and mark them
    {
        return false;
    }

    if (!fillAttributeData()) //recreate accessors based on found similarities
    {
        return false;
    }
    printFailureReason();
    return true;
}

void VertexClustering::printFailureReason()
{
    uint32_t maxFailureCount = 1;
    // Exclude positions as this is in most cases the main reason (obviously)
    //maxFailureCount = std::max(maxFailureCount, m_failCount.position);
    maxFailureCount = std::max(maxFailureCount, m_failCount.normal);
    maxFailureCount = std::max(maxFailureCount, m_failCount.tangent);
    maxFailureCount = std::max(maxFailureCount, m_failCount.texCoord);
    maxFailureCount = std::max(maxFailureCount, m_failCount.color);

    if (maxFailureCount == m_failCount.position) {
        //core::logInfo("Vertex Clustering: unable to merge more vertices because of position differences");
    }
    if (maxFailureCount == m_failCount.normal) {
        core::logInfo("Vertex Clustering: unable to merge more vertices because of normal differences (%d candidates)", m_failCount.normal);
    }
    if (maxFailureCount == m_failCount.tangent) {
        core::logInfo("Vertex Clustering: unable to merge more vertices because of tangent differences (%d candidates)", m_failCount.tangent);
    }
    if (maxFailureCount == m_failCount.texCoord) {
        core::logInfo("Vertex Clustering: unable to merge more vertices because of texCoord differences (%d candidates)", m_failCount.texCoord);
    }
    if (maxFailureCount == m_failCount.color) {
        core::logInfo("Vertex Clustering: unable to merge more vertices because of color differences (%d candidates)", m_failCount.color);
    }
}

bool VertexClustering::setNormalizationValues()
{
    Accessor* positionAccessor = GltfUtils::getAttributeAccessor(m_primitive, "POSITION");
    core::Vector<double>& minimum = positionAccessor->getMin();
    core::Vector<double>& maximum = positionAccessor->getMax();

    if (minimum.size() != 3 || maximum.size() != 3) {
        core::logWarning("Vertex Clustering: position accessor has no min/max values");
        return false;
    }

    m_meshMinimum.x = (float)minimum[0];
    m_meshMinimum.y = (float)minimum[1];
    m_meshMinimum.z = (float)minimum[2];

    double maxDiff = 0.0;
    for (uint32_t i = 0; i < minimum.size(); i++) {
        maxDiff = std::max(maxDiff, maximum[i] - minimum[i]);
    }
    m_meshScaling = maxDiff;

    return true;
}

bool VertexClustering::performClustering(const SimilarityThreshold& _threshold)
{
    utils::BucketGridStructure bucketGridStructure(_threshold.positionDistance);

    for (uint32_t i = 0; i < m_vertices.size(); i++) {

        bucketGridStructure.addElement(i, (m_vertices[i].position - m_meshMinimum) / (float)m_meshScaling);
    }

    for (uint32_t i = 0; i < m_vertices.size(); i++) {
        ClusterVertex& vertexA = m_vertices[i];
        if (vertexA.deleted) {
            continue;
        }
        vertexA.indices.emplace_back(i); // vertex is used by itself

        std::list<uint32_t> elementsNearby = bucketGridStructure.getElements((vertexA.position - m_meshMinimum) / (float)m_meshScaling);

        for (uint32_t j : elementsNearby) {
            if (i == j) {
                continue;
            }

            ClusterVertex& vertexB = m_vertices[j];
            if (vertexB.deleted) {
                continue;
            }

            if (vertexSimilarity(vertexA, vertexB, _threshold)) {
                vertexB.indices.emplace_back(i); // vertexA is used by vertexB...
                vertexB.deleted = true; // ...so we can delete vertexB
            }
        }
    }

    return true;
}

bool VertexClustering::resolveIndexBuffer()
{
    Accessor* indicesAccessor = m_primitive.getIndicesAccessor();
    Accessor* positionAccessor = GltfUtils::getAttributeAccessor(m_primitive, "POSITION");
    Accessor* texCoord0Accessor = GltfUtils::getAttributeAccessor(m_primitive, "TEXCOORD_0");
    Accessor* texCoord1Accessor = GltfUtils::getAttributeAccessor(m_primitive, "TEXCOORD_1");
    Accessor* normalAccessor = GltfUtils::getAttributeAccessor(m_primitive, "NORMAL");
    Accessor* tangentAccessor = GltfUtils::getAttributeAccessor(m_primitive, "TANGENT");
    Accessor* color0Accessor = GltfUtils::getAttributeAccessor(m_primitive, "COLOR_0");

    //find out vertex count dependent on the availability of an indices buffer
    uint32_t vertexCount = 0u;
    if (indicesAccessor != nullptr) {
        vertexCount = indicesAccessor->getCount();
    } else {
        vertexCount = positionAccessor->getCount();
    }

    for (uint32_t i = 0; i < vertexCount; i++) {
        // if we have no indices buffer, we use sequential indexing
        uint32_t vertexIndex = i;
        if (indicesAccessor != nullptr) {
            vertexIndex = std::get<uint32_t>(GltfUtils::getElementAtIndex(indicesAccessor, i, false));
        }

        ClusterVertex clusterVertex;

        if (positionAccessor != nullptr) {
            clusterVertex.position = std::get<glm::vec3>(GltfUtils::getElementAtIndex(positionAccessor, vertexIndex));
        }
        if (normalAccessor != nullptr) {
            clusterVertex.normal = std::get<glm::vec3>(GltfUtils::getElementAtIndex(normalAccessor, vertexIndex));
        }
        if (tangentAccessor != nullptr) {
            clusterVertex.tangent = std::get<glm::vec4>(GltfUtils::getElementAtIndex(tangentAccessor, vertexIndex));
        }
        if (texCoord0Accessor != nullptr) {
            clusterVertex.texCoord0 = std::get<glm::vec2>(GltfUtils::getElementAtIndex(texCoord0Accessor, vertexIndex));
        }
        if (texCoord1Accessor != nullptr) {
            clusterVertex.texCoord1 = std::get<glm::vec2>(GltfUtils::getElementAtIndex(texCoord1Accessor, vertexIndex));
        }
        if (color0Accessor != nullptr) {
            // color attribute can either be vec3 or vec4
            if (color0Accessor->getType() == AttributeType::Vec3) {
                clusterVertex.color0 = std::get<glm::vec3>(GltfUtils::getElementAtIndex(color0Accessor, vertexIndex));
            } else {
                clusterVertex.color0 = std::get<glm::vec4>(GltfUtils::getElementAtIndex(color0Accessor, vertexIndex));
            }
        }

        m_vertices.emplace_back(clusterVertex);
    }

    return true;
}

bool VertexClustering::vertexSimilarity(const ClusterVertex& _vertexA, const ClusterVertex& _vertexB, const SimilarityThreshold& _threshold)
{
    double positionDistance = glm::distance(_vertexA.position, _vertexB.position);
    if (positionDistance > _threshold.positionDistance * m_meshScaling) {
        m_failCount.position++;
        return false;
    }

    double normalDeviation = 1.0 - glm::abs(glm::dot(_vertexA.normal, _vertexB.normal));
    if (normalDeviation > _threshold.normalDeviation + 0.01) // includes angular deviation offset
    {
        m_failCount.normal++;
        return false;
    }

    double tangentDeviation = 1.0 - glm::abs(glm::dot(glm::normalize(glm::vec3(_vertexA.tangent)), glm::normalize(glm::vec3(_vertexB.tangent))));
    if (tangentDeviation > _threshold.tangentDeviation + 0.01 // includes angular deviation offset
        || _vertexA.tangent.w != _vertexB.tangent.w) {
        m_failCount.tangent++;
        return false;
    }

    double texCoord0Distance = glm::compMax(glm::abs(_vertexA.texCoord0 - _vertexB.texCoord0));
    if (texCoord0Distance > _threshold.texCoordDistance) {
        m_failCount.texCoord++;
        return false;
    }

    double texCoord1Distance = glm::compMax(glm::abs(_vertexA.texCoord1 - _vertexB.texCoord1));
    if (texCoord1Distance > _threshold.texCoordDistance) {
        m_failCount.texCoord++;
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
        m_failCount.color++;
        return false;
    }

    return true;
}

bool VertexClustering::calculateIndexOffset()
{
    uint32_t deletedVertices = 0u;
    for (ClusterVertex& vertex : m_vertices) {
        if (vertex.deleted) {
            deletedVertices++;
        }
        vertex.indexOffset = deletedVertices;
    }
    return true;
}

uint32_t VertexClustering::getMaxIndexValue()
{
    uint32_t maxValue = 0;
    for (ClusterVertex& vertex : m_vertices) {
        if (vertex.deleted) {
            continue;
        }
        maxValue++;
    }
    return maxValue;
}

bool VertexClustering::fillAttributeData()
{
    // Choose component type of indices accessor based on its maximum value
    uint32_t maxIndex = getMaxIndexValue();
    bool success = true;
    if (maxIndex >= std::numeric_limits<uint16_t>::max()) {
        success = updateIndicesAccessor<uint32_t>();
    } else if (maxIndex >= std::numeric_limits<uint8_t>::max()) {
        success = updateIndicesAccessor<uint16_t>();
    } else {
        success = updateIndicesAccessor<uint8_t>();
    }

    if (!success) {
        return false;
    }

    for (gltf::Attribute& attribute : m_primitive.getAttributes()) {
        if (attribute.getName() == "POSITION") {
            std::vector<glm::vec3> buffer;
            extractBuffer(&ClusterVertex::position, buffer);

            m_gltf.deleteAccessor(*attribute.getAccessor());
            AccessorCreation accessorCreation(m_gltf);
            Accessor* attributeAccessor = accessorCreation.create(buffer);
            attribute.setAccessor(attributeAccessor);
        }
        if (attribute.getName() == "NORMAL") {
            std::vector<glm::vec3> buffer;
            extractBuffer(&ClusterVertex::normal, buffer);

            m_gltf.deleteAccessor(*attribute.getAccessor());
            AccessorCreation accessorCreation(m_gltf);
            Accessor* attributeAccessor = accessorCreation.create(buffer);
            attribute.setAccessor(attributeAccessor);
        }
        if (attribute.getName() == "TANGENT") {
            std::vector<glm::vec4> buffer;
            extractBuffer(&ClusterVertex::tangent, buffer);
            m_gltf.deleteAccessor(*attribute.getAccessor());
            AccessorCreation accessorCreation(m_gltf);
            Accessor* attributeAccessor = accessorCreation.create(buffer);
            attribute.setAccessor(attributeAccessor);
        }
        if (attribute.getName() == "TEXCOORD_0") {
            std::vector<glm::vec2> buffer;
            extractBuffer(&ClusterVertex::texCoord0, buffer);

            m_gltf.deleteAccessor(*attribute.getAccessor());
            AccessorCreation accessorCreation(m_gltf);
            Accessor* attributeAccessor = accessorCreation.create(buffer);
            attribute.setAccessor(attributeAccessor);
        }
        if (attribute.getName() == "TEXCOORD_1") {
            std::vector<glm::vec2> buffer;
            extractBuffer(&ClusterVertex::texCoord1, buffer);
            m_gltf.deleteAccessor(*attribute.getAccessor());
            AccessorCreation accessorCreation(m_gltf);
            Accessor* attributeAccessor = accessorCreation.create(buffer);
            attribute.setAccessor(attributeAccessor);
        }
        if (attribute.getName() == "COLOR_0") {
            std::vector<std::variant<math::vec3, math::vec4>> buffer;
            extractBuffer(&ClusterVertex::color0, buffer);
            AccessorCreation accessorCreation(m_gltf);
            AttributeType type = attribute.getAccessor()->getType();
            m_gltf.deleteAccessor(*attribute.getAccessor());

            if (type == AttributeType::Vec3) {
                std::vector<glm::vec3> vec3Buffer;
                vec3Buffer.reserve(buffer.size());
                for (std::variant<math::vec3, math::vec4>& var : buffer) {
                    vec3Buffer.push_back(std::get<glm::vec3>(var));
                }
                Accessor* attributeAccessor = accessorCreation.create(vec3Buffer);
                attribute.setAccessor(attributeAccessor);
            } else {
                std::vector<glm::vec4> vec4Buffer;
                vec4Buffer.reserve(buffer.size());
                for (std::variant<math::vec3, math::vec4>& var : buffer) {
                    vec4Buffer.push_back(std::get<glm::vec4>(var));
                }
                Accessor* attributeAccessor = accessorCreation.create(vec4Buffer);
                attribute.setAccessor(attributeAccessor);
            }
        }
    }
    return true;
}
