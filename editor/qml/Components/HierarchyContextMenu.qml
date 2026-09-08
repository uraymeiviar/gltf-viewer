import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.15

import "../js/thirdparty.qml.mjs" as Thirdparty;

import "../Colors.js" as Colors

import Project 1.0 as Project

import Model 1.0 as Model


// the context menu is placed here and not in the line component file to save on resources.
Menu {   // context menu popup
    id: hierarchyContextMenu
    y: parent.height
    width: 200
    height: childrenRect.implicitHeight
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    spacing: 3
    property var gltfPath: undefined

    background: Rectangle {
        width: parent.width
        height: parent.height
        color: Colors.dark0
        radius: 5
    }

    onActiveFocusChanged: {
        if (activeFocus === false) {
            instantiatorModel = []
            gltfPath = undefined
            close();
        }
    }

    property var instantiatorModel: []

    // Menu seperator blueprint so that you can have as many as you want in your menus
    Component{
        id: menuSeperator
        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: 15
            anchors.rightMargin: 15
            anchors.topMargin: 5
            anchors.bottomMargin: 5
            color: Colors.light4
            opacity: 0.5
            
        }
    }
    Component{
        id: iconComponent
        Image {
            sourceSize.width: 24
            sourceSize.height: 12 
        }
    }

    QtObject {
        id: typeMenuEntries
        // each type gets a dedicated array here specifying the available context menu entries
        // Hierarchy
        property var none: ["createNewScene"];
        property var gltf: ["createScene", "importModel", "menuSeperator", "deleteObject"];
        property var scene: ["cut", "copy", "paste", "cloneDeep", "cloneLinks", "menuSeperator", 
            "createChildNode", "addUnusedChildNode", "menuSeperator", "setDefaultScene", "unsetDefaultScene", "openScene"];
        property var node: ["cut", "copy", "paste", "cloneDeep", "cloneLinks", "remove", "menuSeperator", 
            "createChildNode", "addUnusedChildNode", "createAnimation", "menuSeperator", "focusSelection"];
        property var camera: ["cut", "copy", "remove"];
        property var light: ["cut", "copy", "remove"];
        property var mesh: ["cut", "copy", "paste", "remove"];
        property var primitive: [];
        property var material: ["cut", "copy", "remove"];
        property var texture: ["remove"];
        property var image: ["remove"];
        property var skin: ["cut", "copy", "paste", "remove"];
        property var animation: [];
        property var accessor: [];
        property var bufferview: [];
        property var buffer: [];

        // Resources
        property var sceneResources:  ["setDefaultScene", "unsetDefaultScene", "menuSeperator", "openScene", "menuSeperator",
            "createNewScene", "importModel", "menuSeperator", "copy", "cloneLinks", "cloneDeep", "deleteObject"];
        property var nodeResources: ["createNode", "createAnimation", "menuSeperator", "storeNodeTRSAsMatrix", "storeNodeTRSAsTRS", "menuSeperator", "copy", "cloneLinks", "cloneDeep", "deleteObject"];
        property var cameraResources: ["createCamera", "menuSeperator", "copy", "cloneLinksAltText", "deleteObject"];
        property var lightResources: ["createLight", "menuSeperator", "copy", "cloneLinksAltText", "deleteObject"];
        property var materialvariantsResources: ["createMaterialVariants", "menuSeperator", "cloneLinksAltText", "deleteObject"];
        property var meshResources: ["createMesh", "menuSeperator", "copy", "cloneDeepAltText", "deleteObject", "applyDraco", "resolveDraco", "clusterVerticies", "reduceTriangleCount"];
        property var materialResources: ["createMaterial", "importMaterial", "convertSpecularGlossinessToMetalicRoughness", "replaceMaterial", "menuSeperator", "copy", "cloneLinks", "cloneDeep", "deleteObject"];
        property var textureResources: ["createTexture", "importTexture", "menuSeperator", "cloneLinks", "cloneDeep", "deleteObject"];
        property var imageResources: ["importImage", "editImage", "menuSeperator", "storeAsBinaryBuffer", "storeAsFile", "storeAsEmbedded", "menuSeperator", "cloneDeepAltText", "deleteObject"];
        property var samplerResources: ["createSampler", "menuSeperator", "cloneLinksAltText", "deleteObject"];
        property var animationResources: ["createAnimation", "menuSeperator", "deleteObject"];
        property var bufferResources: ["storeAsEmbedded", "storeAsFile"];
        property var accessorResources: ["resolveSparse"];
        property var packetResources: ["importXmpPacket"]; //is deducted from path

        property var createObjsResources: ["createNewScene", "createNode", "createCamera", "createLight", "createMesh", 
            "createMaterial", "createTexture", "createSampler", "createAnimation", "createMaterialVariants", "menuSeperator", "importImage", "importTexture", 
            "importMaterial", "importModel", "importXmpPacket"];  // resources has a btn to create all of the objs and import some objs

        property var scenesCategory: ["createNewScene", "importModel", "Select all"];
        property var nodesCategory: ["createNode", "Select all", "menuSeperator", "storeNodeTRSAsMatrixForAll", "storeNodeTRSAsTRSForAll"];
        property var camerasCategory: ["createCamera", "Select all"];
        property var lightsCategory: ["createLight", "Select all"];
        property var materialvariantsCategory: ["createMaterialVariants", "Select all"];
        property var materialsCategory: ["createMaterial", "Select all"];
        property var meshesCategory: ["createMesh","applyDracoToAllMeshes", "resolveDracoAll", "Select all"];
        property var texturesCategory: ["createTexture", "importTexture", "Select all"];
        property var imagesCategory: ["importImage", "Select all", "menuSeperator", "storeAsBinaryBufferForAllImages", "storeAsFileForAllImages", "storeAsEmbeddedForAllImages"];
        property var samplersCategory: ["createSampler", "Select all"];
        property var animationsCategory: ["createAnimation", "Select all"];
        property var buffersCategory: ["Select all", "menuSeperator", "storeAsEmbeddedForAllBuffers", "storeAsFileForAllBuffers"];
        property var accessorsCategory: ["Select all"];
        property var bufferviewsCategory: ["Select all"];
        property var skinsCategory: ["Select all"];
        property var xmpCategory: ["importXmpPacket", "Select all"];
    }

    //  cleans the menu; fills it with correct entries; and finally opens it
    function prepareAndOpen(type, isHeading, path) {
        type = type.replace(" ", "");
        type = type.toLowerCase();

        if (hierarchyOrResource2 === "Resources"){
            if(isHeading) {
                type += "Category";
            } else {
                type += "Resources";
            }
        }

        let options = typeMenuEntries[type];

        if (options === undefined && isHeading) {
            options = ["Select all"];
        }


        if (JSON.stringify(instantiatorModel) === JSON.stringify(typeMenuEntries.createObjsResources) && hierarchyOrResource2 === "Resources") {
            options = typeMenuEntries.createObjsResources
        } else if (options === undefined || options.length === 0) {
            instantiatorModel = [];
            gltfPath = undefined
            return;
        }
        instantiatorModel = options;
        gltfPath = isHeading ? path : undefined

        open();
    }

    Instantiator {
        model: hierarchyContextMenu.instantiatorModel
        delegate: MenuItem {
            id: delegateMenuItem
            implicitHeight: 22
            font.pixelSize: 11
            Component.onCompleted: {
                // add menuseperator
                if (modelData === "menuSeperator") {
                    let seperator = menuSeperator.createObject(delegateMenuItem);
                    implicitHeight = 11; // = 5 pixels from top and bottom margin (of rect) each plus one
                    enabled = false
                // add actions
                } else if (modelData === "addUnusedChildNode") {
                    action = addUnusedChildNode;
                    text = addUnusedChildNode.text
                } else if (modelData === "replaceMaterial") {
                    action = replaceMaterial;
                    text = replaceMaterial.text
                } else if (modelData === "Select all") { 
                    text = modelData
                } else {
                    if (Project.Actions[modelData] != undefined) {
                        action = Project.Actions[modelData];
                        text = Project.Actions[modelData].text
                    }
                }
            }

            onTriggered: {
                if (modelData === "Select all"){
                    applicationData.selectAllOfType(gltfPath);
                }
            }
        }
        onObjectAdded: function(index, object) {
            hierarchyContextMenu.insertItem(index, object)
        }
        onObjectRemoved: function(index, object) {
            hierarchyContextMenu.removeItem(object)
        }
    }

    onOpened: {
        scrollViewContent.hierarchy.popupOpen = true;
        if(root.height - hierarchyContextMenu.height < hierarchyContextMenu.y){
            hierarchyContextMenu.y = root.height - hierarchyContextMenu.height
        }
    }
    onClosed: {
        scrollViewContent.hierarchy.popupOpen = false;
    }

    property Action addUnusedChildNode: Action {
        text: "Add Unused Child Node"
        onTriggered: {
            const component = Qt.createComponent("../Components/AddChildNodeSubMenu.qml");
            if (component.status != Component.Ready) {
                if (component.status == Component.Error) {
                    console.debug("Error:" + component.errorString());
                }
                return;
            }
            const object = component.createObject(root, {clip: true});
            object.popup()
        }
    }

    property Action replaceMaterial: Action {
        text: "Replace with..."
        onTriggered: {
            const component = Qt.createComponent("../Components/ReplaceMaterialSubMenu.qml");
            if (component.status != Component.Ready) {
                if (component.status == Component.Error) {
                    console.debug("Error:" + component.errorString());
                }
                return;
            }
            const object = component.createObject(root, {clip: true});
            object.popup()
        }            
    }

    Connections {
        target: applicationData
        function onCloneDeepTriggered() {
            Project.Actions.cloneDeep.trigger();
        }
        function onCloneLinkedTriggered() {
            Project.Actions.cloneLinks.trigger();
        }
    }
}
