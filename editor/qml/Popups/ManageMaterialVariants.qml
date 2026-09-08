import QtQuick 2.15
import QtQuick.Controls
import QtQml
import "../Colors.js" as Colors

import "." as Popups
import "../Components/inspectorTypes" as InspectorComponents

import Model 1.0 as Model

Popups.PopupTemplate {
    id: popup
    contentComponent: popupContent
    height: 600
    showApply: false
    showCancel: false

    property var gltf: Model.Observables.gltf
    property var selection: Model.Observables.selection

    function getPrimitive() {
        try {
            let matches = /\/meshes\/(\d+)\/primitives\/(\d+)/.exec(selection[0].elementAbsolutPath);
            let meshIdx = matches[1];
            let primitiveIdx = matches[2];
            return gltf.meshes[meshIdx].primitives[primitiveIdx];
        } catch (e) {
            return null;
        }
    }

    function getPrimitivePath() {
        try {
            let matches = /\/meshes\/(\d+)\/primitives\/(\d+)/.exec(selection[0].elementAbsolutPath);
            let meshIdx = matches[1];
            let primitiveIdx = matches[2];
            return "/meshes/" + meshIdx + "/primitives/" + primitiveIdx;
        } catch (e) {
            return null;
        }
    }

    function getLastIndexFromPath(path) {
        const elements = path.split("/");
        return parseInt(elements[elements.length - 1]);
    }

    function loadDataFromGltf() {
        if (!gltf) {
            return;
        }

        // load materials
        availableMaterialsModel.clear()
        // init with none option
        availableMaterialsModel.append({
            text: "None",
        })
        for (var i = 0; i < gltf.materials.length; i++) {
            availableMaterialsModel.append({
                text: gltf.materials[i].name ?? ("Material " + i),
            })
        }

        if(!gltf.extensions || !gltf.extensions.KHR_materials_variants){
            return;
        }

        // create empty mapping variant to material
        let variants = gltf.extensions.KHR_materials_variants.variants;
        materialVariantsModel.clear()
        for (var i = 0; i < variants.length; i++) {
            materialVariantsModel.append({
                name: variants[i].name ?? ("Material Variant " + i),
                idx: i,
                material: -1,
            })
        }

        let primitive = getPrimitive();
        if (!primitive || !primitive.extensions || !primitive.extensions.KHR_materials_variants) {
            return;
        }

        // fill mapping variant to material with correct materials
        let variantMappings = primitive.extensions.KHR_materials_variants.mappings;
        for (var i = 0; i < variantMappings.length; i++) {
            let mapping = variantMappings[i];
            const materialIdx = getLastIndexFromPath(mapping.material);
            for (var j = 0; j < mapping.variants.length; j++) {
                const variantIdx = getLastIndexFromPath(mapping.variants[j]);
                materialVariantsModel.setProperty(variantIdx, "material", materialIdx); // +1 to account for the "none" option
            }
        }
    }

    onGltfChanged: {
        loadDataFromGltf();
    }

    Component.onCompleted: {
        loadDataFromGltf();
    }

    function setVariant(variantIDX, materialIDX){
        const primitivePath = getPrimitivePath();
        if(primitivePath === null){
            return;
        }

        let parameters = {};
        // rework this to use the actual previous value
        parameters["path"] = "/gltf" + primitivePath;
        parameters["materialIDX"] = "/gltf/materials/" + ( materialIDX - 1); // -1 to account for the "none" option
        parameters["variantIDX"] = "/gltf/extensions/KHR_materials_variants/variants/" + variantIDX;
        Model.Operations.resolvePromiseToCard(Model.Operations.execute("add_material_variant_to_primitive", parameters));

    }

    function removeVariant(variantIDX) {
        const primitivePath = getPrimitivePath();
        if(primitivePath === null){
            return;
        }

        // remove mapping
        let parameters = {};
        parameters["path"] = "/gltf" + primitivePath;
        parameters["variantIDX"] = "/gltf/extensions/KHR_materials_variants/variants/" + variantIDX;
        Model.Operations.resolvePromiseToCard(Model.Operations.execute("remove_material_variant_from_primitive", parameters));
    }

    ListModel {
        id: materialVariantsModel
        // ListElement {
        //     name: "Material Variant 1"
        //     material: 0
        //     idx: 0
        // }
    }

    ListModel {
        id: availableMaterialsModel
        ListElement {
            text: "None"
        }
    }
    
    Component {
        id: popupContent

        Rectangle {
            anchors.fill: parent
            anchors.margins: 3
            color: Colors.dark1

            // border around variants
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: addVariantButton.top
                anchors.bottomMargin: 5

                border.color: Colors.dark205
                border.width: 1
                radius: 3
                color: Colors.dark1

                ScrollView {
                    id: mappingsScrollView
                    anchors.fill: parent
                    anchors.margins: 2
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    clip: true

                    ListView {
                        id: mappingsList
                        anchors.right: mappingsScrollView.right
                        anchors.left: mappingsScrollView.left
                        anchors.margins: 1
                        anchors.rightMargin: 10
                        spacing: 2
                        model: materialVariantsModel
                        delegate: Rectangle {
                            id: variantItem
                            color: Colors.dark2
                            width: ListView.view.width
                            height: childrenRect.height + 16 // + 5 for padding at bottom

                            Label {
                                id: variantIndex
                                width: 100
                                text: "Variant " + idx
                                anchors.left: parent.left
                                anchors.leftMargin: 5
                                anchors.top: parent.top
                                anchors.topMargin: 8
                            }

                            TextField {
                                id: variantName
                                text: name
                                anchors.left: variantIndex.right
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.topMargin: 8
                                anchors.rightMargin: 5
                                leftPadding: 11
                                rightPadding: 5
                                Rectangle {
                                    z: -1
                                    anchors.fill: parent
                                    color: Colors.dark1
                                }
                                onEditingFinished: {
                                    let oldValue = null;
                                    let parameters = {};
                                    // rework this to use the actual previous value
                                    if (oldValue !== text && idx !== undefined) {
                                        parameters["value"] = text;
                                        parameters["path"] = "/gltf/extensions/KHR_materials_variants/variants/" + idx + "/name";
                                        Model.Operations.resolvePromiseToCard(Model.Operations.execute("set_gltf_value", parameters));
                                    }
                                }
                            }

                            Label {
                                id: materialText
                                text: "Material"
                                anchors.left: parent.left
                                anchors.leftMargin: 5
                                anchors.right: variantName.left
                                anchors.verticalCenter: materialSelector.verticalCenter
                            }

                            Image {
                                id: imageObj
                                source: "qrc:icons/Icon_Content_Material.svg";
                                sourceSize.width:  20
                                sourceSize.height: 20
                                anchors.right: materialSelector.left
                                anchors.rightMargin: 8 
                                anchors.verticalCenter: materialSelector.verticalCenter
                            }

                            ComboBox {
                                id: materialSelector
                                model: availableMaterialsModel
                                currentIndex: material + 1 // +1 to account for the "none" option
                                anchors.left: materialText.right
                                anchors.right: parent.right
                                anchors.rightMargin: 5
                                anchors.top: variantName.bottom
                                topPadding: -4 
                                bottomPadding: -4 
                                height: 30
                                textRole: "text"
                                valueRole: "value"
                                font.pixelSize: 12
                                indicator: Image {
                                    anchors.right: parent.right
                                    anchors.rightMargin: 5
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 15
                                    height: 15
                                    source: "qrc:icons/Icon_down_20px.svg"
                                }
                                onActivated: {
                                    if(currentIndex === 0) {
                                        removeVariant(idx)
                                    } else {
                                        setVariant(idx, currentIndex)
                                    }
                                }
                            }
                        }
                    }
                }
                
            }

            Button {
                id: addVariantButton
                flat: true
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 5
                anchors.left: parent.left
                anchors.right: parent.right
                contentItem: Item {
                    Image {
                        id: buttonPlusIcon
                        source: "qrc:icons/Icon_AddGreen.svg";
                        sourceSize.width:  20
                        sourceSize.height: 20
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Label {
                        anchors.left: buttonPlusIcon.right
                        anchors.leftMargin: 5
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Create Material Variant"
                    }
                }
                onClicked: {
                    let parameters = {};
                    parameters["path"] = "/gltf/extensions/KHR_materials_variants/variants";
                    parameters["name"] = "Material variant";
                    Model.Operations.resolvePromiseToCard(Model.Operations.execute("create_gltf_component", parameters));
                }
            }
        }
    }
}