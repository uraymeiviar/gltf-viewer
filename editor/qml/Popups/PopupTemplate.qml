import QtQuick 2.15
import QtQuick.Controls
import "../Colors.js" as Colors
    

Rectangle {  // background
    id: containerPopup
    property string title: ""
    property var contentComponent: undefined
    property var tooltip: undefined
    property var applyText: undefined
    property var cancelText: undefined
    property var showApply: true
    property var showCancel: true
    anchors.fill: parent
    color: Colors.dark1
    Label {   // title row
        id: titlePopUp
        anchors.fill: parent
        anchors.topMargin: 6
        anchors.leftMargin: 10
        anchors.rightMargin: 30
        text: containerPopup.title
        font.pixelSize: 11
        color: Colors.light3
        font.weight: Font.Bold

        MouseArea{
            anchors.fill: parent
            onPressed:(mouse) =>  {
                containerPopup.Window.window.startSystemMove()
            }
        }


    }
    Button {
        id: closeBtn
        implicitWidth: 20 
        implicitHeight: 20
        flat: true
        padding: 0
        topInset: 0
        bottomInset: 0
        anchors.right: parent.right
        anchors.rightMargin: 2
        anchors.top: parent.top
        anchors.topMargin: 4
        icon.source: "qrc:/icons/Icon_Close.svg"
        onClicked: {
            applicationData.closePopup()
        }
        background: Rectangle {
            anchors.fill: parent
            color: "transparent"
        }
    }
    Rectangle {   
        id: contentRow
        color: Colors.dark1
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 25 
        height: containerPopup.height - 30

        Loader {
            id: contentLoader
            sourceComponent: containerPopup.contentComponent
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: containerPopup.showCancel ? cancelBtn.top : parent.bottom
            anchors.bottomMargin: 1
        }

        Rectangle {   // last row background
            id: lastRow 
            visible: containerPopup.tooltip !== undefined ? true : false
            height: 55
            color: Colors.dark2
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 60
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.right: parent.right
            anchors.rightMargin: 10
            border.color: Colors.light4 
            border.width: 1
            radius: 4
            Image {
                id: esclamationPoint
                source: "qrc:/icons/Icon_Alert_Information.svg"
                sourceSize.width: 12
                sourceSize.height: 12
                anchors.left: parent.left
                anchors.leftMargin: 175
                anchors.top: parent.top
                anchors.topMargin: 10
            }
            Label {  // last row left label
                anchors.fill: parent
                anchors.topMargin: 22 
                anchors.leftMargin: 10
                text: containerPopup.tooltip !== undefined ? containerPopup.tooltip : ""
                font.pixelSize: 11
                color: Colors.light4
                wrapMode: Text.WordWrap  // important!
                horizontalAlignment: Text.AlignHCenter
            }
        }

        Button {
            id: cancelBtn
            flat: true
            visible: containerPopup.showCancel
            text: containerPopup.cancelText ?? "Cancel"
            anchors.left: parent.left
            anchors.leftMargin: 185
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 10
            font.pixelSize: 12
            onClicked: {
                if (contentLoader.item.cancel !== undefined){
                    contentLoader.item.cancel();
                }
                applicationData.closePopup()
            }
        }
        Button {
            id: applyBtn
            flat: true
            visible: containerPopup.showApply
            text: containerPopup.applyText ?? "Apply"
            anchors.left: parent.left
            anchors.leftMargin: 265
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 10
            font.pixelSize: 12
            onClicked: {
                let result = contentLoader.item.accept();
                if (result !== undefined){
                    errorMessage.show(result, 3000);
                } else {
                    applicationData.closePopup()
                }
            }

            ToolTip { // tooltip input text
                id: errorMessage
                contentItem: Text {
                    text: errorMessage.text
                    font: errorMessage.font
                    color: Colors.dark4
                    wrapMode: Text.WordWrap
                }
                background: Rectangle {
                    anchors.fill: parent
                    color: Colors.redlight1
                    radius: 5
                }
            }
        }
    }
}
