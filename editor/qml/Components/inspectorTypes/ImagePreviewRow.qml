import QtQuick 2.15
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.15

import "../../Colors.js" as Colors
import "../../js/GlobalFunctions.js" as GlobalFunctions;
import Model 1.0 as Model

import "." as InspectorRow

Rectangle {
    id: mainRow
    color:  Colors.dark3
    width: parent.width

    property var infos: undefined

    // filled automatically by treeview
    required property int depth

    // dynamic height 
    property int heightRow: childrenRect.height + 10

    property int waitCounter: 0

    property var imageMetadata: Model.Observables.imageMetadata
    property var selection: Model.Observables.selection


    Text {  // label 
        id: rootTitle
        anchors.top: parent.top
        anchors.topMargin: 5
        anchors.left: parent.left
        anchors.leftMargin: 5 * (depth + 1)
        text: control.displayLabel(infos.title)
        font.pixelSize: 11
        color: Colors.light5
        elide: Text.ElideRight
    }
    
    Timer {
        id: fetchImage
        interval: 50
        onTriggered: {
            mainRow.waitCounter += fetchImage.interval
            fetchImage.interval = 500
            image.source = "image://provider/" + infos.value.source
        }
    }

    BusyIndicator {
        id: busy
        x: parent.width / 2 - width / 2
        y: parent.height / 2 - height / 2
        running: image.status === Image.Loading
        height: 35
        width: 35
    }

    Image {
        id: image
        anchors.top: rootTitle.bottom
        anchors.topMargin: 5
        source: getSource(infos.value)
        fillMode: Image.PreserveAspectFit
        width: parent.width
        sourceSize.width: getWidth(source, width)
        cache: false

        function getSource(value) {
            if(value === undefined || value.source === undefined) {
                return ""
            }

            return "image://provider/" + value.source
        }

        function getWidth(source, width) {
            if (source === undefined) {
                return 0
            }
            // for some reason this can be a string
            width = parseInt(width);
            return source == "qrc:/icons/image_not_available.svg" ? width / 2 : width

        }

        onStatusChanged: {
            if (status == Image.Error) {
                busy.running = true
                // After one minute stop
                if (mainRow.waitCounter < 60000){
                    fetchImage.restart()
                } else {
                    source = "qrc:/icons/image_not_available.svg"
                }
            }
            if (status == Image.Loading) {
                busy.running = true
            }
            if (status == Image.Ready) {
                busy.running = false
                forceLayout()
            }
        }
    }

    Text {
        id: size
        anchors.top: image.bottom
        anchors.topMargin: 5
        anchors.right: parent.right
        anchors.rightMargin: 5 * (depth + 1)
        text: getSize(infos.value)
        font.pixelSize: 11
        color: Colors.light5
        function getSize(value) {
            if(value === undefined || value.size === undefined) {
                return ""
            }
            return GlobalFunctions.prettyPrintByteLength(value.size)
        }
    }

    Text {
        id: width
        anchors.top: size.bottom
        anchors.topMargin: 5
        anchors.right: parent.right
        anchors.rightMargin: 5 * (depth + 1)
        text: getDimensions(imageMetadata, selection)
        font.pixelSize: 11
        color: Colors.light5
        function getDimensions(imageMetadata, selection) {
            if(selection.length === 0 || !imageMetadata) {
                return ""
            }
            let uuid = selection[0].uuid;
            if(!(uuid in imageMetadata)) {
                return ""
            }
            if (imageMetadata[uuid].width && imageMetadata[uuid].height) {
                return "Dimensions " + imageMetadata[uuid].width + "x" + imageMetadata[uuid].height
            } else {
                return ""
            }
        }
    }

}
