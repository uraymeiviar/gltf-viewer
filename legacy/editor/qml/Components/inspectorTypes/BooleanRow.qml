import QtQuick 2.15
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.15

import Model 1.0 as Model
import "../../Colors.js" as Colors

import "." as InspectorRow

Rectangle {
    implicitWidth: control.columnWidthInt
    width: control.columnWidthInt
    implicitHeight: 30
    height: 30

    property var severity: -1
    property var issueMessage: ""
    property var tooltipColor: Colors.light3
    property var issueIcon: ""
    property string title: ""

    property var infos

    // filled automatically by treeview
    required property int depth


    Text {  // label 
        id: rootTitle
        anchors.top: parent.top
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.leftMargin: 5 * (depth + 1)
        text: infos["additionalValue"] ? title : displayLabel(title)
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

    CheckBox {
        id: doubleSidedCheck
        anchors.right: iconAction.left
        anchors.verticalCenter: parent.verticalCenter
        checked: infos.value || infos.default
        enabled: Model.Operations.getSetterOperation(infos.operations) ? true : false
        onToggled: {
            let parameters = {};
            parameters["value"] = checked;
            parameters["path"] = infos.path;
            const operationName = Model.Operations.getSetterOperation(infos.operations);
            Model.Operations.execute(operationName, parameters).catch((message) => {
                checked = !checked;
                doubleSidedCheck.reject();
                errorMessage.show(message, 3000);
            });
        }
    }
    Button {  // reset action 
        id: iconAction
        flat: true
        icon.source: "qrc:/icons/Icon_Override.svg" 
        visible: Model.Operations.getResetOperation(infos.operations) ? true : false
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
        enabled: infos.value !== undefined 
        icon.color: enabled ? Colors.light1 : Colors.light4
        hoverEnabled: true
        onClicked: {
            let parameters = {};
            parameters["path"] = infos.path;
            const operationName = Model.Operations.getResetOperation(infos.operations);
            Model.Operations.execute(operationName, parameters).catch((message) => {
                iconAction.reject();
                errorMessage.show(message, 3000);
            });
        }
        background: Rectangle {
            anchors.fill: parent
            color: "transparent"
        }
    }

    ToolTip { // tooltip input text
        id: errorMessage
        parent: iconAction
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