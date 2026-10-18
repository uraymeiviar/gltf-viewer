import QtQuick
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.15

import "../../Colors.js" as Colors

import Model 1.0 as Model
import "." as InspectorRow


// row 
Rectangle {
    id: mainRow
    color: Colors.getInspectorRowColor(depth, isHovered)
    implicitWidth: columnWidthInt
    width: columnWidthInt
    implicitHeight: depth === 0 ? 26 : 22 
    height: depth === 0 ? 26 : 22 

    property var severity: -1
    property var issueMessage: ""
    property var tooltipColor: Colors.light3
    property var issueIcon: ""
    
    // imported for each row 
    property var infos: undefined
    property string title: ""
    property string path: ""
    property string extensionLabel: ""
    property var operations: undefined
    property var objValue: undefined
    property bool isExpandedFromModel: false

    // var used for checking the type (for the height)
    property var objType: true

    // filled automatically by treeview
    required property TreeView treeView
    required property bool expanded
    required property int hasChildren
    required property int depth

    property var isHovered: mouseArea.containsMouse && mouseArea.enabled && !mouseArea.containsPress
    onIsHoveredChanged: {
        mainRow.color = Colors.getInspectorRowColor(depth, isHovered);
    }

    onIsExpandedFromModelChanged: {
        if(isExpandedFromModel !== expanded) {
            treeView.toggleExpanded(row);
        }
    }

    MouseArea {  // mouse area to expand-collapse the row
        id: mouseArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 25
        hoverEnabled: true
        enabled: hasChildren > 0 ? true : false 
        onClicked: {
            // object rows will expand to show their children 
            treeView.toggleExpanded(row);
            // do this after toggle expand so that expanded has the correct value
            control.model.setIsRowExpanded(expanded, modelIndex(row, column));
            control.forceLayout();
        }
        onEntered: {
            mouseArea.cursorShape = Qt.PointingHandCursor;
        }
        onExited: {
            mouseArea.cursorShape = Qt.ArrowCursor;
        }

        Image {
            id: collapsibleIcon
            width: 15
            height: 15
            anchors.left: parent.left
            anchors.leftMargin: 5 * (depth + 1)
            anchors.top: parent.top
            anchors.topMargin: depth === 0 ? 6 : 3
            source: expanded ? "qrc:icons/Navigation_down_20px.svg" : "qrc:icons/Icon_right_16px.svg"
            sourceSize.height: 25
            sourceSize.width: 25
            opacity:  0.7
            visible: hasChildren > 0 ? true : false
        }

        Text {  // label 
            id: rootTitle
            anchors.top: parent.top
            anchors.topMargin: depth === 0 ? 5 : 2
            anchors.left: collapsibleIcon.visible ? collapsibleIcon.right : parent.left
            anchors.leftMargin: collapsibleIcon.visible ? 2 : 10
            text: infos["additionalValue"] ? title : displayLabel(title)
            font.pixelSize: 11
            font.weight: objValue !== undefined ? Font.DemiBold : Font.Normal
            color: objValue !== undefined ? Colors.light3 : Colors.light4
            elide: Text.ElideRight
        }

        function isKhronosExtension() {
            return extensionLabel.length > 0 && extensionLabel === "KHR"
        }

        Image {  // icon extensions
            id: iconExtension
            source: "qrc:/icons/Icon_Label_KHR.svg"
            visible: mouseArea.isKhronosExtension() ? true : false 
            sourceSize.width: 22
            sourceSize.height: 12
            anchors.left: rootTitle.right
            anchors.leftMargin: 5
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 6
        }

        Image {
            id: validationIcon
            width: 15
            height: 15
            anchors.left: iconExtension.right
            anchors.leftMargin: 7
            anchors.verticalCenter: rootTitle.verticalCenter
            source: issueIcon
            sourceSize.height: 25
            sourceSize.width: 25
            visible: source !== ""
        }

        InspectorRow.IndicatorToolTip {
            visible: mouseArea.containsMouse && severity !== -1
            issueString: issueMessage
            offset: validationIcon.x
            toolTipColor: tooltipColor
        }

        Button {  // reset action 
            id: iconAction
            flat: true
            icon.source: objValue !== undefined ? "qrc:icons/Icon_trash.svg" : "qrc:icons/Icon_Add.svg" 
            visible: (Model.Operations.getResetOperation(operations) !== undefined && objValue !== undefined) || (operations["create_gltf_object"] !== undefined && objValue === undefined)
            padding: 0
            topInset: 0
            bottomInset: 0
            implicitWidth: 16
            implicitHeight: 16
            opacity: 0.7
            anchors.right: parent.right
            anchors.rightMargin: 5
            anchors.bottom: parent.bottom
            anchors.bottomMargin: depth === 0 ? 5 : 2
            enabled: visible
            //icon.color: enabled ? Colors.light1 : Colors.light4
            hoverEnabled: true
            onClicked: {
                let parameters = {};
                parameters["path"] = path;
                if (objValue !== undefined) {
                    const operationName = Model.Operations.getResetOperation(operations);
                    Model.Operations.execute(operationName, parameters).catch((message) => {
                        iconAction.reject();
                        errorMessage.show(message, 3000);
                    });
                } else {
                    Model.Operations.execute("create_gltf_object", parameters).catch((message) => {
                        iconAction.reject();
                        errorMessage.show(message, 3000);
                    });
                }
            }
            background: Rectangle {
                anchors.fill: parent
                radius: 2
                color: iconAction.hovered ? Colors.dark4 : "transparent"
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

}
