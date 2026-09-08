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
    title: "Reduce Triangle Count"
    contentComponent: component
    height: 217
    
    Component {
        id: component
        Rectangle {
            id: reduce
            color: Colors.dark1
            property var toleranceCurrentValue: "Low"
            anchors.fill: parent

            function accept() {
                let parameters = {};
                parameters["targetSize"] = reducePercentage.valueNum / 100
                let level = reduce.toleranceCurrentValue
                parameters["quality"] = level === "Low" ? 20 : level === "Medium" ? 7 : 2;

                Model.Operations.resolvePromiseToCard(Model.Operations.runOperationOnSelection("reduce_triangle_count", parameters))
            }


            Rectangle {
                id: reduceToleranceEntry
                color: Colors.dark2
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 5
                anchors.rightMargin: 5
                height: 30

                Text {
                    text: "Tolerance"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 5
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    horizontalAlignment: Text.AlignLeft
                    color: Colors.light1 
                }

                ComboBox {
                    id: reduceTolerance
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 5
                    model: ["Low", "Medium", "High"]
                    width: 170
                    flat: true
                    topPadding: -4
                    bottomPadding: -4
                    height: 20
                    indicator: Image {
                        anchors.right: parent.right
                        y: 1
                        width: 20
                        height: 20
                        source: "qrc:icons/Icon_down_20px.svg"
                    }
                    background: Rectangle {
                        anchors.fill: parent
                        color: Colors.dark4
                    }
                    onActivated: {
                        reduce.toleranceCurrentValue = model[currentIndex]
                    }
                }
            }

            InspectorComponents.SliderPopupRow {
                id: reducePercentage
                anchors.top: reduceToleranceEntry.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 5
                anchors.rightMargin: 5
                anchors.topMargin: 1

                labelTitle: "Target Percentage"
                isPercentage: true
                valueNum: 90
            }
        }
    }
}