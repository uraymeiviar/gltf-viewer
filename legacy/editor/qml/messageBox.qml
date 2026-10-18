import QtQuick 2.14
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.15

import "./Colors.js" as Colors

Rectangle {
    id: mainRect
    width: 700
    height: 450
    color: Colors.dark2

    Text {
        width: textRect.width
        wrapMode: Text.WordWrap
        color: Colors.light3
        text: messageBox.text
        textFormat: Text.PlainText
        anchors.left: parent.left;
        anchors.leftMargin: 30
        anchors.top: parent.top;
        anchors.topMargin: 10
    }

    Rectangle {
        id: textRect
        color: Colors.dark1
        anchors.left: parent.left;
        anchors.leftMargin: 30
        anchors.right: parent.right;
        anchors.rightMargin: 30
        anchors.top: parent.top;
        anchors.topMargin: 40
        anchors.bottom: parent.bottom;
        anchors.bottomMargin: 80

        ScrollView {
            id: scrollView
            Layout.alignment: Qt.AlignTop
            ScrollBar.vertical.policy: ScrollBar.AlwaysOn
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            width: parent.width
            contentWidth: width
            anchors.fill: parent
            clip: true;

            TextEdit {
                width: textRect.width - scrollView.ScrollBar.vertical.width
                wrapMode: Text.WordWrap
                color: Colors.light3
                leftPadding: 5
                bottomPadding: 5
                topPadding: 5
                textFormat: Text.PlainText
                text: messageBox.detailedText
                readOnly: true
                selectByMouse:true
            }
        }
    }

    Button {
        id: closeButton
        text: "Continue"
        onClicked: {
            messageBox.closeMessageBox();
            messageBox.wasAccepted();
        }
        anchors.right: parent.right;
        anchors.rightMargin: 58
        anchors.bottom: parent.bottom;
        anchors.bottomMargin: 22
        flat: true
        background: Rectangle {
            anchors.fill: parent
            color: closeButton.hovered ? Colors.dark3 : "transparent"
        }

        contentItem: Text {
            text: closeButton.text
            color: Colors.light1
            font.pointSize: 12
            font.weight: Font.DemiBold
        }
    }

    Button {
        visible: messageBox.askUser
        id: cancelButton
        text: "Cancel"
        onClicked: {
            messageBox.closeMessageBox();
        }
        anchors.right: closeButton.left;
        anchors.rightMargin: 20
        anchors.bottom: parent.bottom;
        anchors.bottomMargin: 22
        flat: true
        background: Rectangle {
            anchors.fill: parent
            color: cancelButton.hovered ? Colors.dark3 : "transparent"
        }

        contentItem: Text {
            text: cancelButton.text
            color: Colors.light1
            font.pointSize: 12
            font.weight: Font.DemiBold
        }
    }
}
