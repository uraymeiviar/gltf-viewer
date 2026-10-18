import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQml.Models 2.15


import "./js/thirdparty.qml.mjs" as Thirdparty;

import "./Components" as Components

import "Colors.js" as Colors

import Model 1.0 as Model
import ValidationModel 1.0

Item {
    id: tabRoot
    property string hierarchyOrResource2: "Hierarchy"
    function createTree2() {
        root.createTree()
    }

    TabBar {
        id: bar
        width: tabRoot.width
        leftPadding: 5
        contentHeight: 25
        visible: hierarchyOrResource2 === "Hierarchy"
        TabButton {
            // the +6 was carefully chosen to match the padding "Resources" text in the resources panel
            width: implicitWidth + 6 // +6 for 3 px margin to left and right of text
            hoverEnabled: false
            padding: 0
            height: bar.height
            text: "<font color='#d3d3d3'>HIERARCHY</font>"
            font.pointSize: 8
            font.family: "MS Shell Dlg 2"
            font.bold: true
        }
        TabButton {
            width: implicitWidth + 10 // +6 for 3 px margin to left and right of text
            height: bar.height
            hoverEnabled: false
            padding: 0
            font.pointSize: 8
            font.family: "MS Shell Dlg 2"
            font.bold: true
            text: "<font color='#d3d3d3'>VALIDATION</font>"
        }
    }

    StackLayout {
        anchors.top: bar.visible ? bar.bottom : bar.top
        currentIndex: bar.currentIndex
        Rectangle {
            id: root
            color: Colors.dark1
            Layout.preferredWidth: tabRoot.width
            Layout.preferredHeight: bar.visible ? tabRoot.height - bar.height : tabRoot.height

            signal createTwinPanel()

            property var scrollIndicator: 9 // root.scrollIndicator width + 1px margin 

            property string hierarchyOrResource: tabRoot.hierarchyOrResource2 // the user can choose heirarchy or resources on the combobox, the default one is hierarchy
            property var infoVisibility: ["Name", "Title", "Icon", "Index"]   // user default choices of display for obj info

            property var createObjsResources: ["createNewScene", "createNode", "createCamera", "createLight", "createMesh", 
                    "createMaterial", "createTexture", "createSampler", "createAnimation", "createMaterialVariants", "menuSeperator", "importImage", "importTexture", 
                    "importMaterial", "importModel", "importXmpPacket"]  // resources has a btn to create all of the objs and import some objs

            function scrollToDesiredPosition(row) { 
                // TODO: whenever a new obj is created, the scrollbar will be moved to display the new row
                let desiredPosition = (row * 22) / scrollViewContent.height
                hierarchyScroll.ScrollBar.vertical.position = desiredPosition 
            }


            // create button (only in resources), burger menu and scrollView with resources or hierarchy model
            MouseArea {
                id: filterRow 
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.left: parent.left
                anchors.leftMargin: 3
                anchors.rightMargin: 11
                height: 30
                onClicked: {
                    twinPanelPopUp.close()
                    hierarchyContextMenu.close()
                    Model.Operations.runEditorOperation("editor.clearSelection", {});
                }

                Rectangle {  // background rect
                    z: -1 
                    anchors.fill: parent
                    color: Colors.dark1
                }

                Button {     // create + ⬇ btn, for resources only
                    id: createPanelBtn
                    visible: root.hierarchyOrResource === "Resources" ? true : false
                    anchors.right: twinPanelBtn.left
                    anchors.verticalCenter: filterRow.verticalCenter
                    padding: 0
                    width: 30
                    horizontalPadding: 0
                    hoverEnabled: true
                    icon.source: "qrc:icons/Add_Down.svg"
                    icon.height: 25
                    icon.width: 35
                    icon.color: {  // whenever its popup is open, the icon will be green
                        if (JSON.stringify(hierarchyContextMenu.instantiatorModel) === JSON.stringify(root.createObjsResources)) {
                            Colors.green
                        } else {
                            Colors.light1 
                        }
                    } 
                    background: Rectangle {
                        z: -1
                        color: createPanelBtn.hovered ? Colors.dark4 : "transparent"
                        radius: 2
                    }
                    onClicked: {
                        hierarchyContextMenu.instantiatorModel = root.createObjsResources
                        root.openHierarchyContextMenu(mapToGlobal(createPanelBtn.x, createPanelBtn.height), "root.createObjsResources");
                    }

                }

                Button {     // burger --- btn, after combobox, for the panel options
                    id: twinPanelBtn
                    anchors.right: parent.right
                    anchors.verticalCenter: filterRow.verticalCenter
                    flat: true
                    hoverEnabled: true
                    padding: 2
                    icon.source: "qrc:icons/Icon_More.svg"
                    icon.color: twinPanelPopUp.visible ? Colors.green : Colors.light1
                    background: Rectangle {
                        z: -1
                        anchors.fill: parent
                        color: twinPanelBtn.hovered ? Colors.dark4 : "transparent"
                        radius: 2
                    }
                    onClicked: {
                        twinPanelPopUp.open()
                        twinPanelPopUp.forceActiveFocus()
                    }

                    Menu {   // pop up for twin panel options
                        id: twinPanelPopUp
                        width: 130
                        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnPressOutsideParent 
                        y: twinPanelBtn.height
                        x: -width + twinPanelBtn.width
                        background: Rectangle {
                            width: parent.width
                            height: parent.height
                            color: Colors.dark0
                            radius: 5
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
                            anchors.topMargin: 5
                            anchors.left: parent.left
                            spacing: 2
                            model: ["Show Icon", "Show Title", "Show Index", "Show Name"]
                            delegate: MenuItem {
                                text: model.modelData
                                height: 25
                                width: parent.width
                                leftPadding: 20
                                font.pixelSize: 11
                                Button {     // tick icon
                                    id: tickIcon
                                    flat: true
                                    icon.source: "qrc:icons/Icon_tick.svg"
                                    implicitWidth: 14
                                    implicitHeight: 14
                                    padding: 0
                                    topInset: 0
                                    bottomInset: 0
                                    anchors.left: parent.left
                                    anchors.leftMargin: 5
                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: root.infoVisibility.find(el => el === model.modelData.slice(5, model.modelData.length)) ? true : false
                                    hoverEnabled: true 
                                    enabled: false
                                    icon.color: parent.enabled ? Colors.light1 : Colors.dark4
                                }
                                background: Rectangle {
                                    anchors.fill: parent
                                    anchors.bottomMargin: 2
                                    anchors.topMargin: 2
                                    color: tickIcon.hovered ? Colors.dark3 : "transparent" 
                                }
                                onClicked: {
                                    if (text.includes("Show")) {  // user can choose to display obj infos: name, title, index and icon
                                        if ( root.infoVisibility.find(el => el === text.slice(5, text.length)) ) {
                                            root.infoVisibility = root.infoVisibility.filter(el => el !== text.slice(5, text.length))
                                        } else {
                                            root.infoVisibility = [...root.infoVisibility, text.slice(5, model.modelData.length)]
                                        }
                                    } 
                                    twinPanelPopUp.visible = false
                                }
                                enabled: {
                                    if (createScene.visible) {
                                        false
                                    } else if (text.includes("Show") && root.infoVisibility.length > 2) {
                                        true 
                                    } else if (text.includes("Show") && root.infoVisibility.length === 2) {
                                        // if user disables 2 out of 4 infos about the obj, at least icon, title or name needs to be visible.
                                        if (root.infoVisibility.find(el => el === "Index")) {
                                            text.slice(5, model.modelData.length) !== "Index" && 
                                            root.infoVisibility.find(el => el === text.slice(5, model.modelData.length)) 
                                            ? false : true
                                        } else {
                                            true
                                        }
                                    } else if (text.includes("Show") && root.infoVisibility.length === 1) {
                                        // at least icon, title or name needs to be visible.
                                        if (text.slice(5, model.modelData.length) === root.infoVisibility[0]) {
                                            false
                                        } else {
                                            true
                                        }
                                    } 
                                }
                            }
                        }
                    }
                }
            }

            ScrollView {     // gltf hierarchy or resources list 
                id: hierarchyScroll
                ScrollBar.vertical.policy: ScrollBar.AlwaysOn
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                anchors.top: filterRow.bottom
                anchors.topMargin: 1
                anchors.left: root.left
                anchors.right: root.right
                anchors.leftMargin: 3
                anchors.rightMargin: 2
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 2
                clip: true
                implicitHeight: 1
                contentHeight: scrollViewContent.childrenRect.height > 2 ? scrollViewContent.childrenRect.height + emptyArea.height 
                    : createScene.height + emptyArea.height  

                MenuItem {  // row to create scene only if the hierarchy is empty
                    id: createScene 
                    height: 22
                    width: hierarchyScroll.width - root.scrollIndicator
                    visible: scrollViewContent.childrenRect.height > 2 || root.hierarchyOrResource === "Resources" ? false : true 
                    Button { // add + btn
                        id: addIcon
                        flat: true
                        icon.source: "qrc:icons/Icon_Add.svg"
                        implicitWidth: 14
                        implicitHeight: 14
                        padding: 0
                        topInset: 0
                        bottomInset: 0
                        anchors.left: parent.left
                        anchors.leftMargin: 23
                        anchors.verticalCenter: parent.verticalCenter
                        enabled: false
                        icon.color: createScene.enabled ? Colors.green : Colors.darkgreen2
                    }
                    Label {
                        text: "Scene"
                        color: createScene.enabled ? Colors.green : Colors.darkgreen2
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        anchors.left: addIcon.right
                        anchors.leftMargin: 5
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    background: Rectangle {
                        anchors.fill: parent
                        color: Colors.dark2
                    }
                    onClicked: {
                        Model.Operations.execute("create_scene", {})
                    }
                }

                Connections {
                    target: Model.Observables
                    function onGltfLoaded(newGltf) {
                        scrollViewContent.reloadHierarchy();         
                    }
                }

                // item needed for correct scrollbar behavior
                Item {
                    id: scrollViewContent
                    implicitHeight: 100
                    implicitWidth: hierarchyScroll.width - root.scrollIndicator

                    property var hierarchy: undefined

                    function reloadHierarchy() {
                        // clear binding to avoid errors
                        hierarchy.implicitHeight = 0;
                        hierarchy.implicitHeight = Qt.binding(function() { return hierarchy.childrenRect.height});
                        hierarchy.width = Qt.binding(function() { return hierarchyScroll.width - root.scrollIndicator });
                        hierarchy.visible = true; 
                        hierarchy.enabled = true;
                    }
                }

                // empty area for deselection
                MouseArea {
                    id: emptyArea
                    anchors.top: createScene.visible ? createScene.bottom : scrollViewContent.bottom
                    width: hierarchyScroll.width - root.scrollIndicator
                    height: computeHeight()
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: function(mouseEvent) {
                        forceActiveFocus();
                        Model.Operations.runEditorOperation("editor.clearSelection", {});
                        if (mouseEvent.button === Qt.RightButton && Model.Observables.selection.length === 0) {
                            root.openHierarchyContextMenu(mapToGlobal(mouseEvent.x, mouseEvent.y), "none");
                        } else if (mouseEvent.button === Qt.LeftButton) {
                            hierarchyContextMenu.close()
                        }
                    }

                    // returns remaining height in scroll area (depends on tree view height) (cant be lower than zero)
                    function computeHeight() {
                        let calcHeight = hierarchyScroll.height - scrollViewContent.height;
                        calcHeight = Math.max(calcHeight, 0);
                        return calcHeight;
                    }
                }
            }

            function createTree() {
                const hierarchy = root.hierarchyOrResource === "Hierarchy"
                let component = Qt.createComponent("./Components/HierarchyTreeView.qml");
                if( component.status != Component.Ready )
                {
                    if( component.status == Component.Error )
                        console.debug("Error:"+ component.errorString() );
                    return; // or maybe throw
                }
                let object = component.createObject(scrollViewContent, {
                    clip: true,
                    interactive: false,
                    isHierarchy: hierarchy,
                });

                if (object == null) {
                    // Error Handling
                    console.log("Error creating object");
                    return;
                }

                scrollViewContent.hierarchy = object
                scrollViewContent.implicitHeight = Qt.binding(function() { return scrollViewContent.hierarchy.childrenRect.height + 1})
                object.openContextMenu.connect(root.openHierarchyContextMenu);
                if (hierarchy){
                    //object.model.hierarchy = Qt.binding(function() {return object.model.filter(Model.Observables.gltf, Model.Observables.gltfHierarchy, searchString)});
                    object.model.init("hierarchy");
                } else {
                    //object.model.hierarchy = Qt.binding(function() {return object.model.filter(Model.Observables.gltf, Model.Observables.gltfResources, searchString)});
                    object.model.init("resources");
                }
                scrollViewContent.reloadHierarchy();

            }

            function openHierarchyContextMenu(globalPosition, type, isHeading, path) {
                let localPosition = mapFromGlobal(globalPosition.x, globalPosition.y);
                // check if the width of the pop up will fit within the width of the panel starting on the x position of the click
                let correctX = localPosition.x
                if ((correctX + hierarchyContextMenu.width) > root.width) {
                    correctX = root.width - hierarchyContextMenu.width - 20
                }
                hierarchyContextMenu.x = correctX;
                hierarchyContextMenu.y = localPosition.y;
                hierarchyContextMenu.prepareAndOpen(type, isHeading, path)
                hierarchyContextMenu.forceActiveFocus();
            }
            
            Components.HierarchyContextMenu {
                id: hierarchyContextMenu
            }

        }
        Rectangle{
            
            color: Colors.dark1
            Layout.preferredWidth: bar.width
            Layout.preferredHeight: tabRoot.height - bar.height

            ListView {
                id: listViewValidation
                anchors.fill: parent
                anchors.topMargin: 5
                spacing: 5
                model: ValidationModel {}
                focus: true
                clip: true
                currentIndex: -1
                ScrollBar.vertical: ScrollBar {
                    active: true
                }

                delegate: Components.ValidationCard {
                    implicitHeight: 30
                    implicitWidth: listViewValidation.width
                    validationData: display
                    selected: ListView.isCurrentItem
                    parentView: ListView.view
                }
            }
        }
    }


}
