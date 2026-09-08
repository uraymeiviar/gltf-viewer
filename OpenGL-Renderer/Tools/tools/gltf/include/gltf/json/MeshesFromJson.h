#pragma once

#include "ChildOfRootFromJson.h"
#include "core/String.h"

namespace ux3d::gltf {
class Mesh;
class Primitive;
class Attribute;
class DracoMeshCompression;
class MaterialVariantsPrimitiveMapping;

class MeshesFromJson : public ChildOfRootFromJson {
private:
    ConversionResult convertPrimitive(const nlohmann::json& _jsonPrimitive, Mesh& _mesh);
    void convertAttribute(const core::string& _name, Primitive& _primitive);
    void createTarget(Primitive& _primitive);

    ConversionResult resolvePrimitives(const nlohmann::json& _jsonPrimitive, Primitive& _primitive);
    ConversionResult fillDracoMeshCompression(const nlohmann::json& _jsonPrimitive, DracoMeshCompression& _dracoMeshCompression);
    ConversionResult fillMaterialVariantsMapping(const nlohmann::json& _jsonPrimitive, MaterialVariantsPrimitiveMapping& _materialVariantsMapping);

public:
    using ChildOfRootFromJson::ChildOfRootFromJson;
    ConversionResult convert(const nlohmann::json& _jsonMesh) override;
    ConversionResult resolve(const nlohmann::json& _jsonMesh, Mesh& _mesh);
};
}
