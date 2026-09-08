import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

// this contains the custom predefined colors
import "../Colors.js" as Colors

Rectangle {
    id: mainRect
    color: Colors.dark2
    width: parent.width
    implicitWidth: width
    height: 30
    implicitHeight: height

    default property alias contents: rowLayout.data
    property int leftCustomPadding: 10
    property int rightCustomPadding: 10


    RowLayout {
        id: rowLayout
        anchors.rightMargin: rightCustomPadding
        anchors.leftMargin: leftCustomPadding
        anchors.left: parent.left
        anchors.right: parent.right
        Layout.alignment: Qt.AlignVCenter
        anchors.verticalCenter: parent.verticalCenter
    }
}
