import QtQuick 2.14
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.15

import "../Colors.js" as Colors

Rectangle {
    id: mainRect
    width: 470
    height: 495
    visible: true
    color: Colors.dark2

    signal close()

    ColumnLayout {
        anchors.left: parent.left;
        anchors.leftMargin: 50
        anchors.right: parent.right;
        anchors.rightMargin: 50
        anchors.top: parent.top;
        anchors.topMargin: 40
        anchors.bottom: parent.bottom;
        anchors.bottomMargin: 80

        Text {
            padding: 0
            text: appName + " " + "Copyright © since 2019 UX3D GmbH. \nAll rights reserved"
            color: Colors.light1
        }

        Text {
            padding: 0
            text: '<a href=\"' + uri + '\""><font color="' + Colors.darkgreen + '">' + uriText + '</font></a>'
            color: Colors.light1
            onLinkActivated: Qt.openUrlExternally(link)
        }

        Item {
            Layout.fillWidth: true
        }

        Text {
            Layout.preferredWidth: parent.width
            padding: 0
            wrapMode: Text.WordWrap
            text: "This Program relies on the dynamically linked Qt library version 6.3.1, which is published under the Lesser GNU Public License v3 (LGPL v3).\nView the LGPLv3 and GPLv3 licenses at:"
            color: Colors.light1
            onLinkActivated: Qt.openUrlExternally(link)
        }

        Text {
            Layout.preferredWidth: parent.width
            padding: 0
            wrapMode: Text.WordWrap
            text: qtLicensePath
            color: Colors.light1
            onLinkActivated: Qt.openUrlExternally(link)
        }

        Item {
            Layout.fillWidth: true
        }

        Text {
            Layout.preferredWidth: parent.width
            padding: 0
            bottomPadding: 20
            wrapMode: Text.WordWrap
            text: "Source code and build instructions are available in this repository. See the project README for setup and licensing details."
            color: Colors.light1
            onLinkActivated: Qt.openUrlExternally(link)
        }

        Image {
            mipmap: true
            sourceSize.width: 140
            sourceSize.height: 50
            Layout.alignment: Qt.AlignLeft
            source: "qrc:/icons/3DCommerce_Certified.png"
        }
    }

    Button {
        id: closeButton
        text: "Close"
        onClicked: mainRect.close()
        anchors.right: parent.right;
        anchors.rightMargin: 58
        anchors.bottom: parent.bottom;
        anchors.bottomMargin: 22
        flat: true
        background: Rectangle {
            width: parent.width
            height: parent.height
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
