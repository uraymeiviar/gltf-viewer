import QtQuick 2.15
import QtQuick.Controls
import QtQml
import "../Colors.js" as Colors

import "." as Popups
import "../Components/inspectorTypes" as InspectorComponents

import Model 1.0 as Model

Popups.PopupTemplate {
    id: popup
    title: "Convert Storage Type"
    contentComponent: component
    height: 120 + 30
    
    Component {
        id: component
    
        Rectangle {
            color: Colors.dark1
            anchors.fill: parent
            
            property var storageType: "File"

            function accept(){
                let parameters = {};
                parameters["storageType"] = storageType;
                Model.Operations.resolvePromiseToCard(Model.Operations.runOperationOnSelection("convert_storage_type", parameters))
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right 
                anchors.leftMargin: 5
                anchors.rightMargin: 5
                implicitWidth: parent.width - 10 
                implicitHeight: 30
                color: Colors.dark2 
                Text {
                    text: "Storage Type"
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 5
                    color: Colors.light1 
                }
                ComboBox {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 5
                    model: Model.Observables.selection[0]["elementAbsolutPath"].includes("images") ? ["File", "BufferView", "Embedded"] : ["File", "Embedded"] // differentiate between images and buffers
                    width: 180
                    flat: true
                    implicitHeight: 21
                    topPadding: -4 
                    bottomPadding: -4 
                    indicator: Image {
                        x: 160
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
                        storageType = model[currentIndex]
                    }
                    hoverEnabled: true
                }
            }
        }
    }
}