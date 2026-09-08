#include "gltf/MeshDecimation.h"

#include "core/Logger.h"
#include "core/MemoryRange.h"
#include "gltf/Accessor.h"
#include "gltf/AccessorCreation.h"
#include "gltf/Gltf.h"
#include "gltf/GltfUtils.h"
#include "gltf/fast_quadric_mesh_simplification/Simplify.h"
#include "gltf/mesh/Primitive.h"

#include <string>
#include <type_traits>
#include <utility>

using namespace ux3d;
using namespace ux3d::gltf;

MeshDecimation::MeshDecimation(Gltf& _gltf, Primitive& _primitive)
    : m_gltf(_gltf)
    , m_primitive(_primitive)
{
}

bool MeshDecimation::execute(float _targetPercentage, uint32_t _aggressiveness)
{
    AttributeCollection attributeCollection;

    if (!setData()) {
        return false;
    }

    if (!callFQMSLibrary(_targetPercentage, _aggressiveness)) {
        return false;
    }

    getData(attributeCollection);

    fillAttributeData(attributeCollection);

    return true;
}

bool MeshDecimation::setData()
{
    if (m_primitive.getIndicesAccessor() == nullptr) {
        core::logError("Mesh Decimation: mesh indices are missing");
        return false; // we need an index buffer
    }

    Accessor* indicesAccessor = m_primitive.getIndicesAccessor();
    Accessor* positionAccessor = GltfUtils::getAttributeAccessor(m_primitive, "POSITION");
    Accessor* texCoord0Accessor = GltfUtils::getAttributeAccessor(m_primitive, "TEXCOORD_0");
    Accessor* texCoord1Accessor = GltfUtils::getAttributeAccessor(m_primitive, "TEXCOORD_1");
    Accessor* normalAccessor = GltfUtils::getAttributeAccessor(m_primitive, "NORMAL");
    Accessor* tangentAccessor = GltfUtils::getAttributeAccessor(m_primitive, "TANGENT");
    Accessor* color0Accessor = GltfUtils::getAttributeAccessor(m_primitive, "COLOR_0");

    if (indicesAccessor->getCount() == positionAccessor->getCount()) {
        core::logWarning("Mesh Decimation: indices=%d positions=%d", indicesAccessor->getCount(), positionAccessor->getCount());
        core::logWarning("Mesh Decimation: positions of triangles are not connected to neighboring triangles. Consider recreating the index buffer by applying vertex clustering.");
        return false;
    }

    Simplify::vertices.clear();
    Simplify::triangles.clear();

    float tangentHandedness = 0;
    if (tangentAccessor != nullptr) {
        tangentHandedness = std::get<glm::vec4>(GltfUtils::getElementAtIndex(tangentAccessor, 0)).w;
    }

    for (uint32_t i = 0; i < indicesAccessor->getCount() / 3; i++) {
        Simplify::Triangle triangle;
        triangle.attr = 0;

        triangle.v[0] = static_cast<int32_t>(std::get<uint32_t>(GltfUtils::getElementAtIndex(indicesAccessor, i * 3 + 0, false)));
        triangle.v[1] = static_cast<int32_t>(std::get<uint32_t>(GltfUtils::getElementAtIndex(indicesAccessor, i * 3 + 1, false)));
        triangle.v[2] = static_cast<int32_t>(std::get<uint32_t>(GltfUtils::getElementAtIndex(indicesAccessor, i * 3 + 2, false)));

        if (texCoord0Accessor != nullptr) {
            triangle.texCoord0[0] = std::get<glm::vec2>(GltfUtils::getElementAtIndex(texCoord0Accessor, static_cast<uint32_t>(triangle.v[0])));
            triangle.texCoord0[1] = std::get<glm::vec2>(GltfUtils::getElementAtIndex(texCoord0Accessor, static_cast<uint32_t>(triangle.v[1])));
            triangle.texCoord0[2] = std::get<glm::vec2>(GltfUtils::getElementAtIndex(texCoord0Accessor, static_cast<uint32_t>(triangle.v[2])));
        }
        if (texCoord1Accessor != nullptr) {
            triangle.texCoord1[0] = std::get<glm::vec2>(GltfUtils::getElementAtIndex(texCoord1Accessor, static_cast<uint32_t>(triangle.v[0])));
            triangle.texCoord1[1] = std::get<glm::vec2>(GltfUtils::getElementAtIndex(texCoord1Accessor, static_cast<uint32_t>(triangle.v[1])));
            triangle.texCoord1[2] = std::get<glm::vec2>(GltfUtils::getElementAtIndex(texCoord1Accessor, static_cast<uint32_t>(triangle.v[2])));
        }
        if (normalAccessor != nullptr) {
            triangle.normal[0] = std::get<glm::vec3>(GltfUtils::getElementAtIndex(normalAccessor, static_cast<uint32_t>(triangle.v[0])));
            triangle.normal[1] = std::get<glm::vec3>(GltfUtils::getElementAtIndex(normalAccessor, static_cast<uint32_t>(triangle.v[1])));
            triangle.normal[2] = std::get<glm::vec3>(GltfUtils::getElementAtIndex(normalAccessor, static_cast<uint32_t>(triangle.v[2])));
        }
        if (tangentAccessor != nullptr) {
            for (uint32_t i = 0; i < 3; i++) {
                glm::vec4 tangent = std::get<glm::vec4>(GltfUtils::getElementAtIndex(tangentAccessor, static_cast<uint32_t>(triangle.v[i])));
                if (tangent.w != tangentHandedness) {
                    tangent *= -1.0f;
                }
                triangle.tangent[i] = tangent;
            }
        }
        if (color0Accessor != nullptr) {
            if (color0Accessor->getType() == AttributeType::Vec3) {
                glm::vec3 v1 = std::get<glm::vec3>(GltfUtils::getElementAtIndex(color0Accessor, static_cast<uint32_t>(triangle.v[0])));
                glm::vec3 v2 = std::get<glm::vec3>(GltfUtils::getElementAtIndex(color0Accessor, static_cast<uint32_t>(triangle.v[1])));
                glm::vec3 v3 = std::get<glm::vec3>(GltfUtils::getElementAtIndex(color0Accessor, static_cast<uint32_t>(triangle.v[2])));
                triangle.color0[0] = glm::vec4(v1, 0);
                triangle.color0[1] = glm::vec4(v2, 0);
                triangle.color0[2] = glm::vec4(v3, 0);
            } else {
                triangle.color0[0] = std::get<glm::vec4>(GltfUtils::getElementAtIndex(color0Accessor, static_cast<uint32_t>(triangle.v[0])));
                triangle.color0[1] = std::get<glm::vec4>(GltfUtils::getElementAtIndex(color0Accessor, static_cast<uint32_t>(triangle.v[1])));
                triangle.color0[2] = std::get<glm::vec4>(GltfUtils::getElementAtIndex(color0Accessor, static_cast<uint32_t>(triangle.v[2])));
            }
        }

        Simplify::triangles.push_back(triangle);
    }

    for (uint32_t i = 0; i < positionAccessor->getCount(); i++) {
        Simplify::Vertex simplifyVertex;

        simplifyVertex.position = std::get<glm::vec3>(GltfUtils::getElementAtIndex(positionAccessor, i));

        Simplify::vertices.push_back(simplifyVertex);
    }
    return true;
}

