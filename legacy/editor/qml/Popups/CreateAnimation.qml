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
    title: "Create Animation"
    contentComponent: component
    height: 270

    Component {
        id: component

        Rectangle {
            id: create
            color: Colors.dark1
            anchors.fill: parent

            function getComboModel(objName, path) {
                let comboModel = []
                let arrayPointer = Model.Observables.getAvailablePointers("/nodes", "/gltf/nodes/\\d+")
                let arrayValue = undefined
                let arrayText = undefined

                // if the arrayPointer has found any values, it will look like this: ["/scenes/0", "'Scene'"]
                // the arrayPointer has both the name and the path of the obj, therefore it will be divided in 2 separate arrays
                if (arrayPointer !== undefined && arrayPointer.length > 0) {
                    arrayValue = arrayPointer.filter((el, index) => index % 2 === 0)  // path will be in the value array "/scenes/0"
                    arrayText = arrayPointer.filter((el, index) => index % 2 !== 0)  // name will be in the text array "'Scene'"
                    arrayText = arrayText.map((el, index) => el = el.slice(1, 2) + el.slice(2, el.length -1) + " " + index) // removing quotes from the name "Scene"

                    // fill the combobox array model in this way: [{value: path, text: name}]
                    for (let i=0; i < arrayValue.length; i++) {
                        comboModel.push({
                            text: arrayText[i], 
                            value: arrayValue[i] 
                        })
                    }
                }

                if (comboModel.length === 0) {
                    comboModel.push({
                        text: "None Available"
                    });
                }
                
                // it will be an array of objects : [{value: "/scenes/0", text: "Scene"}] with a value corresponding to the path and a text with the object name to display 
                return comboModel
            }

            function getCurrentIndex() {
                for(let selectedElement of Model.Observables.selection) {
                    if (selectedElement["elementAbsolutPath"].startsWith("/nodes/")){
                        return Number(selectedElement["elementAbsolutPath"].substring(7));
                    }
                }
                return 0;
            }

            function accept() {
                if (target.currentText === "None Available") {
                    return "Target needs to be set"
                }
                let parameters = {
                    path: "/gltf" + target.currentValue,
                    start: Number(offset.text || offset.placeholderText),
                    duration: Number(duration.text || duration.placeholderText),
                    bounce: bounce.checked,
                }
                if (type.currentText === "Rotation") {
                    parameters["rotationAxis"] = [Number(x.text || x.placeholderText), Number(y.text || y.placeholderText), Number(z.text || z.placeholderText)]
                    parameters["degrees"] = Number(degrees.text || degrees.placeholderText) 
                } else {
                    parameters["translation"] = [Number(x.text || x.placeholderText), Number(y.text || y.placeholderText), Number(z.text || z.placeholderText)]
                }
                Model.Operations.resolvePromiseToCard(Model.Operations.execute("create_animation", parameters), "Animation creation successful");
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
                            text: "Type"
                            color:  Colors.light1
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        ComboBox {
                            id: type
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
                            model: ["Rotation", "Translation"]
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
                            text: "Offset"
                            color:  Colors.light1
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        TextField {
                            id: offset
                            Layout.margins: 5
                            implicitWidth: 60
                            font.pixelSize: 12
                            color: Colors.light1
                            placeholderText: "0"
                            placeholderTextColor: Colors.light4
                            background: Rectangle { color: Colors.dark1 }
                            validator: DoubleValidator{ notation: DoubleValidator.StandardNotation}
                        }

                        Text {
                            Layout.margins: 5
                            text: "s"
                            font.pixelSize: 11
                            color:  Colors.light4
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
                            text: "Duration"
                            color:  Colors.light1
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        TextField {
                            id: duration
                            Layout.margins: 5
                            implicitWidth: 60
                            font.pixelSize: 12
                            color: Colors.light1
                            placeholderText: "1"
                            placeholderTextColor: Colors.light4
                            validator: DoubleValidator{ notation: DoubleValidator.StandardNotation}
                            background: Rectangle { color: Colors.dark1 }
                        }

                        Text {
                            Layout.margins: 5
                            text: "s"
                            font.pixelSize: 11
                            color:  Colors.light4
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
                            text: "Bounce"
                            color:  Colors.light1
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        CheckBox {
                            id: bounce
                            Layout.margins: 0
                            topPadding: 0
                            bottomPadding: 0
                            checked: false
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
                    visible: type.currentText === "Rotation"

                    RowLayout {
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right

                        Text {
                            Layout.margins: 5
                            text: "Degrees"
                            color:  Colors.light1
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        TextField {
                            id: degrees
                            Layout.margins: 5
                            implicitWidth: 60
                            font.pixelSize: 12
                            color: Colors.light1
                            placeholderText: "360"
                            placeholderTextColor: Colors.light4
                            background: Rectangle { color: Colors.dark1 }
                            validator: IntValidator{}
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
                            text: type.currentText === "Rotation" ? "Rotation Axis" : "Translation" 
                            color:  Colors.light1
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Text {
                            Layout.margins: 5
                            text: "X"
                            font.pixelSize: 11
                            color:  Colors.light4
                        }

                        TextField {
                            id: x
                            Layout.margins: 5
                            implicitWidth: 60
                            font.pixelSize: 12
                            color: Colors.light1
                            placeholderText: "0"
                            placeholderTextColor: Colors.light4
                            background: Rectangle { color: Colors.dark1 }
                            validator: DoubleValidator{ notation: DoubleValidator.StandardNotation}
                            onEditingFinished: {
                            if (y.text === undefined || y.text === "") { y.text = "0" }
                            if (z.text === undefined || z.text === "") { z.text = "0" }
                            }
                        }

                        Text {
                            Layout.margins: 5
                            text: "Y"
                            font.pixelSize: 11
                            color:  Colors.light4
                        }

                        TextField {
                            id: y
                            Layout.margins: 5
                            implicitWidth: 60
                            font.pixelSize: 12
                            color: Colors.light1
                            placeholderText: "1"
                            placeholderTextColor: Colors.light4
                            background: Rectangle { color: Colors.dark1 }
                            validator: DoubleValidator{ notation: DoubleValidator.StandardNotation}
                            onEditingFinished: {
                            if (x.text === undefined || x.text === "") { x.text = "0" }
                            if (z.text === undefined || z.text === "") { z.text = "0" }
                            }
                        }

                        Text {
                            Layout.margins: 5
                            text: "Z"
                            font.pixelSize: 11
                            color:  Colors.light4
                        }

                        TextField {
                            id: z
                            Layout.margins: 5
                            implicitWidth: 60
                            font.pixelSize: 12
                            color: Colors.light1
                            placeholderText: "0"
                            placeholderTextColor: Colors.light4
                            background: Rectangle { color: Colors.dark1 }
                            validator: DoubleValidator{ notation: DoubleValidator.StandardNotation}
                            onEditingFinished: { 
                            if (y.text === undefined || y.text === "") { y.text = "0" }
                            if (x.text === undefined || x.text === "") { x.text = "0" }
                            }
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
                            text: "Target"
                            color:  Colors.light1
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        ComboBox {
                            id: target
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
                            textRole: "text"
                            valueRole: "value"
                            model: create.getComboModel()
                            currentIndex: create.getCurrentIndex()
                        }
                    }
                }
            }
        }
    }
}
