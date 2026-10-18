import QtQuick 2.15
import QtQuick.Controls
import QtQml
import "../Colors.js" as Colors

import "." as Popups
import "../Components/inspectorTypes" as InspectorComponents

import Model 1.0 as Model

Popups.PopupTemplate {
    id: popup
    title: "Draco Mesh Compression"
    contentComponent: dracoComponent
    height: 600

    property bool applyToAllMeshesInGLTF: false

    Component {
        id: dracoComponent

        Rectangle {
            id: draco
            color: Colors.dark1
            anchors.fill: parent

            function accept(){
                let parameters = {};
                parameters["sequentialEncoding"] = firstRows.model[0].value
                parameters["targetCompressionLevel"] = dracoNumbers.model.get(0).value  
                parameters["quantizationBitsPosition"] = dracoNumbers.model.get(1).value 
                parameters["quantizationBitsNormal"] = dracoNumbers.model.get(2).value   
                parameters["quantizationBitsUv"] = dracoNumbers.model.get(3).value
                parameters["quantizationBitsColor"] = dracoNumbers.model.get(4).value  
                parameters["quantizationBitsGeneric"] = dracoNumbers.model.get(5).value  

                parameters["keepUncompressedData"] = true // This is necessary for the renderer

                if(applyToAllMeshesInGLTF) {
                    Model.Operations.resolvePromiseToCard(Model.Operations.execute("apply_draco_for_each", parameters), "Draco compression successful");
                } else {
                    Model.Operations.resolvePromiseToCard(Model.Operations.runOperationOnSelection("apply_draco", parameters), "Draco compression successful");
                }

            }

            ListView { 
                id: firstRows
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                interactive: false
                anchors.leftMargin: 5
                anchors.rightMargin: 5
                height: childrenRect.height 
                width: parent.width - 30
                spacing: 1 
                model: [{"label":"Preserve triangle order", "value": false }, {"label": "Optimize vertex indices", "value": false}]
                delegate: Rectangle {
                    id: rowDelegate
                    anchors.left: parent.left
                    anchors.right: parent.right
                    implicitWidth: draco.width
                    implicitHeight: 30
                    color: Colors.dark2 
                    Text {
                        id: labelLeft
                        text: model.modelData.label
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 5
                        color:  Colors.light1 
                    }

                    CheckBox {
                        id: boolCheck
                        anchors.right: parent.right
                        anchors.rightMargin: 5
                        anchors.verticalCenter: parent.verticalCenter
                        checked: model.modelData.value 
                        onToggled: {
                            model.modelData.value = checked
                        }
                    }
                }
            }
            
            ListModel {
                id: dracoListModel
                ListElement {
                    label: "Compression level (0 = highest / 10 = lowest)"
                    value: 7
                    min: 0
                    max: 10
                }
                ListElement {
                    label: "Position quantization bits"
                    value: 11
                    min: 2
                    max: 32
                }
                ListElement {
                    label: "Normal quantization bits"
                    value: 8
                    min: 2
                    max: 32
                }
                ListElement {
                    label: "UV quantization bits"
                    value: 10
                    min: 2
                    max: 32
                }
                ListElement {
                    label: "Color quantization bits"
                    value: 8
                    min: 2
                    max: 32
                }
                ListElement {
                    label: "Generic quantization bits"
                    value: 8
                    min: 2
                    max: 32
                }
            }
            
            ListView {
                id: dracoNumbers
                anchors.top: firstRows.bottom
                anchors.topMargin: 1
                anchors.left: parent.left
                anchors.right: parent.right
                interactive: false
                height: childrenRect.height 
                width: parent.width - 30
                spacing: 1 
                model: dracoListModel
                delegate: InspectorComponents.SliderPopupRow {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 5
                    anchors.rightMargin: 5

                    labelTitle: label
                    
                    isPercentage: false
                    minValue:  min
                    maxValue:  max

                    valueNum:  value
                    onValueNumChanged: {
                        if (value !== valueNum){
                            dracoListModel.setProperty(index, "value", valueNum)
                        }
                    }
                }
            }
        }
    }
}