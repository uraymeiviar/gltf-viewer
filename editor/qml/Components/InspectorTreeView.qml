import "../js/thirdparty.qml.mjs" as Thirdparty;
import "../js/GlobalFunctions.js" as GlobalFunctions
import "../Components" as Components
import "./inspectorTypes" as InspectorRow
import "../Colors.js" as Colors

import QtQuick
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.15
import Qt.labs.qmlmodels 1.0

import Model 1.0 as Model


TreeView {
    id: control
    width: columnWidthInt
    height: childrenRect.height + 30 
    anchors.fill: parent  // important! it is needed otherwise it will show only one row and not the full treeView!
    rowSpacing: 1  // important, as default is 0
    reuseItems: false 

    model: undefined

    // dynamic width, changing based on the inspector panel's width 
    property var columnWidthInt: scrollInspector.width - mainRect.scrollIndicator
    property var columnWidth: columnWidthInt
    columnWidthProvider: function () { 
        return columnWidth
    }
    onColumnWidthIntChanged: {
        control.columnWidth = columnWidthInt
        control.forceLayout(); // important, or it won't update for all of the rows 
    }
    
    // dynamic height, changing based on the content and the width 
    property var rowHeights: Array(control.rows).fill(30)
    rowHeightProvider: function (row) { 
        const item = control.itemAtCell(0, row)
        let heightRow = 30 // default value 
        if (item.isCollapsible) {
            // changing height based on the expanded status only for array (list comp)
            heightRow = item.collapsed ? 30 : item.expandedHeight 
        } else if (item.objType) {
            // changing height based on depth and expanded status only for obj 
            heightRow = item.depth === 0 ? 26 : 22 
        } else {
            // changing height based on the height of the row 
            heightRow = item.heightRow ? item.heightRow : 30 
        }
        return rowHeights[row] = heightRow 
    }


    // display the row label 
    function displayLabel(string) {
        let title = ""
        const capitalLetters = /[A-Z]/g;  // capitalized letters
        const exemptionCapitalized = ["COLOR", "JOINT", "POSITION", "WEIGHT", "TEXCOORD", "TANGENT", "NORMAL"] // exemptions 
        if (exemptionCapitalized.find((el) => string.includes(el))) { 
            title = string.charAt(0).toUpperCase() + string.slice(1).toLowerCase()
        } else if (string.includes("KHR_materials_variants")) {
            // use whole name of extension instead of truncating it as for the other material extensions
            title = "Material Variants";
        } else if (string.includes("KHR_materials")) { 
            // first letter capitalized + space instead of undescore and letter after underscore capitalized
            // result is e.g.: KHR_materials_specular -> Specular
            title = string.charAt(14).toUpperCase() + string.slice(15).replace(/(_)./g, s => ' ' + s.slice(-1).toUpperCase());
        } else if (string.includes("KHR")) { 
            // first letter capitalized + space instead of undescore and letter after underscore capitalized
            // result is e.g.: KHR_draco_mesh_compression -> Draco Mesh Compression
            title = string.charAt(4).toUpperCase() + string.slice(5).replace(/(_)./g, s => ' ' + s.slice(-1).toUpperCase());
        } else {
            // first letter capitalized + adding space before any capital letter to divide words 
            title = string.charAt(0).toUpperCase() + string.slice(1).replace(/[A-Z]/g, ' $&');
        }
        return title
    }

    // show the correct label for each index of the input array, if color rgba, if vector xyzw 
    function getCorrectLabel(valueType, index) {
        const uvLabel = ["U", "V"]
        const colorLabel = ["R", "G", "B", "A"]
        const vectorLabel = ["X", "Y", "Z", "W"]
        if (valueType === "uv") {
            return uvLabel[index]
        } else if (valueType === "color") {
            return colorLabel[index]
        } else if (valueType === "vector") {
            return vectorLabel[index]
        } else {
            return ""
        }
    }

    MouseArea { // inspector's row inputs loose focus if clicking outside the texfield
        anchors.fill: parent
        onClicked: {
            focus = true
        }
    }
    
    QtObject {
        // Don't leak API that might not be support
        // by all styles into the public API.
        id: backgroundObject
        property int hoveredColumn: -1
        property int hoveredRow: -1

    } 

    HoverHandler {
        onPointChanged: {
            if (point.position === Qt.point(0, 0)) {
                // Work around HoverHandler bug that
                // happens when opening a popup Menu.
                return
            }
            var posInTreeView = control.mapFromItem(parent, point.position)
            //backgroundObject.hoveredColumn = control.columnAtX(posInTreeView.x, true)
            //backgroundObject.hoveredRow = control.rowAtY(posInTreeView.y, true)
        }
        onHoveredChanged: {
            if (!hovered) {
                backgroundObject.hoveredColumn = -1
                backgroundObject.hoveredRow = -1
            }
        }
    }


    // the treeView uses a delegateChooser to generate different types of rows. 
    DelegateChooser { 
        id: chooser
        role: "decoration" // params that specifiy the ui type
        
        DelegateChoice {   // right column component for object
            roleValue: "object";
            column: 0
            delegate: InspectorRow.ObjectRow {
                infos: model.display
                operations: infos.operations
                title: infos.title
                path: infos.path
                objValue: infos.value
                extensionLabel: infos.extensionPrefix ? infos.extensionPrefix : ""
                severity: GlobalFunctions.getMaxSeverity(infos.validationResults)
                color: GlobalFunctions.getValidationBackgroundColor(depth === 0 ? Colors.dark3 : depth === 1 ? Colors.dark305 : Colors.dark205, severity)
                tooltipColor: GlobalFunctions.getValidationTooltipColor(Colors.light3, severity)
                issueMessage: GlobalFunctions.combineValidationText(severity, infos.validationResults)
                issueIcon: GlobalFunctions.getValidationIcon(severity)
                isExpandedFromModel: infos.isExpanded
            }
        }

        DelegateChoice {   // right column component for pointer
            roleValue: "pointer"; 
            column: 0
            delegate: InspectorRow.PointerComboRow {
                function getCorrectIcon(model) {
                    if (model.textureSource !== undefined) {
                        return "image://provider/" + model.textureSource;
                    } else {
                        return GlobalFunctions.getObjIcon(model.icon)
                    }
                }
                displayModel: model.display
                sourceIcon: getCorrectIcon(displayModel)
                isIconPreviewImage: Boolean(displayModel.textureSource)
                severity: GlobalFunctions.getMaxSeverity(displayModel.validationResults)
                color: GlobalFunctions.getValidationBackgroundColor(Colors.dark2, severity)
                tooltipColor: GlobalFunctions.getValidationTooltipColor(Colors.light3, severity)
                issueMessage: GlobalFunctions.combineValidationText(severity, displayModel.validationResults)
                issueIcon: GlobalFunctions.getValidationIcon(severity)

            }
        }

        DelegateChoice {   // right column component for enum
            roleValue: "enum"; 
            column: 0
            delegate: InspectorRow.EnumComboRow {
                displayModel: model.display
                severity: GlobalFunctions.getMaxSeverity(displayModel.validationResults)
                color: GlobalFunctions.getValidationBackgroundColor(Colors.dark2, severity)
                tooltipColor: GlobalFunctions.getValidationTooltipColor(Colors.light3, severity)
                issueMessage: GlobalFunctions.combineValidationText(severity, displayModel.validationResults)
                issueIcon: GlobalFunctions.getValidationIcon(severity)
            }
        }


        DelegateChoice {   // right column component for boolean
            roleValue: "boolean"; 
            column: 0
            delegate: InspectorRow.BooleanRow {
                infos: model.display
                title: infos.title
                severity: GlobalFunctions.getMaxSeverity(infos.validationResults)
                color: GlobalFunctions.getValidationBackgroundColor(Colors.dark2, severity)
                tooltipColor: GlobalFunctions.getValidationTooltipColor(Colors.light3, severity)
                issueMessage: GlobalFunctions.combineValidationText(severity, infos.validationResults)
                issueIcon: GlobalFunctions.getValidationIcon(severity)
            }
        }

        DelegateChoice { // right column component for array
            roleValue: "array"; 
            column: 0
            delegate: InspectorRow.ListRow {
                infos: model.display
                path: infos.path
                items: infos.items
                operations: infos.operations !== undefined ? infos.operations : {}
                title: infos.title
                severity: GlobalFunctions.getMaxSeverity(infos.validationResults)
                color: GlobalFunctions.getValidationBackgroundColor(Colors.dark2, severity)
                tooltipColor: GlobalFunctions.getValidationTooltipColor(Colors.light3, severity)
                issueMessage: GlobalFunctions.combineValidationText(severity, infos.validationResults)
                issueIcon: GlobalFunctions.getValidationIcon(severity)
                isExpandedFromModel: infos.isExpanded
            }
        }

        DelegateChoice {   // right column component for vector
            roleValue: "vector";  
            column: 0
            delegate: InspectorRow.ArrayInputRow {
                infos: model.display
                operations: infos.operations
                path: infos.path
                valueType: {
                    if (infos.maxItems === 2 && infos.minItems === 2) {
                        "uv"
                    } else {
                        infos.type
                    }
                } 
                defaultValue: infos.default ? infos.default  : undefined
                amountOfItems: infos.minItems ? infos.minItems : 1
                arrayValue: infos.value
                title: infos.title
                severity: GlobalFunctions.getMaxSeverity(infos.validationResults)
                color: GlobalFunctions.getValidationBackgroundColor(Colors.dark2, severity)
                tooltipColor: GlobalFunctions.getValidationTooltipColor(Colors.light3, severity)
                issueMessage: GlobalFunctions.combineValidationText(severity, infos.validationResults)
                issueIcon: GlobalFunctions.getValidationIcon(severity)
            }
        }

        DelegateChoice {   // right column component for color
            roleValue: "color";  
            column: 0
            delegate: InspectorRow.ArrayInputRow {
                infos: model.display
                operations: infos.operations
                path: infos.path
                valueType: infos.type
                defaultValue: infos.default ? infos.default  : undefined
                amountOfItems: infos.minItems ? infos.minItems : 1
                arrayValue: infos.value
                title: infos.title
                severity: GlobalFunctions.getMaxSeverity(infos.validationResults)
                color: GlobalFunctions.getValidationBackgroundColor(Colors.dark2, severity)
                tooltipColor: GlobalFunctions.getValidationTooltipColor(Colors.light3, severity)
                issueMessage: GlobalFunctions.combineValidationText(severity, infos.validationResults)
                issueIcon: GlobalFunctions.getValidationIcon(severity)
            }
        }

        DelegateChoice {   // right column component for number 
            roleValue: "number";  
            column: 0
            delegate: InspectorRow.NumberInputRow {
                infos: model.display
                severity: GlobalFunctions.getMaxSeverity(infos.validationResults)
                color: GlobalFunctions.getValidationBackgroundColor(Colors.dark2, severity)
                tooltipColor: GlobalFunctions.getValidationTooltipColor(Colors.light3, severity)
                issueMessage: GlobalFunctions.combineValidationText(severity, infos.validationResults)
                issueIcon: GlobalFunctions.getValidationIcon(severity)
                operations: infos.operations
                defaultValue: infos.default ? infos.default  : 0
                path: infos.path
                numberValue: infos.value 
                title: infos.title
            }
        }

        DelegateChoice {   // right column component for string
            roleValue: "string";  
            column: 0
            delegate: InspectorRow.StringInputRow {
                infos: model.display
                operations: infos.operations
                defaultValue: infos.default
                path: infos.path
                stringValue: infos.value 
                title: infos.title
                severity: GlobalFunctions.getMaxSeverity(infos.validationResults)
                color: GlobalFunctions.getValidationBackgroundColor(Colors.dark2, severity)
                tooltipColor: GlobalFunctions.getValidationTooltipColor(Colors.light3, severity)
                issueMessage: GlobalFunctions.combineValidationText(severity, infos.validationResults)
                issueIcon: GlobalFunctions.getValidationIcon(severity)
            }
        }

        DelegateChoice {   // right column component for readonly
            roleValue: "readonly";  
            column: 0
            delegate: InspectorRow.LabelRow {
                infos: model.display
                stringValue: String(infos.value) || String(infos.default) 
                operations: infos.operations
                title: infos.title
                severity: GlobalFunctions.getMaxSeverity(infos.validationResults)
                color: GlobalFunctions.getValidationBackgroundColor(Colors.dark2, severity)
                tooltipColor: GlobalFunctions.getValidationTooltipColor(Colors.light3, severity)
                issueMessage: GlobalFunctions.combineValidationText(severity, infos.validationResults)
                issueIcon: GlobalFunctions.getValidationIcon(severity)
            }
        }

        DelegateChoice {   // reuse readonly for null type
            roleValue: "null";  
            column: 0
            delegate: InspectorRow.LabelRow {
                infos: model.display
                stringValue: String(infos.value) || String(infos.default) 
                operations: infos.operations
                title: infos.title
                severity: GlobalFunctions.getMaxSeverity(infos.validationResults)
                color: GlobalFunctions.getValidationBackgroundColor(Colors.dark2, severity)
                tooltipColor: GlobalFunctions.getValidationTooltipColor(Colors.light3, severity)
                issueMessage: GlobalFunctions.combineValidationText(severity, infos.validationResults)
                issueIcon: GlobalFunctions.getValidationIcon(severity)
            }
        }

       DelegateChoice {   // right column component for readonly
            roleValue: "integer";  
            column: 0
            delegate: InspectorRow.NumberInputRow {
                infos: model.display
                operations: infos.operations
                defaultValue: infos.default ? infos.default  : 0
                path: infos.path
                numberValue: infos.value 
                title: infos.title
                severity: GlobalFunctions.getMaxSeverity(infos.validationResults)
                color: GlobalFunctions.getValidationBackgroundColor(Colors.dark2, severity)
                tooltipColor: GlobalFunctions.getValidationTooltipColor(Colors.light3, severity)
                issueMessage: GlobalFunctions.combineValidationText(severity, infos.validationResults)
                issueIcon: GlobalFunctions.getValidationIcon(severity)
            }
        }

        DelegateChoice {   // right column component for matrix
            roleValue: "matrix";  
            column: 0
            delegate: InspectorRow.MatrixRow {
                infos: model.display
                matrixArray: infos.value ? infos.value : infos.default
                operations: infos.operations
                title: infos.title
                severity: GlobalFunctions.getMaxSeverity(infos.validationResults)
                color: GlobalFunctions.getValidationBackgroundColor(Colors.dark2, severity)
                tooltipColor: GlobalFunctions.getValidationTooltipColor(Colors.light3, severity)
                issueMessage: GlobalFunctions.combineValidationText(severity, infos.validationResults)
                issueIcon: GlobalFunctions.getValidationIcon(severity)
                isExpandedFromModel: infos.isExpanded
            }
        }

        DelegateChoice {
            roleValue: "imagePreview"
            column: 0
            delegate: InspectorRow.ImagePreviewRow {
                infos: model.display
            }
        }

        DelegateChoice {   // right column component for undefined 
            roleValue: undefined; 
            column: 0
            delegate: Rectangle {
                implicitHeight: 30
                implicitWidth: columnWidthInt
                width: columnWidthInt
                height: 30
                color: Colors.dark2
                Button {  
                    flat: true
                    icon.source: "qrc:/icons/Icon_Add.svg"
                    padding: 0
                    topInset: 0
                    bottomInset: 0
                    implicitWidth: 10
                    implicitHeight: 10
                    anchors.left: parent.left
                    anchors.leftMargin: 3
                    anchors.top: parent.top
                    anchors.topMargin: 8
                    onClicked: {
                        console.log("HERE undefined INFOS: " + JSON.stringify(infos))
                    }
                }
                Text {
                    id: infoLabel
                    anchors.left: parent.left
                    anchors.leftMargin: 50
                    text: infos.type
                    anchors.verticalCenter: parent.verticalCenter
                    clip: true
                    color: Colors.red
                }
            }
        }

    }

    delegate: chooser

}
