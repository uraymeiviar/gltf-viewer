import QtQuick 2.14
import QtQuick.Controls 2.15

import "../Colors.js" as Colors

Rectangle {
    id: mainRect
    width: 400
    height: 125
    visible: true
    color: Colors.dark2

    signal close()
    signal accept()

    Text {
        anchors.left: parent.left;
        anchors.leftMargin: 10
        anchors.top: parent.top;
        anchors.topMargin: 10
        wrapMode: Text.WordWrap
        text: "Will you allow UX3D Team to receive anonymous usage \nstatistics for this Gestaltor instance? \nYou can change this setting at any time in the settings."
        color: Colors.light1
    }

    Button {
        id: acceptButton
        text: "Yes"
        onClicked: mainRect.accept()
        anchors.right: closeButton.left;
        anchors.rightMargin: 5
        anchors.bottom: parent.bottom;
        anchors.bottomMargin: 10
        flat: true
        background: Rectangle {
            width: acceptButton.width
            height: acceptButton.height
            color: acceptButton.hovered ? Colors.dark3 : "transparent"
        }

        contentItem: Text {
            text: acceptButton.text
            color: Colors.light1
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }
    }
    Button {
        id: closeButton
        text: "No"
        onClicked: mainRect.close()
        anchors.right: parent.right;
        anchors.rightMargin: 10
        anchors.bottom: parent.bottom;
        anchors.bottomMargin: 10
        flat: true
        background: Rectangle {
            width: closeButton.width
            height: closeButton.height
            color: closeButton.hovered ? Colors.dark3 : "transparent"
        }

        contentItem: Text {
            text: closeButton.text
            color: Colors.light1
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }
    }
}

