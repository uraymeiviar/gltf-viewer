import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.15
import InspectorModel 1.0

import Model 1.0 as Model
import Project 1.0 as Project

import "./js/thirdparty.qml.mjs" as Thirdparty;

import "./Colors.js" as Colors
import "./Components" as Components
import "./Components/inspectorTypes" as InspectorRow


Rectangle {
    id: mainRect
    color: Colors.dark1
    implicitWidth: 175
    implicitHeight: 2

    property var scrollIndicator: 9 // scrollIndicator width + 1px margin 
    
    // handling selection and inspector panel's path, and saving a selection history for the "back" button 
    property var selectionPath: Model.Observables.selection.length > 0 || Model.Observables.selection[0] !== undefined ? 
        Model.Observables.selection[0].elementAbsolutPath : "" // path of the current selected object, if nothing has been selected it will direct to the gltf panel 
    property var pathPanel: ""  // path of the object shown currently in the inspector panel
    property var selectionHistory: [] // saving all the selctions in an array for the "back" button 
    onPathPanelChanged: { // if the path changes, it will redraw the new inspector 
        inspectorTree.forceLayout(); 
    }
    onSelectionPathChanged: { // handling selectionHistory for the "back" button 
        pathPanel = selectionPath 
        
        let newSelectedObj // = {uuid: elementUUID, path: elementAbsolutPath, localPath: localPath}
        // saving all the selections in an array for the "back" button 
        if (selectionHistory.length > 0 && (pathPanel === selectionHistory[0].path || Model.Observables.selection[0] === undefined)) { 
            return; // if user selects the current obj, the obj will not be added or if the selection is undefined 
        } else if (selectionHistory.length === 0 && pathPanel.length === 0) { 
            // at the start the selection is empty, it will show the gltf panel or if the selection is undefined 
            newSelectedObj = {uuid: "", path: "", localPath: ""}
            selectionHistory = [newSelectedObj, ...selectionHistory] 
        } else { // add new selected obj in the array of selectionHistory
            newSelectedObj =  {uuid: Model.Observables.selection[0].uuid, path: pathPanel, localPath: Model.Observables.selection[0].localPath}
            selectionHistory = [newSelectedObj, ...selectionHistory] 
        }
    }
    Connections { // loading a new gltf will reset the selectionHistory array 
        target: Model.Observables
        function onGltfLoaded(newGltf) {
            selectionHistory = [{uuid: "", path: "", localPath: ""}];
        }
    }
    property var titlePanel: pathPanel.length > 0 ? pathPanel : "glTF" // current title of the inspector panel

    // array containing object with buttons to be displayed 
    property var objecstWithButton: ["materials", "images", "meshes", "nodes", "accessors", "cameras", "buffers", "animations"] 
    // string with the name of the popup to be opened and its model 
    property var popupChoosen: { "popupTitle": "", "popupModel": "", "popupHeight": 0, "tooltip": "" }
    // array with all of the buttons of the object (if any)
    QtObject {
        id: objBtnArray
        property var primitives: [ {btnText: "Reduce", btnOperation: "", popupTitle: "Reduce Triangle Count", popupModel: "qrc:/Popups/ReduceTrianglesPopup.qml", popupHeight: 217}, 
            {btnText: "Cluster", btnOperation: "", popupTitle: "Cluster Triangle Vertices", popupModel: "qrc:/Popups/ClusterVerticiesPopup.qml", popupHeight: 389}, 
            {btnText: "Bake Ambient Occlusion", btnOperation: "", popupTitle: "Bake Ambient Occlusion Map", popupModel: "qrc:/Popups/BakeAOMapPopup.qml", popupHeight: 270}, 
            {btnText: "Generate Texture Coordinates", btnOperation: "", popupTitle: "Generate Texture Coordinates", popupModel: "qrc:/Popups/GenerateTexCoordPopup.qml", popupHeight: 156}, 
            {btnText: "Draco Mesh Compression", btnOperation: "", popupTitle: "Draco Mesh Compression", popupModel: "qrc:/Popups/DracoPopup.qml", popupHeight: 522}, 
            {btnText: "Create Tangents", btnOperation: "create_tangents"}, 
            {btnText: "Manage Material Variants", btnOperation: "", popupTitle: "Manage Material Variants", popupModel: "qrc:/Popups/ManageMaterialVariants.qml", popupHeight: 522}, 
        ] 
        property var nodes: [
            {btnText: "Apply Transform", btnOperation: "apply_node_transform"}, 
            {btnText: "Convert Storage Type of Transform", btnOperation: "change_transformation_format"},
            {btnText: "Add Light", btnOperation: "create_light"}
            ]  
        property var meshes: [{btnText: "Join Primitives", btnOperation: "join_primitives"}, 
            {btnText: "Split Primitives", btnOperation: "split_up_primitives"}] 
        property var images: [{btnText: "Convert and Resize", btnOperation: "", popupTitle: "Image Convert", popupModel: "qrc:/Popups/ConvertImagePopup.qml", popupHeight: 485},
            {btnText: "Change Storage Type", btnOperation: "", popupTitle: "Convert Storage Type", popupModel: "qrc:/Popups/ConvertStorageTypePopup.qml", popupHeight: 170},
            {btnText: "Reload File", btnOperation: "reload_file", parameters: {"resourceDirectory": Model.Observables.gltfDirectory}},
            {btnText: "Edit Externally", btnOperation:"", btnFunction: function(){Project.Actions.editImage.trigger()}},
        ]
        property var buffers: [{btnText: "Convert Storage Type", btnOperation: "", popupTitle: "Convert Storage Type", popupModel: "qrc:/Popups/ConvertStorageTypePopup.qml", popupHeight: 120},
        ]
        property var cameras: [{btnText: "Convert Camera Type", btnOperation: "change_camera_type"}]
        property var accessors: [{btnText: "Resolve Sparse", btnOperation: "resolve_sparse_accessor"}]  
        property var animations: [{btnText: "Optimize Keyframes", btnOperation: "optimize_keyframes"}]  
        property var materials: [{btnText: "Convert Specular Glossiness to Metallic Roughness", btnOperation: "convert_specular_glossiness_to_metallic_roughness"}
        /*{btnText: "Apply to all KHR_texture_transform", btnOperation: "", popupTitle: "Texture Transform", popupModel: "apply", popupHeight: 144}*/]  
    }
    

    MouseArea { // inspector's row inputs lose focus if clicking outside the texfield
        anchors.fill: parent
        onClicked: {
            focus = true
        }
    }


    Rectangle {  // breadcrumbs 
        id: breadcrumbs
        color: "transparent"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 28

        Button {  // back btn
            id: backBtn
            flat: true
            padding: 0
            topInset: 0
            bottomInset: 0
            implicitWidth: 50
            implicitHeight: 20
            opacity: 0.7
            icon.source: "qrc:icons/Icon_Left.svg"
            icon.width: 8
            icon.height: 8
            anchors.left: parent.left
            anchors.leftMargin: 5 
            anchors.top: parent.top
            anchors.topMargin: 4
            enabled: selectionHistory.length > 1 ? true : false 
            hoverEnabled: true
            text: "Back"
            font.pixelSize: 12
            icon.color: !backBtn.enabled ? Colors.light4 : backBtn.hovered ? Colors.light1 : Colors.light5
            onClicked: { 
                // clicking on the back btn will sleect the previous selected obj  
                if (selectionHistory.length > 1) { 
                    let tmpSelection = {}; // selection params 
                    tmpSelection["uuid"] = selectionHistory[1].uuid; // previous obj uuid 
                    tmpSelection["localPath"] = selectionHistory[1].localPath; // previous obj localPath 

                    if (selectionHistory.length === 2) { // the first element is the gltf at the start, and the selection is empty so no uuid available 
                        selectionHistory = [] // empty array 
                        Model.Operations.runEditorOperation("editor.clearSelection", {}); // clearing the selection 
                    } else { 
                        selectionHistory = selectionHistory.slice(1) // slicing the array to remove the current selected obj 
                        Model.Operations.runEditorOperation("editor.setSelection", [tmpSelection]); // selecting the previous obj 
                    }
                } 
            }
            background: Rectangle {
                anchors.fill: parent
                color: "transparent"
            }
        }
    }

    Rectangle {   // panel title section: icon + label
        id: titleBox
        color: Colors.dark4
        anchors.top: breadcrumbs.bottom
        anchors.topMargin: 1
        anchors.left: parent.left
        anchors.leftMargin: 4
        anchors.right: parent.right
        anchors.rightMargin: 9
        height: 36
        Image {
            source: {
                let typeName = titlePanel.charAt(1).toUpperCase() + titlePanel.slice(2, titlePanel.lastIndexOf("/") - 1)
                if (typeName.includes("materials_variants")) {
                    typeName = "MaterialVariants"
                } else if (typeName.includes("primitive")) {
                    typeName = "Primitive"
                } else if (typeName === "Meshe") {
                    typeName = "Mesh"
                } else if (typeName.includes("light")) {
                    typeName = "Light"
                }
                let iconSource = titlePanel === "glTF" ? "qrc:/icons/glTF_500px_June16.png" : "qrc:/icons/Icon_Content_" + typeName + ".svg" 
                return iconSource
            } 
            sourceSize.width: 16
            sourceSize.height: 16
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 10
        }
        Label {
            width: 100
            height: 16
            anchors.fill: parent
            anchors.topMargin: 10
            anchors.leftMargin: 36
            text: {
                if (titlePanel.includes("glTF")) { 
                    return "glTF";
                }
                // replace all slashes with spaces
                const spaceName = titlePanel.slice(1).replace(/\//g, ' ');

                const lastSpaceIndex = spaceName.lastIndexOf(' ');
                const objectIndex = spaceName.slice(lastSpaceIndex);
                let objectName = spaceName.slice(0, lastSpaceIndex - 1);

                const lastSpaceIndexTwo = objectName.lastIndexOf(' ');
                if(lastSpaceIndexTwo != -1) {
                    // extract stem e.g. "primitive 0 extensions KHR_materials_variants mapping" -> "mapping"
                    objectName = objectName.slice(lastSpaceIndexTwo + 1);
                }

                if (objectName.includes("meshe")) {
                    // turn e.g. "meshe 0" into "mesh 0"
                    objectName = objectName.slice(0, objectName.length - 1);
                }

                const displayName = objectName + objectIndex;
                // capitalize first letter
                return displayName.charAt(0).toUpperCase() + displayName.slice(1);
            } 
            font.pixelSize: 12
            font.weight: Font.Bold
            color: Colors.light3
        }
    }

    Rectangle { // area above the properties that holds button for operations and popups
        id: buttonRect
        implicitWidth: parent.width 
        width: parent.width 
        height: !visible || btnComponent.modelArray === undefined ? 0 : btnComponent.height
        anchors.top: titleBox.bottom
        anchors.left: mainRect.left
        anchors.leftMargin: 4
        anchors.right: mainRect.right
        anchors.rightMargin: 9
        color: Colors.dark3
        visible: objecstWithButton.find((el) => pathPanel.includes(el)) ? true : false 
        
        // Some objects (e.g. primitives, images) have buttons that can open popups or apply an operation
        InspectorRow.ButtonRow {
            id: btnComponent
            modelArray: {
                let objName =  pathPanel.slice(1, pathPanel.lastIndexOf("/"))
                objName = objName.includes("primitives") && !objName.includes("materials_variants") ? "primitives" : objName
                objBtnArray[objName]
            }
        }
    }
    
    ScrollView {  // section with all the property rows of the object selected (e.g. node)
        id: scrollInspector
        anchors.top: buttonRect.bottom
        anchors.topMargin: 2
        anchors.left: mainRect.left
        anchors.leftMargin: 4
        anchors.right: mainRect.right
        anchors.bottom: mainRect.bottom
        anchors.bottomMargin: 4
        clip: true
        wheelEnabled: true
        ScrollBar.vertical.policy: ScrollBar.AlwaysOn
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        contentHeight: inspectorTree.childrenRect.height + 30 

        MouseArea {  // inspector's row inputs lose focus when clicking outside the textfield
            anchors.top: inspectorTree.top
            height: scrollInspector.height + inspectorTree.height
            width: parent.width
            onClicked: {
                focus = true
            }
        }
        
        Components.InspectorTreeView {
            id: inspectorTree
            width: scrollInspector.width - scrollIndicator
            height: childrenRect.height + 30
            clip: true
            anchors.fill: parent
            interactive: false
            model: InspectorModel {
                id: inspectorModel
                path: pathPanel
            }
        }
    }
}
