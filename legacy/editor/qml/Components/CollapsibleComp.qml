import QtQuick 2.15
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.15

// this contains the custom predefined colors
import "../Colors.js" as Colors

ColumnLayout {
    id: root
    spacing: 1

    default property alias contents: collapsibleContainer.data
    property alias collapsibleSideEntry: rowLayout.data
    property string collapsibleText: ""
    property int leftCustomPadding: 0

    Rectangle {
        color: Colors.dark2 
        height: 30
        Layout.fillWidth: true

        RowLayout {
            id: rowLayout
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left;
            anchors.right: parent.right;
            anchors.rightMargin: 10

            Button {
                Layout.leftMargin: leftCustomPadding
                flat: true
                backgroundColor: Colors.dark2
                padding: 0
                horizontalPadding: 0
                verticalPadding: 0
                Layout.preferredWidth: 30
                Layout.preferredHeight: 30
                icon.source: collapsibleContainer.visible ? "qrc:icons/Navigation_down_20px.svg" : "qrc:icons/Icon_right_16px.svg"
                onClicked: {
                    if (collapsibleContainer.visible === false)
                    
                        collapsibleContainer.visible = true
                    else
                        collapsibleContainer.visible = false
                }
            }
            Label {
                text: collapsibleText
            }
            Item {
                Layout.fillWidth: true
            }
        }
    }

    // child items of this component are added here
    ColumnLayout {
        id: collapsibleContainer
        width: root.width
        visible: true
        spacing: root.spacing
    }
}