bool MeshDecimation::callFQMSLibrary(float _targetPercentage, uint32_t _aggressiveness)
{
    size_t initialTriangleCount = Simplify::triangles.size();
    Simplify::simplify_mesh(static_cast<int>(static_cast<float>(initialTriangleCount) * _targetPercentage), _aggressiveness, true);
    if (initialTriangleCount == Simplify::triangles.size()) {
        core::logWarning("Mesh Decimation: unable to reduce triangle count. Consider recreating the index buffer by applying vertex clustering with a higher threshold.");
        return false;
    }
    return true;
}

void MeshDecimation::getData(AttributeCollection& _attributeCollection)
{
    _attributeCollection.positions.clear();
    _attributeCollection.indices32u.clear();
    _attributeCollection.indices16u.clear();

    size_t vertexCount = Simplify::vertices.size();

    for (uint32_t i = 0; i < vertexCount; i++) {
        _attributeCollection.positions.push_back(Simplify::vertices[i].position);
    }

    // Potential improvement: do not resize if this attribute is not used
    _attributeCollection.texCoords0.resize(vertexCount);
    _attributeCollection.texCoords1.resize(vertexCount);
    _attributeCollection.normals.resize(vertexCount);
    _attributeCollection.tangents.resize(vertexCount);
    _attributeCollection.color0.resize(vertexCount);

    for (uint32_t i = 0; i < Simplify::triangles.size(); i++) {
        Simplify::Triangle triangle = Simplify::triangles[i];

        uint32_t vertexIndex[3];
        vertexIndex[0] = (uint32_t)triangle.v[0];
        vertexIndex[1] = (uint32_t)triangle.v[1];
        vertexIndex[2] = (uint32_t)triangle.v[2];

        // Select indices component type based on vertex count
        if (vertexCount >= std::numeric_limits<uint16_t>::max()) {
            _attributeCollection.indices32u.push_back(vertexIndex[0]);
            _attributeCollection.indices32u.push_back(vertexIndex[1]);
            _attributeCollection.indices32u.push_back(vertexIndex[2]);
        } else {
            _attributeCollection.indices16u.push_back(static_cast<uint16_t>(vertexIndex[0]));
            _attributeCollection.indices16u.push_back(static_cast<uint16_t>(vertexIndex[1]));
            _attributeCollection.indices16u.push_back(static_cast<uint16_t>(vertexIndex[2]));
        }

        _attributeCollection.texCoords0[vertexIndex[0]] = triangle.texCoord0[0];
        _attributeCollection.texCoords0[vertexIndex[1]] = triangle.texCoord0[1];
        _attributeCollection.texCoords0[vertexIndex[2]] = triangle.texCoord0[2];

        _attributeCollection.texCoords1[vertexIndex[0]] = triangle.texCoord1[0];
        _attributeCollection.texCoords1[vertexIndex[1]] = triangle.texCoord1[1];
        _attributeCollection.texCoords1[vertexIndex[2]] = triangle.texCoord1[2];

        _attributeCollection.normals[vertexIndex[0]] = triangle.normal[0];
        _attributeCollection.normals[vertexIndex[1]] = triangle.normal[1];
        _attributeCollection.normals[vertexIndex[2]] = triangle.normal[2];

        _attributeCollection.tangents[vertexIndex[0]] = triangle.tangent[0];
        _attributeCollection.tangents[vertexIndex[1]] = triangle.tangent[1];
        _attributeCollection.tangents[vertexIndex[2]] = triangle.tangent[2];

        _attributeCollection.color0[vertexIndex[0]] = triangle.color0[0];
        _attributeCollection.color0[vertexIndex[1]] = triangle.color0[1];
        _attributeCollection.color0[vertexIndex[2]] = triangle.color0[2];
    }
}

