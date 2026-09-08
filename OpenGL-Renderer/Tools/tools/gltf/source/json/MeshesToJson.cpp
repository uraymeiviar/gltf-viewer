#include "gltf/json/MeshesToJson.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/Mesh.h"
#include "gltf/ResourceCollector.h"
#include "gltf/XMP.h"
#include "gltf/json/GltfObjectToJson.h"
#include "gltf/mesh/DracoMeshCompression.h"
#include "gltf/mesh/MaterialVariantsMapping.h"

using namespace ux3d;
using namespace ux3d::gltf;

void MeshesToJson::convert(const Mesh& _mesh, nlohmann::json& _jsonMesh, const JsonPath& _jsonPath) const
{
    convertBase(_mesh, _jsonMesh);
    convertPrimitives(_mesh, _jsonMesh, _jsonPath);
    convertWeights(_mesh, _jsonMesh);
    convertXMP(_mesh, _jsonMesh);
}

void MeshesToJson::convertPrimitives(const Mesh& _mesh, nlohmann::json& _jsonMesh, const JsonPath& _meshPath) const
{
    const core::LinkedList<Primitive>& primitiveList = _mesh.getPrimitives();

    if (primitiveList.size() == 0) {
        return;
    }

    nlohmann::json jsonPrimitives = nlohmann::json::array();
    JsonPath primitivesPath = _meshPath;
    primitivesPath.emplace_back("primitives");

    uint32_t prmitiveIndex = 0;
    for (const Primitive& primitive : primitiveList) {
        if (primitive.isValid() == false) {
            core::logWarning("MeshesToJson: Invalid primitive");
            continue;
        }
        JsonPath primitivePath = primitivesPath;
        primitivePath.emplace_back(prmitiveIndex);

        nlohmann::json jsonPrimitive = nlohmann::json::object();
        convertPrimitive(primitive, jsonPrimitive, primitivePath);
        jsonPrimitives.emplace_back(jsonPrimitive);
        prmitiveIndex++;
    }

    _jsonMesh["primitives"] = jsonPrimitives;
}

void MeshesToJson::convertPrimitive(const Primitive& _primitive, nlohmann::json& _jsonPrimitive, const JsonPath& _primitivePath) const
{
    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(_primitive, _jsonPrimitive);
    gltfObjectToJson.convertUnknownExtensions(_primitive, _jsonPrimitive);

    {
        const core::LinkedList<Attribute>& attributeList = _primitive.getAttributes();
        nlohmann::json jsonAttributes;

        for (const Attribute& attribute : attributeList) {
            const core::string key = attribute.getName();
            if (attribute.getAccessor() == nullptr) {
                core::logWarning("MeshesToJson: Invalid attribute accessor");
                continue;
            }

            size_t indexAttributeAccessor = m_indices[attribute.getAccessor()];
            jsonAttributes[key] = indexAttributeAccessor;
        }

        _jsonPrimitive["attributes"] = jsonAttributes;
    }

    if (_primitive.getIndicesAccessor() != nullptr) {
        size_t indexToIndicesAccessor = m_indices[_primitive.getIndicesAccessor()];
        _jsonPrimitive["indices"] = indexToIndicesAccessor;
    }

    if (_primitive.getMaterial() != nullptr) {
        size_t indexMaterial = m_indices[_primitive.getMaterial()];
        _jsonPrimitive["material"] = indexMaterial;
    }

    _jsonPrimitive["mode"] = _primitive.getPrimitiveMode();

    {
        const core::LinkedList<MorphTarget>& targetList = _primitive.getTargets();
        nlohmann::json jsonTargets = nlohmann::json::array();

        for (const MorphTarget& target : targetList) {
            nlohmann::json jsonMorphTargetObject = nlohmann::json::object();

            if (target.getNormalAccessor() != nullptr) {
                jsonMorphTargetObject["NORMAL"] = m_indices[target.getNormalAccessor()];
            }
            if (target.getPositionAccessor() != nullptr) {
                jsonMorphTargetObject["POSITION"] = m_indices[target.getPositionAccessor()];
            }
            if (target.getTangentAccessor() != nullptr) {
                jsonMorphTargetObject["TANGENT"] = m_indices[target.getTangentAccessor()];
            }
            if (target.getTexCoord0Accessor() != nullptr) {
                jsonMorphTargetObject["TEXCOORD_0"] = m_indices[target.getTexCoord0Accessor()];
            }
            if (target.getTexCoord1Accessor() != nullptr) {
                jsonMorphTargetObject["TEXCOORD_1"] = m_indices[target.getTexCoord1Accessor()];
            }
            if (target.getColor0Accessor() != nullptr) {
                jsonMorphTargetObject["COLOR_0"] = m_indices[target.getColor0Accessor()];
            }

            jsonTargets.emplace_back(jsonMorphTargetObject);
        }
        if (targetList.empty() == false) {
            _jsonPrimitive["targets"] = jsonTargets;
        }
    }

    if (_primitive.getDracoMeshCompression().has_value()) {
        JsonPath dracoPath = _primitivePath;
        dracoPath.emplace_back("extensions");
        dracoPath.emplace_back("KHR_draco_mesh_compression");
        nlohmann::json dracoMeshCompression = nlohmann::json::object();
        fillDracoMeshCompression(_primitive.getDracoMeshCompression().value(), dracoMeshCompression, dracoPath);
        _jsonPrimitive["extensions"]["KHR_draco_mesh_compression"] = dracoMeshCompression;
    }

    if (_primitive.getMaterialVariantsMapping().has_value() && !_primitive.getMaterialVariantsMapping()->getMappings().empty()) {
        nlohmann::json materialVariantsMapping = nlohmann::json::object();
        fillMaterialVariantsMapping(_primitive.getMaterialVariantsMapping().value(), materialVariantsMapping);
        _jsonPrimitive["extensions"]["KHR_materials_variants"] = materialVariantsMapping;
    }
}

