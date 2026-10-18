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

    property var severity: -1
    property var issueMessage: ""
    property var tooltipColor: Colors.light3
    property var issueIcon: ""
    property string sourceIcon: ""  // icon 
    property var isIconPreviewImage: false
    property var displayModel: {
        value: undefined;
        options: [];
        optionsAsText: undefined;
        name: "";
    }

    // filled automatically by treeview
    required property int depth

    // dynamic height 
    property int heightRow: 30

    Text {  // label 
        id: typeTitle
        anchors.top: parent.top
        anchors.topMargin: heightRow === 30 ? 8 : 5 
        anchors.left: parent.left
        anchors.leftMargin: 5 * (depth + 1)
        text: control.displayLabel(displayModel.title)
        font.pixelSize: 11
        color: Colors.light5
        elide: Text.ElideRight
        font.bold: mouseArea.containsMouse && mouseArea.enabled
    }

    HoverHandler { id: hoverHandler}

    Image {
        id: validationIcon
        width: 15
        height: 15
        anchors.left: typeTitle.right
        anchors.leftMargin: 7
        anchors.verticalCenter: typeTitle.verticalCenter
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
        anchors.left: typeTitle.right
        height: 10
        width: 5
        color: "transparent"
    }

    InspectorRow.Spacer { // space 
        leftItem: marginLeft
        rightItem: iconAction
        elementWidth: root.width + 15 + rootCombo.width 
    }

    function getButtonIconColor(enabled, isIconPreviewImage) {
        if(isIconPreviewImage) {
            return "transparent"
        }
        if (enabled) {
            return Colors.green
        }
        else {
            return Colors.light4
        }
    }

    Button {  // icon obj
        id: root
        flat: true
        visible: sourceIcon.length > 0 ? true : false 
        padding: 0
        topInset: 0
        bottomInset: 0
        width: 20
        height: 20
        opacity: 0.7
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 5
        anchors.right: rootCombo.left 
        anchors.rightMargin: 5 
        enabled: Boolean(displayModel.value) && Boolean(sourceIcon)
        hoverEnabled: true
        onClicked: {
            let tmpSelection = {};
            tmpSelection["uuid"] = displayModel.uuid;
            tmpSelection["localPath"] = displayModel.localPath; 
            Model.Operations.runEditorOperation("editor.setSelection", [tmpSelection]);
        }
        background: Rectangle {
            anchors.fill: parent
            radius: 2
            color: root.hovered ? Colors.dark4 : "transparent"
        }

        // Sometimes this PointerComboRow causes a crash.
        // This happens (for example) when you click (to open it) on a texture within the inspector
        // The crash seems to be caused by the IconImage if the texture has an image attached (and a preview is drawn).
        // This loader prevents this crash, since normal Image components do not seem to be affected.
        // We still need the IconImage component to color the icon green when the reference is filed.
        // e.g. if the reference points to a Material.
        Loader {
            anchors.fill: parent
            sourceComponent: isIconPreviewImage ? imagePreviewIcon : normalIcon
        }

        Component {
            id: normalIcon
            IconImage {
                source: sourceIcon
                fillMode: Image.PreserveAspectFit
                visible: isIconPreviewImage === false
                enabled: isIconPreviewImage === false
                anchors.fill: parent
                sourceSize.width: 20
                sourceSize.height: 20
                cache: false
                color: getButtonIconColor(enabled, isIconPreviewImage)
            }
        }

        Component {
            id: imagePreviewIcon
            Image {
                source: sourceIcon
                fillMode: Image.PreserveAspectFit
                visible: isIconPreviewImage
                enabled: isIconPreviewImage
                anchors.fill: parent
                sourceSize.width: 20
                sourceSize.height: 20
                cache: false
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.left: parent.left
        anchors.right: rootCombo.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        hoverEnabled: true
        enabled: Boolean(displayModel.value) && Boolean(sourceIcon)
        z: -1
        onClicked: {
            let tmpSelection = {};
            tmpSelection["uuid"] = displayModel.uuid;
            tmpSelection["localPath"] = displayModel.localPath; 
            Model.Operations.runEditorOperation("editor.setSelection", [tmpSelection]);
        }
        onEntered: {
            mouseArea.cursorShape = Qt.PointingHandCursor;
        }
        onExited: {
            mouseArea.cursorShape = Qt.ArrowCursor;
        }
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
        
    ComboBox {  // combobox
        id: rootCombo
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 5
        anchors.right: heightRow === 30 ? iconAction.left : mainRow.right
        anchors.rightMargin: heightRow === 30 ? 10 : 30
        flat: true
        implicitHeight: 21 
        implicitWidth: 330
        topPadding: -4 
        bottomPadding: -4 
        font.pixelSize: 12
        indicator: Image {
            x: rootCombo.width - 20
            y: 3
            width: 15
            height: 15
            source: "qrc:icons/Icon_down_20px.svg"
        }
        background: Rectangle {
            anchors.fill: parent
            color: Colors.dark4
        }

        textRole: "text"
        valueRole: "value"
        model: displayModelToComboModel(displayModel)

        delegate: ItemDelegate {
            width: rootCombo.width
            text: modelData.text
            font.pixelSize: 12
            highlighted: rootCombo.highlightedIndex === index
            padding: 5
            background: Rectangle {
                color: rootCombo.highlightedIndex === index ? Colors.dark3 : Colors.dark4
            }
        }

        currentIndex: -1
        Component.onCompleted: currentIndex = indexOfValue(displayModel.value ?? displayModel.default)
        displayText: Boolean(displayModel.value) ? currentText : "None"

        onActivated: {
            if (!currentValue) {
                currentIndex = indexOfValue(displayModel.value ?? displayModel.default);
                return;
            }

            if (displayModel.value !== currentValue){
                let parameters = {};
                parameters["path"] = displayModel.path;
                parameters["pointer"] = "/gltf" + currentValue;
                Model.Operations.execute("set_gltf_pointer", parameters).catch((message) => {
                    currentIndex = indexOfValue(displayModel.value ?? displayModel.default);
                    rootCombo.reject();
                    errorMessage.show(message, 3000);
                });
            }
        }

        hoverEnabled: true
        // Component type of accessors disabled 
        enabled: displayModel && "set_gltf_pointer" in displayModel.operations
    }

    function displayModelToComboModel(displayModel) {
        if (!displayModel.referenceCandidates || displayModel.referenceCandidates.length === 0) {
            return [
                {
                    text: "None Available",
                    value: undefined
                }
            ];
        }
        return displayModel.referenceCandidates.map((candidate) => {
            return {
                text: candidate.name,
                value: candidate.path
            }
        });
    }

    Button {  // reset action 
        id: iconAction
        property bool reset: "reset" in displayModel.operations
        flat: true
        icon.source: reset ? "qrc:/icons/Icon_Override.svg" : "qrc:/icons/Icon_trash.svg"
        visible: "reset" in displayModel.operations || "dangerous.removeValue" in displayModel.operations
        padding: 0
        topInset: 0
        bottomInset: 0
        implicitWidth: 16
        implicitHeight: 16
        opacity: 0.7
        anchors.right: parent.right
        anchors.rightMargin: 5
        anchors.top: parent.top
        anchors.topMargin: reset ? 5 * (depth + 1) : 8
        icon.color: reset ? (enabled ? Colors.light1 : Colors.light4) : Colors.red 
        enabled: Boolean(displayModel.value)
        onClicked: {
            let parameters = {};
            parameters["path"] = displayModel.path;
            let operation = reset ? "reset" : "dangerous.removeValue";
            Model.Operations.execute(operation, parameters).catch((message) => {
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
}
