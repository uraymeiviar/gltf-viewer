import QtQuick 2.15
import QtQuick.Controls
import QtQml
import "../Colors.js" as Colors

import "." as Popups
import "../Components/inspectorTypes" as InspectorComponents

import Model 1.0 as Model

Popups.PopupTemplate {
    id: popup
    title: "Cluster Triangle Vertices"
    contentComponent: component
    height: 459 + 30
    
    Component {
        id: component
        Rectangle {
            id: cluster
            color: Colors.dark1
            anchors.fill: parent

            function accept(){
                let parameters = {};
                parameters["positionThreshold"] = clusterList.model.get(0).value
                parameters["normalThreshold"] = clusterList.model.get(1).value         
                parameters["tangentThreshold"] = clusterList.model.get(2).value        
                parameters["texCoordThreshold"] = clusterList.model.get(3).value
                parameters["colorThreshold"] = clusterList.model.get(4).value

                Model.Operations.runOperationOnSelection("apply_vertex_clustering", parameters).then(function (message) {
                    if (message !== "") {
                        applicationData.info(message)
                    } else {
                        applicationData.success("Vertex clustering successful")
                    }
                    }).catch(function (message) {
                        if (message === "") {
                            message = "Operation failed"
                        }
                        applicationData.error(message)
                    })
            }

            ListModel {
                id: clusterModel
                ListElement{
                    name: "Position Threshold"
                    value: 0.0
                }
                ListElement{
                    name: "Normal Threshold"
                    value: 0.05
                }
                ListElement{
                    name: "Tangent Threshold"
                    value: 0.05
                }
                ListElement{
                    name: "TexCoord Threshold"
                    value: 0.0
                }
                ListElement{
                    name: "Color Threshold"
                    value: 0.01
                }
            }
            
            ListView {
                id: clusterList
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                interactive: false
                anchors.leftMargin: 5
                anchors.rightMargin: 5
                height: childrenRect.height 
                width: parent.width
                spacing: 1
                model: clusterModel
                delegate: InspectorComponents.SliderPopupRow {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 5
                    anchors.rightMargin: 5

                    labelTitle: model.name
                    isPercentage: true
                    minValue: 0
                    maxValue: model.name === "Position Threshold" ? 5 : 100
                    stepSize: model.name === "Position Threshold" ? 0.001 : 0.01
                    valueNum: model.value
                    onValueNumChanged: {
                        if (model.value != valueNum){
                            model.value = valueNum
                        }
                    }
                }
            }
        }
    }
}