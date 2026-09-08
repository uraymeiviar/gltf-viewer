#pragma once

#include "gltf/AccessorCreation.h"
#include "math/VectorTypes.h"
#include <variant>
#include <vector>

namespace ux3d::gltf {
class Gltf;
class Primitive;
class Accessor;
class string;

struct ClusterVertex {
    math::vec3 position = math::vec3(0.0);
    math::vec3 normal = math::vec3(1.0, 0.0, 0.0);
    math::vec4 tangent = math::vec4(1.0, 0.0, 0.0, 1.0);
    math::vec2 texCoord0 = math::vec2(0.0);
    math::vec2 texCoord1 = math::vec2(0.0);
    std::variant<math::vec3, math::vec4> color0 = math::vec4(0.0);

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

class VertexClustering {
public:
    VertexClustering(gltf::Gltf& _gltf, gltf::Primitive& _primitive);
    bool execute(const SimilarityThreshold& _threshold);

private:
    bool performClustering(const SimilarityThreshold& _threshold);
    bool resolveIndexBuffer();
    uint32_t getMaxIndexValue();
    template <typename TValue>
    bool createIndicesBuffer(std::vector<TValue>& _indices);
    bool calculateIndexOffset();
    template <typename TValue>
    bool updateIndicesAccessor();
    bool fillAttributeData();
    bool vertexSimilarity(const ClusterVertex& _vertexA, const ClusterVertex& _vertexB, const SimilarityThreshold& _threshold);
    bool setNormalizationValues();
    void printFailureReason();

    template <typename TMember, typename TValue>
    bool extractBuffer(TMember member, std::vector<TValue>& _buffer);

    template <typename TObject, typename TMember, typename TValue>
    void get(TObject* object, TMember member, TValue* value);

    gltf::Gltf& m_gltf;
    gltf::Primitive& m_primitive;
    std::vector<ClusterVertex> m_vertices;
    double m_meshScaling = 0.0;
    math::vec3 m_meshMinimum = math::vec3(0.0f);
    FailCount m_failCount;
};

template <typename TMember, typename TValue>
inline bool VertexClustering::extractBuffer(TMember _member, std::vector<TValue>& _buffer)
{
    for (ClusterVertex& vertex : m_vertices) {
        if (vertex.deleted) {
            continue;
        }
        TValue v;
        get(&vertex, _member, &v);
        _buffer.emplace_back(v);
    }
    return true;
}

// Generic Template to access elements of a struct
template <typename TObject, typename TMember, typename TValue>
inline void VertexClustering::get(TObject* _object, TMember _member, TValue* _value)
{
    *_value = (*_object).*_member;
}

template <typename TValue>
inline bool VertexClustering::createIndicesBuffer(std::vector<TValue>& _indices)
{
    for (ClusterVertex& vertex : m_vertices) {
        for (uint32_t index : vertex.indices) {
            uint32_t shiftedIndex = index - m_vertices[index].indexOffset;
            _indices.emplace_back(static_cast<TValue>(shiftedIndex));
        }
    }
    return true;
}

template <typename TValue>
inline bool VertexClustering::updateIndicesAccessor()
{
    std::vector<TValue> indices;
    if (!calculateIndexOffset()) {
        return false;
    }

    if (!createIndicesBuffer(indices)) {
        return false;
    }

    if (m_primitive.getIndicesAccessor() != nullptr) {
        m_gltf.deleteAccessor(*m_primitive.getIndicesAccessor());
    }

    AccessorCreation accessorCreation(m_gltf);
    Accessor* indicesAccessor = accessorCreation.create(indices);

    if (indicesAccessor == nullptr) {
        return false;
    }

    m_primitive.setIndicesAccessor(*indicesAccessor);
    return true;
}

}
