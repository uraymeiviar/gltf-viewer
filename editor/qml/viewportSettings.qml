import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQml.Models
import Qt.labs.platform

import "./Components" as Components
import "Colors.js" as Colors

Rectangle {
    id: mainRectangle
    color: Colors.dark1

    // models
    property var navigationEntries: ["Environment", "Light", "Post Processing", "Extensions", "Variants"]
    property var frontFaceValues: ["+X", "+Z", "-X", "-Z"]
    property var environmentLightingIntensityLinear: 0.0
    property var exposureValue100: 0.0 //EV for ISO 100

    function scrollToTop() {  // scroll bar back to top if clicking to a different navigationEntries
        scrollView.ScrollBar.vertical.position = 0.0
    }

    function significantDigits(number,  digits) {
        // number.toPrecision(5) does not consider localization
        var trailingDigits = digits  // digits after '.'
        if(number > 1.0) {
            trailingDigits = digits - Math.ceil(Math.log10(number)) 
        }
        return number.toLocaleString(Qt.locale(), "f", trailingDigits)
    }
    ListModel {
        id: hdrFilesModel
        ListElement { text: "Default" }
    }
    ListModel {
        id: materialVariantsModel
    }

    property real topRowBottomBorderWidth: 1
    property real scrollbarOffset: scrollView.ScrollBar.vertical.width + 1

    ListView {
        id: navigationMenu
        width: parent.width
        height: 30
        orientation: ListView.Horizontal

        WheelHandler {
            target: navigationMenu
            onWheel: (event)=>{
                navigationMenu.flick(event.angleDelta.y*event.y * 0.3, 0)
            }
        }

        model: navigationEntries
        delegate: ItemDelegate {
            topInset: 0
            bottomInset: 0
            width: delegateText.width
            height: parent.height
            onClicked: {
                scrollToTop()  // scroll bar back to top if clicking to a different navigationEntries
                navigationMenu.currentIndex = index  
            } 
            background: Rectangle {
                color: Colors.dark1
                height: parent.height

                // border bottom
                Rectangle {
                    color: (navigationMenu.currentIndex != index) && navigationHoverHandler.hovered ? Colors.light1 : Colors.green
                    visible: (navigationMenu.currentIndex == index) || navigationHoverHandler.hovered
                    z : -1
                    anchors
                    {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                        bottom: parent.bottom
                        bottomMargin : -topRowBottomBorderWidth
                    }
                }
            }
            RowLayout {
                width: parent.width
                height: parent.height
                Text {
                    id: delegateText
                    Layout.alignment: Qt.AlignHCenter
                    leftPadding: 10
                    rightPadding: 10
                    text: qsTr(model.modelData)
                    color: navigationMenu.currentIndex == index ? Colors.green :
                            navigationHoverHandler.hovered ? Colors.light1 : Colors.light3
                    font.weight: navigationMenu.currentIndex == index ? Font.DemiBold : Font.Normal
                }
            }

            HoverHandler {
                id: navigationHoverHandler
            }

        }
    }

    // content
    ScrollView {
        id: scrollView
        Layout.alignment: Qt.AlignTop
        anchors.left: parent.left;
        anchors.right: parent.right;
        anchors.top: navigationMenu.bottom;
        anchors.topMargin: topRowBottomBorderWidth;
        anchors.bottom: parent.bottom;
        width: mainRectangle.width;
        contentWidth: availableWidth;
        clip: true;
        ScrollBar.vertical.policy: ScrollBar.AlwaysOn
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            
        StackLayout {
            id: stackLayout
            width: scrollView.width - scrollbarOffset
            currentIndex: navigationMenu.currentIndex

            // Environment
            ColumnLayout {
                width: stackLayout.width
                spacing: 1

                // Show background
                Components.PrefPanelRowComp {
                    Label {
                        id: showBackgroundLabel
                        text: "Show background"
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Switch {
                        id: showBackgroundSwitch
                        onCheckedChanged: viewportSettings.showBackground = checked
                        checked: viewportSettings.showBackground
                    }
                }

                // Background color
                Components.PrefPanelRowComp {
                    leftCustomPadding: 35

                    Label {
                        text: "Background color"
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Button {
                        onClicked: viewportSettings.openBackgroundColorDialog();
                        background: Rectangle {
                            implicitWidth: 70
                            implicitHeight: 20
                            color: viewportSettings.backgroundColor
                        }
                        padding: 0
                    }
                }

                // HDR images
                Components.CollapsibleComp {
                    spacing: 1
                    width: stackLayout.width
                    collapsibleText: "HDR images"
                    collapsibleSideEntry : ComboBox {
                        id: hdrImagesComboBox
                        model: hdrFilesModel
                        onActivated: {
                            viewportSettings.currentHDRFile = currentText
                        }
                        implicitWidth: 250
                        implicitHeight: 32 // => actually results in 21px
                        padding: 0
                    }

                    // Blur
                    Components.PrefPanelRowComp {
                        leftCustomPadding: 52
                        Label {
                            id: blur
                            text: "Blur"
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Slider {
                            id: blurSlider
                            implicitWidth: Math.max(stackLayout.width-250, 200) 
                            from: 0
                            to: 100
                            stepSize: 1
                            value: viewportSettings.blur
                            onMoved: { viewportSettings.blur = value}
                        }

                        TextField {
                            implicitWidth: 56
                            validator: DoubleValidator {
                                bottom: blurSlider.from
                                top: blurSlider.to
                                decimals: 0
                            }
                            text: viewportSettings.blur.toFixed(0)
                            onEditingFinished: { viewportSettings.blur = parseInt(text)}
                        }

                        Label{
                            text: "%"
                        }
                    }

                    // Environment light intensity
                    Components.PrefPanelRowComp {
                        leftCustomPadding: 52
                        Label {
                            id: environmentLightIntensity
                            text: "Intensity"
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Slider {
                            id: environmentLightingIntensitySlider
                            implicitWidth: Math.max(stackLayout.width-250, 200) 
                            from: -2.0
                            to: 5.0
                            stepSize: 0.00001
                            value: environmentLightingIntensityLinear
                            onMoved: { 
                                viewportSettings.environmentLightingIntensity = Math.pow(10,value) - 0.01
                                // pow(10,-2) == 0.01 -> we subtract 0.01 to be able to set a value of 0.0 via the slider
                                environmentLightingIntensityLinear = value 
                            }
                        }

                        TextField {
                            implicitWidth: 60
                            validator: DoubleValidator {
                                bottom: Math.pow(10, environmentLightingIntensitySlider.from) - 1
                                top: Math.pow(10, environmentLightingIntensitySlider.to) - 1
                                decimals: 5
                                locale: Qt.locale().name
                            }
                            text: significantDigits(viewportSettings.environmentLightingIntensity, 5)
                            
                            onEditingFinished: { 
                                viewportSettings.environmentLightingIntensity = Number.fromLocaleString(Qt.locale(), text) 
                                environmentLightingIntensityLinear = Math.log10(viewportSettings.environmentLightingIntensity + 0.01)
                                // If you want to use a specific base use: Math.log(x) / Math.log(base) 
                            }
                        }
                    }

                    // Front face
                    Components.PrefPanelRowComp {
                        leftCustomPadding: 52
                        Label {
                            id: frontface
                            text: "Front face"
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        ComboBox {
                            id: frontFaceComboBox
                            model: frontFaceValues
                            onActivated: {
                                viewportSettings.frontFace = currentText
                            }
                            implicitWidth: 70
                            implicitHeight: 32 // => actually results in 21px
                        }
                    }

                    // Add new HDR file
                    Components.PrefPanelRowComp {
                        leftCustomPadding: 52
                        Label {
                            id: addNewHDRFile
                            text: "Add new HDR file"
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Button {
                            text: "Add"
                            icon.source: "qrc:icons/ViewportSettings_HDR_add.svg"
                            icon.width: 18
                            icon.height: 18
                            implicitHeight: 20
                            leftPadding: 0
                            topPadding: -5 // this property indirectly controls the icon size
                            bottomPadding: -5 // this property indirectly controls the icon size; nagative value to get rid of text cutoff
                            topInset: 0 // this property has an effect on the actual size
                            bottomInset: 0
                            font.capitalization: Font.MixedCase
                            onClicked: {
                                viewportSettings.addNewHDRFile();
                            }
                            background: Rectangle {
                                color: addNewHandler.hovered ? Colors.light4 :  Colors.dark4
                            }
                            HoverHandler {
                                id: addNewHandler
                            }
                        }
                    }

                    // Delete current HDR file
                    Components.PrefPanelRowComp {
                        leftCustomPadding: 52
                        Label {
                            id: deleteCurrentHDRFile
                            text: "Delete current HDR file"
                            enabled: hdrImagesComboBox.currentText !== "Default"
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Button {
                            text: "Delete"
                            icon.source: "qrc:icons/ViewportSettings_HDR_trash.svg"
                            icon.width: 18
                            icon.height: 18
                            enabled: hdrImagesComboBox.currentText !== "Default"
                            implicitHeight: 20
                            leftPadding: 0
                            topPadding: -5 // this property indirectly controls the icon size
                            bottomPadding: -5 // this property indirectly controls the icon size; nagative value to get rid of text cutoff
                            topInset: 0 // this property has an effect on the actual size
                            bottomInset: 0
                            font.capitalization: Font.MixedCase
                            onClicked: {
                                viewportSettings.deleteCurrentHDRFile();
                            }
                            background: Rectangle {
                                color: deleteCurrentHandler.hovered ? Colors.light4 : Colors.dark4
                            }
                            HoverHandler {
                                id: deleteCurrentHandler
                            }
                        }
                    }
                }
            }


            // Light
            ColumnLayout {
                width: stackLayout.width
                spacing: 1

                // Environment light
                Components.PrefPanelRowComp {
                    Label {
                        id: environmentLightingLabel
                        text: "Environment lighting"
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Switch {
                        id: environmentLightingSwitch
                        onCheckedChanged: viewportSettings.environmentLighting = checked
                        checked: viewportSettings.environmentLighting
                    }
                }

                // Punctual lights
                Components.PrefPanelRowComp {
                    Label {
                        id: punctualLightsLabel
                        text: "Punctual lights"
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Switch {
                        id: punctualLightsSwitch
                        onCheckedChanged: viewportSettings.punctualLights = checked
                        checked: viewportSettings.punctualLights
                    }
                }
            }


            // Post Processing
            ColumnLayout {
                width: stackLayout.width
                spacing: 1
        
                // Tone Mapping
                Components.CollapsibleComp {
                    collapsibleText: "Tone mapping"
                    width: parent.width
                    spacing: 1
                    
                    RadioButton {
                        id: tmButton1
                        background: Rectangle {
                            color: Colors.dark2
                        }
                        text: "None"
                        leftPadding: 37
                        onCheckedChanged: viewportSettings.toneMapping = "None"
                        checked: true
                        implicitHeight: 30
                        implicitWidth: parent.width
                    }

                    RadioButton {
                        id: tmButton2
                        background: Rectangle {
                            color: Colors.dark2
                        }
                        text: "ACES Filmic Tone Mapping (Narkowicz)"
                        leftPadding: 37
                        onCheckedChanged: viewportSettings.toneMapping = "ACES Filmic Tone Mapping (Narkowicz)"
                        implicitHeight: 30
                        implicitWidth: parent.width
                    }

                    RadioButton {
                        id: tmButton3
                        background: Rectangle {
                            color: Colors.dark2
                        }
                        text: "ACES Filmic Tone Mapping (Hill)"
                        leftPadding: 37
                        onCheckedChanged: viewportSettings.toneMapping = "ACES Filmic Tone Mapping (Hill)"
                        implicitHeight: 30
                        implicitWidth: parent.width
                    }

                    RadioButton {
                        id: tmButton4
                        background: Rectangle {
                            color: Colors.dark2
                        }
                        text: "ACES Filmic Tone Mapping (Hill - Exposure Boost)"
                        leftPadding: 37
                        onCheckedChanged: viewportSettings.toneMapping = "ACES Filmic Tone Mapping (Hill - Exposure Boost)"
                        implicitHeight: 30
                        implicitWidth: parent.width
                    }

                    RadioButton {
                        id: tmButton5
                        background: Rectangle {
                            color: Colors.dark2
                        }
                        text: "PBR Neutral Tone Mapping"
                        leftPadding: 37
                        onCheckedChanged: viewportSettings.toneMapping = "PBR Neutral Tone Mapping"
                        implicitHeight: 30
                        implicitWidth: parent.width
                    }
                }

                // Exposure
                Components.PrefPanelRowComp {
                    width: stackLayout.width
                    Label {
                        id: exposure
                        text: "Exposure"
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Slider {
                        id: exposureSlider
                        implicitWidth: Math.max(stackLayout.width-250, 200) 
                        from: 21.0
                        to: -6.0
                        stepSize: 0.1
                        value: exposureValue100
                        onMoved: { 
                            // Function plot: https://www.wolframalpha.com/input?i=1.0%2F%28+2%5Ey%29+from+-6+to+21
                            // Convert EV100 to photometric exposure (saturation based sensitivity)
                            // See https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf chapter 5.1.2
                            viewportSettings.exposure = 1.0 / Math.pow(2.0, value)
                            exposureValue100 = value 
                        }
                    }

                    TextField {
                        implicitWidth: 70
                        validator: DoubleValidator {
                            bottom:  1.0/Math.pow(2.0, exposureSlider.from)
                            top: 1.0/Math.pow(2.0, exposureSlider.to)
                            decimals: 5
                            locale: Qt.locale().name
                        }
                        text: significantDigits(viewportSettings.exposure, 5)
                          
                        onEditingFinished: {
                            viewportSettings.exposure = Number.fromLocaleString(Qt.locale(), text) 
                            exposureValue100 = -1.4427 * Math.log(viewportSettings.exposure) 
                            // Inverse function of 1.0 / pow(2.0, x) 
                            // https://www.wolframalpha.com/input?i=invert+1.0%2Fpow%282.0%2Cx%29+
                        }
                    }
                }
            }


            // Extensions
            ColumnLayout {
                width: stackLayout.width
                spacing: 1

                // KHR_materials_specular glossiness
                Components.PrefPanelRowComp {
                    Label {
                        text: "KHR_materials_pbrSpecularGlossiness"
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Switch {
                        onCheckedChanged: viewportSettings.pbrSpecularGlossiness = checked
                        checked: viewportSettings.pbrSpecularGlossiness
                    }
                }
                // KHR_materials_unlit
                Components.PrefPanelRowComp {
                    Label {
                        text: "KHR_materials_unlit"
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Switch {
                        onCheckedChanged: viewportSettings.unlit = checked
                        checked: viewportSettings.unlit
                    }
                }
                // KHR_materials_clearcoat
                Components.PrefPanelRowComp {
                    Label {
                        text: "KHR_materials_clearcoat"
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Switch {
                        onCheckedChanged: viewportSettings.clearCoat = checked
                        checked: viewportSettings.clearCoat
                    }
                }

                // KHR_materials_sheen
                Components.PrefPanelRowComp {
                    Label {
                        text: "KHR_materials_sheen"
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Switch {
                        onCheckedChanged: viewportSettings.sheen = checked
                        checked: viewportSettings.sheen
                    }
                }

                // KHR_materials_transmission
                Components.PrefPanelRowComp {
                    Label {
                        text: "KHR_materials_transmission"
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Switch {
                        id: transmissionSwitch
                        onCheckedChanged: viewportSettings.transmission = checked
                        checked: viewportSettings.transmission
                    }
                }

                // KHR_materials_volume
                Components.PrefPanelRowComp {
                    Label {
                        text: "KHR_materials_volume"
                        enabled: transmissionSwitch.checked
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Switch {
                        onCheckedChanged: viewportSettings.volume = checked
                        checked: viewportSettings.volume
                        enabled: transmissionSwitch.checked
                    }
                }

                // KHR_materials_ior
                Components.PrefPanelRowComp {
                    Label {
                        text: "KHR_materials_ior"
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Switch {
                        onCheckedChanged: viewportSettings.ior = checked
                        checked: viewportSettings.ior
                    }
                }

                // KHR_materials_specular
                Components.PrefPanelRowComp {
                    Label {
                        text: "KHR_materials_specular"
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Switch {
                        onCheckedChanged: viewportSettings.specular = checked
                        checked: viewportSettings.specular
                    }
                }

                // KHR_materials_emissive_strength
                Components.PrefPanelRowComp {
                    Label {
                        text: "KHR_materials_emissive_strength"
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Switch {
                        onCheckedChanged: viewportSettings.emissiveStrength = checked
                        checked: viewportSettings.emissiveStrength
                    }
                }
            

                // KHR_materials_iridescence
                Components.PrefPanelRowComp {
                    Label {
                        text: "KHR_materials_iridescence"
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Switch {
                        onCheckedChanged: viewportSettings.iridescence = checked
                        checked: viewportSettings.iridescence
                    }
                }
            }

            // Variants
            ColumnLayout {
                width: stackLayout.width
                spacing: 1

                // Material variants
                Components.CollapsibleComp {
                    collapsibleText: "Material variants"
                    spacing: 1

                    ListView {
                        id: materialVariantsList
                        implicitHeight: childrenRect.height
                        width: stackLayout.width 
                        orientation: ListView.Vertical
                        spacing: 1

                        model: materialVariantsModel
                        delegate: RadioButton {
                            id: mvButton
                            background: Rectangle {
                                color: Colors.dark2
                            }
                            width: stackLayout.width
                            implicitHeight: 30
                            leftPadding: 37
                            height: 30
                            text: name
                            checked: selected
                            onCheckedChanged: viewportSettings.setMaterialVariants(index)
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: viewportSettings
        function onSetupHDRComboBox(hdrFileList)
        {
            hdrFilesModel.clear();
            for(let i = 0; i < hdrFileList.length; ++i)
            {
                hdrFilesModel.append({ text: hdrFileList[i] });
            }

            onFrontFaceChanged()
            onCurrentHDRFileChanged()
        }

        function onFrontFaceChanged()
        {
            frontFaceComboBox.currentIndex = frontFaceComboBox.indexOfValue(viewportSettings.frontFace)
        }

        function onCurrentHDRFileChanged()
        {
            hdrImagesComboBox.currentIndex = hdrImagesComboBox.indexOfValue(viewportSettings.currentHDRFile)
        }

        function onEnvironmentLightingIntensityChanged()
        {
            environmentLightingIntensityLinear = Math.log10(viewportSettings.environmentLightingIntensity + 0.01)
        }

        function onMaterialVariantsChanged(selectedIndex, variantsList)
        {
            materialVariantsModel.clear();
            for(let i = 0; i < variantsList.length; ++i)
            {
                materialVariantsModel.append({ name: variantsList[i], selected: i == selectedIndex})
            }
        }
    }
}
