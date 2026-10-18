import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.11
import QtQml.Models 2.2

import "./Components" as Components
import "Colors.js" as Colors

Rectangle {
    id: mainRect
    color: Colors.dark1

    property ListModel fileStatisticsModel: ListModel {}
    property ListModel gltfStatisticsModel: ListModel {}
    property ListModel sceneStatisticsModel: ListModel {}

    Connections {
        target: statistics

        function onFileStatisticsChanged() {
            fileStatisticsModel.clear();
            let values = statistics.fileStatistics;
            for (let key in values) {
                fileStatisticsModel.append({
                    key: key,
                    value: values[key],
                });
            }
        }

        function onGltfStatisticsChanged() {
            gltfStatisticsModel.clear();
            let values = statistics.gltfStatistics;
            for (let key in values) {
                gltfStatisticsModel.append({
                    key: key,
                    value: values[key],
                });
            }
        }

        function onSceneStatisticsChanged() {
            sceneStatisticsModel.clear();
            let values = statistics.sceneStatistics;
            for (let key in values) {
                sceneStatisticsModel.append({
                    key: key,
                    value: values[key],
                });
            }
        }
    }

    Row {
        id: topRow
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.topMargin: 6
        anchors.bottomMargin: 6
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        spacing: 6

        Button {
            text: "Export"
            icon.source: "qrc:icons/Icon_Export.svg"
            icon.width: 18
            icon.height: 18
            onClicked: function() {
                statistics.saveToCsvFile(convertToCsv());
            }
        }

        Button {
            text: "Copy"
            icon.source: "qrc:icons/Icon_CopyToClipboard.svg"
            icon.width: 18
            icon.height: 18
            onClicked: function() {
                statistics.copyToClipboard(convertToCsv());
            }
        }
    }

    ScrollView {
        anchors.top: topRow.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: 6
        ScrollBar.vertical.policy: ScrollBar.AlwaysOn
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        clip: true

        ColumnLayout {
            id: mainContentLayout
            anchors.fill: parent

            Components.CollapsibleComp {
                collapsibleText: "File"
                ListView {
                    implicitHeight: childrenRect.height
                    interactive: false
                    spacing: 1
                    model: fileStatisticsModel
                    delegate: itemDelegate
                }
            }

            Components.CollapsibleComp {
                collapsibleText: "glTF"
                ListView {
                    implicitHeight: childrenRect.height
                    interactive: false
                    spacing: 1
                    model: gltfStatisticsModel
                    delegate: itemDelegate
                }
            }

            Components.CollapsibleComp {
                collapsibleText: "Scenes"
                ListView {
                    implicitHeight: childrenRect.height
                    interactive: false
                    spacing: 1
                    model: sceneStatisticsModel
                    delegate: Component {
                        Components.CollapsibleComp {
                            collapsibleText: value.key
                            width: mainContentLayout.width
                            leftCustomPadding: 16

                            ListView {
                                implicitHeight: childrenRect.height
                                interactive: false
                                spacing: 1
                                delegate: itemDelegate
                                model: ListModel {
                                    Component.onCompleted: {
                                        for (let key in value.values) {
                                            let innerValue = value.values[key];
                                            this.append({
                                                key: key,
                                                value: innerValue,
                                            });
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: itemDelegate
        Rectangle {
            id: backgoundRect
            color: Colors.dark2
            width: mainContentLayout.width 
            height: 30

            RowLayout {
                height: parent.height 
                id: rowLayout
                width: 200
                anchors.left: parent.left
                anchors.leftMargin: 64
                anchors.right: parent.right
                anchors.rightMargin: 10

                Label {
                    text: key
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Label {
                    text: value
                    elide: Text.ElideRight
                    Layout.alignment: Qt.AlignRight
                }
            }
        }
    }

    function convertToCsv() {
        let csvString = ""

        for (let key in statistics.fileStatistics) {
            let value = statistics.fileStatistics[key];
            csvString += key + ',' + value + '\n';
        }
        
        for (let key in statistics.gltfStatistics) {
            let value = statistics.gltfStatistics[key];
            csvString += key + ',' + value + '\n';
        }
        
        for (let sceneKey in statistics.sceneStatistics) {
            let sceneValue = statistics.sceneStatistics[sceneKey];
            for (let key in sceneValue.values) {
                let value = sceneValue.values[key];
                csvString += sceneValue.key + key + ',' + value + '\n';
            }
        }

        return csvString;
    }
}
