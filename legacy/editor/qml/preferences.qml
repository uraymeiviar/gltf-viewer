import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Qt.labs.platform

import "./Components" as Components

import "Colors.js" as Colors

Rectangle {
    id: window
    width: 700
    height: 450
    visible: true
    color: Colors.dark1

    property var navEntries: ["General", "Load and save", "Viewport", "Camera control", "Compression"]
    property var msaaOptions: ["Disabled", "2x", "4x", "8x", "16x", "32x", "64x"]
    property var iblMaxResolutionOptions: [16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192]
    //// These settings are disabled as there is currently no property available for these in the preferences
    //property var envMapPath: "nothing selected"
    //property var renderFileFormats: ["png", "jpg"];
    //property real jpgCompression: 90

    Rectangle {
        width: childrenRect.width
        height: parent.height
        color : Colors.dark2

        Column {
            id: navListColumn
            x: 0
            y: 0
            width: 180
            height: parent.height

            ListView {
                id: preferencesMenu
                x: 0
                y: 0
                width: parent.width
                height: parent.height
                interactive: false


                Component {
                    id: highlight
                    Rectangle {
                        color: Colors.dark4
                    }
                }
                highlight: highlight
                highlightMoveDuration: 0
                highlightFollowsCurrentItem: true
                focus: true

                model: navEntries
                delegate: ItemDelegate {
                    text: qsTr(model.modelData)
                    width: preferencesMenu.width
                    onClicked: preferencesMenu.currentIndex = index
                    font.weight: preferencesMenu.currentIndex == index ? Font.DemiBold : Font.Normal
                    Image {
                        source: "qrc:icons/Icon_right_16px.svg"
                        width: 16
                        height: 16
                        anchors.right: parent.right
                        anchors.rightMargin: 5
                        anchors.verticalCenter: parent.verticalCenter
                        visible: preferencesMenu.currentIndex == index ? true : false
                    }
                }
            }
        }
    }

    Column {
        id: contentColumn
        x: navListColumn.width
        y: 0
        width: parent.width - navListColumn.width
        height: parent.height

        StackLayout {
            id: stackLayout
            x: 0
            y: 0
            width: parent.width
            height: parent.height
            currentIndex: preferencesMenu.currentIndex

            // general settings
            Pane {
                width: parent.width
                height: parent.height
                padding: 10
                
                ColumnLayout {
                    width: parent.width
                    spacing: 1

                    Label {
                        text: "EXTERNAL IMAGE EDITOR"
                        Layout.preferredHeight: 14
                        Layout.topMargin: 11
                        Layout.bottomMargin: 5
                        Layout.leftMargin: 5
                        color: Colors.dark4
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }
                    
                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("Choose image editing software")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        TextField {
                            id: imageEditingToolTextField
                            text: preferences.imageEditingToolPath
                            onEditingFinished: { preferences.imageEditingToolPath = text }
                            background: Rectangle {
                                implicitWidth: 200
                                implicitHeight: 20
                                color: Colors.dark1
                            }
                            leftPadding: 3
                            rightPadding: 3
                            topPadding: 0
                            bottomPadding: 0
                        }

                        Button {
                            id: imageEditingToolButton
                            text: "Choose"
                            implicitWidth: 70
                            implicitHeight: 20
                            leftPadding: 0
                            topPadding: -5
                            bottomPadding: -5
                            topInset: 0
                            bottomInset: 0
                            onClicked: {
                                preferences.chooseImageEditingTool();
                            }
                            background: Rectangle {
                                color: imageEditingHoverHandler.hovered ? Colors.light4 : Colors.dark4
                            }
                            HoverHandler {
                                id: imageEditingHoverHandler
                            }
                        }

                    }

                    Label {
                        text: "USAGE STATISTICS"
                        Layout.preferredHeight: 14
                        Layout.topMargin: 11
                        Layout.bottomMargin: 5
                        Layout.leftMargin: 5
                        color: Colors.dark4
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }


					//// These settings are disabled as there is currently no property available for these in the preferences
                    // Label {
                    //    text: "DEFAULT RENDERING SETTINGS"
                    //    color: Colors.dark4
                        // font.pixelSize: 10
                        // font.weight: Font.DemiBold
                    // }
                    // Components.PrefPanelRowComp {
                    //     Label {
                    //         text: qsTr("Image size")
                    //         horizontalAlignment: Text.AlignLeft
                    //         verticalAlignment: Text.AlignVCenter
                    //     }

                    //     Item {
                    //         Layout.fillWidth: true
                    //     }

                    //     SpinBox
                    //     {
                    //         from: 0
                    //         editable: true
                    //     }
                    //     SpinBox
                    //     {
                    //         from: 0
                    //         editable: true
                    //     }
                    //     Label {
                    //         text: "Pixels"
                    //     }
                    // }
                    // Components.PrefPanelRowComp {
                    //     Layout.preferredHeight: 40
                    //     Layout.fillWidth: true

                    //     Label {
                    //         text: qsTr("Image location")
                    //         horizontalAlignment: Text.AlignLeft
                    //         verticalAlignment: Text.AlignVCenter
                    //     }

                    //     Item {
                    //         Layout.fillWidth: true
                    //     }

                    //     TextField {
                    //         id: envPath
                    //         text: envMapPath
                    //         horizontalAlignment: TextInput.AlignRight
                    //         background: Rectangle {
                    //             implicitWidth: 350
                    //             color: "transparent"
                    //         }
                    //     }
                    //     Button {
                    //         id: envPathSelect
                    //         text: "Select"
                    //         onClicked: envMapFileDialog.open()
                    //     }
                    //     FileDialog {
                    //         id: envMapFileDialog
                    //         selectFolder: true
                    //         folder: shortcuts.pictures
                    //         onAccepted: {
                    //             var path = envMapFileDialog.fileUrl.toString();
                    //             // remove prefixed "file:///"
                    //             path= path.replace(/^(file:\/{3})|(qrc:\/{2})|(http:\/{2})/,"");
                    //             // unescape html codes like '%23' for '#'
                    //             envMapPath = decodeURIComponent(path);
                    //         }
                    //     }
                    // }
                    // Components.PrefPanelRowComp {
                    //     Label {
                    //         text: qsTr("Render file format")
                    //         horizontalAlignment: Text.AlignLeft
                    //         verticalAlignment: Text.AlignVCenter
                    //     }

                    //     Item {
                    //         Layout.fillWidth: true
                    //     }

                    //     ComboBox {
                    //         id: renderFormatCB
                    //         model: renderFileFormats
                    //     }
                    // }
                    // Components.PrefPanelRowComp {
                    //     visible: renderFormatCB.currentValue == "jpg"
                    //     Label {
                    //         text: qsTr("Compression (0 = max, 100 = none)")
                    //         horizontalAlignment: Text.AlignLeft
                    //         verticalAlignment: Text.AlignVCenter
                    //     }
                    //     Item {
                    //         Layout.fillWidth: true
                    //     }
                    //     Slider {
                    //         id: jpgCompressionSlider
                    //         from: 1
                    //         to: 100
                    //         stepSize: 1
                    //         value: jpgCompression
                    //         onMoved: { jpgCompression = value}
                    //     }
                    //     TextField {
                    //         horizontalAlignment: TextInput.AlignRight
                    //         validator: IntValidator {
                    //             bottom: jpgCompressionSlider.from
                    //             top: jpgCompressionSlider.to
                    //         }
                    //         text:  jpgCompression
                    //         onEditingFinished: { jpgCompression = parseFloat(text)}
                    //         background: Rectangle {
                    //             implicitWidth: 25
                    //             color: "transparent"
                    //         }
                    //     }
                    // }
                } // end of column layout
            } // end of frame

            // load and save
            Pane {
                width: parent.width
                height: parent.height
                padding: 10

                ColumnLayout {
                    width: parent.width
                    spacing: 1

                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("Load sample model on startup")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        CheckBox {
                            onCheckedChanged: preferences.loadSampleAssetOnStartup = checked
                            checked: preferences.loadSampleAssetOnStartup
                            padding: 0
                        }
                    }

                    Label
                    {
                        text: "FILES";
                        Layout.preferredHeight: 14
                        Layout.topMargin: 11
                        Layout.bottomMargin: 5
                        Layout.leftMargin: 5
                        color: Colors.dark4
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }

                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("Number of recent files")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        SpinBox {
                            implicitHeight: 30
                            implicitWidth: 120
                            from: 0
                            to: 20
                            editable: true
                            value: preferences.maxRecentFiles
                            onValueChanged: preferences.maxRecentFiles = value
                            topPadding: 0
                            bottomPadding: 5
                        }
                    }
                    
                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("Overwrite existing images and binary files")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        CheckBox {
                            checked: preferences.overwriteResourceFilesWithoutAsking
                            onCheckedChanged: preferences.overwriteResourceFilesWithoutAsking = checked
                            padding: 0
                        }
                    }

                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("Run validator after save")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        CheckBox {
                            id: validateAfterSave
                            checked: preferences.validateAfterSave
                            onCheckedChanged: preferences.validateAfterSave = checked
                            padding: 0
                        }
                    }                

                } // end of column layout
            } // end of frame
            
            // Viewport
            Pane {
                width: parent.width
                height: parent.height
                padding: 10

                ColumnLayout {
                    width: parent.width
                    spacing: 1

                    Label
                    {
                        text: "3D MODEL";
                        Layout.preferredHeight: 14
                        Layout.topMargin: 11
                        Layout.bottomMargin: 5
                        Layout.leftMargin: 5
                        color: Colors.dark4
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }

                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("Selection outline thickness")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        SpinBox {
                            implicitHeight: 30
                            implicitWidth: 120
                            id: outlineThicknessSBox
                            from: 0
                            to: 10
                            value: preferences.outlineThickness
                            onValueChanged: preferences.outlineThickness = value
                            editable: true
                            topPadding: 0
                            bottomPadding: 5
                        }
                    }

                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("Selection outline color")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Button {
                            onClicked: preferences.openOutlineColorDialog();
                            background: Rectangle {
                                implicitWidth: 60
                                implicitHeight: 20
                                color: preferences.outlineColor
                            }
                            padding: 0
                        }
                    }

                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("Wireframe color")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }
                        
                        Button {
                            onClicked: preferences.openWireframeColorDialog();
                            background: Rectangle {
                                implicitWidth: 60
                                implicitHeight: 20
                                color: preferences.wireframeColor
                            }
                            padding: 0
                        }
                    }

                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("Bounding box color")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }
                        
                        Button {
                            onClicked: preferences.openBoundingBoxColorDialog();
                            background: Rectangle {
                                implicitWidth: 60
                                implicitHeight: 20
                                color: preferences.boundingBoxColor
                            }
                            padding: 0
                        }
                    }

                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("Skeleton color")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }
                        
                        Button {
                            onClicked: preferences.openSkeletonColorDialog();
                            background: Rectangle {
                                implicitWidth: 60
                                implicitHeight: 20
                                color: preferences.skeletonColor
                            }
                            padding: 0
                        }
                    }

                    Label
                    {
                        text: "DISPLAY";
                        Layout.preferredHeight: 14
                        Layout.topMargin: 11
                        Layout.bottomMargin: 5
                        Layout.leftMargin: 5
                        color: Colors.dark4
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }

                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("Gizmo size")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        SpinBox {
                            implicitHeight: 30
                            implicitWidth: 120
                            id: gizmoSizeSBox
                            editable: true
                            topPadding: 0
                            bottomPadding: 5

                            // code to make this spin box a double spin box
                            property int decimals: 1
                            property real realValue: preferences.gizmoScaling
                            property real realFrom: 0.1
                            property real realTo: 5.0
                            property real realStepSize: 0.1
                            property real factor: Math.pow(10, decimals)

                            stepSize: realStepSize * factor
                            value: realValue * factor
                            to : realTo * factor
                            from : realFrom * factor

                            onValueChanged: preferences.gizmoScaling = realValue

                            validator: DoubleValidator
                            {
                                bottom: Math.min(gizmoSizeSBox.from, gizmoSizeSBox.to) * gizmoSizeSBox.factor
                                top:  Math.max(gizmoSizeSBox.from, gizmoSizeSBox.to) * gizmoSizeSBox.factor
                                locale: Qt.locale().name
                            }

                            textFromValue: function(value, locale) {
                                gizmoSizeSBox.setRealValue(value)
                                return Number(gizmoSizeSBox.realValue).toLocaleString(Qt.locale(), 'f', gizmoSizeSBox.decimals)
                            }

                            valueFromText: function(text, locale) {
                                return Number.fromLocaleString(Qt.locale(), text) * gizmoSizeSBox.factor
                            }

                            function setRealValue(_value) {
                                gizmoSizeSBox.realValue = _value  / gizmoSizeSBox.factor;
                            }
                        }
                    }

                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("Grid color")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Button {
                            onClicked: preferences.openGridColorDialog();
                            background: Rectangle {
                                implicitWidth: 60
                                implicitHeight: 20
                                color: preferences.gridColor
                            }
                            padding: 0
                        }
                    }
                    
                    Label
                    {
                        text: "QUALITY";
                        Layout.preferredHeight: 14
                        Layout.topMargin: 11
                        Layout.bottomMargin: 5
                        Layout.leftMargin: 5
                        color: Colors.dark4
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }

                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("MSAA")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        ComboBox {
                            implicitHeight: 32 // => actually results in 21px
                            implicitWidth: 120
                            model: msaaOptions
                            // in a previous version -1 was used as disabled value but the current version uses 0 for this.
                            // to avoid issues we have this check.
                            currentIndex: preferences.msaaExp == -1 ? 0 : preferences.msaaExp
                            onCurrentIndexChanged: preferences.msaaExp = currentIndex
                            padding: 0
                        }
                    }

                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("IBL resolution limit")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        ComboBox {
                            implicitHeight: 32
                            implicitWidth: 120
                            model: iblMaxResolutionOptions
                            currentIndex: preferences.iblMaxResolutionExp - 4
                            onCurrentIndexChanged: preferences.iblMaxResolutionExp = currentIndex + 4
                            padding: 0
                            ToolTip.delay: 1000
                            ToolTip.timeout: 5000
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Maximal IBL map resolution used in image based lighting")
                        }
                    }
                } // end of column layout
            } // end of frame

            // Camera control
            Pane {
                background: Rectangle{border.color: "transparent"; color: "transparent"}
                width: parent.width
                height: parent.height
                padding: 10
                focusPolicy: Qt.ClickFocus
                
                ColumnLayout {
                    width: parent.width
                    spacing: 1

                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("Free camera speed")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Slider {
                            id: freeCamSpeedSlider
                            from: 1
                            to: 20
                            stepSize: 0.1
                            value: preferences.freeCameraSpeedScale
                            onMoved: { preferences.freeCameraSpeedScale = value}
                        }

                        TextField {
                            implicitWidth: 60
                            validator: DoubleValidator {
                                bottom: freeCamSpeedSlider.from
                                top: freeCamSpeedSlider.to
                                decimals: 1
                                locale: Qt.locale().name
                            }
                            text: preferences.freeCameraSpeedScale.toLocaleString(Qt.locale(), "f", 1)
                            onEditingFinished: { preferences.freeCameraSpeedScale = Number.fromLocaleString(Qt.locale(), text) }
                        }
                    }
                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("Pan speed")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Slider {
                            id: panSpeedSlider
                            from: 1
                            to: 20
                            stepSize: 0.1
                            value: preferences.cameraPanSpeedScale
                            onMoved: { preferences.cameraPanSpeedScale = value}
                        }

                        TextField {
                            implicitWidth: 60
                            validator: DoubleValidator {
                                bottom: panSpeedSlider.from
                                top: panSpeedSlider.to
                                decimals: 1
                                locale: Qt.locale().name
                            }
                            text: preferences.cameraPanSpeedScale.toLocaleString(Qt.locale(), "f", 1)
                            onEditingFinished: { preferences.cameraPanSpeedScale = Number.fromLocaleString(Qt.locale(), text) }
                        }
                    }
                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("Zoom speed")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Slider {
                            id: zoomSpeedSlider
                            from: 1
                            to: 20
                            stepSize: 0.1
                            value: preferences.cameraZoomSpeedScale
                            onMoved: { preferences.cameraZoomSpeedScale = value}
                        }

                        TextField {
                            implicitWidth: 60
                            validator: DoubleValidator {
                                bottom: zoomSpeedSlider.from
                                top: zoomSpeedSlider.to
                                decimals: 1
                                locale: Qt.locale().name
                            }
                            text: preferences.cameraZoomSpeedScale.toLocaleString(Qt.locale(), "f", 1)
                            onEditingFinished: { preferences.cameraZoomSpeedScale = Number.fromLocaleString(Qt.locale(), text) }
                        }
                    }
                } // end of column layout
            } // end of frame

            // Compression
            Pane {
                background: Rectangle{border.color: "transparent"; color: "transparent"}
                width: parent.width
                height: parent.height
                padding: 10
                focusPolicy: Qt.ClickFocus
                
                ColumnLayout {
                    width: parent.width
                    spacing: 1

                    Label
                    {
                        text: "DRACO MESH COMPRESSION";
                        Layout.preferredHeight: 14
                        Layout.topMargin: 11
                        Layout.bottomMargin: 5
                        Layout.leftMargin: 5
                        color: Colors.dark4
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }

                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("Compression level (0 = highest / 10 = lowest)")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        SpinBox {
                            implicitHeight: 30
                            implicitWidth: 120
                            id: dracoCompLvl
                            from: 0
                            to: 10
                            editable: true
                            value: preferences.dracoMeshCompressionLevel
                            onValueChanged: preferences.dracoMeshCompressionLevel = value
                            topPadding: 0
                            bottomPadding: 5
                        }
                    }

                    Label
                    {
                        text: "ADVANCED SETTINGS";
                        Layout.preferredHeight: 14
                        Layout.topMargin: 11
                        Layout.bottomMargin: 5
                        Layout.leftMargin: 5
                        color: Colors.dark4
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }

                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("Preserve triangle order")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        CheckBox {
                            id: dracoTriangleOrder
                            checked: preferences.dracoMeshCompressionPreserveOrder
                            onCheckedChanged: preferences.dracoMeshCompressionPreserveOrder = checked
                            padding: 0
                        }
                    }
                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("Optimize vertex indices")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        CheckBox {
                            id: dracoOptimizeIndices
                            checked: preferences.dracoMeshCompressionOptimizeIndices
                            onCheckedChanged: preferences.dracoMeshCompressionOptimizeIndices = checked
                            padding: 0
                        }
                    }
                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("Position quantization bits")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        SpinBox {
                            implicitHeight: 30
                            implicitWidth: 120
                            id: dracoPosQBits
                            from: 2
                            to: 32
                            editable: true
                            value: preferences.dracoMeshCompressionPositionQuantizationBits
                            onValueChanged: preferences.dracoMeshCompressionPositionQuantizationBits = value
                            topPadding: 0
                            bottomPadding: 5
                        }
                    }
                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("Normal quantization bits")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        SpinBox {
                            implicitHeight: 30
                            implicitWidth: 120
                            id: dracoNormalQBits
                            from: 2
                            to: 32
                            editable: true
                            value: preferences.dracoMeshCompressionNormalQuantizationBits
                            onValueChanged: preferences.dracoMeshCompressionNormalQuantizationBits = value
                            topPadding: 0
                            bottomPadding: 5
                        }
                    }
                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("UV quantization bits")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        SpinBox {
                            implicitHeight: 30
                            implicitWidth: 120
                            id: dracoUVQBits
                            from: 2
                            to: 32
                            editable: true
                            value: preferences.dracoMeshCompressionUVQuantizationBits
                            onValueChanged: preferences.dracoMeshCompressionUVQuantizationBits = value
                            topPadding: 0
                            bottomPadding: 5
                        }
                    }
                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("Color quantization bits")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        SpinBox {
                            implicitHeight: 30
                            implicitWidth: 120
                            id: dracoColorQbits
                            from: 2
                            to: 32
                            editable: true
                            value: preferences.dracoMeshCompressionColorQuantizationBits
                            onValueChanged: preferences.dracoMeshCompressionColorQuantizationBits = value
                            topPadding: 0
                            bottomPadding: 5
                        }
                    }
                    Components.PrefPanelRowComp {
                        Label {
                            text: qsTr("Generic quantization bits")
                            horizontalAlignment: Text.AlignLeft
                            verticalAlignment: Text.AlignVCenter
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        SpinBox {
                            implicitHeight: 30
                            implicitWidth: 120
                            id: dracoGenericQBits
                            from: 2
                            to: 32
                            editable: true
                            value: preferences.dracoMeshCompressionGenericQuantizationBits
                            onValueChanged: preferences.dracoMeshCompressionGenericQuantizationBits = value
                            topPadding: 0
                            bottomPadding: 5
                        }
                    }
                } // end of column layout
            } // end of frame
        }
    }
}
