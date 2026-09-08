import QtQuick 2.15
import QtQuick.Layouts
import QtQuick.Controls
import QtQml
import Qt.labs.qmlmodels
import "../Colors.js" as Colors

import "." as Popups
import "../Components/inspectorTypes" as InspectorComponents

import Model 1.0 as Model

Popups.PopupTemplate {
    id: popup
    title: "Bake Thickness Map"
    contentComponent: component
    height: 270

    Component {
        id: component

        Rectangle {
            id: bake
            color: Colors.dark1
            anchors.fill: parent

            function accept() {
                let parameters = {
                        width: Number(bakeAoMapWidth.text || bakeAoMapWidth.placeholderText),
                        height: Number(bakeAoMapHeight.text || bakeAoMapHeight.placeholderText),
                        texCoordSet: Number(bakeAoMapTexCoordSet.currentValue),
                        occluderDistance: bakeAoMapOccluderDistance.valueNum / 100,
                        thickness: bakeAoMapThicknessMap.checked,
                    };
                Model.Operations.resolvePromiseToCard(Model.Operations.runOperationOnSelection("bake_ao_map", parameters), "Baking successfull");
            }

            function availableTexCoordSets() {
                let hasSet0 = true;
                let hasSet1 = true;
                for (let selectedElement of Model.Observables.selection) {
                    let attributes = Model.Observables.gltfValue(selectedElement.elementAbsolutPath + "/attributes", {});
                    hasSet0 &= "TEXCOORD_0" in attributes;
                    hasSet1 &= "TEXCOORD_1" in attributes;
                }
                let sets = [];
                if (hasSet0) {
                    sets.push(0);
                }
                if (hasSet1) {
                    sets.push(1);
                }
                return sets;
            }

            ColumnLayout {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 1

                Rectangle {
                    Layout.margins: 0
                    Layout.leftMargin: 5
                    Layout.rightMargin: 5
                    Layout.fillWidth: true
                    height: children[0].height
                    color: Colors.dark2

                    RowLayout {
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right

                        Text {
                            Layout.margins: 5
                            text: "Image Resolution"
                            color:  Colors.light1
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Text {
                            Layout.margins: 5
                            text: "W"
                            font.pixelSize: 11
                            color:  Colors.light4
                        }

                        TextField {
                            id: bakeAoMapWidth
                            Layout.margins: 5
                            implicitWidth: 60
                            font.pixelSize: 12
                            color: Colors.light1
                            placeholderText: "512"
                            placeholderTextColor: Colors.light4
                            background: Rectangle { color: Colors.dark1 }
                        }

                        Text {
                            Layout.margins: 5
                            text: "H"
                            font.pixelSize: 11
                            color:  Colors.light4
                        }

                        TextField {
                            id: bakeAoMapHeight
                            Layout.margins: 5
                            implicitWidth: 60
                            font.pixelSize: 12
                            color: Colors.light1
                            placeholderText: "512"
                            placeholderTextColor: Colors.light4
                            background: Rectangle { color: Colors.dark1 }
                        }
                    }
                }

                Rectangle {
                    Layout.margins: 0
                    Layout.leftMargin: 5
                    Layout.rightMargin: 5
                    Layout.fillWidth: true
                    height: children[0].height
                    color: Colors.dark2

                    RowLayout {
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right

                        Text {
                            Layout.margins: 5
                            text: "TexCoord Set"
                            color:  Colors.light1
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        ComboBox {
                            id: bakeAoMapTexCoordSet
                            Layout.margins: 5
                            implicitWidth: 200
                            implicitHeight: 21
                            flat: true
                            indicator: Image {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                width: 20
                                height: 20
                                source: "qrc:icons/Icon_down_20px.svg"
                            }
                            font.pixelSize: 12
                            background: Rectangle {
                                anchors.fill: parent
                                color: Colors.dark4
                            }
                            model: bake.availableTexCoordSets()
                        }
                    }
                }

                Rectangle {
                    Layout.margins: 0
                    Layout.leftMargin: 5
                    Layout.rightMargin: 5
                    Layout.fillWidth: true
                    height: children[0].height
                    color: Colors.dark2

                    RowLayout {
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right

                        Text {
                            Layout.margins: 5
                            text: "Thickness Map"
                            color:  Colors.light1
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        CheckBox {
                            id: bakeAoMapThicknessMap
                            Layout.margins: 0
                            topPadding: 0
                            bottomPadding: 0
                            checked: false
                        }
                    }
                }

                InspectorComponents.SliderPopupRow {
                    id: bakeAoMapOccluderDistance
                    Layout.margins: 0
                    Layout.leftMargin: 5
                    Layout.rightMargin: 5
                    Layout.fillWidth: true
                    labelTitle: "Occluder Distance"
                    isPercentage: true
                    valueNum: 20
                }

                Rectangle {
                    Layout.margins: 0
                    Layout.leftMargin: 5
                    Layout.rightMargin: 5
                    Layout.fillWidth: true
                    height: children[0].height
                    color: Colors.dark2

                    RowLayout {
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right

                        Text {
                            Layout.margins: 5
                            text: "Attach to Material"
                            color:  Colors.light1
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        CheckBox {
                            id: bakeAoMapAttachToMaterial
                            Layout.margins: 0
                            topPadding: 0
                            bottomPadding: 0
                            checked: true
                        }
                    }
                }
            }
        }
    }
}
