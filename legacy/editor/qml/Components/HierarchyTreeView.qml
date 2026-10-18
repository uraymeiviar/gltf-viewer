
import "../js/thirdparty.qml.mjs" as Thirdparty;
import "../Components" as Components
import "../Colors.js" as Colors

import Model 1.0 as Model
import QtQuick
import Qt.labs.qmlmodels 1.0

import HierarchyModel 1.0

TreeView {
    id: control
    width: columnWidthInt
    implicitWidth: columnWidthInt
    anchors.fill: parent

    property int indent: 18
    property int columnPadding: 20

    property var backgroundOdd: Colors.dark2
    property var backgroundCurrent: Colors.darkgreen2
    property var backgroundHovered: Colors.dark3 // the background color of the current item when hovered (or the whole row, if selectionMode is TreeView.List)

    property var backgroundDragForbidden: Colors.red2
    property var backgroundSameNameOfCurrent: Colors.darkgreen3
    property var backgroundSecondarySelection: Colors.darkgreen3

    // passing var from hierarchy to HierarchyCollapsibleLineComp.
    property int dragTargetRow: -1
    property int dragTargetColumn: -1
    property int dragSourceRow: -1
    property int dragSourceColumn: -1
    property bool isDragAllowed: false

    property int enterExitEvents: 0 // fixes conflicting order of enter/exit events when dragging items
    property bool popupOpen: false
    property bool isHierarchy: true

    property var rootInfoVisibility: root.infoVisibility
    
    model: HierarchyModel {}
    
    property var treeSelection: Model.Observables.selection
    property var gltf: Model.Observables.gltf 
    property var listeners: [];
    
    signal openContextMenu(var globalPosition, var type, bool isHeading, string path)

    // dynamic width, changing based on the hierarchy panel's width 
    property var columnWidthInt: hierarchyScroll.width - root.scrollIndicator
    property var columnWidths: [columnWidthInt]
    columnWidthProvider: function (column) { 
        return columnWidths[column] 
    }
    onColumnWidthIntChanged: {
        control.columnWidths[0] = columnWidthInt
        control.forceLayout(); // important, or it won't update for all of the rows 
    }

    QtObject {
        // Don't leak API that might not be support
        // by all styles into the public API.
        id: backgroundObject
        property int hoveredColumn: -1
        property int hoveredRow: -1

        function isSelected(selection, parentUUID, localPath) {
            if(selection.uuid === parentUUID && selection.localPath === localPath) {
                return true;
            } else {
                return false;
            }
        }

        function updateBackgroundColor(column, row, parentUUID, localPath, isDefaultScene) {
            // if drag is happening and user hovers drag origin row
            if (dragSourceRow == row && dragSourceColumn == column)
            {
                return backgroundHovered;
            }
            // if drag is happening
            else if(dragSourceRow != -1)
            {
                if (dragTargetRow == row && dragTargetColumn == column && enterExitEvents > 0)
                {
                    return isDragAllowed ? backgroundCurrent : control.backgroundDragForbidden;
                }
            }
            // if row selected
            else if (treeSelection.some(e => {return isSelected(e, parentUUID, localPath)})) {
                if(isSelected(treeSelection[0], parentUUID, localPath)) {
                    return backgroundCurrent;
                } else {
                    return backgroundSecondarySelection;
                }
            }
            // on row hover
            else if (row === backgroundObject.hoveredRow
                && column === backgroundObject.hoveredColumn)
            {
                // ensure hover is not updated "through" the context menu
                if (!popupOpen) 
                {
                    return backgroundHovered;
                } 
                else
                {
                    return backgroundOdd;
                }
            }
            if (isDefaultScene) {
                return Colors.dark4;
            }
            return backgroundOdd;
        } 
    } 

    HoverHandler {
        onPointChanged: {
            if (point.position === Qt.point(0, 0)) {
                // Work around HoverHandler bug that
                // happens when opening a popup Menu.
                return
            }
            //var posInTreeView = control.mapFromItem(parent, point.position)
            var cell = control.cellAtPos(point.position, true)
            backgroundObject.hoveredColumn = cell.x
            backgroundObject.hoveredRow = cell.y
        }
        onHoveredChanged: {
            if (!hovered) {
                backgroundObject.hoveredColumn = -1
                backgroundObject.hoveredRow = -1
            }
        }
    }

    delegate: Components.HierarchyCollapsibleLineComp{
        id: treeNode 
        hierarchyData: JSON.parse(model.display)
        isExpanded: control.isExpanded(row)
        amountHierarchyLines: control.depth(row)
        color: backgroundObject.updateBackgroundColor(column, row, parentUUID, localPath, hierarchyData !== undefined && hierarchyData.isDefaultScene !== undefined && hierarchyData.isDefaultScene)
        
        onClicked: function(uuidTmp, localPathTmp, keyboardModifiers) {
            forceActiveFocus();

            let tmpSelection = {};
            tmpSelection["uuid"] = uuidTmp;
            tmpSelection["localPath"] = localPathTmp;

            let ctrlPressed = keyboardModifiers !== undefined && (keyboardModifiers & Qt.ControlModifier);

            // use ctrl to remove from selection
            // check if elment is already selected
            let elemFound = treeSelection.find(elem => {
                if(elem.uuid === uuidTmp && elem.localPath === localPathTmp) {
                    return true;
                }
                return false;
            });

            if (elemFound != undefined && ctrlPressed) {
                Model.Operations.runEditorOperation("editor.removeSelection", tmpSelection);
            }
            else {
                // add to selection if it already exists
                if (ctrlPressed) {
                    Model.Operations.runEditorOperation("editor.addSelection", tmpSelection);
                } else {
                    Model.Operations.runEditorOperation("editor.setSelection", [tmpSelection]);
                }

            }            
        }
    }

    function getTypeFromPath(path)
    {
        if (path === undefined) {
            return "";
        }
        if(path === "") {
            return "glTF";
        }
        let pathVector = path.split("/");
        let typeName = "";
        if (path.startsWith("/")) {
            typeName = pathVector[pathVector.length - 2];       
        } else {
            typeName = path;
        }
        typeName = typeName.slice(0, typeName.length - 1)
        if (typeName === "meshe")
        {
            // meshes are the only type with a two-letter long plural, so we need to remove two letters
            typeName = typeName.slice(0, typeName.length - 1);
        }
        return `${capitalizeFirstLetter(typeName)}`;
    }

    function getIndex(path)
    {
        let pathVector = path.split("/");
        return pathVector[pathVector.length - 1];
    }

    function capitalizeFirstLetter(string) {
        return string.charAt(0).toUpperCase() + string.slice(1);
    }


    function gltfHierarchyAt(path) {
        return Thirdparty.jsonpointer.get(model.hierarchy, path);
    }
    function gltfHierarchyValue(path, defaultValue) {
        let value = gltfHierarchyAt(path);
        return value ? value : defaultValue;
    }

}
