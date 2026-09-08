import QtQuick 2.15
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.15

import Model 1.0 as Model
import "../../Colors.js" as Colors

import "." as InspectorRow


// row
Rectangle {   //container
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
    property var defaultValue: ""
    property var path: undefined 
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
        visible: hoverHandler.hovered && issueMessage !== ""
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
        elementWidth: root.width + 10 
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
        anchors.top: parent.top
        anchors.topMargin: 8
        enabled: (stringValue !== undefined) 
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
        anchors.right: heightRow === 30 ? iconAction.left : mainRow.right
        anchors.rightMargin: heightRow === 30 ? 10 : 30
        readOnly: Model.Operations.getSetterOperation(operations) ? false : true
        placeholderText: stringValue === undefined ? "Write name here" : ""
        text: stringValue === undefined ? "" : stringValue 
        implicitHeight: 20
        implicitWidth: 350
        font.pixelSize: 12
        placeholderTextColor: Colors.light4
        autoScroll: false 
        leftPadding: 2
        color: Colors.light1 
        Rectangle {
            z: -1
            anchors.fill: parent
            color: Colors.dark1
        }
        onEditingFinished: {
            // We need to do deep copies of the arrays. this is done with parse/stringifiy
            let oldValue = stringValue;
            let parameterValue = text;
            let parameters = {};
            if (oldValue !== parameterValue) {
                parameters["value"] = text;
                parameters["path"] = path;
                const operationName = Model.Operations.getSetterOperation(operations);
                Model.Operations.execute(operationName, parameters).catch((message)=> {
                    root.undo();
                    root.reject();
                    errorMessage.show(message, 3000);
                });
            }
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
