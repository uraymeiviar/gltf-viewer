import QtQuick 2.14
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.15

import "../Colors.js" as Colors

Rectangle {
    id: mainRect
    width: 700
    height: 450
    visible: true
    color: Colors.dark2

    signal close()

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

            Text {
                width: textRect.width - scrollView.ScrollBar.vertical.width
                wrapMode: Text.WordWrap
                color: Colors.light3
                leftPadding: 5
                bottomPadding: 5
                topPadding: 5
                textFormat: Text.RichText
                onLinkActivated: Qt.openUrlExternally(link)
                text: `<html font-weight=\"200\" style="word-wrap: break-word">
                    <h1><font color=\"` + Colors.light1 + `\">MIT License</font></h1>
                    <p>Copyright (c) 2022-2026 UX3D GmbH</p>
                    <p>Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:</p>
                    <p>The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.</p>
                    <p>THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.</p>
                `
            }
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
            color: closeButtonHoverHandler.hovered ? Colors.dark3 : "transparent"
        }

        contentItem: Text {
            text: closeButton.text
            color: Colors.light1
            font.pointSize: 12
            font.weight: Font.DemiBold
        }

        HoverHandler {
            id: closeButtonHoverHandler
        }
    }
}
