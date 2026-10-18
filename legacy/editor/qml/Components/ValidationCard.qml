import QtQuick 2.15
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.15

import "../Colors.js" as Colors
import "../js/GlobalFunctions.js" as GlobalFunctions

Rectangle{
    id: root
    color: selected ? Colors.dark3 : hover.hovered ? Colors.dark205 : Colors.dark2
    height: message.contentHeight + path.contentHeight + 20
    radius: 5

    property var validationData: undefined
    property var severity: validationData ? validationData.severity : undefined
    property string message: validationData ? validationData.message : ""
    property string pointer: validationData ? validationData.pointer : ""
    property bool selected: ListView.isCurrentItem
    property var parentView: undefined

    MouseArea {
        anchors.fill: parent
        onClicked: {
            applicationData.clearAndSetSelectionPath("/gltf" + pointer)
            root.parentView.currentIndex = index
        }
    }

    HoverHandler {
        id: hover
    }

    Image {
        id: validationIcon
        width: 15
        height: 15
        anchors.left: root.left
        anchors.verticalCenter: root.verticalCenter
        anchors.leftMargin: 7
        source: GlobalFunctions.getValidationIcon(root.severity)
        sourceSize.height: 25
        sourceSize.width: 25
    }
    
    Text {
        id: message
        anchors.right: parent.right
        anchors.left: validationIcon.right
        anchors.leftMargin: 10
        anchors.topMargin: 10
        anchors.top: parent.top
        text: root.message
        color: Colors.light5
        wrapMode: Text.Wrap
    }

    Text {
        id: path
        anchors.top: message.bottom
        anchors.right: parent.right
        anchors.left: validationIcon.right
        anchors.leftMargin: 10
        text: root.pointer
        color: Colors.light5
        wrapMode: Text.Wrap
        font.italic: true
        font.pointSize: 7
    }
}