void MeshDecimation::fillAttributeData(AttributeCollection& _attributeCollection)
{

    AccessorCreation accessorCreation(m_gltf);

    Accessor* indicesAccessor = nullptr;
    if (_attributeCollection.indices32u.empty() == false) {
        indicesAccessor = accessorCreation.create(_attributeCollection.indices32u);
    } else if (_attributeCollection.indices16u.empty() == false) {
        indicesAccessor = accessorCreation.create(_attributeCollection.indices16u);
    } else {
        core::debugLog(core::LogLevel::Error, "Mesh Decimation: index accessor is empty ");
        return;
    }

    m_gltf.deleteAccessor(*m_primitive.getIndicesAccessor());
    m_primitive.setIndicesAccessor(*indicesAccessor);

    for (gltf::Attribute& attribute : m_primitive.getAttributes()) {
        if (attribute.getName() == "POSITION") {
            m_gltf.deleteAccessor(*attribute.getAccessor());
            AccessorCreation accessorCreation(m_gltf);
            Accessor* attributeAccessor = accessorCreation.create(_attributeCollection.positions);
            attribute.setAccessor(attributeAccessor);
        }
        if (attribute.getName() == "NORMAL") {
            m_gltf.deleteAccessor(*attribute.getAccessor());
            AccessorCreation accessorCreation(m_gltf);
            Accessor* attributeAccessor = accessorCreation.create(_attributeCollection.normals);
            attribute.setAccessor(attributeAccessor);
        }
        if (attribute.getName() == "TANGENT") {
            m_gltf.deleteAccessor(*attribute.getAccessor());
            AccessorCreation accessorCreation(m_gltf);
            Accessor* attributeAccessor = accessorCreation.create(_attributeCollection.tangents);
            attribute.setAccessor(attributeAccessor);
        }
        if (attribute.getName() == "TEXCOORD_0") {
            m_gltf.deleteAccessor(*attribute.getAccessor());
            AccessorCreation accessorCreation(m_gltf);
            Accessor* attributeAccessor = accessorCreation.create(_attributeCollection.texCoords0);
            attribute.setAccessor(attributeAccessor);
        }
        if (attribute.getName() == "TEXCOORD_1") {
            m_gltf.deleteAccessor(*attribute.getAccessor());
            AccessorCreation accessorCreation(m_gltf);
            Accessor* attributeAccessor = accessorCreation.create(_attributeCollection.texCoords1);
            attribute.setAccessor(attributeAccessor);
        }
        if (attribute.getName() == "COLOR_0") {
            m_gltf.deleteAccessor(*attribute.getAccessor());
            AccessorCreation accessorCreation(m_gltf);
            Accessor* attributeAccessor = accessorCreation.create(_attributeCollection.color0);
            attribute.setAccessor(attributeAccessor);
        }
    }
}
