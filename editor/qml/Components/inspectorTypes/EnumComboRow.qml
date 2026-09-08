import QtQuick 2.15
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.15

import Model 1.0 as Model
import "../../Colors.js" as Colors

import "." as InspectorRow


// row
Rectangle {   // container
    id: mainRow
    color: Colors.dark2
    implicitWidth: control.columnWidthInt
    width: control.columnWidthInt
    implicitHeight: heightRow
    height: heightRow
    // dynamic height 
    property int heightRow: 30


    property var severity: -1
    property var issueMessage: ""
    property var tooltipColor: Colors.light3
    property var issueIcon: ""

    // filled automatically by treeview
    required property int depth

    property var displayModel: {
        value: undefined;
        options: [];
        optionsAsText: undefined
    }

    Text {  // label 
        id: rootTitle
        anchors.top: parent.top
        anchors.topMargin: heightRow === 30 ? 8 : 5 
        anchors.left: parent.left
        anchors.leftMargin: 5 * (depth + 1)
        text: control.displayLabel(displayModel.title)
        font.pixelSize: 11
        color: Colors.light5
        elide: Text.ElideRight
    }

    HoverHandler { id: hoverHandler}

    Image {
        id: validationIcon
        width: 15
        height: 15
        anchors.left: rootTitle.right
        anchors.leftMargin: 7
        anchors.verticalCenter: rootTitle.verticalCenter
        source: issueIcon
        sourceSize.height: 25
        sourceSize.width: 25
        visible: source !== ""
    }

    InspectorRow.IndicatorToolTip {
        visible: hoverHandler.hovered && severity !== -1
        issueString: issueMessage
        offset: validationIcon.x
        toolTipColor: tooltipColor
    }
    
    Rectangle {
        id: marginLeft
        anchors.left: rootTitle.right
        height: 10
        width: 5
        color: "transparent"
    }

    InspectorRow.Spacer { // space 
        leftItem: marginLeft
        rightItem: iconAction
        elementWidth: root.width + 10 
    }

    Button {  // reset action 
        id: iconAction
        flat: true
        icon.source: "qrc:/icons/Icon_Override.svg" 
        visible: displayModel.operations["reset"] ? true : false
        padding: 0
        topInset: 0
        bottomInset: 0
        implicitWidth: 16
        implicitHeight: 16
        opacity: 0.7
        anchors.right: parent.right
        anchors.rightMargin: 5
        anchors.top: parent.top
        anchors.topMargin: 8
        icon.color: enabled ? Colors.light1 : Colors.light4 
        enabled: Boolean(displayModel.value)
        onClicked: {
            let parameters = {};
            parameters["path"] = displayModel.path;
            Model.Operations.execute("reset", parameters).catch((message) => {
                iconAction.reject();
                errorMessage.show(message, 3000);
            });
        }
        background: Rectangle {
            anchors.fill: parent
            radius: 2
            color: iconAction.hovered ? Colors.dark4 : "transparent"
        }
    }

    
    ComboBox {  // combobox
        id: root
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 5
        anchors.right: heightRow === 30 ? iconAction.left : mainRow.right
        anchors.rightMargin: heightRow === 30 ? 10 : 30
        implicitWidth: 350
        anchors.leftMargin: 5
        flat: true
        implicitHeight: 21
        topPadding: -4 
        bottomPadding: -4 
        font.pixelSize: 12
        indicator: Image {
            x: root.width - 20
            y: 3
            width: 15
            height: 15
            source: "qrc:icons/Icon_down_20px.svg"
        }

        textRole: "text"
        valueRole: "value"
        model: {
            const listModel = [];
            if(!displayModel || !displayModel.options) {
                return listModel;
            }
            for (let i = 0; i < displayModel.options.length; i++) {
                let optionTitle = String(displayModel.options[i]);
                if (displayModel.optionsAsText && !(displayModel.options.includes("OPAQUE") || displayModel.options.includes("directional"))) {
                    optionTitle = displayModel.optionsAsText[i];
                }
                listModel.push({
                    key: optionTitle,
                    text: optionTitle.slice(0, 1).toUpperCase() + optionTitle.slice(1).toLowerCase(),
                    value: displayModel.options[i]
                });
            }
            return listModel;
        }
        

        currentIndex: -1
        Component.onCompleted: currentIndex = indexOfValue(displayModel.value ?? displayModel.default)
        displayText: Boolean(displayModel.value) ? currentText : "Not set"

        background: Rectangle {
            anchors.fill: parent
            color: Colors.dark4
        }
        onActivated: {
            if (displayModel.value !== currentValue){
                let parameters = {};
                parameters["path"] = displayModel.path;
                parameters["value"] = currentValue;
                Model.Operations.execute("set_gltf_value", parameters).catch((message) => {
                    currentIndex = indexOfValue(displayModel.value ?? displayModel.default);
                    root.reject();
                    errorMessage.show(message, 3000);
                });
            }
        }

        hoverEnabled: true
        // Component type of accessors disabled 
        enabled: displayModel.path.includes("accessors") && displayModel.path.includes("componentType") ? false : displayModel.operations["set_gltf_value"] ? true : false
    }

    ToolTip { // tooltip input text
        id: errorMessage
        parent: root
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
