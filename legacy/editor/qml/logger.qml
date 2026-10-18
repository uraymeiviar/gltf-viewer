import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Templates 2.15 as T

import "./Components" as Components

import "Colors.js" as Colors


Rectangle {
    id: mainRect
    color: Colors.dark1

    property bool infoVisible: true;
    property bool warnVisible: true;
    property bool errorVisible: true;

    property var prefSelectedItem: null;
    property var selectedText: null;

    property real scrollbarOffset: loggerScrollView.ScrollBar.vertical.width + 1
    property real logFontSize: 12

    ListModel {
        id: logLevels
        ListElement { displayText: "Error"; logLevel: "error"; iconUrl: "qrc:icons/Icon_logger_alert_error.svg"}
        ListElement { displayText: "Warning"; logLevel: "warning"; iconUrl: "qrc:icons/Icon_logger_alert_warning.svg"}
        ListElement { displayText: "Information"; logLevel: "info"; iconUrl: "qrc:icons/Icon_logger_alert_information.svg"}
    }

    Row {
        id: topRow
        width: parent.width
        height: childrenRect.height - 1
        Rectangle { // this outer rectangel is here so that positioning (right alignment) within the row works
            width: parent.width
            height: childrenRect.height
            color: "transparent"

            ComboBox {
                id: logLevelSelector
                width: 150
                model: logLevels
                displayText: "Filter alerts"
                implicitHeight: 32 // => actually results in 20px
                anchors.right: clearLog.left;
                anchors.rightMargin: 5
                anchors.top: parent.top
                popup: T.Popup {
                    id: optionsPopup
                    width: logLevelSelector.width
                    height: Math.min(contentItem.implicitHeight, logLevelSelector.Window.height - topMargin - bottomMargin)

                    contentItem: ListView {
                        clip: true
                        implicitHeight: contentHeight
                        model: logLevelSelector.delegateModel
                        currentIndex: logLevelSelector.highlightedIndex
                        highlightMoveDuration: 0
                        spacing: 1

                        T.ScrollIndicator.vertical: ScrollIndicator { }
                    }

                    background: Rectangle {
                        color: Colors.dark2
                    }
                }
                delegate: ItemDelegate {
                    width: parent.width
                    height: 30
                    background: Rectangle {
                        color: Colors.dark0
                    }
                    RowLayout {
                        width: parent.width
                        height: parent.height
                        CheckBox {
                            id: logLevelCheckBox
                            checked:  evaluateVisibility(logLevel)
                            Layout.alignment: Qt.AlignVCenter
                            implicitHeight: 20
                            verticalPadding: 0
                            onToggled:
                            {
                                if (logLevel === "info") {
                                    infoVisible = checkState == Qt.Checked;
                                }
                                if (logLevel === "warning") {
                                    warnVisible = checkState == Qt.Checked;
                                }
                                if (logLevel === "error") {
                                    errorVisible = checkState == Qt.Checked;
                                }
                                optionsPopup.visible = false
                            }
                        }
                        Label {
                            text: displayText
                            Layout.alignment: Qt.AlignVCenter
                        }
                        Item {
                            Layout.fillWidth: true
                        }
                        Image {
                            width: 15
                            height: 15
                            source: iconUrl
                            Layout.rightMargin: 6
                            sourceSize.width: 25
                            sourceSize.height: 25
                        }
                    }
                    onClicked:
                    {
                        if (logLevel === "info") {
                            infoVisible = !infoVisible;
                        }
                        if (logLevel === "warning") {
                            warnVisible = !warnVisible;
                        }
                        if (logLevel === "error") {
                            errorVisible = !errorVisible;
                        }
                        optionsPopup.visible = false
                    }
                }
            }
            Button {
                id: clearLog
                text: "Clear log"
                font.capitalization: Font.MixedCase
                implicitHeight: 20
                topPadding: 0 // this property indirectly controls the icon size
                bottomPadding: 0 // this property indirectly controls the icon size
                topInset: 0
                bottomInset: 0
                leftInset: 2
                onClicked: {
                    mainRect.clearLog();
                }
                anchors.right: parent.right;
                anchors.rightMargin: scrollbarOffset
                anchors.top: parent.top
                anchors.topMargin: 5
            }
        }

    }
    
    ScrollView {
        id: loggerScrollView
        Layout.alignment: Qt.AlignTop
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: topRow.bottom
        anchors.bottom: parent.bottom
        clip: true;

        function scrollToBottom() {
            ScrollBar.vertical.position = 1.0
        }

        ListView {
            id: logOutput
            width: loggerScrollView.width
            spacing: 1
            model: loggerContent
            delegate: itemDelegate
        }
        Component {
            id: itemDelegate
            Rectangle {
                id: backgoundRect
                color: Colors.dark2
                width: logOutput.width
                // reuse visibility function to avoid writing a new one
                height: evaluateVisibility(logLevel) ? evaluateTextHeight(msg) : 0 - logOutput.spacing // if you do not subtract spacing it will still be applied to the invisible item
                visible: evaluateVisibility(logLevel)

                ListView.onAdd: {
                    loggerScrollView.scrollToBottom();
                }

                MouseArea {
                    onClicked: (mouse) => {
                        // reset color on previously selected item
                        logger.onDeselect();

                        backgoundRect.color = Colors.dark4;
                        prefSelectedItem = backgoundRect;
                        selectedText = msg;

                        logger.selectionChanged();


                        if (mouse.button === Qt.RightButton)
                        {
                            rightClickMenu.popup();
                        }
                    }
                    width: parent.width
                    height: parent.height
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    propagateComposedEvents: true
                    preventStealing: false

                    RowLayout {
                        width: parent.width
                        height: parent.height
                        id: rowLayout
                        anchors.verticalCenter: parent.verticalCenter

                        Image {
                            source: getIconFromLogLevel(logLevel)
                            sourceSize.width: 13
                            sourceSize.height: 13
                            Layout.leftMargin: 12
                        }
                        Label {
                            id: loggerLabel
                            text: msg
                            color: Colors.light1
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            Layout.leftMargin: 5
                            Layout.fillWidth: true
                            font.pixelSize: logFontSize
                            topInset: 0
                            bottomInset: 0
                        }
                            // use this to have line selection
                            // TextEdit {
                            //     text: msg
                            //     color: Colors.light1
                            //     selectionColor: Colors.green1

                            //     horizontalAlignment: Text.AlignLeft
                            //     verticalAlignment: Text.AlignVCenter
                            //     selectByMouse: true
                            //     readOnly: true
                            //     persistentSelection: true
                            //     clip: true
                            //     // elide: Text.ElideRight
                            //     Layout.leftMargin: 5
                            //     Layout.fillWidth: true
                            // }
                    }
                }
                // right click menu
                Menu {
                    id: rightClickMenu
                    MenuItem {
                        text: "Copy selection"
                        onTriggered: {
                            if(selectedText !== null)
                            {
                                logger.copyTextToClipboard(selectedText);
                                selectedText = null;
                            }
                        }
                    }
                    MenuItem {
                        text: "Copy all"
                        onTriggered: {
                            let allMessages = "";
                            for(var i = 0; i < loggerContent.rowCount(); i++) {
                                allMessages += loggerContent.get(i).msg + "\n";
                            }
                            logger.copyTextToClipboard(allMessages);
                        }
                    }
                }
            }
        }
        ListModel {
            id: loggerContent
        }
    }
    

    states: State {
        name: "scrollBarActive"
        when: (loggerScrollView.ScrollBar.vertical.active && loggerScrollView.ScrollBar.vertical.size < 1.0)
    }

    transitions: [
        Transition {
            to: "scrollBarActive"
            NumberAnimation { targets: logOutput; property: "width"; to: loggerScrollView.width - scrollbarOffset; duration: 0}
        },
        Transition {
            from: "scrollBarActive"
            SequentialAnimation {
                PropertyAction{ targets: logOutput; property: "width"; value: loggerScrollView.width - scrollbarOffset }
                PauseAnimation { duration: 2450 }
                NumberAnimation { targets: logOutput; property: "width"; to: loggerScrollView.width }
            }
        }
    ]

    Connections {
        target: logger
        function onLogToQml(message, logLevel) {
            loggerContent.append({
                msg: message,
                logLevel: logLevel,
            });
        }
        function onDeselect() {
            if(prefSelectedItem !== null && prefSelectedItem !== undefined)
            {
                prefSelectedItem.color = Colors.dark2;
            }
        }
    }

    function getIconFromLogLevel(logLevel)
    {
        switch (logLevel) {
        case "info":
            return "qrc:icons/Icon_logger_alert_information.svg"
        break;
        case "warning":
            return "qrc:icons/Icon_logger_alert_warning.svg"
        break;
        case "error":
            return "qrc:icons/Icon_logger_alert_error.svg"
        break;
        case "fatal":
            return "qrc:icons/Icon_logger_alert_error.svg"
        break;
        default:
            return "qrc:icons/Icon_logger_alert_information.svg"
        }
    }

    function evaluateVisibility(logLevel)
    {
        switch (logLevel) {
        case "info":
            if(infoVisible)
            {
                return true;
            }
            return false;
        break;
        case "warning":
            if(warnVisible)
            {
                return true;
            }
            return false;
        break;
        case "error":
            if(errorVisible)
            {
                return true;
            }
            return false;
        break;
        case "fatal":
            return true;
        break;
        default:
            return true;
        }
    }

    function clearLog()
    {
        loggerContent.clear()
    }

    function evaluateTextHeight(text)
    {
        let lineCount = text.split(/\r\n|\r|\n/).length
        if(lineCount * logFontSize > 30)
        {
            return lineCount * (logFontSize + 2)
        }
        return 30
    }
}
