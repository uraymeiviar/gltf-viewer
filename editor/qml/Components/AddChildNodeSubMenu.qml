import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.15

import "../js/thirdparty.qml.mjs" as Thirdparty;

import "../Colors.js" as Colors

import Project 1.0 as Project

import Model 1.0 as Model


Menu {  
    id: addNodeSubMenu
    title: "Add Unused Child Node"
    y: 0
    width: 160
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: Rectangle {
        width: parent.width
        height: parent.height
        color: Colors.dark0
        radius: 5
    }
    Instantiator {  
        model: addNodeMenuFunctions.getNodeEntries()
        onObjectAdded: addNodeSubMenu.insertItem(index, object)
        onObjectRemoved: addNodeSubMenu.removeItem(object)
        delegate: MenuItem {
            implicitHeight: 22
            font.pixelSize: 11
            Component.onCompleted: {
                text = modelData.name;
            }

            onTriggered: {
                const selection = Model.Observables.selection;
                if (selection) {
                    Model.Operations.runEditorOperation("editor.clearSelection", {});
                    Model.Operations.execute("add_node", { 
                        path: "/gltf" + selection[0]["elementAbsolutPath"],
                        pathSrc: "/gltf"+ modelData.path
                    })
                }
            }
        }  
    }

    onOpened: {
        if (root.height - addNodeSubMenu.height < addNodeSubMenu.y) {
            addNodeSubMenu.y = root.height - addNodeSubMenu.height
        }
    }

    onActiveFocusChanged: {
        if (!activeFocus) {
            close();
        }
    }

    QtObject {
        id: addNodeMenuFunctions
        function getNodeEntries() { 
            const nodeList = [];
            const unusedNodes = Model.Observables.unusedNodes;

            if(unusedNodes.length > 0) {
                unusedNodes.forEach(unusedNodePath => {
                    const nodeName = Model.Observables.gltfValue(unusedNodePath + "/name", "");
                    let displayedName = "Node " + unusedNodePath.split("/").pop();
                    if (nodeName.length !== 0) {
                        displayedName += " \"" + nodeName + "\"";
                    }
                    nodeList.push({name: displayedName, path: unusedNodePath});
                });
            }

            if (nodeList.length === 0) {
                nodeList.push({name: "No nodes available", path: undefined});
            }

            return nodeList;
        }
    }
}
