import QtQuick
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.15

import "../../js/GlobalFunctions.js" as GlobalFunctions
import Model 1.0 as Model
import "../../Colors.js" as Colors
import "." as InspectorRow

// row
Rectangle {   // container
    id: mainRow
    color:  Colors.getInspectorRowColor(-1, isHovered)

    function calculateHeight() {
        // if there are items to be shown
        if (items !== undefined && childrenAmount > 0) {
            //  max items shown will be 5, as there will be a scrollbar to navigate through them
            if (childrenAmount > 4) {
                return 210
            } else {
                // if there is the operation to add new items, it will be added an extra row
                if (operations["insert_gltf_pointer"] || operations["create_gltf_component"]) {
                    return (childrenAmount * 30) + 60
                } else {
                    return (childrenAmount * 30) + 30
                }
            }
        } else if (operations["insert_gltf_pointer"] || operations["create_gltf_component"]) { 
            // if there is the operation to add new items, it will be added an extra row
            return 60 
        } else {
            30 
        }
    }

    property var severity: -1
    property var issueMessage: ""
    property var tooltipColor: Colors.light3
    property var issueIcon: ""
    
    // imported data
    property var infos: undefined
    property string path: ""
    property var operations: {}   // operations of the list group
    property var items: []  // array containing the actual value and its design info (type, icon, operation)
    property string title: "" 
    property bool isExpandedFromModel: false

    // filled automatically by treeview
    required property int depth
    required property int row
    required property int column
    required property var index
    required property TreeView treeView

    // expand-collapse
    property bool isCollapsible: operations["insert_gltf_pointer"] || operations["create_gltf_component"] || childrenAmount > 0 ? true : false 
    property bool collapsed: true 
    property var childrenAmount: infos === undefined || infos.childCount === undefined ? 0 : infos.childCount
    property var expandedHeight: calculateHeight()

    property var isHovered: mouseArea.containsMouse && mouseArea.enabled && !mouseArea.containsPress
    onIsHoveredChanged: {
        mainRow.color = Colors.getInspectorRowColor(-1, isHovered);
    }

    implicitWidth: control.columnWidthInt
    width: control.columnWidthInt
    implicitHeight: collapsed ? 30 : expandedHeight
    height: collapsed ? 30 : expandedHeight
    
    onIsExpandedFromModelChanged: {
        if(isExpandedFromModel === collapsed) {
            collapsed = !collapsed;
        }
    }

    MouseArea {  // mouse area to expand-collapse the row
        id: mouseArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 25
        hoverEnabled: true
        enabled: isCollapsible 
        onClicked: {
            if (isCollapsible) {
                // array rows will expand to show their items
                collapsed = !collapsed;
                control.model.setIsRowExpanded(!collapsed, control.modelIndex(row, column));
                mainRow.expandedHeight = calculateHeight();
                treeView.forceLayout();
            }
        }
        onEntered: {
            if (isCollapsible) {
                mouseArea.cursorShape = Qt.PointingHandCursor;
            }
        }
        onExited: {
            if (isCollapsible) {
                mouseArea.cursorShape = Qt.ArrowCursor;
            }
        }

        Image {
            id: collapsibleIcon
            width: 15
            height: 15
            anchors.left: parent.left
            anchors.leftMargin: 5 * (depth + 1)
            anchors.top: parent.top
            anchors.topMargin: 8
            source: collapsed ? "qrc:icons/Icon_right_16px.svg" : "qrc:icons/Navigation_down_20px.svg" 
            sourceSize.height: 25
            sourceSize.width: 25
            opacity: isCollapsible ? 0.7 : 0.4
        }

        Text {  // label 
            id: rootTitle
            anchors.top: parent.top
            anchors.topMargin: 8
            anchors.left: collapsibleIcon.right
            anchors.leftMargin: collapsibleIcon.visible ? 2 : 5 * (depth + 1)
            text: infos["additionalValue"] ? title : control.displayLabel(title)
            font.pixelSize: 11
            color: Colors.light5
            elide: Text.ElideRight
        }

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
            visible: mouseArea.containsMouse && severity !== -1
            issueString: issueMessage
            offset: validationIcon.x
            toolTipColor: tooltipColor
        }

        Text {
            id: root
            text: childrenAmount
            anchors.top: parent.top
            anchors.topMargin: 8
            anchors.right: parent.right
            anchors.rightMargin: 10
            font.pixelSize: 11
            color: Colors.light1 
        }
    }


    ScrollView {  // if the array list has too many children, it will be scrollable with max 5 items visible 
        id: scrollChildrenArea
        visible: !collapsed 
        width: parent.width 
        anchors.top: mouseArea.bottom
        anchors.topMargin: 2
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 5 * (depth + 1)
        clip: true
        ScrollBar.vertical.policy: items !== undefined && visible && childrenAmount > 4 ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
        height: {
            if (items !== undefined && childrenAmount > 0) {
                if (childrenAmount > 4) {
                    return 150
                } else {
                    return childrenAmount * 30
                }
            } 
            0
        }
        background: Rectangle {
            anchors.fill: parent
            anchors.leftMargin: 5
            color: "transparent" 
            border.width: 1 
            border.color: Colors.dark305
            radius: 2
        }
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ListView {  // list of rows 
            id: rootList
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 1
            anchors.bottomMargin: 1
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.right: parent.right
            anchors.rightMargin: 5
            width: parent.width - 30
            implicitHeight: childrenRect.height + 2 
            spacing: 0
            model: items
            clip: true
            visible: scrollChildrenArea.visible
            delegate: Rectangle {
                id: rowRect
                width: rootList.width
                height: 30
                color: Colors.dark2

                property var itemInfos: model.modelData

                MouseArea {  // mouse area to expand-collapse the row
                    id: mouseAreaRow
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: childrenRect.width
                    hoverEnabled: true
                    enabled: itemInfos.type === "pointer" ? true : false
                    onClicked: {
                        let tmpSelection = {};
                        tmpSelection["uuid"] = itemInfos.uuid;
                        tmpSelection["localPath"] = itemInfos.localPath; 
                        Model.Operations.runEditorOperation("editor.setSelection", [tmpSelection]);
                    }
                    onEntered: {
                        mouseAreaRow.cursorShape = Qt.PointingHandCursor;
                    }
                    onExited: {
                        mouseAreaRow.cursorShape = Qt.ArrowCursor;
                    }

                    // row with icon + label 
                    Button {  // icon obj
                        id: objIcon
                        flat: true
                        visible: itemInfos.type === "pointer" ? true : false 
                        padding: 0
                        topInset: 0
                        bottomInset: 0
                        width: 16
                        height: 16
                        opacity: 0.7
                        anchors.left: parent.left
                        anchors.leftMargin: 3
                        anchors.verticalCenter: parent.verticalCenter
                        hoverEnabled: true
                        background: Rectangle {
                            anchors.fill: parent
                            radius: 2
                            color: objIcon.hovered ? Colors.dark4 : "transparent"
                        }
                        onClicked: { 
                            let tmpSelection = {};
                            tmpSelection["uuid"] = itemInfos.uuid;
                            tmpSelection["localPath"] = itemInfos.localPath; 
                            Model.Operations.runEditorOperation("editor.setSelection", [tmpSelection]);
                        }
                        IconImage {
                            id: image
                            source: getPreviewIcon()
                            fillMode: Image.PreserveAspectFit
                            anchors.fill: parent
                            sourceSize.width: 16
                            sourceSize.height: 16
                            cache: false
                            color: getButtonIconColor()
                            function getPreviewIcon() {
                                if(itemInfos.textureSource) {
                                    return "image://provider/" + itemInfos.textureSource
                                }
                                return GlobalFunctions.getObjIcon(itemInfos.icon)
                            }
                            function getButtonIconColor() {
                                if(itemInfos.textureSource) {
                                    return "transparent"
                                }
                                return Colors.green
                            }
                        }
                    }
                    Text {  // label
                        anchors.left: objIcon.visible ? objIcon.right : parent.left
                        anchors.leftMargin: 5
                        anchors.right: iconAction.left
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.rightMargin: 10
                        font.bold: mouseAreaRow.containsMouse
                        text: {
                            // pointers will show the name of the obj (like Node 0 for nodes in a scene)
                            if (itemInfos.type && itemInfos.type === "pointer") {
                                if (itemInfos.name) {
                                    return itemInfos.name;
                                }
                                return GlobalFunctions.getDisplayName(itemInfos.value)
                            // text for numbers: label + int "X 1"
                            } else if (itemInfos.type && itemInfos.type === "number") { 
                                const placeholderLabel = control.getCorrectLabel("vector", index) + " "
                                return "<font style=color:" + Colors.light4 + ">" + placeholderLabel + "</font>" + itemInfos.value
                            } else {
                                return itemInfos.value
                            }
                        } 
                        textFormat: Text.RichText  // needed for styling the label 
                        font.pixelSize: 11
                        color: Colors.light1 
                    }
                }


                Button {  // operation of each row 
                    id: iconAction
                    padding: 0
                    topInset: 0
                    bottomInset: 0
                    implicitWidth: 16
                    implicitHeight: 16
                    opacity: 0.7
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.top: parent.top
                    anchors.topMargin: 8
                    flat: true
                    visible: operations["remove_gltf_pointer"] || operations["remove"]
                    //enabled: true
                    hoverEnabled: true
                    icon.source: {
                        if (operations["remove_gltf_pointer"] || operations["remove"]) { 
                            "qrc:/icons/Icon_trash.svg"
                        } else {
                            return ""
                        }
                    }
                    icon.color: {
                        if (operations["remove_gltf_pointer"] || operations["remove"]) {
                            return Colors.red
                        }  else {
                            return Colors.light1
                        }
                    } 
                    background: Rectangle {
                        anchors.fill: parent
                        radius: 2
                        color: iconAction.hovered ? Colors.dark4 : "transparent"
                    }
                    onClicked: {
                            let parameters = {};
                            parameters["path"] = `${path}/${index}`;
                            let operation = operations["remove_gltf_pointer"] ? "remove_gltf_pointer" : "remove"
                            Model.Operations.execute(operation, parameters).catch((message) => {
                                iconAction.reject();
                                errorMessage.parent = iconAction;
                                errorMessage.show(message, 3000);
                            });                           
                    }
                }

            }
            
        } 

    }
    
    // row with icon add + label add obj
    Button {  // icon obj
        id: addElementButton
        flat: true
        icon.source: "qrc:/icons/Icon_Add.svg"
        icon.height: 16
        icon.width: 16
        padding: 0
        opacity: 0.7
        anchors.top: scrollChildrenArea.bottom
        anchors.topMargin: 8 
        anchors.left: parent.left
        anchors.leftMargin: 10
        visible: !collapsed && (operations["insert_gltf_pointer"] ? true : false || operations["create_gltf_component"] ? true : false)
        height: visible ? 16 : 0
        topInset: 0
        bottomInset: 0
        icon.color: enabled ? Colors.green : Colors.light4
        // if there are available options the btn will be enabled otherwise no
        enabled: operations["create_gltf_component"] !== undefined || operations["insert_gltf_pointer"] !== undefined
        hoverEnabled: true
        background: Rectangle {
            anchors.fill: parent
            radius: 2
            color: addElementButton.hovered ? Colors.dark4 : Colors.dark2
        }
        onClicked: {
            
            if (operations["insert_gltf_pointer"]) {
                // open the pop up for add new element with all available options (minus the options already used)

                addElementMenuLoader.active = true
                addElementMenuLoader.item.visible = true

                const objName = infos.items !== undefined && infos.items.length > 0 ? 
                        infos.items[0].icon : typeof items === "object" ? infos.items.icon : "" 
                        //infos.path.slice(infos.path.lastIndexOf("/")) 

                addElementMenuLoader.item.addElementList = getAddElementList(objName, infos.items, infos.path);
            } else {
                let parameters = {};
                parameters["path"] = `${path}`;
                Model.Operations.execute("create_gltf_component", parameters).then(() =>{
                    mainRow.expandedHeight = calculateHeight();
                    treeView.forceLayout();
                }).catch((message) => {
                    iconAction.reject();
                    errorMessage.parent = iconAction;
                    errorMessage.show(message, 3000);
                });
            }
        }

        text: operations["insert_gltf_pointer"] ? "Add Reference" : "Add New"
        font.pixelSize: 11

        // provide to the ListRow the add elements popup model with the all available pointers (minus the one already added), (ex: all the available meshes) 
        function getAddElementList(objName, items, path) {
            let arrayOption = undefined
            let arrayItems = items !== undefined && items.length > 0 ? items.map((el) => el.value) : [] // array with the elements already added and listed
            
            let arrayPointer = Model.Observables.getAvailablePointers(objName, path)
            let arrayValue = arrayPointer.filter((el, index) => index % 2 === 0)  // path will be in the value array "/scenes/0"
            let arrayText = arrayPointer.filter((el, index) => index % 2 !== 0)  // name will be in the text array "'Scene'"
            let availablePointers = arrayValue.map((el, index) => { return { path: el, name: arrayText[index]} }) // array with the elements already added and listed

            if (availablePointers !== undefined && availablePointers.length > 0) { 

                // filter the array from the element already added
                arrayOption = availablePointers.filter((availableItem) => !arrayItems.find((item) => availableItem.path === item))

                // filter the array from its parent element (if this is the array children of node 0, node 0 needs to be removed)
                let parentPath = path.slice(5, path.lastIndexOf("/"))
                arrayOption = arrayOption.filter((item) => item.path !== parentPath)
            }

            return arrayOption
        }
    }

    

    Loader {
        id: addElementMenuLoader
        // Explicitly set the size of the
        // Loader to the parent item's size
        anchors.fill: parent
        sourceComponent: addElementMenuComponent
        active: false
    }


    Component {
        id: addElementMenuComponent
        Menu {   // pop up for add new element with all available options (minus the options already used)
        id: addElementMenu
        visible: false
        property var addElementList: []
        height: childrenRect.implicitHeight
        width: parent.width - 20
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnPressOutsideParent 
        y: addElementButton.y + addElementButton.height
        x: addElementButton.x + 5
        background: Rectangle {
            width: parent.width
            height: parent.height
            color: Colors.dark1
            border.color: Colors.light4
            border.width: 1
        }
        onActiveFocusChanged: {
            if (activeFocus === false) {
                close();
            }
        }
        contentItem: ListView {
            implicitWidth: parent.width
            implicitHeight: childrenRect.height
            anchors.top: parent.top
            anchors.left: parent.left
            model: addElementList
            delegate: MenuItem {
                id: addRow
                text: model.modelData.name ?? "Not available :("
                implicitHeight: 25
                width: parent.width
                font.pixelSize: 12
                HoverHandler {
                    id: addRowHoverHandler
                }
                background: Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1
                    color: addRowHoverHandler.hovered ? Colors.dark2 : "transparent"
                }
                onClicked: {
                    let parameters = {};
                    parameters["path"] = path;
                    parameters["pointer"] = "/gltf" + addElementList[index].path;
                    Model.Operations.execute("insert_gltf_pointer", parameters).then(() =>{
                        mainRow.expandedHeight = calculateHeight();
                        treeView.forceLayout()
                    }).catch(function (message) {
                            errorMessage.parent = addRow;
                            errorMessage.show(message, 3000);
                    });
                    addElementMenu.close();
                }
            }
        }
    }
}

    Button {  // action reset
        id: iconAction
        flat: true
        icon.source: "qrc:/icons/Icon_Override.svg" 
        visible: Model.Operations.getResetOperation(operations) ? true : false
        padding: 0
        topInset: 0
        bottomInset: 0
        implicitWidth: 16
        implicitHeight: 16
        opacity: 0.7
        anchors.right: parent.right
        anchors.rightMargin: 5
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        icon.color: enabled ? Colors.light1 : Colors.light4
        hoverEnabled: true
        onClicked: {
            let parameters = {};
            parameters["path"] = path;
            const operationName = Model.Operations.getResetOperation(operations);
            Model.Operations.execute(operationName, parameters).catch((message) => {
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

    ToolTip { // tooltip input text
        id: errorMessage
        parent: rootList
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
