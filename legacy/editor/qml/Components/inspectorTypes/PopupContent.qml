import QtQuick 2.15
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.15
import Qt.labs.qmlmodels 1.0

import "." as InspectorComponents

import Model 1.0 as Model

import "../../Colors.js" as Colors 


QtObject {
    id: contentOption

    property var popupHeight: 1 // height of the popup

    property var gltfPath: undefined // e.g. /images/0


    property var apply: Rectangle {
        id: apply
        parent: contentRow
        color: Colors.dark1
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: contentOption.popupHeight
        visible: popupChoosen.popupModel === "apply" ? true : false 
        
        TableView {
            anchors.fill: parent
            interactive: false
            anchors.margins: 5
            rowSpacing: 1
            clip: true 

            model: TableModel {
                TableModelColumn { display: "label" }
                TableModelColumn { display: "value" }
                rows: [
                    { "label": "Offset", "value": [0, 0] }, 
                    { "label": "Rotation", "value": [0] }, 
                    { "label": "Scale", "value": [0, 0] }
                ]
            }

            delegate: Rectangle {
                implicitWidth: column === 0 ? 120 : 250
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

                ListView { 
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 5
                    height: parent.height
                    width: childrenRect.width
                    orientation: ListView.Horizontal
                    visible: column === 1 ? true : false
                    model: column === 1 ? display : undefined
                    interactive: false   // important or the row is all "moveable"! 
                    spacing: 10 
                    delegate: Rectangle {
                        height: parent.height
                        width: childrenRect.width
                        color: "transparent"
                        Label { 
                            id: inputLabel
                            width: 12
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 6
                            text: row === 1 ? "" : index === 0 ? "U" : "V"
                            font.pixelSize: 11
                            color: Colors.light4
                        }
                        TextField {  // actual input
                            id: inputBox
                            anchors.left: inputLabel.right
                            anchors.leftMargin: 2
                            anchors.top: parent.top 
                            anchors.topMargin: 4 
                            placeholderText: model.modelData 
                            implicitWidth: 60 
                            implicitHeight: 20
                            font.pixelSize: 12
                            placeholderTextColor: Colors.light4
                            leftPadding: 2
                            color: Colors.light1 
                            Rectangle {
                                z: -1
                                anchors.fill: parent
                                color: Colors.dark1
                            }
                            onEditingFinished: {
                                // 
                            }
                        }
                    }
                }
            }
        }
    }
}
