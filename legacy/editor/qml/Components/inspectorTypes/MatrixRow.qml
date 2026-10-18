import QtQuick 2.15
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.15

import Model 1.0 as Model
import "../../Colors.js" as Colors

import "." as InspectorRow


// row
Rectangle {   // container
    id: mainRow
    color: Colors.getInspectorRowColor(-1, isHovered)
    implicitWidth: control.columnWidthInt
    width: control.columnWidthInt
    implicitHeight: collapsed ? 30 : expandedHeight
    height: collapsed ? 30 : expandedHeight

    property var severity: -1
    property var issueMessage: ""
    property var tooltipColor: Colors.light3
    property var issueIcon: ""
    
    // imported data
    property var infos: undefined
    property string path: ""
    property var operations: {}   // operations of the list group
    property var matrixArray: []  // 
    property string title: ""

    // filled automatically by treeview
    required property int depth

    // expand-collapse
    property bool isCollapsible: matrixArray !== undefined && matrixArray.length > 0 ? true : false
    property bool collapsed: true 
    property var expandedHeight: 150
    property bool isExpandedFromModel: false

    property var isHovered: mouseArea.containsMouse && mouseArea.enabled && !mouseArea.containsPress
    onIsHoveredChanged: {
        mainRow.color = Colors.getInspectorRowColor(-1, isHovered);
    }

    onIsExpandedFromModelChanged: {
        if(isExpandedFromModel === collapsed) {
            collapsed = !collapsed;
        }
    }

    MouseArea {  // mouse area to expand-collapse the row
        id: mouseArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 25
        hoverEnabled: true
        enabled: isCollapsible 
        onClicked: {
            if (isCollapsible) {
                // array rows will expand to show the values
                collapsed = !collapsed;
                control.model.setIsRowExpanded(!collapsed, modelIndex(row, column));
                control.forceLayout();
            }
        }
        onEntered: {
            if (isCollapsible) {
                mouseArea.cursorShape = Qt.PointingHandCursor;
            }
        }
        onExited: {
            if (isCollapsible) {
                mouseArea.cursorShape = Qt.ArrowCursor;
            }
        }

        Image {
            id: collapsibleIcon
            width: 15
            height: 15
            anchors.left: parent.left
            anchors.leftMargin: 5 * (depth + 1)
            anchors.top: parent.top
            anchors.topMargin: 8
            source: collapsed ? "qrc:icons/Icon_right_16px.svg" : "qrc:icons/Navigation_down_20px.svg" 
            sourceSize.height: 25
            sourceSize.width: 25
            opacity: isCollapsible ? 0.7 : 0.4
        }

        Text {  // label 
            id: rootTitle
            anchors.top: parent.top
            anchors.topMargin: 8
            anchors.left: collapsibleIcon.right
            anchors.leftMargin: 2
            text: control.displayLabel(title)
            font.pixelSize: 11
            color: Colors.light5
            elide: Text.ElideRight
        }

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
            visible: mouseArea.containsMouse && severity !== -1
            issueString: issueMessage
            offset: validationIcon.x
            toolTipColor: tooltipColor
        }

        Text {
            id: labelmatrix
            text: matrixArray !== undefined && matrixArray.length > 0 ? matrixArray.length : 0 
            anchors.top: parent.top
            anchors.topMargin: 8
            anchors.right: parent.right
            anchors.rightMargin: 10
            font.pixelSize: 11
            color: Colors.light1 
            height: 20
        }
    }


    Item {
        id: matrixBox
        visible: !collapsed 
        anchors.top: mouseArea.bottom
        anchors.topMargin: 2
        width: parent.width
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 10
        height: 120 
        Rectangle {  // left side of matrix grid to display "["
            id: gridHeader
            width: 20
            height: parent.height - 14
            color: "transparent"
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.topMargin: 7
            border.width: 1
            border.color: Colors.light4
            Rectangle {  // hiding the right border of the rectangle, therefore the rect has this shape "["
                width: 6
                height: parent.height
                anchors.right: parent.right
                color: mainRow.color
            }
        }
        GridView {  // matrix values
            id: gridMatrix
            height: parent.height 
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.topMargin: 15
            anchors.right: parent.right
            anchors.leftMargin: 80
            anchors.rightMargin: 20
            cellWidth: gridMatrix.width / 4
            cellHeight: 25
            model: matrixArray
            delegate: Text {
                text: model.modelData.toFixed(8).toString().replace(/[.,]0+$/, "");
                color: Colors.light1
                font.pixelSize: 11
            }
        }
        Rectangle {   // right side of matrix grid to display "]"
            id: gridFooter
            width: 20
            height: parent.height - 14
            color: "transparent"
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.topMargin: 7
            border.width: 1
            border.color: Colors.light4
            Rectangle {  // hiding the left border of the rectangle, therefore the rect has this shape "]"
                width: 1
                height: parent.height
                anchors.left: parent.left
                color: mainRow.color
            }
        }
    }

}
