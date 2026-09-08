import QtQuick 2.15
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15

import Model 1.0 as Model
import "../Colors.js" as Colors

// the context menu is not implemented in this file to save on resources
// (so that there is only one overall and not one per row in the hierarchy).

Rectangle {     // it contains a rect with all the content (btn, text, icon) 
    id: root 
    implicitHeight: 22 
    implicitWidth: control.columnWidthInt
    width: control.columnWidthInt

    Rectangle {  // line divider
        anchors.bottom: parent.bottom
        height: 1
        anchors.left: parent.left
        width: parent.width
        color: Colors.dark1
    }

    signal clicked(string uuidTmp, string localPathTmp, int keyboardModifiers)

    // all of the properties for customizing the row with the data taken from the hierarchy object
    property var hierarchyData: {}
    property int index: hierarchyData.index === undefined ? -1 : hierarchyData.index
    property string path: hierarchyData.id_resolved.path === undefined ? "" : hierarchyData.id_resolved.path
    property string type: fixTyp(hierarchyData.type)
    property string uuid: hierarchyData.id_resolved.uuid === undefined ? "" : hierarchyData.id_resolved.uuid
    property string localPath: hierarchyData.id_info.local_path // if no local path found (eg for scenes) set to "" pointer to root
    property string parentUUID: hierarchyData.id_info.uuid // if no parent path found (eg for scenes) set to own uuid
    property string objectIcon: getIconFromType(type, hierarchyData)  // icon based on the obj type
	property var mimeType : type
    property bool isExpanded: false  // current obj's children expanded or not
    property int amountHierarchyLines: 0     // times x l (depth level)
    required property bool hasChildren; // This property comes from TreeModel
    property int waitCounter: 0

    // ui properties, not imported 
    property bool isHeading: uuid === ""  // resources heading rows ex: nodes

    property string headingToExpand: getCategory()  // resources heading rows to expand when creating a new obj

    function getCategory() {  // whenever a new obj is created, its category will be stored in the var
        if (isHeading && control.treeSelection.length > 0) {
            let nameCategory = control.treeSelection[0].referencePath
            let lastSlash = nameCategory.lastIndexOf("/")
            nameCategory =  nameCategory.slice(0, lastSlash)
            return nameCategory
        } else {
            return ""
        }
    } 

    onHeadingToExpandChanged: function getCategoryExpanded() {  
        // whenever there is a category whit a new obj, the category will be expanded and the scrollbar will be moved to display the new row
        if (headingToExpand === path) {
            if (!isExpanded) {
                control.expand(row)
            } 
        } else {
            return;
        }
    }

    function fixTyp(type) {
        if (type === "variants" || type === "KHR_materials_variants") {
            return "Material Variants";
        }
        if (type === "Mesh Primitive") {
            return "Primitive";
        }
        if (type === "KHR_lights_punctual") {
            return "Lights";
        }
        if (type === "packets"){
            return "XMP";
        }
        if (type === undefined) {
            return "";
        }
        return capitalizeFirstLetter(type);
    }

    function getAcceptedTypes(type) {
        let result = [];
        switch (type) {
            case "Node":
                result = ["Node", "Camera", "Mesh", "Light"];
                break;
            case "Scene":
                result = ["Node"];
                break;
            case "Primitive":
                result = ["Material"];
                break;
            case "Skin":
                result = ["Node"];
                break;
            default:
                result = ["Nothing"];
                break;
        }
        return result;
    }

    /**
    returns true if at least one node is selected
    */
    function areNodesSelected() {
        let selectionTmp = Model.Observables.selection;
        let nodesSelected = false;
        
        for(let elem of selectionTmp) {
            nodesSelected = elem["elementAbsolutPath"].startsWith("/nodes/");
            if(nodesSelected) {
                break;
            }
        }
        return nodesSelected;
    }

    function isDropAllowed(dragSource) {
        let acceptedTypes = getAcceptedTypes(type);

        // Check if source is in selection
        let inSelection = false;
        for (let selectionElement of Model.Observables.selection){
            if (selectionElement.elementUUID === dragSource.uuid){
                inSelection = true;
                break;
            }
        }

        // in case of multiselection return if only nodes are selected and if the target allows node drops
        if(inSelection && Model.Observables.selection.length > 1) {
            if(areNodesSelected()) {
                return acceptedTypes.includes("Node");
            } else {
                return false;
            }
        } else {
            return acceptedTypes.includes(dragSource.type);
        }
    }

    function dragAndDrop(source, target) {
        let selectionTmp = Model.Observables.selection;

        applicationData.clearAndSetSelection(target.parentUUID, target.localPath)

        // Check if source is in selection
        let inSelection = false;
        for (let selectionElement of selectionTmp){
            if (selectionElement.elementUUID === source.uuid){
                inSelection = true;
                break;
            }
        }

        if(inSelection) {
            applicationData.dragAndDropSelection(selectionTmp, target.uuid)
        } else {
            applicationData.dragAndDropSingle(source.parentUUID, source.localPath, source.uuid, target.uuid)
        }
    }

    function calculateSpace() {   // space available in the row for the textfield/label of the obj name
        let totMargins = 40
        let itemsWidth = root.width - typeObj.width - imageObj.width - navigationBtn.width - elleText.width
        let spaceAvailable
        if (wireframeBtn.visible) {
            spaceAvailable =  itemsWidth - wireframeBtn.width - totMargins - 5 - hideBtn.width
        } else if (hideBtn.visible) {
            spaceAvailable =  itemsWidth - hideBtn.width - totMargins 
        } else {
            spaceAvailable =  itemsWidth - totMargins 
        }
        if (spaceAvailable >= 1) {
            return spaceAvailable 
        } else {
            return 1
        }
    }
    
    function getIconFromType(type, data) {  // get the icon of this object based on the type
        if(type === "GlTF") {
            return "qrc:icons/glTF_500px_June16.png";
        } else if (type === "Variants") {
            return "qrc:icons/Icon_Content_MaterialVariants.svg";
        } else if (type === "Buffers" || type === "BufferViews" || type === "Primitives") {
            return "qrc:icons/Icon_Content_" + type.slice(0, type.length - 1).replace(/\s/g, '') + ".svg";
        } else if ((type === "Image" || type === "Texture") && data.source !== undefined) {
            return "image://provider/" + data.source;
        } else if (type === undefined){
            return "";
        
        } else {
            return "qrc:icons/Icon_Content_" + type.replace(/\s/g, '') + ".svg";
        }
        return "";
    }

    function getTypeText(isHeading, type) { // get the text of this object based on the type
        if(isHeading) {
            if(type === "") {
                return getTypeFromPath(path);
            } else {
                return capitalizeFirstLetter(type);
            }
        } else {
            return getTypeFromPath(path);
        }
    }

    /**
    * keyboardModeifiers are keys like ctrlm and shift.
    * see: https://doc.qt.io/qt-5/qml-qtquick-mouseevent.html#modifiers-prop
    */
    function selectCurrent(keyboardModifiers) {
        // select only if it is not a heading resources row (ex: nodes)
        root.clicked(parentUUID, localPath, keyboardModifiers);        
    }


    Component.onCompleted: function() {
        if(type === "GlTF" && hasChildren) {
            control.expand(row);
        }
    }


    MouseArea {
        id: mouseRowArea
        anchors.fill: root
        acceptedButtons: Qt.RightButton | Qt.LeftButton
        propagateComposedEvents: true
        hoverEnabled: true

        Drag.source: root
        Drag.active: dragHandler.active
        Drag.dragType: Drag.Automatic
        Drag.supportedActions: Qt.MoveAction
        Drag.mimeData: [mimeType]
        
        onClicked: function(mouseEvent) {     // pop up openable with the right btn of the mouse
            hierarchyContextMenu.close() 

            // heading rows won't be selected 
            if (!isHeading) {
                if (mouseEvent.button === Qt.RightButton){
                    let isInSelection = Model.Observables.selection.some((elem) => {
                    if(elem.uuid === root.uuid && elem.localPath === root.localPath) {
                        return true;
                    }
                    return false;
                    })
                    if (!isInSelection){
                        selectCurrent(mouseEvent.modifiers)
                    }
                } else {
                    selectCurrent(mouseEvent.modifiers)
                }
            }

            if (isHeading && type !== "GlTF" && mouseEvent.button === Qt.LeftButton) {
                // resources heading rows on click will expand/collapse their children
                control.toggleExpanded(row)
                // not heading rows: selection + on right click open of the context menu
            } else {
                if (mouseEvent.button === Qt.RightButton) {
                    control.openContextMenu(mapToGlobal(mouseEvent.x, mouseEvent.y), type, isHeading, root.path);
                }
            } 
        }

        DropArea {
            id: dragTarget
            anchors.fill: parent

            onDropped: drag => {
                if (isDropAllowed(drag.source)) {
                    drag.accept()
                    dragAndDrop(drag.source, root)
                }
            }

            onEntered: drag => {
                control.enterExitEvents++;
                control.dragTargetRow = row;
                control.dragTargetColumn = column;
                // drag is only allowed the the accepted types match and in the case of multiselection only if only nodes are selected
                control.isDragAllowed = isDropAllowed(drag.source);
            }

            onExited: () => {
                control.enterExitEvents--;
            }
        }
        
        DragHandler {
            id: dragHandler
            target: null
            enabled: !nameTextField.focus && !isHeading

            onActiveChanged: {
                if (active) {
                    control.enterExitEvents = 0;
                    control.dragSourceRow = row;
                    control.dragSourceColumn = column;
                    // showing a drag preview does not work consistently (e.g. image missing, wrong image, wrong background color)
                    // root.grabToImage(function(result) {
                    //     mouseRowArea.Drag.imageSource = result.url;
                    // });
                }
                else 
                {
                    control.enterExitEvents = 0;
                    control.dragSourceRow = -1;
                    control.dragSourceColumn = -1;
                    control.dragTargetRow = -1;
                    control.dragTargetColumn = -1;
                }
            }
        }

        ToolTip { // tooltip default scene, if hovering on the row
            id: defaultSceneTip 
            visible: mouseRowArea.containsMouse && hierarchyData.isDefaultScene && !control.popupOpen && 
                // if open and default scene are the same, the 2 tooltips won't be shown at the same time
                !typeObjHoverHandler.hovered && !imageObjHoverHandler.hovered ? true : false
            x: mouseRowArea.width - defaultSceneTip.width - 15
            y: 24
            width: 102
            height: 27
            timeout: 2000
            delay: 1000
            padding: 4
            Image {
                source:  "qrc:icons/Tooltip_Rectangle.svg"
                sourceSize.width: 25
                sourceSize.height: 25
                x: 2
                y: -12
            }
            Label {  
                text: "Default Scene"
                font.pixelSize: 12
                color: Colors.dark2
            }
            background: Rectangle {
                anchors.fill: parent
                color: Colors.light3
                radius: 5
            }
        }

        Text {     // lines for hierarchy: it's a repeated "l"
            id: elleText
            text: {
                var correctText= []
                for(let x = 0; x < amountHierarchyLines; x++ ){
                    correctText.push("l")
                }
                correctText.join(" ")
            }
            color: Colors.dark5
            font.pixelSize: 28   // important: it takes the exact height space of the row
            opacity: 0.5
            font.weight: Font.Thin
            anchors.left: parent.left
            anchors.leftMargin: 8  
            anchors.verticalCenter: mouseRowArea.verticalCenter
        } 

        Button {     // collapsible btn : opening closing of the column (of the object's children)
            id: navigationBtn
            flat: true
            implicitHeight: 14
            implicitWidth: 14
            padding: 0    //control the size of the icon 
            topInset: 0     //control the background on hover
            bottomInset: 0     //control the background on hover
            //logic -> 3 options: closed or opened if the object has children, disabled if it does not have children
            icon.source: !enabled ? "qrc:icons/Icon_right_16px.svg" : 
                root.isExpanded ? "qrc:icons/Navigation_down_20px.svg" : "qrc:icons/Icon_right_16px.svg"
            onPressed: {
                control.toggleExpanded(row) //open or close children row
            }
            anchors.left: elleText.text.length > 0 ? elleText.right : parent.left
            anchors.leftMargin: elleText.text.length > 0 ? 2 : 5 
            anchors.verticalCenter: mouseRowArea.verticalCenter
            enabled: hasChildren
            icon.color: navigationBtnHoverHandler.hovered && enabled ? Colors.light1 : 
                enabled ? Colors.light3 : Colors.dark4
            background: Rectangle {
                anchors.fill: parent
                color: "transparent"
            }
            HoverHandler {
                id: navigationBtnHoverHandler
            }
        }

        Image {     // object type icon 
            id: imageObj
            visible: {
                if (type === "GlTF") {
                    return true;
                } else {
                    // user choice to display obj infos: icon, name, title, index
                    return control.rootInfoVisibility.find(el => el === "Icon") ? true : false;
                }
            }
            source: {
                if (path === Model.Observables.openScenePath && Model.Observables.openScenePath !== "") {
                    return "qrc:icons/Icon_Content_SceneGreen.svg";
                } else {
                    return objectIcon;
                }
            }
            fillMode: Image.PreserveAspectFit
            sourceSize.width: type === "GlTF" ? 28 : this.visible ? 14 : 0
            sourceSize.height: 14
            anchors.left: navigationBtn.right
            anchors.leftMargin: 2
            anchors.verticalCenter: mouseRowArea.verticalCenter
            HoverHandler {
                id: imageObjHoverHandler
            }

            onStatusChanged: {
                if (status == Image.Error) {
                    if (root.waitCounter < 60000){
                        fetchImage.restart()
                    } else {
                        source = "qrc:/icons/image_not_available.svg"
                    }
                }
            }
        }

        Text {     // object type
            id: typeObj
            visible: {
                if(type === "GlTF") {
                    return true;
                } else {
                    // user choice to display obj infos: icon, name, title, index
                    return (control.rootInfoVisibility.find(el => el === "Title") || control.rootInfoVisibility.find(el => el === "Index")) ? true : false;
                }
            }
            // if it is not a heading resources row (ex: nodes), it will show also the index
            text: {
                if (isHeading || type === "GlTF") {
                    return type;
                } else if (control.rootInfoVisibility.find(el => el === "Title") && control.rootInfoVisibility.find(el => el === "Index")) {
                    return type + " " + index;
                } else if (control.rootInfoVisibility.find(el => el === "Title")) {
                    return type;
                } else if (control.rootInfoVisibility.find(el => el === "Index")) {
                    return index;
                } else {
                    return "";
                }
            } 
            color: {
                if (path === Model.Observables.openScenePath && Model.Observables.openScenePath !== "") {                    
                    return Colors.green  // open scene
                } else if (typeObjHoverHandler.hovered && isHeading && type !== "GlTF") {
                    return Colors.light1   // resources heading row ex: Nodes on hovered
                } else {
                    return Colors.light4
                }
            }  
            font.pixelSize: typeObjHoverHandler.hovered && isHeading && type !== "GlTF" ? 12 : 11
            font.weight: typeObjHoverHandler.hovered && isHeading && type !== "GlTF" ? Font.Medium : Font.Normal 
            anchors.left: imageObj.visible ? imageObj.right : navigationBtn.right
            anchors.leftMargin: 5
            anchors.verticalCenter: mouseRowArea.verticalCenter
            HoverHandler {
                id: typeObjHoverHandler
            }
        }

        ToolTip { // tooltip over scene, if hovering icon or type text
            id: openSceneTip
            visible: (typeObjHoverHandler.hovered && path === Model.Observables.openScenePath && Model.Observables.openScenePath !== "" && !control.popupOpen) ||
                (imageObjHoverHandler.hovered && path === Model.Observables.openScenePath && Model.Observables.openScenePath !== "" && !control.popupOpen)
            x: typeObj.x 
            y: 24
            width: 136
            height: 27
            timeout: 2000
            delay: 1000
            padding: 4

            Image {
                source:  "qrc:icons/Tooltip_Rectangle.svg"
                sourceSize.width: 25
                sourceSize.height: 25
                x: 2
                y: -12
            }
            Label {  
                text: "Current open Scene" 
                font.pixelSize: 12
                color: Colors.dark2
                leftPadding: 2
            }
            background: Rectangle {
                anchors.fill: parent
                color: Colors.light3
                radius: 5
            }
        }

        Label {   // "object title" label displayed
            id: displayedName
            text: {
                let rightPoint = 4
                if (calculateSpace() > 32) {
                    while ((rightPoint * 8) < calculateSpace()) {
                        rightPoint++
                    }
                } else {
                    rightPoint = 4
                }
                if (isHeading) {
                    return "";
                }

                if(hierarchyData.name === undefined) {
                    return "";
                } else if(nameTextField.text.length * 8 > calculateSpace()) {
                    return "<font>\"\</font>" + nameTextField.text.slice(0, rightPoint - 4) + ".." + "<font>\"\</font>";
                } else if(nameTextField.text.length > 0) {
                    return "<font>\"\</font>" + nameTextField.text + "<font>\"\</font>";
                } else {
                    return "<font>\"\</font>" + "<font>\"\</font>";
                }
            }
            visible: {
                // user choice to display obj infos: icon, name, title, index
                if(control.rootInfoVisibility.find(el => el === "Name") || type === "GlTF") {
                    return true;
                } else {
                    return false;
                }
            }
            width: {
                if (!displayedName.visible || isHeading) {
                    return 0;
                } else if (nameTextField.text.length === 0 && displayedName.visible) {
                    return 60;
                } else if ( (nameTextField.text.length * 8) < calculateSpace()) {
                    return nameTextField.text.length * 8;
                } else {
                    return calculateSpace();
                } 
            }  // width explaination: it is the available amount of space if the text is too long, needed for the eliding the full name
            color: Colors.light1
            font.pixelSize: 12
            anchors.left: typeObj.right
            anchors.leftMargin: 10
            anchors.verticalCenter: mouseRowArea.verticalCenter

            MouseArea {
                enabled: type !== "Primitive" && type !== "XMP"
                anchors.fill: parent
                width: parent.width
                height: parent.height
                hoverEnabled: true     
                acceptedButtons: Qt.LeftButton
                property int previousModifiers: 0
                propagateComposedEvents: true
                Timer {
                    id: textFieldTimer
                    interval: 200
                }
                // double click event, to show the obj name input to edit/rename it. 
                onClicked: (mouse) => {
                    previousModifiers = mouse.modifiers;
                    mouse.accepted = false // set this so that click event is passed through
                    if(mouse.button == Qt.LeftButton && !isHeading) {
                        if(textFieldTimer.running)
                        {
                            displayedName.visible = false;
                            nameTextField.visible = true;
                            mouse.accepted = true // set this so that click event is not passed through 
                                                    // preventing name text field from loosing focus
                            nameTextField.forceActiveFocus();
                            nameTextField.selectAll();
                            textFieldTimer.stop();
                        }
                        else
                        {
                            textFieldTimer.restart();
                        }
                    }
                }
                onFocusChanged: {
                    if(focus == false) {
                        textFieldTimer.stop();
                    }
                }
            }
        }

        TextField {     // "object name" input for renaming and editing, visible only after double clicking on the label
            id: nameTextField
            visible: false
            text: hierarchyData.name ?? ""
            width: {
                if (nameTextField.text.length === 0 && nameTextField.visible) {
                    return 60;
                } else if ( (nameTextField.text.length * 8) < calculateSpace()) {
                    return nameTextField.text.length * 8;
                } else {
                    return calculateSpace();
                } 
            }  // width explaination: it is the available amount of space if the text is too long
            color: Colors.light1
            selectionColor: Colors.darkgreen2
            font.pixelSize: 12
            anchors.left: typeObj.right
            anchors.leftMargin: 10
            anchors.verticalCenter: mouseRowArea.verticalCenter
            implicitHeight: 16 
            leftPadding: 2
            bottomPadding: 1
            topPadding: 1 
            function hideTextField() {
                displayedName.visible = true;
                nameTextField.visible = false;
            }
            onEditingFinished: {
                Model.Operations.execute("set_gltf_value", { 
                    path: "/gltf" + path + "/name", 
                    value: text
                })
                hideTextField();
            }
            onFocusChanged: {
                if(focus == false) {
                    hideTextField();
                }
            }
            background: Rectangle {
                z: -1
                anchors.fill: parent
                color: Colors.dark1
            }
        }

        Button {     // wireframe mode on hierarchy panel for meshes only
            id: wireframeBtn
            flat: true
            implicitHeight: 14
            implicitWidth: 14
            padding: 0
            topInset: 0
            bottomInset: 0
            icon.source: Model.Observables.wireframe.includes(uuid) ? "qrc:icons/Icon_Viewport_wireframe.svg" :
                "qrc:icons/Icon_Viewport_DefaultShaded.svg"
            onPressed: { // listen to onPressed instead of onClicked to catch all mouse clicks
                Model.Operations.runEditorOperation("editor.toggleWireframe", { 
                    uuid: root.uuid
                });
            }
            anchors.right: hideBtn.left
            anchors.rightMargin: 5
            anchors.verticalCenter: mouseRowArea.verticalCenter
            visible: control.isHierarchy && type === "Mesh" 
            hoverEnabled: true
            icon.color: Colors.light3
            background: Rectangle {
                color: wireframeBtn.hovered ? Colors.dark4 : "transparent"
                radius: 2
                anchors.fill: parent
            }

            ToolTip { // tooltip with wireframe btn action explanation
                visible: wireframeBtn.hovered
                y: 20
                width: 140
                height: 27
                timeout: 2000
                delay: 1000
                padding: 4
                Image {
                    source:  "qrc:icons/Tooltip_Rectangle.svg"
                    sourceSize.width: 25
                    sourceSize.height: 25
                    x: 100
                    y: -12
                }
                Label {  
                    text: Model.Observables.wireframe.includes(uuid) ? "Wireframe toggle" : "Default shading toggle"
                    font.pixelSize: 12
                    color: Colors.dark2
                }
                background: Rectangle {
                    anchors.fill: parent
                    color: Colors.light3
                    radius: 5
                }
            }
        }

        Button {     // hide show action on hierarchy panel for node and mesh
            id: hideBtn
            flat: true
            implicitHeight: 14
            implicitWidth: 14
            padding: 0
            topInset: 0
            bottomInset: 0
            icon.source: Model.Observables.hiddenObjects.includes(uuid) ? "qrc:icons/Icon_Hierarchy_Hide.svg" : 
                        "qrc:icons/Icon_Hierarchy_Show.svg"
            onPressed: { // listen to onPressed instead of onClicked to catch all mouse clicks
                Model.Operations.runEditorOperation("editor.toggleVisibility", { 
                        uuid: root.uuid
                });
            }
            anchors.right: mouseRowArea.right
            anchors.rightMargin: 3
            anchors.verticalCenter: mouseRowArea.verticalCenter
            visible: control.isHierarchy && type === "Mesh" || control.isHierarchy && type === "Node"
            hoverEnabled: true
            icon.color: Colors.light3
            background: Rectangle {
                color: hideBtn.hovered ? Colors.dark4 : "transparent"
                radius: 2
                anchors.fill: parent
            }
            ToolTip { // tooltip with hide btn action explanation
                visible: hideBtn.hovered
                y: 20
                height: 27
                timeout: 2000
                delay: 1000
                Image {
                    source:  "qrc:icons/Tooltip_Rectangle.svg"
                    sourceSize.width: 25
                    sourceSize.height: 25
                    x: 70
                    y: -18
                }
                contentItem: Text {
                    text:  Model.Observables.hiddenObjects.includes(uuid) ? "Toggle to show" : "Toggle to hide"
                    font.pixelSize: 12
                    color: Colors.dark2
                }
                background: Rectangle {
                    anchors.fill: parent
                    color: Colors.light3
                    radius: 5
                }
            }
        }
        Button {
            id: openContextMenuBtn
            flat: true
            implicitHeight: 14
            implicitWidth: 14
            visible: mouseRowArea.containsMouse
            icon.source: "qrc:icons/Icon_menu.svg"
            icon.color: Colors.light3
            padding: 0
            topInset: 0
            bottomInset: 0
            anchors.right: isHierarchy ? wireframeBtn.left : mouseRowArea.right
            anchors.rightMargin: 5
            anchors.verticalCenter: mouseRowArea.verticalCenter
            hoverEnabled: true
            background: Rectangle {
                color: parent.hovered ? Colors.dark4 : "transparent"
                radius: 2
                anchors.fill: parent
            }
            onPressed: function() {
                if (!isHeading) {
                    let isInSelection = Model.Observables.selection.some(elem => elem.uuid === root.uuid && elem.localPath === root.localPath)
                    if (!isInSelection){
                        selectCurrent()
                    }
                }
                hierarchyContextMenu.close() 
                control.openContextMenu(mapToGlobal(pressX, pressY), type, isHeading, root.path); 
            }
        }
    }
}
