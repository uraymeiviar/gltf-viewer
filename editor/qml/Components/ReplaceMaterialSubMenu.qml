import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.15

import "../js/thirdparty.qml.mjs" as Thirdparty;

import "../Colors.js" as Colors

import Project 1.0 as Project

import Model 1.0 as Model


Menu {  
    id: replaceMaterialSubMenu
    title: "Replace Material"
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
        model: getMaterials()
        onObjectAdded: function(index, object) {
            replaceMaterialSubMenu.insertItem(index, object)
        }
        onObjectRemoved: function(index, object) {
            replaceMaterialSubMenu.removeItem(object)
        }
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
                    Model.Operations.resolvePromiseToCard(Model.Operations.execute("replace_material", { 
                        path: "/gltf" + selection[0]["elementAbsolutPath"],
                        replacement: "/gltf"+ modelData.path
                    }));
                }
            }
        }  
    }

    onOpened: {
        if (root.height - replaceMaterialSubMenu.height < replaceMaterialSubMenu.y) {
            replaceMaterialSubMenu.y = root.height - replaceMaterialSubMenu.height
        }
    }

    onActiveFocusChanged: {
        if (!activeFocus) {
            close();
        }
    }

    function getMaterials() { 
        const materialsList = [];
        const materials = Model.Observables.gltf.materials;

        if(materials.length > 0) {
            let index = 0;
            materials.forEach(material => {
                try {
                    if("/materials/" + index === Model.Observables.selection[0]["elementAbsolutPath"]) {
                        index++;
                        return;
                    }
                    const materialName = material.name;
                    let displayedName = "Material " + index;
                    if (materialName && materialName.length !== 0) {
                        displayedName += " \"" + materialName + "\"";
                    }
                    materialsList.push({name: displayedName, path: "/materials/" + index});
                } catch (e) {
                    console.log(e);
                }
                index++;
            });
        }

        if (materialsList.length === 0) {
            materialsList.push({name: "No materials available", path: undefined});
        }

        return materialsList;
    }
}
