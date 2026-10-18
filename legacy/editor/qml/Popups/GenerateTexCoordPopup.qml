import QtQuick 2.15
import QtQuick.Controls
import QtQml
import Qt.labs.qmlmodels
import "../Colors.js" as Colors

import "." as Popups
import "../Components/inspectorTypes" as InspectorComponents

import Model 1.0 as Model

Popups.PopupTemplate {
    id: popup
    title: "Generate Texture Coordinates"
    contentComponent: component
    height: 126 + 30
    
    Component {
        id: component
        Rectangle {
            id: generate
            color: Colors.dark1
            anchors.fill: parent

            function accept()
            {
                let parameters = {};
                parameters["proxyShape"] = generateTexCoordModel.rows[1].currentValue;
                parameters["texCoordIndex"] = Number(generateTexCoordModel.rows[0].currentValue.split('_')[1]);

                Model.Operations.resolvePromiseToCard(Model.Operations.runOperationOnSelection("create_texture_coordinates", parameters));
            }

            function getPossibleTexCoords(){
                //TODO: Fix for multiselection?
                let attributes = Model.Observables.gltfValue(Model.Observables.selection[0]["elementAbsolutPath"] + "/attributes", {});
                let texCoords = [];
                for (let attribute in attributes) {
                    if (attribute.startsWith("TEXCOORD_")){
                        let number = Number(attribute.split('_')[1]);
                        texCoords[number] = number;
                    }
                }
                let result = [];
                for (let i = 0; i < texCoords.length; i++){
                    if (texCoords[i] === undefined){
                        result.push('Create Texcoord_' + i);
                    }
                }

                result.push('Create Texcoord_' + texCoords.length);
                return result;
            }

            TableView {
                id: generateTableView
                anchors.fill: parent
                interactive: false
                anchors.margins: 5
                rowSpacing: 1
                clip: true
                property var texCoordOptions: generate.getPossibleTexCoords()

                model: TableModel {
                    id: generateTexCoordModel
                    TableModelColumn { display: "label" }
                    TableModelColumn { display: "value" }
                    rows: [
                        { "label": "Texture Coordinates", "value": generateTableView.texCoordOptions, "currentValue": generateTableView.texCoordOptions[0] }, 
                        { "label": "Creation Type", "value": ["Sphere", "Cylinder", "Plane", "Cube"],  "currentValue": "Sphere" }
                    ]
                }

                delegate: Rectangle {
                    implicitWidth: column === 0 ? 140 : 230
                    implicitHeight: 30
                    color: Colors.dark2

                    Text {
                        text: column === 0 ? display : ""
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 5
                        color:  Colors.light1 
                        visible: column === 0 ? true : false
                    }

                    ComboBox {  // combobox
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right
                        anchors.rightMargin: 5
                        visible: column === 1 ? true : false
                        model: column === 1 ? display : undefined
                        width: 190
                        flat: true
                        implicitHeight: 21
                        topPadding: -4 
                        bottomPadding: -4 
                        indicator: Image {
                            x: 170
                            y: 1
                            width: 20
                            height: 20
                            source: "qrc:icons/Icon_down_20px.svg"
                        }
                        font.pixelSize: 12
                        background: Rectangle {
                            anchors.fill: parent
                            color: Colors.dark4
                        }
                        onActivated: {
                            model[row].currentValue = currentText
                        }
                        hoverEnabled: true
                    }
                }
            }
        }
    }
}
