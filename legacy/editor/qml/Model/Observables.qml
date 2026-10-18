pragma Singleton

import "../js/thirdparty.qml.mjs" as Thirdparty;
import "../Components" as Components

import QtQuick 2.15

Item {
    property var gltfHierarchy: {};
    property var gltfResources: {};
    property var gltf;
    property var listeners: [];
    property var rawSelection: [];
    property var selection: [];
    property var wireframe: [];
    property string openScenePath;
    property string gltfDirectory;
    property var hiddenObjects: [];
    property var unusedNodes: [];
    property var imageMetadata: [];

    signal gltfLoaded(var gltf);

    Connections {
        target: applicationData
        function onGltfChanged(updatedGltf) {
            gltf = JSON.parse(updatedGltf);
        }
        function onGltfLoaded(newGltf) {
            gltfLoaded(newGltf);
        }
        function onUnusedNodesChanged(value) {
            unusedNodes = value;
        }
        function onEditorStateChanged(editorState) {
            let tmpJson = JSON.parse(editorState);
            if("selection" in tmpJson) {
                rawSelection = tmpJson["selection"];
            } else {
                rawSelection = [];
            }
            if("wireframe" in tmpJson) {
                wireframe = tmpJson["wireframe"];
            } else {
                wireframe = [];
            }
            if("visibility" in tmpJson) {
                hiddenObjects = tmpJson["visibility"];
            } else {
                hiddenObjects = [];
            }
            if("openScene" in tmpJson) {
                openScenePath = tmpJson["openScene"];
            }
            if("gltfDirectory" in tmpJson) {
                gltfDirectory = tmpJson["gltfDirectory"];
            }
        }
        function onSelectionChanged(tmpSelection) {
            selection = JSON.parse(tmpSelection);
        }
        function onGltfHierarchyChanged(updatedGltfHierarchy) {
            gltfHierarchy = JSON.parse(updatedGltfHierarchy);
        }
        function onGltfResourcesChanged(updatedGltfResources) {
            gltfResources = JSON.parse(updatedGltfResources);
        }
        function onImageMetadataChanged(updatedImageMetadata) {
            imageMetadata = JSON.parse(updatedImageMetadata);
        }
    }

    function gltfAt(path) {
        return Thirdparty.jsonpointer.get(gltf, path);
    }

    function gltfValue(path, defaultValue) {
        let value = defaultValue;
        try {
            value = Thirdparty.jsonpointer.get(gltf, path);
        }   catch (error) {
            return defaultValue;
        }
        return value ? value : defaultValue;
    }

    function parentPointer(pointer) {
        var split = pointer.split("/")
        let splitPointer = [ split.slice(0, -1).join("/") ].concat(split.slice(-1));
        if (splitPointer[0] === ""){
           return "/";
        }
        return splitPointer[0];
    }

    // get all the available pointers of a specific object type (ex: all the available meshes)
    function getAvailablePointers(objName, path) {
        let pointersArray = undefined
        // query data: all available pointer options
        if (gltf !== undefined) { 
            if (objName !== undefined) { 
                pointersArray = applicationData.getReferenceCandidates(JSON.stringify(gltf), objName, path);
            }
        }

        // if the pointersArray has found any values, it will look like this: ["/scenes/0", "'Scene'"]
        return pointersArray
    }
}
