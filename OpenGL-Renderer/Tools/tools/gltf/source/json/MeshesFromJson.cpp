#include "gltf/json/MeshesFromJson.h"
#include "gltf/Gltf.h"
#include "gltf/accessor/AccessorFlags.h"
#include "gltf/json/GltfObjectFromJson.h"
#include "gltf/json/PointerLookup.h"
#include "gltf/mesh/MaterialVariantsMapping.h"

using namespace ux3d;
using namespace ux3d::gltf;

ConversionResult MeshesFromJson::convert(const nlohmann::json& _jsonMesh)
{
    Mesh* pMesh = m_gltf.create<Mesh>(getID(_jsonMesh));
    if (pMesh == nullptr) {
        return ConversionError::AllocationFailure;
    }

    if (!m_pointerLookup.add(pMesh)) {
        return ConversionError::AllocationFailure;
    }

    bool containsErrors = false;
    for (const auto& [key, value] : _jsonMesh.items()) {
        if (key == "name") {
            if (value.is_string()) {
                pMesh->setName(value.get<std::string>());
            } else {
                core::logWarning("mesh.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "primitives") {
            if (value.is_array()) {
                for (const auto& jsonPrimitve : value) {
                    if (jsonPrimitve.is_object()) {
                        if (!convertPrimitive(jsonPrimitve, *pMesh)) {
                            containsErrors = true;
                        }
                    } else {
                        core::logWarning("mesh.primitive with value '%s' has invalid type", jsonPrimitve.dump().c_str());
                        containsErrors = true;
                    }
                }
            } else {
                core::logWarning("mesh.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }

        } else if (key == "weights") {
            if (value.is_array()) {
                std::vector<double> weights;
                for (const auto& jsonWeight : value) {
                    if (!jsonWeight.is_number()) {
                        core::logWarning("mesh.%s has element with value '%s' of invalid type", key.c_str(), jsonWeight.dump().c_str());
                        containsErrors = true;
                    }
                    weights.emplace_back(jsonWeight.get<float>());
                }
                pMesh->weights = weights;
            } else {
                core::logWarning("mesh.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "extensions") {
            if (value.is_object()) {
                for (const auto& [extensionName, extension] : value.items()) {
                    if (extensionName == "KHR_xmp_json_ld") {
                        if (extension.is_object()) {
                            for (const auto& [xmpKey, xmpValue] : extension.items()) {
                                if (xmpKey == "packet") {
                                    if (m_gltf.getXMP().has_value() == false) {
                                        core::logWarning("MeshesFromJson: Mesh.XMP is defined, but the root extension KHR_xmp_json_ld is missing");
                                        containsErrors = true;
                                        continue;
                                    }

                                    XMP& xmp = m_gltf.getXMP().value();
                                    core::LinkedList<Packet>& packets = xmp.getPackets();
                                    const size_t index = xmpValue.get<size_t>();

                                    Packet* pPacket = &packets.at(index);
                                    pMesh->setXMPPacket(pPacket);
                                }
                            }
                            if (pMesh->hasXMPPacket()) {
                                if (!GltfObjectFromJson::convertUnknownExtensions(extension, &pMesh->getXMPPacket().value(), "mesh.extensions.KHR_xmp_json_ld")) {
                                    containsErrors = true;
                                }
                                GltfObjectFromJson::convertExtras(extension, &pMesh->getXMPPacket().value());
                            }
                        } else {
                            core::logWarning("mesh.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension.dump().c_str());
                            containsErrors = true;
                        }
                    }
                }
            }
        }
    }
    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonMesh, pMesh, "mesh")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonMesh, pMesh);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult MeshesFromJson::convertPrimitive(const nlohmann::json& _jsonPrimitive, Mesh& _mesh)
{
    Primitive* pPrimitive = _mesh.createPrimitive();
    if (pPrimitive == nullptr) {
        return ConversionError::AllocationFailure;
    }

    bool containsErrors = false;
    for (const auto& [key, value] : _jsonPrimitive.items()) {
        if (key == "mode") {
            if (value.is_number_integer()) {
                pPrimitive->setPrimitiveMode(PrimitiveMode(value));
            } else {
                core::logWarning("mesh.primitive.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "attributes") {
            if (value.is_object()) {
                for (const auto& [name, accessorIndex] : value.items()) {
                    convertAttribute(name, *pPrimitive);
                }
            } else {
                core::logWarning("mesh.primitive.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "targets") {
            if (value.is_array()) {
                for (const auto& [attributeName, accessorIndex] : value.items()) {
                    (void)attributeName;
                    (void)accessorIndex;
                    createTarget(*pPrimitive);
                }
            } else {
                core::logWarning("mesh.primitive.%s with value '%s' has invalid type", key.c_str(), value.dump().c_str());
                containsErrors = true;
            }
        } else if (key == "extensions") {
            if (value.is_object()) {
                for (const auto& [extensionName, extension] : value.items()) {
                    if (extensionName == "KHR_draco_mesh_compression") {
                        if (extension.is_object()) {
                            pPrimitive->createDracoMeshCompression();
                            if (core::Failed(fillDracoMeshCompression(extension, pPrimitive->getDracoMeshCompression().value()))) {
                                containsErrors = true;
                            }
                        } else {
                            core::logWarning("mesh.primitive.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension.dump().c_str());
                            containsErrors = true;
                        }
                    } else if (extensionName == "KHR_materials_variants") {
                        if (extension.is_object()) {
                            pPrimitive->createMaterialVariantsMapping();
                        } else {
                            core::logWarning("mesh.primitive.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension.dump().c_str());
                            containsErrors = true;
                        }
                    }
                }
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonPrimitive, pPrimitive, "mesh.primitive")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonPrimitive, pPrimitive);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

void MeshesFromJson::convertAttribute(const core::string& _name, Primitive& _primitive)
{
    Attribute* pAttribute = _primitive.addAttribute();
    if (pAttribute == nullptr) {
        // TODO: error!
        return;
    }

    pAttribute->setName(_name);
}

void MeshesFromJson::createTarget(Primitive& _primitive)
{
    MorphTarget* pTarget = _primitive.addTarget();
    if (pTarget == nullptr) {
        // TODO: error!
        return;
    }
}

ConversionResult MeshesFromJson::resolve(const nlohmann::json& _jsonMesh, Mesh& _mesh)
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonMesh.items()) {
        if (key == "primitives") {
            if (value.is_array()) {
                auto jsonIt = value.begin();
                auto jsonEnd = value.end();
                auto gltfIt = _mesh.getPrimitives().begin();
                auto gltfEnd = _mesh.getPrimitives().end();

                for (; jsonIt != jsonEnd && gltfIt != gltfEnd; jsonIt++, gltfIt++) {
                    if ((*jsonIt).is_object()) {
                        if (!resolvePrimitives(*jsonIt, *gltfIt)) {
                            containsErrors = true;
                        }
                    } else {
                        core::logWarning("mesh.%s has element with value '%s' of invalid type", key.c_str(), (*jsonIt).dump().c_str());
                        containsErrors = true;
                    }
                }
            }
        }
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult MeshesFromJson::resolvePrimitives(const nlohmann::json& _jsonPrimitive, Primitive& _primitive)
{
    bool containsErrors = false;
    for (const auto& [key, value] : _jsonPrimitive.items()) {
        if (key == "attributes") {
            if (value.is_object()) {
                auto jsonIt = value.begin();
                auto jsonEnd = value.end();
                auto gltfIt = _primitive.getAttributes().begin();
                auto gltfEnd = _primitive.getAttributes().end();

                for (; jsonIt != jsonEnd && gltfIt != gltfEnd; jsonIt++, gltfIt++) {
                    if (core::Succeeded(checkIndex((*jsonIt), m_pointerLookup.Accessors.size(), "mesh.primitive.attribute"))) {
                        (*gltfIt).setAccessor(m_pointerLookup.Accessors[*jsonIt]);
                    } else {
                        containsErrors = true;
                    }
                }
            }
        }
        if (key == "indices") {
            if (core::Succeeded(checkIndex(value, m_pointerLookup.Accessors.size(), "mesh.primitive.indices"))) {
                Accessor* pIndices = m_pointerLookup.Accessors[value];
                pIndices->setUsageFlag(UsageFlags::MeshIndices);
                _primitive.setIndicesAccessor(*pIndices);
            } else {
                containsErrors = true;
            }
        }
        if (key == "material") {
            if (core::Succeeded(checkIndex(value, m_pointerLookup.Materials.size(), "mesh.primitive.material"))) {
                Material* pMaterial = m_pointerLookup.Materials[value];
                _primitive.setMaterial(*pMaterial);
            } else {
                containsErrors = true;
            }
        }
        if (key == "targets") {
            if (value.is_array()) {
                auto jsonIt = value.begin();
                auto jsonEnd = value.end();
                auto gltfIt = _primitive.getTargets().begin();
                auto gltfEnd = _primitive.getTargets().end();

                for (; jsonIt != jsonEnd && gltfIt != gltfEnd; jsonIt++, gltfIt++) {
                    if ((*jsonIt).is_object()) {
                        for (const auto& [attributeName, jsonIndex] : (*jsonIt).items()) {

                            if (core::Succeeded(checkIndex(jsonIndex, m_pointerLookup.Accessors.size(), "mesh.primitive.targets[" + attributeName + "]"))) {
                                Accessor* pAccessor = m_pointerLookup.Accessors[jsonIndex];
                                if (attributeName == "POSITION") {
                                    (*gltfIt).setPositionAccessor(*pAccessor);
                                } else if (attributeName == "NORMAL") {
                                    (*gltfIt).setNormalAccessor(*pAccessor);
                                } else if (attributeName == "TANGENT") {
                                    (*gltfIt).setTangentAccessor(*pAccessor);
                                } else if (attributeName == "TEXCOORD_0") {
                                    (*gltfIt).setTexCoord0Accessor(*pAccessor);
                                } else if (attributeName == "TEXCOORD_1") {
                                    (*gltfIt).setTexCoord1Accessor(*pAccessor);
                                } else if (attributeName == "COLOR_0") {
                                    (*gltfIt).setColor0Accessor(*pAccessor);
                                } else {
                                    core::logWarning("mesh.primitive.targets has unsupported morph target attribute");
                                }

                            } else {
                                containsErrors = true;
                            }
                        }
                    } else {
                        core::logWarning("mesh.primitive.targets has element with value '%s' of invalid type", (*jsonIt).dump().c_str());
                        containsErrors = true;
                    }
                }
            }
        } else if (key == "extensions") {
            if (value.is_object()) {
                for (const auto& [extensionName, extension] : value.items()) {
                    if (extensionName == "KHR_materials_variants") {
                        if (extension.is_object()) {
                            if (core::Failed(fillMaterialVariantsMapping(extension, _primitive.getMaterialVariantsMapping().value()))) {
                                containsErrors = true;
                            }
                        } else {
                            core::logWarning("mesh.primitive.%s.%s with value '%s' has invalid type", key.c_str(), extensionName.c_str(), extension.dump().c_str());
                            containsErrors = true;
                        }
                    }
                }
            }
        }
    }

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult MeshesFromJson::fillDracoMeshCompression(const nlohmann::json& _jsonDracoMeshCompression, DracoMeshCompression& _dracoMeshCompression)
{
    bool containsErrors = false;

    if (_jsonDracoMeshCompression.contains("bufferView")) {
        const nlohmann::json& jsonBufferView = _jsonDracoMeshCompression["bufferView"];
        auto indexCheck = checkIndex(jsonBufferView, m_pointerLookup.BufferViews.size(), "mesh.primitive.extensions.KHR_draco_mesh_compression.bufferView");
        if (core::Succeeded(indexCheck)) {
            BufferView* pBufferView = m_pointerLookup.BufferViews[jsonBufferView.get<uint32_t>()];
            _dracoMeshCompression.setBufferView(*pBufferView);
        } else {
            containsErrors = true;
        }
    }

    if (_jsonDracoMeshCompression.contains("attributes")) {
        const nlohmann::json& jsonAttributes = _jsonDracoMeshCompression["attributes"];
        auto& dracoAttribute = _dracoMeshCompression.getAttributes();

        for (const auto& [attributeName, index] : jsonAttributes.items()) {
            if (index.is_number_integer() && index.is_number_unsigned()) {
                core::string name = attributeName;
                dracoAttribute.emplace_back(name, index.get<uint32_t>());
            } else {
                core::logWarning("mesh.primitive.extensions.KHR_draco_mesh_compression.attributes with value %s has invalid type", jsonAttributes.dump().c_str());
                containsErrors = true;
            }
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonDracoMeshCompression, &_dracoMeshCompression, "mesh.primitive.extensions.KHR_draco_mesh_compression")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonDracoMeshCompression, &_dracoMeshCompression);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}

ConversionResult MeshesFromJson::fillMaterialVariantsMapping(const nlohmann::json& _jsonMaterialVariants, MaterialVariantsPrimitiveMapping& _materialVariantsMapping)
{
    bool containsErrors = false;

    if (_jsonMaterialVariants.contains("mappings")) {
        const nlohmann::json& jsonMappings = _jsonMaterialVariants["mappings"];

        // iterate over variant array
        for (const auto& jsonVariant : jsonMappings) {
            Material* material = nullptr;
            core::Vector<gltf::MaterialVariant*> materialVariants;
            // iterate over one variant
            for (const auto& [key, value] : jsonVariant.items()) {
                if (key == "material") {
                    if (core::Succeeded(checkIndex(value, m_pointerLookup.Materials.size(), "mesh.primitive.material"))) {
                        material = m_pointerLookup.Materials[value];
                    } else {
                        containsErrors = true;
                    }
                }
                if (key == "variants") {
                    for (const auto& variantId : value) {
                        materialVariants.emplace_back(m_pointerLookup.MaterialVariants[variantId]);
                    }
                }
            }

            // returns false if it uniqueness of variants is violated -> negate it to check for error
            if (_materialVariantsMapping.addVariant(materialVariants, material) == MaterialVariantsError::UniquenessViolated) {
                core::logWarning("mesh.primitive.extensions.KHR_materials_variants violated uniqueness of variants");
                containsErrors |= true;
            }

            MaterialVariantPrimitive& matVar = _materialVariantsMapping.getMappings().back();
            if (!GltfObjectFromJson::convertUnknownExtensions(jsonVariant, &matVar, "mesh.primitive.extensions.KHR_materials_variants")) {
                containsErrors = true;
            }
            GltfObjectFromJson::convertExtras(jsonVariant, &matVar);
        }
    }

    if (!GltfObjectFromJson::convertUnknownExtensions(_jsonMaterialVariants, &_materialVariantsMapping, "mesh.primitive.extensions.KHR_materials_variants")) {
        containsErrors = true;
    }
    GltfObjectFromJson::convertExtras(_jsonMaterialVariants, &_materialVariantsMapping);

    if (containsErrors) {
        return ConversionError::SuccessWithErrors;
    }

    return ConversionResult::Success;
}
