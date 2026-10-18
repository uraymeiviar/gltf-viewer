import QtQuick 2.15
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.15

import Model 1.0 as Model
import "../../Colors.js" as Colors

import "." as InspectorRow


// row
Rectangle {   //container
    id: mainRow
    color: Colors.dark2
    implicitWidth: control.columnWidthInt
    width: control.columnWidthInt
    implicitHeight: 30
    height: 30

    property var severity: -1
    property var issueMessage: ""
    property var tooltipColor: Colors.light3
    property var issueIcon: ""

    // imported for each row 
    property var infos: undefined 
    property var operations: []   // operations of the row
    property var numberValue: undefined  // value displayed 
    property var defaultValue: undefined
    property var path: undefined 
    property string title: ""

    // filled automatically by treeview
    required property int depth

    Text {  // label 
        id: rootTitle
        anchors.top: parent.top
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.leftMargin: 5 * (depth + 1)
        text: infos["additionalValue"] ? title : control.displayLabel(title)
        font.pixelSize: 11
        color: Colors.light5
        elide: Text.ElideRight
    }

    HoverHandler { id: hoverHandler}

    Image {
        id: validationIcon
        width: 15
        height: 15
        anchors.left: rootTitle.right
        anchors.leftMargin: 7
        anchors.verticalCenter: rootTitle.verticalCenter
        source: issueIcon
        sourceSize.height: 25
        sourceSize.width: 25
        visible: source !== ""
    }

    InspectorRow.IndicatorToolTip {
        visible: hoverHandler.hovered && severity !== -1
        issueString: issueMessage
        offset: validationIcon.x
        toolTipColor: tooltipColor
    }

    Button {  // action reset
        id: iconAction
        flat: true
        icon.source: "qrc:/icons/Icon_Override.svg" 
        visible: Model.Operations.getResetOperation(operations) ? true : false
        padding: 0
        topInset: 0
        bottomInset: 0
        implicitWidth: 16
        implicitHeight: 16
        opacity: 0.7
        anchors.right: parent.right
        anchors.rightMargin: 5
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        enabled: (numberValue !== undefined) 
        icon.color: enabled ? Colors.light1 : Colors.light4
        hoverEnabled: true
        onClicked: {
            let parameters = {};
            parameters["path"] = path;
            const operationName = Model.Operations.getResetOperation(operations);
            Model.Operations.execute(operationName, parameters).catch((message) => {
                iconAction.reject();
                errorMessage.show(message, 3000);
            });
        }
        background: Rectangle {
            anchors.fill: parent
            radius: 2
            color: iconAction.hovered ? Colors.dark4 : "transparent"
        }
    }

    TextField {  // actual input
        id: root
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 5
        anchors.right: iconAction.left
        anchors.rightMargin: 10
        leftPadding: 2
        implicitHeight: 20
        implicitWidth: 36
        width: 70
        font.pixelSize: 12
        color: Colors.light1 
        placeholderTextColor: Colors.light4
        autoScroll: false
        readOnly: Model.Operations.getSetterOperation(operations) ? false : true
        // values: if default value it will be a placeholder, otherwise if a value is set it will be shown 
        placeholderText: numberValue === undefined ? defaultValue : ""
        text: {
            let locale = Qt.locale()
            let precision = 0 
            let text = numberValue === undefined ? "" : JSON.stringify(numberValue)
            if (text.includes(".")) {
                precision = text.slice(text.indexOf(".") + 1).length
                text = Number(numberValue).toLocaleString(locale, "f", precision)
            }
            return text
        } 
        validator: DoubleValidator{}
        Rectangle {
            z: -1
            anchors.fill: parent
            color: Colors.dark1
        }
        onEditingFinished: {
            if (defaultValue === undefined && text === "") {
                return;
            }
            // We need to do deep copies of the arrays. this is done with parse/stringifiy
            let locale = Qt.locale()
            let parameterValue = text === "" ? defaultValue : Number.fromLocaleString(locale, text);
            let parameters = {};
            if (numberValue !== parameterValue) {
                parameters["value"] = parameterValue;
                parameters["path"] = path;
                const operationName = Model.Operations.getSetterOperation(operations);
                Model.Operations.execute(operationName, parameters).catch((message) => {
                    root.undo();
                    root.reject();
                    errorMessage.show(message, 3000);
                });
            }
        }
    }
    ToolTip { // tooltip input text
        id: textTip 
        visible: root.hovered && (root.text !== undefined && root.text.length > 6) && severity === -1
        y: -26
        height: 25
        timeout: 2500
        delay: 1000
        padding: 4
        Label {  
            text: textTip.text
            font.pixelSize: 12
            color: Colors.dark2
        }
        background: Rectangle {
            anchors.fill: parent
            color: Colors.light3
            radius: 5
        }
    }
    ToolTip { // tooltip input text
        id: errorMessage
        parent: root
        contentItem: Text {
            text: errorMessage.text
            font: errorMessage.font
            color: Colors.dark4
            wrapMode: Text.WordWrap
        }
        background: Rectangle {
            anchors.fill: parent
            color: Colors.redlight1
            radius: 5
        }
    }

}
