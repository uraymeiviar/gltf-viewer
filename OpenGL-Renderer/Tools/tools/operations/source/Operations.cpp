// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "operations/Operations.h"

#include "AdvancedOperations.h"
#include "DangerousOperations.h"
#include "EditOperations.h"
#include "Editor.h"
#include "FileOperations.h"
#include "GeneratedOperations.h"
#include "ImageOperations.h"
#include "OptimizationOperations.h"
#include "OverloadOperations.h"
#include "PropertyOperations.h"
#include "StorageOperations.h"

#include <string>

using namespace ux3d;

std::vector<operations::Operation> operations::gltfOperations(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    nlohmann::json knownExtensionsArray = nlohmann::json::array();
    for (const auto& [key, value] : linearizedGltfSchema.items()) {
        if (key.ends_with("extensions") && value.contains("properties")) {
            const nlohmann::json& extensions = value["properties"];
            for (const auto& [extKey, extValue] : extensions.items()) {
                auto it = std::find(knownExtensionsArray.begin(), knownExtensionsArray.end(), extKey);
                if (it == knownExtensionsArray.end()) {
                    knownExtensionsArray.push_back(extKey);
                }
            }
        }
    }

    return {
        overload::setTRSValue("set_gltf_value", gltfSchema, linearizedGltfSchema),
        overload::setTexCoord("set_gltf_value", gltfSchema, linearizedGltfSchema),
        overload::setCameraOrthographicProperties("set_gltf_value", gltfSchema, linearizedGltfSchema),
        overload::setCameraPerspectiveProperties("set_gltf_value", gltfSchema, linearizedGltfSchema),
        overload::setImageURI("set_gltf_value", gltfSchema, linearizedGltfSchema),

        overload::setMaterialExtensionsWithSpecGlossConflict("set_gltf_value", gltfSchema, linearizedGltfSchema,
            "Set glTF value (resolve specular glossiness conflicts)", "Sets a primitive value in material extensions. Resets KHR_materials_pbrSpecularGlossiness if it exists."),
        overload::setMaterialExtensionsWithSpecGlossConflict("set_gltf_pointer", gltfSchema, linearizedGltfSchema,
            "Set glTF pointer (resolve specular glossiness conflicts)", "Sets a pointer in a material extension. Resets KHR_materials_pbrSpecularGlossiness if it exists."),
        overload::setMaterialExtensionsWithSpecGlossConflict("create_gltf_object", gltfSchema, linearizedGltfSchema,
            "Creates a glTF object (resolve specular glossiness conflicts)", "Creates a material extension object. Needs to set all required properties. Resets KHR_materials_pbrSpecularGlossiness if it exists."),
        overload::setMaterialExtensionsWithSpecGlossConflict("insert_gltf_pointer", gltfSchema, linearizedGltfSchema,
            "Insert pointer (resolve specular glossiness conflicts)", "Inserts a pointer value into an array in a material extension. Resets KHR_materials_pbrSpecularGlossiness if it exists."),

        overload::setMaterialExtensionsWithUnlitConflict("set_gltf_value", gltfSchema, linearizedGltfSchema,
            "Set glTF value (resolve unlit conflicts)", "Sets a primitive value in material extensions. Resets KHR_materials_unlit if it exists."),
        overload::setMaterialExtensionsWithUnlitConflict("set_gltf_pointer", gltfSchema, linearizedGltfSchema,
            "Set glTF pointer (resolve unlit conflicts)", "Sets a pointer in a material extension. Resets KHR_materials_unlit if it exists."),
        overload::setMaterialExtensionsWithUnlitConflict("create_gltf_object", gltfSchema, linearizedGltfSchema,
            "Creates a glTF object (resolve unlit conflicts)", "Creates a material extension object. Resets KHR_materials_unlit if it exists."),
        overload::setMaterialExtensionsWithUnlitConflict("insert_gltf_pointer", gltfSchema, linearizedGltfSchema,
            "Insert pointer (resolve unlit conflicts)", "Inserts a pointer value into an array in a material extension. Resets KHR_materials_unlit if it exists."),

        overload::setMaterialUnlit("set_gltf_value", gltfSchema, linearizedGltfSchema, "Set glTF value (KHR_materials_unlit)", "Sets a primitive value in KHR_materials_unlit. Resets conflicting extensions."),
        overload::setMaterialUnlit("set_gltf_pointer", gltfSchema, linearizedGltfSchema, "Set glTF pointer (KHR_materials_unlit)", "Sets a pointer in KHR_materials_unlit. Resets conflicting extensions."),
        overload::setMaterialUnlit("create_gltf_object", gltfSchema, linearizedGltfSchema, "Creates a glTF object (KHR_materials_unlit)", "Creates a KHR_materials_unlit object. Resets conflicting extensions."),
        overload::setMaterialUnlit("insert_gltf_pointer", gltfSchema, linearizedGltfSchema, "Insert pointer (KHR_materials_unlit)", "Inserts a pointer value into an array in KHR_materials_unlit. Resets conflicting extensions."),

        overload::setMaterialSpecGloss("set_gltf_value", gltfSchema, linearizedGltfSchema, "Set glTF value (pbrSpecularGlossiness)", "Sets a primitive value in KHR_materials_pbrSpecularGlossiness. Resets conflicting extensions."),
        overload::setMaterialSpecGloss("set_gltf_pointer", gltfSchema, linearizedGltfSchema, "Set glTF pointer (pbrSpecularGlossiness)", "Sets a pointer in KHR_materials_pbrSpecularGlossiness. Resets conflicting extensions."),
        overload::setMaterialSpecGloss("create_gltf_object", gltfSchema, linearizedGltfSchema, "Creates a glTF object (pbrSpecularGlossiness)", "Creates a KHR_materials_pbrSpecularGlossiness object. Resets conflicting extensions."),
        overload::setMaterialSpecGloss("insert_gltf_pointer", gltfSchema, linearizedGltfSchema, "Insert pointer (pbrSpecularGlossiness)", "Inserts a pointer value into an array in KHR_materials_pbrSpecularGlossiness. Resets conflicting extensions."),

        overload::setExtensionUsed("set_gltf_value", gltfSchema, linearizedGltfSchema,
            "Set glTF value (extensionsUsed)", "Sets a primitive value in an extensions object. Adds new extensions to the extensionsUsed array."),
        overload::setExtensionUsed("set_gltf_pointer", gltfSchema, linearizedGltfSchema,
            "Set glTF pointer (extensionsUsed)", "Sets a pointer in an extensions object. Adds new extensions to the extensionsUsed array."),
        overload::setExtensionUsed("create_gltf_object", gltfSchema, linearizedGltfSchema,
            "Creates a glTF object (extensionsUsed)", "Creates a glTF object in an extensions object. Needs to set all required properties. Adds new extensions to the extensionsUsed array."),
        overload::setExtensionUsed("create_gltf_component", gltfSchema, linearizedGltfSchema,
            "Creates a glTF component  (extensionsUsed)", "Creates a glTF component. Needs to set all required properties"),
        overload::setExtensionUsed("insert_gltf_pointer", gltfSchema, linearizedGltfSchema,
            "Insert pointer (extensionsUsed)", "Inserts a pointer value into an array in an extensions object. Adds new extensions to the extensionsUsed array."),

        overload::applyOperationToArrayProperty("apply_draco", gltfSchema, linearizedGltfSchema, "/gltf/meshes/\\d+/primitives",
            "Apply Draco Mesh Compression to a Mesh", "Apply draco mesh compression on each primitive in a mesh with given compression settings"),
        overload::applyOperationToArrayProperty("resolve_draco", gltfSchema, linearizedGltfSchema, "/gltf/meshes/\\d+/primitives",
            "Resolve Draco Mesh Compression for a Mesh", "Decodes each compressed primitive in a mesh and removes the draco mesh compression extension from it"),
        overload::applyOperationToArrayProperty("apply_vertex_clustering", gltfSchema, linearizedGltfSchema, "/gltf/meshes/\\d+/primitives",
            "Apply Vertex Clustering for a Mesh", "Cluster vertices of each primitive in a mesh with deviation thresholds given as parameter"),
        overload::applyOperationToArrayProperty("reduce_triangle_count", gltfSchema, linearizedGltfSchema, "/gltf/meshes/\\d+/primitives",
            "Reduce Triangle Count for a Mesh", "Reduce the triangle count for each primitive in a mesh by approximating the shape"),

        overload::joinPrimitives("join_primitives", gltfSchema, linearizedGltfSchema),
        overload::resolveDraco("reset", gltfSchema, linearizedGltfSchema),
        overload::setAnimationSamplerReference("set_gltf_pointer", gltfSchema, linearizedGltfSchema),

        dangerous::setValue,
        dangerous::removeValue,
        dangerous::forEach(gltfSchema, linearizedGltfSchema),
        dangerous::forEachElement(gltfSchema, linearizedGltfSchema),
        dangerous::forEachElementInArray(gltfSchema, linearizedGltfSchema),
        dangerous::combineOperations(gltfSchema, linearizedGltfSchema),

        generated::createGltfComponent(gltfSchema, linearizedGltfSchema),
        generated::setGltfValue(gltfSchema, linearizedGltfSchema),
        generated::createGltfObject(gltfSchema, linearizedGltfSchema),
        generated::setGltfPointer(gltfSchema, linearizedGltfSchema),
        generated::moveGltfPointer(gltfSchema, linearizedGltfSchema),
        generated::insertGltfPointer(gltfSchema, linearizedGltfSchema),
        generated::removeGltfPointer(gltfSchema, linearizedGltfSchema),
        generated::removeGltfPointerFromArray(gltfSchema, linearizedGltfSchema),
        generated::moveGltfPointerFromArray(gltfSchema, linearizedGltfSchema),
        generated::remove(gltfSchema, linearizedGltfSchema), // actually, this is a delete operation
        generated::reset(gltfSchema, linearizedGltfSchema),

        gltf::addNode,
        gltf::addNewNode,
        gltf::changeCameraType,
        gltf::createScene,
        gltf::createMesh,
        gltf::createAnimation,
        gltf::removeNodeReference,
        gltf::clone(gltfSchema, linearizedGltfSchema),
        gltf::trimBuffer(gltfSchema, linearizedGltfSchema),
        gltf::mergeBuffers(gltfSchema, linearizedGltfSchema),
        gltf::cleanAll(gltfSchema, linearizedGltfSchema),
        gltf::calculateMinMax(gltfSchema, linearizedGltfSchema),
        gltf::cleanNodes(),
        gltf::cleanTexCoordAttributes(),
        gltf::createTexCoordAttribute(),
        gltf::deleteAttribute(),
        gltf::removeProperties(),
        gltf::removeKnownExtensions(knownExtensionsArray),
        gltf::removeUnknownExtensions(knownExtensionsArray),
        gltf::convertPrimitivesToMeshes(),
        gltf::joinMeshes(gltfSchema, linearizedGltfSchema),
        gltf::transformNode(),
        gltf::createEmpty(),
        gltf::createLight(),
        gltf::reassignSkinnedAnimation(),
        gltf::deleteAccessor(gltfSchema, linearizedGltfSchema),
        gltf::deleteBufferView(gltfSchema, linearizedGltfSchema),
        gltf::updateExtensionFlags(gltfSchema, linearizedGltfSchema),
        gltf::resolveSparseAccessor,
        gltf::resetGltf(gltfSchema, linearizedGltfSchema),
        gltf::createCameraFromView(),
        gltf::createTangents(gltfSchema, linearizedGltfSchema),
        gltf::addMaterialVariantToPrimitive(gltfSchema, linearizedGltfSchema),
        gltf::removeMaterialVaraiantFromPrimitive(gltfSchema, linearizedGltfSchema),
        gltf::replaceMaterial(gltfSchema, linearizedGltfSchema),

        edit::load(),
        file::save(),
        file::saveSelection(gltfSchema, linearizedGltfSchema),
        file::exportAccessor(gltfSchema, linearizedGltfSchema),
        file::importImage(gltfSchema, linearizedGltfSchema),
        file::importTexture(gltfSchema, linearizedGltfSchema),
        file::exportImage(gltfSchema, linearizedGltfSchema),
        file::exportUsdz,
        file::reloadFile(gltfSchema, linearizedGltfSchema),

        storage::convertStorageType(),
        storage::convertStorageTypeForEach(),

        optimization::applyDraco(gltfSchema, linearizedGltfSchema),
        optimization::applyDracoForEach(gltfSchema, linearizedGltfSchema),
        optimization::resolveDraco(gltfSchema, linearizedGltfSchema),
        optimization::resolveDracoForEach(gltfSchema, linearizedGltfSchema),
        optimization::removeRedundantDraco(gltfSchema, linearizedGltfSchema),
        optimization::clusterVertices(gltfSchema, linearizedGltfSchema),
        optimization::deinterleaveAccessor(gltfSchema, linearizedGltfSchema),
        optimization::normalizeWeightsAccessor(gltfSchema, linearizedGltfSchema),
        optimization::clampAccessor(gltfSchema, linearizedGltfSchema),
        optimization::convertImage(),
        optimization::convertImageForEach(),
        optimization::changeTRS(gltfSchema, linearizedGltfSchema),
        optimization::changeTRSForAll(gltfSchema, linearizedGltfSchema),
        optimization::joinPrimitives(gltfSchema, linearizedGltfSchema),
        optimization::narrowIndexType(gltfSchema, linearizedGltfSchema),
        optimization::convertSpecularGlossinessToMetallicRoughness(gltfSchema, linearizedGltfSchema),
        optimization::reduceTriangleCount(),
        optimization::resolveMeshQuantization(gltfSchema, linearizedGltfSchema),
        optimization::resolveMeshQuantizationForAll(gltfSchema, linearizedGltfSchema),
        optimization::optimizeAnimation(gltfSchema, linearizedGltfSchema),

        edit::mergeGltf(gltfSchema, linearizedGltfSchema),
        edit::importAsset(gltfSchema, linearizedGltfSchema),
        edit::blendImages(gltfSchema, linearizedGltfSchema),
        edit::applyNodeTransform(gltfSchema, linearizedGltfSchema),

        image::applyTextureTransform,
        image::transform,
        image::hsv,
        image::blur,
        image::laplace,
        image::sharpen,
        image::invert,
        image::colorSpace,
    };
}

std::vector<operations::Operation> operations::advancedGltfOperations(const nlohmann::json& gltfSchema, const nlohmann::json& linearizedGltfSchema)
{
    return {
        advanced::paste(linearizedGltfSchema),
        advanced::cutAndPaste(linearizedGltfSchema),
        gltf::clone(gltfSchema, linearizedGltfSchema),
    };
}

std::vector<operations::Operation> operations::editorOperations()
{
    return {
        dangerous::setValue,
        dangerous::removeValue,
        editor::addSelection,
        editor::setSelection,
        editor::removeSelection,
        editor::clearSelection,
        editor::toggleWireframe,
        editor::toggleVisibility,
    };
}
