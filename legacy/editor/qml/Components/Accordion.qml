import QtQuick 2.15
import QtQuick.Controls 2.15

Column {
    width: parent.width
    height: parent.height

    property alias model: columnRepeater.model

    ListView {
        id: columnRepeater
        delegate: accordion

        width: parent.width
        height: parent.height

        model: ListModel { }

        ScrollBar.vertical: ScrollBar { }
    }

    Component {
        id: accordion
        Column {
            width: parent.width

            Item {
                id: infoRow

                width: parent.width
                height: childrenRect.height
                property bool expanded: false

                MouseArea {
                    anchors.fill: parent
                    onClicked: infoRow.expanded = !infoRow.expanded
                    enabled: childrens ? true : false
                }

                Image {
                    id: carot

                    anchors {
                        top: parent.top
                        left: parent.left
                        margins: 5
                    }

                    sourceSize.width: 16
                    sourceSize.height: 16
                    source: 'images/triangle.svg'
                    visible: childrens.count > 0 ? true : false
                    transform: Rotation {
                        origin.x: 5
                        origin.y: 10
                        angle: infoRow.expanded ? 90 : 0
                        Behavior on angle { NumberAnimation { duration: 150 } }
                    }
                }

                Text {
                    anchors {
                        left: carot.visible ? carot.right : parent.left
                        top: parent.top
                        margins: 5
                    }

                    visible: parent.visible
                    text: label
                }

                Text {
                    visible: infoRow.visible

                    text: type

                    anchors {
                        top: parent.top
                        right: parent.right
                        margins: 5
                        rightMargin: 15
                    }
                }
            }

            ListView {
                id: subentryColumn
                x: 20
                width: parent.width - x
                height: childrenRect.height * opacity
                visible: opacity > 0
                opacity: infoRow.expanded ? 1 : 0
                delegate: accordion
                model: childrens ? childrens : []
                interactive: false
                Behavior on opacity { NumberAnimation { duration: 200 } }
            }
        }
    }
}