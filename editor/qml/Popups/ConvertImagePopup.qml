import QtQuick 2.15
import QtQuick.Controls
import QtQuick.Layouts
import QtQml
import "../Colors.js" as Colors

import "." as Popups
import "../Components/inspectorTypes" as InspectorComponents

import Model 1.0 as Model

Popups.PopupTemplate {
    id: popup
    title: "Image Convert"
    contentComponent: component
    height: 455 + 30
    tooltip: "Conversions are always destructive."
    
    Component {
        id: component
        Rectangle {
            id: convert
            color: Colors.dark1
            anchors.fill: parent

            function getAspectRatio() {
                if(!selection.length) return 1
                if(!imageMetadata[selection[0].uuid]) return 1

                return imageMetadata[selection[0].uuid].width / imageMetadata[selection[0].uuid].height
            }

            function initFormatOBJ() {
                let formatObj = {"format": "PNG", "compression": "none"}

                let currentImgMimeType = imageMetadata[selection[0].uuid].mimeType;
                if (currentImgMimeType === "image/png") {
                    formatObj.format = "PNG"
                } else if (currentImgMimeType === "image/jpeg" || currentImgMimeType === "image/jpg") {
                    formatObj.format = "JPEG"
                } else if (currentImgMimeType === "image/ktx2" || currentImgMimeType === "image/ktx") {
                    formatObj.format = "KTX2"
                } else if (currentImgMimeType === "image/webp") {
                    formatObj.format = "WEBP"
                }
                formatObj.compression = "none"

                if(formatObj.format === "KTX2") {
                    formatObj.compression = "ETC1S + BasisLZ"
                }

                return formatObj;
            }

            function imageMimeTypeToComboboxIDX() {
                let currentImgMimeType = imageMetadata[selection[0].uuid].mimeType;
                if (currentImgMimeType === "image/png") {
                    return 0
                } else if (currentImgMimeType === "image/jpeg" || currentImgMimeType === "image/jpg") {
                    return 1
                } else if (currentImgMimeType === "image/ktx2" || currentImgMimeType === "image/ktx") {
                    return 2
                } else if (currentImgMimeType === "image/webp") {
                    return 3
                }
                else {
                    return 0
                }
            }

            function getFormatModel(formatObj) {
                let rowsArray = {"table": [], "sliderList": undefined}
                if (formatObj.format === "KTX2") {
                    rowsArray.table = [
                        { label: "KTX Compression", value: ["ETC1S + BasisLZ", "UASTC", "UASTC + Zstandard"] }, 
                        { label: "Color Space", value: ["sRGB", "Linear"], index: 0 } // maps to transferFunction in parameters
                    ] 

                    if (formatObj.compression === "ETC1S + BasisLZ") { 
                        rowsArray.sliderList = [{name: "BasisLZ Compression Level", minValue: 0, maxValue: 5, value: 1}, 
                            {name: "Quality Level (1-255)", minValue: 1, maxValue: 255, value: 128}]
                    } else if (formatObj.compression === "UASTC") { 
                        rowsArray.sliderList = [ {name: "Quality Level (0-4)", minValue: 0, maxValue: 4, value: 2}, 
                            {name: "ROD (0.001-10)", minValue: 0, maxValue: 10, value: 1.0}]
                    } else if (formatObj.compression === "UASTC + Zstandard") { 
                        rowsArray.sliderList = [{name: "Quality Level (0-4)", minValue: 0, maxValue: 4, value: 2}, 
                            {name: "ROD (0.001-10)", minValue: 0, maxValue: 10, value: 1.0}, 
                            {name: "Zstandard Compression (1-22)", minValue: 1, maxValue: 22, value: 3}] 
                            // 560 
                    } else {
                        rowsArray.sliderList = undefined
                    }
                } else {
                    rowsArray.table = [
                        //{ label: "Preserve Aspect Ratio", value: true }, 
                        //{ label: "Power of 2", value: false }, 
                        //{ label: "Force Even Resolution", value: false } 
                    ] 
                    // 310 
                    if (formatObj.format === "JPEG" || formatObj.format === "WEBP") { 
                        rowsArray.sliderList = [{name: "Compression", value: 90.0}]
                        // 350 
                    } else {
                        rowsArray.sliderList = undefined
                    }
                } 
                if (formatObj.format === "JPEG") { 
                    rowsArray.sliderList = [{name: "Compression", value: 90.0}]
                }

                return rowsArray
            }

            function accept(){
                let parameters = {};

                if (formatObj.format === "PNG"){
                    parameters["mimeType"] = "image/png"

                } else if (formatObj.format === "JPEG"){
                    parameters["mimeType"] = "image/jpeg"
                    parameters["quality"] = formatModel.sliderList[0].value   
                } else if (formatObj.format === "WEBP"){
                    parameters["mimeType"] = "image/webp"
                    parameters["quality"] = formatModel.sliderList[0].value   
                } else if (formatObj.format === "KTX2"){
                    parameters["mimeType"] = "image/ktx2"
                    parameters["transferFunction"] = formatModel.table[1].index === 0 ? 2 : 1 
                    if (formatObj.compression === "ETC1S + BasisLZ"){
                        parameters["compression"] = "ETC1S"
                        parameters["etc1sQualityLevel"] = formatModel.sliderList[1].value  
                        parameters["etc1sCompressionLevel"] = formatModel.sliderList[0].value 
                    } else {
                        parameters["compression"] = "UASTC"
                        parameters["uastcQualityLevel"] = formatModel.sliderList[0].value 
                        parameters["uastcRDO"] = formatModel.sliderList[1].value 
                        if (formatObj.compression === "UASTC + Zstandard"){
                            parameters["superCompression"] = "ZSTANDARD" 
                            parameters["zstdCompressionLevel"] = formatModel.sliderList[2].value 
                        }
                    }
                }

                parameters["width"] = Number(convertImageWidth.text || convertImageWidth.placeholderText)
                parameters["height"] = Number(convertImageHeight.text || convertImageHeight.placeholderText)

                Model.Operations.runOperationOnSelection("convert_image", parameters).then(function (message) {
                    if (message !== "") {
                        applicationData.info(message)
                    } else {
                        applicationData.success("Image conversion successful")
                    }
                    }).catch(function (message) {
                        if (message === "") {
                            message = "Operation failed"
                        }
                        applicationData.error(message)
                    })
            }
            
            property var imageMetadata: Model.Observables.imageMetadata
            property var selection: Model.Observables.selection

            property var aspectRatio: getAspectRatio();

            property var formatObj: initFormatOBJ()
            property var formatModel: getFormatModel(formatObj)

            Rectangle {
                id: imageDimensions
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 5
                anchors.rightMargin: 5
                height: children[0].height
                color: Colors.dark2

                RowLayout {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right

                    Text {
                        Layout.margins: 5
                        text: "Image Resolution"
                        color:  Colors.light1
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Text {
                        Layout.margins: 5
                        text: "W"
                        font.pixelSize: 11
                        color:  Colors.light4
                    }

                    TextField {
                        id: convertImageWidth
                        implicitWidth: 60
                        font.pixelSize: 12
                        color: Colors.light1
                        text: imageMetadata[selection[0].uuid].width
                        placeholderText: "512"
                        placeholderTextColor: Colors.light4
                        background: Rectangle { color: Colors.dark1 }
                        onEditingFinished: {
                            if (keepAspectRatio.checked) {
                                convertImageHeight.text = Math.round(Number(convertImageWidth.text) / aspectRatio)
                            }
                        }
                    }

                    Text {
                        Layout.margins: 5
                        text: "H"
                        font.pixelSize: 11
                        color:  Colors.light4
                    }

                    TextField {
                        id: convertImageHeight
                        implicitWidth: 60
                        Layout.margins: 5
                        font.pixelSize: 12
                        color: Colors.light1
                        text: imageMetadata[selection[0].uuid].height
                        placeholderText: "512"
                        placeholderTextColor: Colors.light4
                        background: Rectangle { color: Colors.dark1 }
                        onEditingFinished: {
                            if (keepAspectRatio.checked) {
                                convertImageWidth.text = Math.round(Number(convertImageHeight.text) * aspectRatio)
                            }
                        }
                    }
                }
            }

            Rectangle {
                id: keepAspectRatioRect
                anchors.top: imageDimensions.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 5
                anchors.rightMargin: 5
                anchors.topMargin: 1
                height: 30
                color: Colors.dark2

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 5
                    text: "Preserve aspect ratio"
                    color:  Colors.light1
                }

                CheckBox {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    checked: true
                    id: keepAspectRatio
                }

                ToolTip.delay: 1000
                ToolTip.timeout: 5000
                ToolTip.visible: keepAspectHoverHandler.hovered
                ToolTip.text: qsTr("If checked preserve the aspect ratio of the image")

                HoverHandler {
                    id: keepAspectHoverHandler
                }
            }

            Rectangle {
                id: mimeType
                anchors.top: keepAspectRatioRect.bottom
                anchors.left: parent.left
                anchors.right: parent.right 
                anchors.leftMargin: 5
                anchors.rightMargin: 5
                anchors.topMargin: 1
                color: Colors.dark2 
                height: 30
                Text {
                    id: labelLeft
                    text: "Image Format"
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 5
                    color:  Colors.light1 
                }
                ComboBox {
                    id: comboFormatOption
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right
                    anchors.rightMargin: 5
                    model: ["PNG", "JPEG", "KTX2", "WEBP"]
                    currentIndex: imageMimeTypeToComboboxIDX()
                    width: 180
                    flat: true
                    implicitHeight: 21
                    topPadding: -4 
                    bottomPadding: -4 
                    indicator: Image {
                        x: 160
                        y: 1
                        width: 20
                        height: 20
                        source: "qrc:icons/Icon_down_20px.svg"
                    }
                    font.pixelSize: 12
                    background: Rectangle {
                        anchors.fill: parent
                        color: Colors.dark4
                    }
                    onActivated: {
                        convert.formatObj.format = comboFormatOption.model[currentIndex] 
                        convert.formatObj.compression = convert.formatObj.format === "KTX2" ? "ETC1S + BasisLZ" : "none"

                        convert.formatModel = convert.getFormatModel(convert.formatObj)
                    }
                    hoverEnabled: true
                }
            }

            // drop down table
            ListView {
                id: convertTable
                anchors.top: mimeType.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                interactive: false
                height: childrenRect.height
                anchors.leftMargin: 5
                anchors.rightMargin: 5
                width: parent.width
                spacing: 1
                clip: true 
                model: convert.formatModel.table
                delegate: convertDelegate 
            }

            // slider list
            ListView {
                id: sliderList
                anchors.top: convertTable.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                interactive: false
                anchors.leftMargin: 5
                anchors.rightMargin: 5
                anchors.topMargin: 1
                height: childrenRect.height
                width: parent.width
                spacing: 1
                model: convert.formatModel.sliderList
                delegate: InspectorComponents.SliderPopupRow {
                    property int indexOfThisDelegate: index
                    anchors.left: parent.left
                    anchors.right: parent.right
                    labelTitle: model.modelData.name ? model.modelData.name : model.modelData
                    minValue: model.modelData.minValue ? model.modelData.minValue : 0
                    maxValue: model.modelData.maxValue ? model.modelData.maxValue : 100
                    valueNum: model.modelData.value
                    stepSize: model.modelData.name === "ROD (0.001-10)" ? 0.001 : 1
                    onValueNumChanged: {
                        if (model.modelData.value != valueNum){
                            model.modelData.value = valueNum
                            formatModel.sliderList[indexOfThisDelegate].value = valueNum
                        }
                        if (formatModel.sliderList[indexOfThisDelegate].value != valueNum){
                            formatModel.sliderList[indexOfThisDelegate].value = valueNum
                        }
                    }
                }
            }

            Component {
                id : convertDelegate
                Rectangle {
                    id: rowDelegate
                    anchors.left: parent.left
                    anchors.right: parent.right
                    implicitWidth: convertTable.width
                    implicitHeight: 30
                    color: Colors.dark2 
                    Text {
                        id: labelLeft
                        text: convert.formatModel.table[index].label
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.leftMargin: 5
                        color:  Colors.light1 
                    }

                    ComboBox {
                        id: comboOption
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right
                        anchors.rightMargin: 5
                        model: convert.formatModel.table[index].value
                        width: 180
                        flat: true
                        implicitHeight: 21
                        topPadding: -4 
                        bottomPadding: -4 
                        indicator: Image {
                            x: 160
                            y: 1
                            width: 20
                            height: 20
                            source: "qrc:icons/Icon_down_20px.svg"
                        }
                        font.pixelSize: 12
                        background: Rectangle {
                            anchors.fill: parent
                            color: Colors.dark4
                        }
                        onActivated: {
                            if (labelLeft.text === "KTX Compression") {
                                convert.formatObj.compression = comboOption.model[currentIndex]
                            }

                            convert.formatModel = convert.getFormatModel(convert.formatObj)
                            convert.formatModel.table[1].index = currentIndex
                        }
                        hoverEnabled: true
                    }
                }
            }
        }
    }
}