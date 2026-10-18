import QtQuick 2.14
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.15
import QtQml

import "./Colors.js" as Colors
import "./js/GlobalFunctions.js" as GlobalFunctions

Rectangle {
    id: root
    width: 700
    height: message.contentHeight + 30
    color: GlobalFunctions.getValidationTooltipColor("#D9D9D9", severity)
    property var severity: card.severity
    property string message: card.text
    radius: 5

    HoverHandler {
        onHoveredChanged: {
            if (hovered) {
                timer.stop()
            } else {
                timer.start()
            }
        }
    }

    Image {
        id: validationIcon
        width: 15
        height: 15
        anchors.left: root.left
        anchors.verticalCenter: root.verticalCenter
        anchors.leftMargin: 15
        source: root.severity === 2 ? "qrc:/icons/Icon_logger_alert_information_dark.svg" : GlobalFunctions.getValidationIcon(root.severity)
        sourceSize.height: 25
        sourceSize.width: 25
    }

    Text {
        id: message
        anchors.right: closeButton.left
        anchors.left: validationIcon.right
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.topMargin: 15
        anchors.top: parent.top
        text: root.message
        color: root.severity === 4 ? "#FFFFFF" : Colors.dark0
        wrapMode: Text.Wrap
        font.weight: Font.DemiBold
    }

    Button {
        id: closeButton
        anchors.right: parent.right
        anchors.rightMargin: 20
        anchors.verticalCenter: root.verticalCenter
        icon.source: "qrc:/icons/Icon_close_20px_dark.svg"
        icon.width: 30
        icon.height: 30
        icon.color: message.color
        flat: true
        implicitWidth: 20
        implicitHeight: 20
        onClicked: card.closeCard()
        background: Rectangle {
            color: root.color
        }
    }

    Timer {
        id: timer
        interval: 5000
        running: true
        repeat: false
        onTriggered: card.closeCard()
    }

    /*OpacityAnimator {
        target: root;
        from: 0;
        to: 1;
        duration: 1000
        running: true
    }

    OpacityAnimator {
        id: fadeOut
        target: root;
        from: 1;
        to: 0;
        duration: 1000
        running: false
        onFinished: card.closeCard()
    }*/
}