void MeshesToJson::fillDracoMeshCompression(const DracoMeshCompression& _dracoMeshCompression, nlohmann::json& _jsonDracoMeshCompression, const JsonPath& _dracoPath) const
{

    nlohmann::json jsonAttributes = nlohmann::json::object();

    const core::Vector<DracoMeshCompression::DracoAttribute>& dracoAttributes = _dracoMeshCompression.getAttributes();

    for (auto& dracoAttribute : dracoAttributes) {
        jsonAttributes[dracoAttribute.name] = dracoAttribute.id;
    }

    if (dracoAttributes.size() > 0) {
        _jsonDracoMeshCompression["attributes"] = jsonAttributes;
    }

    if (_dracoMeshCompression.getBufferView() != nullptr) {
        size_t index = m_indices[_dracoMeshCompression.getBufferView()];

        _jsonDracoMeshCompression["bufferView"] = index;

        JsonPath bufferViewPath = _dracoPath;
        bufferViewPath.emplace_back("bufferView");

        m_resourceCollector.addGenericResource(_dracoMeshCompression.getBufferView(), bufferViewPath);
    }

    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(_dracoMeshCompression, _jsonDracoMeshCompression);
    gltfObjectToJson.convertUnknownExtensions(_dracoMeshCompression, _jsonDracoMeshCompression);
}

void MeshesToJson::fillMaterialVariantsMapping(const MaterialVariantsPrimitiveMapping& _materialVariantsMapping, nlohmann::json& _jsonMaterialVariantsMapping) const
{
    nlohmann::json jsonMappings = nlohmann::json::array();
    const core::Vector<MaterialVariantPrimitive>& mappings = _materialVariantsMapping.getMappings();
    for (auto& mapping : mappings) {
        nlohmann::json jsonVariant = nlohmann::json::object();
        jsonVariant["material"] = m_indices[mapping.material];

        nlohmann::json jsonVariants = nlohmann::json::array();
        core::Vector<const gltf::MaterialVariant*> materialVariants = mapping.pVariants;
        for (auto& materialVariant : materialVariants) {
            jsonVariants.emplace_back(m_indices[materialVariant]);
        }
        jsonVariant["variants"] = jsonVariants;

        // convert extras and unknown extensions
        GltfObjectToJson gltfObjectToJson(m_exportOptions);
        gltfObjectToJson.convertExtras(mapping, jsonVariant);
        gltfObjectToJson.convertUnknownExtensions(mapping, jsonVariant);

        jsonMappings.emplace_back(jsonVariant);
    }

    _jsonMaterialVariantsMapping["mappings"] = jsonMappings;

    // convert extras and unknown extensions
    GltfObjectToJson gltfObjectToJson(m_exportOptions);
    gltfObjectToJson.convertExtras(_materialVariantsMapping, _jsonMaterialVariantsMapping);
    gltfObjectToJson.convertUnknownExtensions(_materialVariantsMapping, _jsonMaterialVariantsMapping);
}

void MeshesToJson::convertWeights(const Mesh& _mesh, nlohmann::json& _jsonMesh) const
{
    if (_mesh.weights && !_mesh.weights->empty()) {
        nlohmann::json jsonArray = nlohmann::json::array();

        for (double weight : *_mesh.weights) {
            jsonArray.emplace_back(weight);
        }

        _jsonMesh["weights"] = jsonArray;
    }
}

void MeshesToJson::convertXMP(const Mesh& _mesh, nlohmann::json& _jsonMesh) const
{
    if (_mesh.hasXMPPacket()) {
        const PacketContainer& container = _mesh.getXMPPacket().value();
        Packet* pPacket = container.m_packet;
        if (m_gltf.getXMP().has_value()) {
            const XMP& xmp = m_gltf.getXMP().value();
            const core::LinkedList<Packet>& packets = xmp.getPackets();

            for (size_t i = 0; i < packets.size(); ++i) {
                if (&packets.at(i) == pPacket) {
                    _jsonMesh["extensions"]["KHR_xmp_json_ld"]["packet"] = i;
                    break;
                }
            }
        }

        // convert extras and extensions
        GltfObjectToJson gltfObjectToJson(m_exportOptions);
        gltfObjectToJson.convertExtras(container, _jsonMesh["extensions"]["KHR_xmp_json_ld"]);
        gltfObjectToJson.convertUnknownExtensions(container, _jsonMesh["extensions"]["KHR_xmp_json_ld"]);
    }
}
