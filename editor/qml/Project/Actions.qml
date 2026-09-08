pragma Singleton

import QtQuick 2.15
import QtQuick.Controls 2.15

import Model 1.0 as Model

import "../Components" as Components

// actions container
QtObject {
    id: contextMenuActions
    property var clipboard;
        
    // create actions
    property Action createNewScene: Action {
        text: "Create new Scene"
        onTriggered: {
            applicationData.createScene();
        }
    }
    property Action createAnimation: Action {
        text: "Create Animation..."
        onTriggered: {
            applicationData.showPopup("qrc:/Popups/CreateAnimation.qml", "Create Animation", 310, 380)    
        }
    }
    property Action createChildNode: Action {
        text: "Create Child Node"
        onTriggered: {
            if(Model.Observables.selection.length > 0) {
                Model.Operations.execute("add_new_node", { 
                    path: "/gltf" + Model.Observables.selection[0]["elementAbsolutPath"],
                })
            }
        }
    }

    // general actions
    property Action copy: Action {
        text: "Copy"
        onTriggered: {
            applicationData.doCopy();
        }
    }

    property Action cut: Action {
        text: "Cut"
        onTriggered: {
            applicationData.doCut();
        }
    }

    property Action paste: Action {
        text: "Paste"
        onTriggered: {
            applicationData.doPaste();
        }
    }
    // focus is a blocked keyword. Therefore this action is called focusSelection
    property Action focusSelection: Action {
        text: "Focus on Selected Node"
        onTriggered: {
            applicationData.focus();
        }
    }
    property Action cloneLinksAltText: Action {
        // "cloneLinksAltText" is an alias for "cloneLinks" but with the UI text "Clone" instead of "Clone Linked"
        text: "Clone"
        onTriggered: {
            if(Model.Observables.selection.length > 0) {
                Model.Operations.execute("clone_property", { 
                    path: "/gltf" + Model.Observables.selection[0]["elementAbsolutPath"],
                    cloneModeShallow: true,
                    target: "parent_optional"
                })
            }
        }
    }
    property Action cloneDeepAltText: Action {
        // "cloneDeepAltText" is an alias for "cloneDeep" but with the UI text "Clone" instead of "Clone Deep"
        text: "Clone"
        onTriggered: {
            if(Model.Observables.selection.length > 0) {
                Model.Operations.execute("clone_property", { 
                    path: "/gltf" + Model.Observables.selection[0]["elementAbsolutPath"],
                    cloneModeShallow: false,
                    target: "parent_optional"
                })
            }
        }
    }
    property Action cloneLinks: Action {
        text: "Clone Linked"
        onTriggered: {
            if(Model.Observables.selection.length > 0) {
                Model.Operations.execute("clone_property", { 
                    path: "/gltf" + Model.Observables.selection[0]["elementAbsolutPath"],
                    cloneModeShallow: true,
                    target: "parent_optional"
                })
            }
        }
    }
    property Action cloneDeep: Action {
        text: "Clone Deep"
        onTriggered: {
            if(Model.Observables.selection.length > 0) {
                Model.Operations.execute("clone_property", { 
                    path: "/gltf" + Model.Observables.selection[0]["elementAbsolutPath"],
                    cloneModeShallow: false,
                    target: "parent_optional"
                })
            }
        }
    }
    property Action remove: Action {
        text: "Remove"
        onTriggered: {
            applicationData.doRemove();
        }
            
    }
    property Action deleteObject: Action {
        text: "Delete"
        onTriggered: {
            applicationData.doRemove();
        }
            
    }

    property Action setDefaultScene: Action {
        text: "Set as Default Scene"
        onTriggered: {
            applicationData.setDefaultScene();
        }            
    }

    property Action unsetDefaultScene: Action {
        text: "Unset Default Scene"
        onTriggered: {
            applicationData.unsetDefaultScene();
        }            
    }

    property Action openScene: Action {
        text: "Open Scene"
        onTriggered: {
            applicationData.openScene();
        }            
    }
        
    property Action importModel: Action {
        text: "Import 3D Model..."
        onTriggered: {
            applicationData.importModel();
        }            
    }

    property Action createScene: Action {
        text: "Create Scene"
        onTriggered: {
            applicationData.createScene();
        }            
    }

    property Action createNode: Action {
        text: "Create Node"
        onTriggered: {
            applicationData.createNode();
        }            
    }

    property Action createCamera: Action {
        text: "Create Camera"
        onTriggered: {
            applicationData.createCamera();
        }            
    }

    property Action createLight: Action {
        text: "Create Light"
        onTriggered: {
            applicationData.createLight();
        }            
    }

    property Action createMaterialVariants: Action {
        text: "Create Material Variants"
        onTriggered: {
            applicationData.createMaterialVariant();
        }            
    }

    property Action createSampler: Action {
        text: "Create Sampler"
        onTriggered: {
            applicationData.createSampler();
        }            
    }

    property Action createMaterial: Action {
        text: "Create Material"
        onTriggered: {
            applicationData.createMaterial();
        }            
    }

    property Action createTexture: Action {
        text: "Create Texture"
        onTriggered: {
            applicationData.createTexture();
        }            
    }

    property Action createMesh: Action {
        text: "Create Mesh..."
        onTriggered: {
            applicationData.showPopup("qrc:/Popups/CreateMesh.qml", "Create Mesh", 200, 380)
        }            
    }

    property Action importMaterial: Action {
        text: "Import Material..."
        onTriggered: {
            applicationData.importMaterial();
        }            
    }

    property Action importXmpPacket: Action {
        text: "Import XMP..."
        onTriggered: {
            applicationData.importXmpPacket();
        }            
    }

    property Action importImage: Action {
        text: "Import Image..."
        onTriggered: {
            applicationData.importImage();
        }            
    }

    property Action importTexture: Action {
        text: "Import Texture..."
        onTriggered: {
            applicationData.importTexture();
        }            
    }

    property Action editImage: Action {
        text: "Edit Image..." //TODO: add application e.g. Gimp, Photoshop
        onTriggered: {
            applicationData.editImage();
        }            
    }

    property Action removeSpecular: Action {
        text: "KHR_materials_specular"
        onTriggered: {
            let parameters = {};
            parameters["property"] = "KHR_materials_specular";
            Model.Operations.resolvePromiseToCard(Model.Operations.execute("remove_extension", parameters));
        }            
    }

    property Action removeLights: Action {
        text: "KHR_lights_punctual"
        onTriggered: {
            let parameters = {};
            parameters["property"] = "KHR_lights_punctual";
            Model.Operations.resolvePromiseToCard(Model.Operations.execute("remove_extension", parameters));
        }            
    }

    property Action removeClearcoat: Action {
        text: "KHR_materials_clearcoat"
        onTriggered: {
            let parameters = {};
            parameters["property"] = "KHR_materials_clearcoat";
            Model.Operations.resolvePromiseToCard(Model.Operations.execute("remove_extension", parameters));
        }            
    }

    property Action removeEmissive: Action {
        text: "KHR_materials_emissive_strength"
        onTriggered: {
            let parameters = {};
            parameters["property"] = "KHR_materials_emissive_strength";
            Model.Operations.resolvePromiseToCard(Model.Operations.execute("remove_extension", parameters));
        }            
    }

    property Action removeIOR: Action {
        text: "KHR_materials_ior"
        onTriggered: {
            let parameters = {};
            parameters["property"] = "KHR_materials_ior";
            Model.Operations.resolvePromiseToCard(Model.Operations.execute("remove_extension", parameters));
        }            
    }

    property Action removeIridescence: Action {
        text: "KHR_materials_iridescence"
        onTriggered: {
            let parameters = {};
            parameters["property"] = "KHR_materials_iridescence";
            Model.Operations.resolvePromiseToCard(Model.Operations.execute("remove_extension", parameters));
        }            
    }

    property Action removeSheen: Action {
        text: "KHR_materials_sheen"
        onTriggered: {
            let parameters = {};
            parameters["property"] = "KHR_materials_sheen";
            Model.Operations.resolvePromiseToCard(Model.Operations.execute("remove_extension", parameters));
        }            
    }

    property Action removeTransmission: Action {
        text: "KHR_materials_transmission"
        onTriggered: {
            let parameters = {};
            parameters["property"] = "KHR_materials_transmission";
            Model.Operations.resolvePromiseToCard(Model.Operations.execute("remove_extension", parameters));
        }            
    }

    property Action removeUnlit: Action {
        text: "KHR_materials_unlit"
        onTriggered: {
            let parameters = {};
            parameters["property"] = "KHR_materials_unlit";
            Model.Operations.resolvePromiseToCard(Model.Operations.execute("remove_extension", parameters));
        }            
    }

    property Action removeMaterialVariants: Action {
        text: "KHR_materials_variants"
        onTriggered: {
            let parameters = {};
            parameters["property"] = "KHR_materials_variants";
            Model.Operations.resolvePromiseToCard(Model.Operations.execute("remove_extension", parameters));
        }            
    }

    property Action removeVolume: Action {
        text: "KHR_materials_volume"
        onTriggered: {
            let parameters = {};
            parameters["property"] = "KHR_materials_volume";
            Model.Operations.resolvePromiseToCard(Model.Operations.execute("remove_extension", parameters));
        }            
    }

    property Action removeTextureTransform: Action {
        text: "KHR_texture_transform"
        onTriggered: {
            let parameters = {};
            parameters["property"] = "KHR_texture_transform";
            Model.Operations.resolvePromiseToCard(Model.Operations.execute("remove_extension", parameters));
        }            
    }

    property Action removeXMP: Action {
        text: "remove KHR_xmp_json_ld"
        onTriggered: {
            let parameters = {};
            parameters["property"] = "KHR_xmp_json_ld";
            Model.Operations.resolvePromiseToCard(Model.Operations.execute("remove_extension", parameters));
        }            
    }

    property Action resolveQuantizationAll: Action {
        text: "Remove Mesh Quantization"
        onTriggered: {
            let parameters = {};
            Model.Operations.resolvePromiseToCard(Model.Operations.execute("resolve_mesh_quantization_for_each", parameters));
        }            
    }

    property Action resolveDracoAll: Action {
        text: "Remove Draco Mesh Compression"
        onTriggered: {
            let parameters = {};
            Model.Operations.resolvePromiseToCard(Model.Operations.execute("resolve_draco_for_each", parameters));
        }
    }
    
    property Action convertSpecularGlossinessToMetalicRoughness: Action {
        text: "Convert Specular Glossiness to Metalic Roughness"
        onTriggered: {
            Model.Operations.resolvePromiseToCard(
                Model.Operations.runOperationOnSelection("convert_specular_glossiness_to_metallic_roughness"))
        }            
    }

    property Action storeNodeTRSAsTRS: Action {
        text: "Store TRS as TRS Components"
        onTriggered: {
            let parameters = {storageType: "TRS"}
            Model.Operations.resolvePromiseToCard(
                Model.Operations.runOperationOnSelection("change_transformation_format", parameters))
        }            
    }

    property Action storeNodeTRSAsTRSForAll: Action {
        text: "Store TRS as TRS Components"
        onTriggered: {
            let parameters = {storageType: "TRS"}
            Model.Operations.resolvePromiseToCard(
                Model.Operations.execute("change_transformation_format_for_each", parameters))
        }            
    }
    
    property Action storeNodeTRSAsMatrix: Action {
        text: "Store TRS as Matrix"
        onTriggered: {
            let parameters = {storageType: "Matrix"};
            Model.Operations.resolvePromiseToCard(
                Model.Operations.runOperationOnSelection("change_transformation_format", parameters))
        }            
    }

    property Action storeNodeTRSAsMatrixForAll: Action {
        text: "Store TRS as Matrix"
        onTriggered: {
            let parameters = {storageType: "Matrix"};
            Model.Operations.resolvePromiseToCard(
                Model.Operations.execute("change_transformation_format_for_each", parameters))
        }            
    }

    property Action storeAsBinaryBuffer: Action {
        text: "Store as Binary Buffer"
        onTriggered: {
            let parameters = {storageType: "BufferView"};
            Model.Operations.resolvePromiseToCard(
                Model.Operations.runOperationOnSelection("convert_storage_type", parameters))
        }            
    }

    property Action storeAsBinaryBufferForAllImages: Action {
        text: "Store as Binary Buffer"
        onTriggered: {
            let parameters = {storageType: "BufferView", path: "/gltf/images"};
            Model.Operations.resolvePromiseToCard(
                Model.Operations.execute("convert_storage_type_for_each", parameters))
        }            
    }

    property Action storeAsFile: Action {
        text: "Store as File"
        onTriggered: {
            let parameters = {storageType: "File"};
            Model.Operations.resolvePromiseToCard(
                Model.Operations.runOperationOnSelection("convert_storage_type", parameters))
        }            
    }

    property Action storeAsFileForAllImages: Action {
        text: "Store as File"
        onTriggered: {
            let parameters = {storageType: "File", path: "/gltf/images"};
            Model.Operations.resolvePromiseToCard(
                Model.Operations.execute("convert_storage_type_for_each", parameters))
        }            
    }

    property Action storeAsFileForAllBuffers: Action {
        text: "Store as File"
        onTriggered: {
            let parameters = {storageType: "File", path: "/gltf/buffers"};
            Model.Operations.resolvePromiseToCard(
                Model.Operations.execute("convert_storage_type_for_each", parameters))
        }            
    }

    property Action storeAsEmbedded: Action {
        text: "Store embedded in glTF"
        onTriggered: {
            let parameters = {storageType: "Embedded"};
            Model.Operations.resolvePromiseToCard(
                Model.Operations.runOperationOnSelection("convert_storage_type", parameters))
        }            
    }

    property Action storeAsEmbeddedForAllImages: Action {
        text: "Store embedded in glTF"
        onTriggered: {
            let parameters = {storageType: "Embedded", path: "/gltf/images"};
            Model.Operations.resolvePromiseToCard(
                Model.Operations.execute("convert_storage_type_for_each", parameters))
        }            
    }

    property Action storeAsEmbeddedForAllBuffers: Action {
        text: "Store embedded in glTF"
        onTriggered: {
            let parameters = {storageType: "Embedded", path: "/gltf/buffers"};
            Model.Operations.resolvePromiseToCard(
                Model.Operations.execute("convert_storage_type_for_each", parameters))
        }            
    }

    property Action resolveSparse: Action {
        text: "Resolve sparse accessor"
        onTriggered: {
            Model.Operations.resolvePromiseToCard(
                Model.Operations.runOperationOnSelection("resolve_sparse_accessor"))
        }            
    }

    property Action applyDraco: Action {
        text: "Apply Draco Mesh Compression"
        onTriggered: (source) => {
            applicationData.showPopup("qrc:/Popups/DracoPopup.qml", "Draco Mesh Compression", 512, 380)
        }            
    }

    property Action applyDracoToAllMeshes: Action {
        text: "Apply Draco Mesh Compression"
        onTriggered: (source) => {
            applicationData.showPopup("qrc:/Popups/DracoAll.qml", "Draco Mesh Compression", 512, 380)
        }            
    }

    property Action resolveDraco: Action {
        text: "Remove Draco Mesh Compression"
        onTriggered: (source) => {
            let parameters = {deleteEncodedData: true, cleanExtensionsUsed: true};
            Model.Operations.resolvePromiseToCard(
                Model.Operations.runOperationOnSelection("resolve_draco", parameters))
        }            
    }
    
    property Action resolveQuantization: Action {
        text: "Remove Mesh Quantization"
        onTriggered: {
            Model.Operations.resolvePromiseToCard(
                Model.Operations.runOperationOnSelection("resolve_mesh_quantization"))
        }            
    }
    
    property Action clusterVerticies: Action {
        text: "Cluster verticies"
        onTriggered: (source) => {
            applicationData.showPopup("qrc:/Popups/ClusterVerticiesPopup.qml", "Cluster verticies", 389, 380)
        }            
    }

    property Action reduceTriangleCount: Action {
        text: "Reduce triangle count"
        onTriggered: (source) => {
            applicationData.showPopup("qrc:/Popups/ReduceTrianglesPopup.qml", "Reduce triangle count", 217, 380)
        }            
    }
    
    property Action wireframe: Action {
        text: "Toggle Wireframe"
        onTriggered: {
            if(Model.Observables.selection.length > 0) {
                Model.Operations.runEditorOperation("editor.toggleWireframe", { 
                    uuid: Model.Observables.selection[0]["elementUUID"]
                });
            }
        }
    }
    property Action hideShow: Action {
        text: "Toggle Visibility"
        onTriggered: {
            if(Model.Observables.selection.length > 0) {
                Model.Operations.runEditorOperation("editor.toggleVisibility", { 
                    uuid: Model.Observables.selection[0]["elementUUID"]
                });
            }
        }
    }
}
