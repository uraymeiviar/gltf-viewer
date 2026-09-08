import QtQuick 2.15
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.15

import Model 1.0 as Model
import "../../Colors.js" as Colors

import "." as InspectorRow


// row
Rectangle {  // container
    id: mainRow
    color:  Colors.dark2
    implicitWidth: control.columnWidthInt
    width: control.columnWidthInt
    implicitHeight: heightRow
    height: heightRow

    property var severity: -1
    property var issueMessage: ""
    property var tooltipColor: Colors.light3
    property var issueIcon: ""

    // imported for each row  
    property var infos: undefined 
    property var operations: []   // operations of the row
    property var stringValue: ""  // value displayed 
    property string iconObj: ""  // icon displayed 
    property string title: ""

    // filled automatically by treeview
    required property int depth

    // dynamic height 
    property int heightRow: 30

    Text {  // label 
        id: rootTitle
        anchors.top: parent.top
        anchors.topMargin: heightRow === 30 ? 8 : 5  
        anchors.left: parent.left
        anchors.leftMargin: 5 * (depth + 1)
        text: control.displayLabel(title)
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

    Rectangle {
        id: marginLeft
        anchors.left: rootTitle.right
        height: 10
        width: 5
        color: "transparent"
    }

    InspectorRow.Spacer { // space 
        leftItem: marginLeft
        rightItem: iconAction
        elementWidth: root.width + 15 + rootLabel.width 
    }

    Button {  // icon obj
        id: root
        flat: true
        icon.source: {
            iconObj !== "" ? "qrc:icons/Icon_Content_" + iconObj +  ".svg" : ""
        } 
        visible: iconObj !== "" 
        padding: 0
        topInset: 0
        bottomInset: 0
        implicitWidth: 16
        implicitHeight: 16
        opacity: 0.7
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        anchors.left: heightRow === 30 ? marginLeft.left : mainRow.left
        anchors.leftMargin: 5
        icon.color: Colors.green
        enabled: iconObj !== "" 
        hoverEnabled: true
        onClicked: {
            // 
        }
        background: Rectangle {
            anchors.fill: parent
            radius: 2
            color: root.hovered ? Colors.dark4 : "transparent"
        }
    }

    Text {  // label 
        id: rootLabel
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 7
        anchors.right: iconAction.left
        anchors.rightMargin: 10
        text: stringValue + ""
        font.pixelSize: 11
        color: Colors.light1
        elide: Text.ElideRight
    }

    Button {  // icon reset
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
        icon.color: enabled ? Colors.light1 : Colors.light4
        //enabled: true
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
}