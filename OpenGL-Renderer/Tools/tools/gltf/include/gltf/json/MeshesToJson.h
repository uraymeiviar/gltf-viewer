#pragma once

#include "ChildOfRootToJson.h"
#include "gltf/json/JsonPath.h"

namespace ux3d::gltf {
class Gltf;
class Mesh;
class Primitive;
class DracoMeshCompression;
class ResourceCollector;
class MaterialVariantsPrimitiveMapping;

class MeshesToJson : ChildOfRootToJson {
public:
    explicit MeshesToJson(const IndexLookup& _indexLookup, ResourceCollector& _resourceCollector, const ExportOptions& _exportOptions, const Gltf& _gltf)
        : ChildOfRootToJson(_indexLookup, _exportOptions)
        , m_resourceCollector(_resourceCollector)
        , m_gltf(_gltf)
    {
    }

    void convert(const Mesh& _mesh, nlohmann::json& _jsonMesh, const JsonPath& _jsonPath) const;

private:
    void convertPrimitives(const Mesh& _mesh, nlohmann::json& _jsonMesh, const JsonPath& _meshPath) const;
    void convertPrimitive(const Primitive& _primitive, nlohmann::json& _jsonPrimitive, const JsonPath& _primitivePath) const;
    void fillDracoMeshCompression(const DracoMeshCompression& _dracoMeshCompression, nlohmann::json& _jsonDracoMeshCompression, const JsonPath& _dracoPath) const;
    void fillMaterialVariantsMapping(const MaterialVariantsPrimitiveMapping& _materialVariantsMapping, nlohmann::json& _jsonMaterialVariantsMapping) const;
    void convertWeights(const Mesh& _mesh, nlohmann::json& _jsonMesh) const;
    void convertXMP(const Mesh& _mesh, nlohmann::json& _jsonMesh) const;

    ResourceCollector& m_resourceCollector;
    const Gltf& m_gltf;
};
}
