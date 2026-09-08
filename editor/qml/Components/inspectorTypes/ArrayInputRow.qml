import QtQuick 2.15
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.15
import Qt.labs.platform

import Model 1.0 as Model
import "../../Colors.js" as Colors

import "." as InspectorRow
import "../../js/thirdparty.qml.mjs" as Thirdparty;

// row
Rectangle {   //container
    id: mainRow
    color:  Colors.dark2
    implicitWidth: control.columnWidthInt
    width: control.columnWidthInt
    implicitHeight: heightRow
    height: heightRow

    property var severity: -1
    property var issueMessage: ""
    property var tooltipColor: Colors.light3
    property var issueIcon: ""

    // imported for each row  
    property var infos: undefined
    property string valueType: ""  // [uv, color, vector] necessary to make layout and content choices
    property var operations: {}   // operations of the row
    property var arrayValue: undefined  // value displayed 
    property var defaultValue: undefined
    property string path: ""
    property string title: ""
    property int amountOfItems: 1 // number of items in the array
    property bool isEulerMode: isRowShowingRotation() ? true : false // tracks whether quaternion or euler mode is enabled, only relevant for rotations
    property var eulerRotationArray: undefined

    // filled automatically by treeview
    required property int depth

    // dynamic height 
    property int heightRow: 30

    // math based on https://entropymine.com/imageworsener/srgbformula/
    function sRGBToLinear(value) {
        if (value <= 0.04045)
            return value / 12.92;
        else
            return Math.pow((value + 0.055) / 1.055, 2.4);
    }

    // math based on https://entropymine.com/imageworsener/srgbformula/
    function linearToSRGB(value) {
        // clamp to correct range
        value = Math.min(Math.max(value, 0), 1);
        if (value <= 0.0031306)
            return value * 12.92;
        else
            return (Math.pow(value, 1 / 2.4) * 1.055) - 0.055;
    }

    function updateUiColorValue() {
        function setColorArrayToRGBA(r,g,b,a) {
            // set color for button
            colorDialogControlBackground.color.r = linearToSRGB(r)
            colorDialogControlBackground.color.g = linearToSRGB(g)
            colorDialogControlBackground.color.b = linearToSRGB(b)


            // set color for color dialog
            colorDialog.color.r = linearToSRGB(r)
            colorDialog.color.g = linearToSRGB(g)
            colorDialog.color.b = linearToSRGB(b)
            colorDialog.color.a = linearToSRGB(a)
        }
        if(arrayValue) {
            if(arrayValue.length === 3) {
                setColorArrayToRGBA(arrayValue[0], arrayValue[1], arrayValue[2], 1)
            } else if(arrayValue.length > 3) {
                setColorArrayToRGBA(arrayValue[0], arrayValue[1], arrayValue[2], arrayValue[3])
            }
        } else {
            if(defaultValue) {
                if(defaultValue.length === 3) {
                    setColorArrayToRGBA(defaultValue[0], defaultValue[1], defaultValue[2], 1)
                } else if(defaultValue.length > 3) {
                    setColorArrayToRGBA(defaultValue[0], defaultValue[1], defaultValue[2], defaultValue[3])
                }
            } else {
                setColorArrayToRGBA(1,1,1,1)
            }
        }
    }

    function isRowShowingRotation() {
        return title.includes("rotation");
    }

    function degreesToRadians(eulerAngles) {
        return [eulerAngles[0] * (Math.PI/180), eulerAngles[1] * (Math.PI/180), eulerAngles[2] * (Math.PI/180)]; 
    }
    
    function radiansToDegrees(eulerAngles) {
        return [eulerAngles[0] * (180/Math.PI), eulerAngles[1] * (180/Math.PI), eulerAngles[2] * (180/Math.PI)]; 
    }
    
    function eulerToQuaternion (eulerDegrees) {
        const radians = degreesToRadians(eulerDegrees);

        const c1 = Math.cos(radians[0] / 2);
        const s1 = Math.sin(radians[0] / 2);
        const c2 = Math.cos(radians[1] / 2);
        const s2 = Math.sin(radians[1] / 2);
        const c3 = Math.cos(radians[2] / 2);
        const s3 = Math.sin(radians[2] / 2);

        // Order of rotations is YXZ, see three.js quaternion implementation for others
        // code from https://github.com/mrdoob/three.js/blob/master/src/math/Quaternion.js#L230-L233
        const x = s1 * c2 * c3 + c1 * s2 * s3;
        const y = c1 * s2 * c3 - s1 * c2 * s3;
        const z = c1 * c2 * s3 - s1 * s2 * c3;
        const w = c1 * c2 * c3 + s1 * s2 * s3;

        return [x, y, z, w];
    }

    function quaternionToEuler(quat) {
        if (quat === undefined) {
            return undefined;
        }

        // code from https://github.com/mrdoob/three.js/blob/master/src/math/Euler.js#L238

        const mat = quaternionToMatrix4(quat);
        const eulerRadians = eulerFromMatrix(mat);

        const eulerDegrees = radiansToDegrees(eulerRadians);

        return eulerDegrees;
    }

    function quaternionToMatrix4(quaternion) {
        const te = Array(16);

		const x = quaternion[0], y = quaternion[1], z = quaternion[2], w = quaternion[3];
		const x2 = x + x,	y2 = y + y, z2 = z + z;
		const xx = x * x2, xy = x * y2, xz = x * z2;
		const yy = y * y2, yz = y * z2, zz = z * z2;
		const wx = w * x2, wy = w * y2, wz = w * z2;


		te[ 0 ] = (1 - (yy + zz) );
		te[ 1 ] = (xy + wz);
		te[ 2 ] = (xz - wy);
		te[ 3 ] = 0;

		te[ 4 ] = (xy - wz);
		te[ 5 ] = (1 - (xx + zz));
		te[ 6 ] = (yz + wx);
		te[ 7 ] = 0;

		te[ 8 ] = (xz + wy);
		te[ 9 ] = (yz - wx);
		te[ 10 ] = (1 - (xx + yy));
		te[ 11 ] = 0;

		te[ 12 ] = 0;
		te[ 13 ] = 0;
		te[ 14 ] = 0;
		te[ 15 ] = 1;

        return te;
    }

    function clamp( value, min, max ) {
        return Math.max( min, Math.min( max, value ) );
    }

    function eulerFromMatrix(matrix4x4) {
        const te = matrix4x4;
		const m11 = te[ 0 ], m12 = te[ 4 ], m13 = te[ 8 ];
		const m21 = te[ 1 ], m22 = te[ 5 ], m23 = te[ 9 ];
		const m31 = te[ 2 ], m32 = te[ 6 ], m33 = te[ 10 ];

        const x = Math.asin( - clamp( m23, - 1, 1 ) );
        let y, z;
        if ( Math.abs( m23 ) < 0.9999999 ) {
            y = Math.atan2( m13, m33 );
            z = Math.atan2( m21, m22 );
        } else {
            y = Math.atan2( - m31, m11 );
            z = 0;
        }

        return [x, y, z];
    }

    HoverHandler { id: hoverHandler}

    Text {  // label 
        id: rootTitle
        anchors.top: parent.top
        anchors.topMargin: heightRow === 30 ? 8 : 5 
        anchors.left: parent.left
        anchors.leftMargin: 5 * (depth + 1)
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

    Rectangle {
        id: marginLeft
        anchors.left: rootTitle.right
        height: 10
        width: 5
        color: "transparent"
    }

    InspectorRow.Spacer { // space 
        leftItem: marginLeft
        rightItem: iconAction
        elementWidth: textFieldsList.width + 10 + colorDialogControl.width + colorDialogControl.anchors.rightMargin + textFieldsList.anchors.rightMargin
    }

    onArrayValueChanged: {
        updateUiColorValue();
        
        // only relevant for rotations
        if (isRowShowingRotation) {
            eulerRotationArray = quaternionToEuler(arrayValue);
        }
    }

    Component.onCompleted : {
        updateUiColorValue();
    }

    Button {
        id: colorDialogControl
        anchors.right: textFieldsList.left
        anchors.rightMargin: 5
        anchors.bottomMargin: 5
        anchors.bottom: parent.bottom
        flat: true
        visible: valueType === "color"
        enabled: valueType === "color"
        hoverEnabled: true
        width: 20
        height: 20
        onClicked: colorDialog.open()
        background: Rectangle {
            id: colorDialogControlBackground
            anchors.fill: parent
            radius: 2
            color: "#ffffff"
        }
    }
    
    Button {
        id: rotationEulerSwitch
        flat: true
        icon.source: isEulerMode ? "qrc:/icons/Icon_Rotation_Euler.svg" : "qrc:/icons/Icon_Rotation_Quaternion.svg"
        visible: isRowShowingRotation()
        padding: 0
        topInset: 0
        bottomInset: 0
        width: 16
        height: 16
        opacity: 0.7
        anchors.right: textFieldsList.left
        anchors.rightMargin: 5
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        icon.color: enabled ? Colors.light1 : Colors.light4
        hoverEnabled: true
        onClicked: {
            isEulerMode = !isEulerMode;
        }
        background: Rectangle {
            anchors.fill: parent
            radius: 2
            color: rotationEulerSwitch.hovered ? Colors.dark4 : "transparent"
        }
        ToolTip { // tooltip input text
            id: textTip 
            visible: rotationEulerSwitch.hovered
            y: -26
            height: 25
            timeout: 2500
            delay: 1000
            padding: 4
            Label {  
                text: "Switch between Euler and Quaternion rotation representation"
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

    ColorDialog {
        id: colorDialog
        color: "#ffffff"
        options: arrayValue && arrayValue.length === 4 ? ColorDialog.ShowAlphaChannel : 0

        onAccepted: {
            if (defaultValue === undefined && (text === "" || arrayValue === undefined)) {
                return;
            }

            // We need to do deep copies of the arrays. this is done with parse/stringifiy
            let oldValue = JSON.stringify(arrayValue);
            let parameterValue = arrayValue ? JSON.parse(oldValue) : JSON.parse(JSON.stringify(defaultValue));
            let parameters = {};
            parameterValue[0] = sRGBToLinear(color.r);
            parameterValue[1] = sRGBToLinear(color.g);
            parameterValue[2] = sRGBToLinear(color.b);
            if(parameterValue.length === 4) {
                parameterValue[3] = color.a;
            }
            
            if (oldValue !== JSON.stringify(parameterValue)) {
                parameters["value"] = parameterValue;
                parameters["path"] = path;
                Model.Operations.execute("set_gltf_value", parameters);
            }
        }
    }

    ListView {  // list view of textfields for x, y, z (,w) / r, g, b (,a) / uv 
        id: textFieldsList
        anchors.bottom: parent.bottom
        anchors.right: heightRow === 30 ? iconAction.left : mainRow.right
        anchors.rightMargin: {
            if ((valueType === "vector" && amountOfItems === 3) || (isRowShowingRotation && isEulerMode)) {
                heightRow === 30 ? 97 : 117
            } else {
                heightRow === 30 ? 10 : 30
            }
        } 
        height: 30
        width: childrenRect.width
        orientation: ListView.Horizontal
        model: { 
            if (isEulerMode) {
                if (eulerRotationArray === undefined) {
                    return Array(3);
                } else { 
                    return eulerRotationArray;
                }
            }
            if (arrayValue === undefined) {
                return Array(amountOfItems);
            } else { 
                return arrayValue;
            }
        }
        interactive: false   // important or the row is all "moveable"! 
        spacing: 5
        delegate: Rectangle {
            height: parent.height
            width: inputBox.width + inputLabel.width 
            color: "transparent"
            id: rowBox
            Label {  // some textfields (ex: position, color) require a label in front of the input
                id: inputLabel
                width: 12
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 7
                anchors.left: parent.left
                text: control.getCorrectLabel(valueType, index)
                font.pixelSize: 11
                color: Colors.light4
            }
            TextField {  // actual input
                id: inputBox
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 5
                anchors.left: inputLabel.right
                leftPadding: 3
                rightPadding: 3
                implicitWidth: 70
                implicitHeight: 20
                font.pixelSize: 12
                color: Colors.light1 
                placeholderTextColor: Colors.light4
                autoScroll: false
                readOnly: Model.Operations.getSetterOperation(operations) ? false : true
                // values: if default value it will be a placeholder, otherwise if a value is set it will be shown 
                placeholderText: model.modelData === undefined && defaultValue !== undefined ? defaultValue[index] : ""
                text: {
                    let locale = Qt.locale()
                    let precision = 0
                    let text = model.modelData === undefined ? "" : JSON.stringify(model.modelData)
                    if (text.includes(".")) {
                        precision = text.slice(text.indexOf(".") + 1).length
                        text = Number(model.modelData).toLocaleString(locale, "f", precision)
                    }
                    return text
                } 
                validator: DoubleValidator{notation: DoubleValidator.StandardNotation}
                Rectangle {
                    z: -1
                    anchors.fill: parent
                    color: Colors.dark1
                }
                onEditingFinished: {
                    const isTextEmpty = text === "" || isEulerMode ? eulerRotationArray === undefined : arrayValue === undefined;
                    if (defaultValue === undefined && isTextEmpty) {
                        return;
                    }
                    const operationName = Model.Operations.getSetterOperation(operations);

                    if (isRowShowingRotation && isEulerMode) {
                        // only relevant for rotations
                        let oldValue = JSON.stringify(arrayValue);
                        let parameterValue = eulerRotationArray ? eulerRotationArray : JSON.parse(JSON.stringify(quaternionToEuler(defaultValue)));
                        let parameters = {};
                        let locale = Qt.locale();
                        parameterValue[index] = text === "" ? defaultValue[index] : Number.fromLocaleString(locale, text);
                        
                        let quaternionArray = eulerToQuaternion(parameterValue);
                        if (oldValue !== JSON.stringify(quaternionArray)) {
                            parameters["value"] = quaternionArray;
                            parameters["path"] = path;
                            Model.Operations.execute(operationName, parameters).then((message) => {
                            }).catch((reason) => {
                                inputBox.undo();
                                inputBox.reject();
                            })
                        }
                    } else {
                        // We need to do deep copies of the arrays. this is done with parse/stringifiy
                        let oldValue = JSON.stringify(arrayValue);
                        let parameterValue = arrayValue ? JSON.parse(oldValue) : JSON.parse(JSON.stringify(defaultValue));
                        let parameters = {};
                        let locale = Qt.locale();
                        parameterValue[index] = text === "" ? defaultValue[index] : Number.fromLocaleString(locale, text);
                    
                        if (oldValue !== JSON.stringify(parameterValue)) {
                            parameters["value"] = parameterValue;
                            parameters["path"] = path;
                            Model.Operations.execute(operationName, parameters).then((message) => {
                            }).catch((reason) => {
                                inputBox.undo();
                                inputBox.reject();
                            })
                        }
                    }
                }
            }
            ToolTip { // tooltip input text
                id: textTip 
                visible: inputBox.hovered && (inputBox.text !== undefined && inputBox.text.length > 6) && severity === -1
                y: -26
                height: 25
                timeout: 2500
                delay: 1000
                padding: 4
                Label {  
                    text: inputBox.text
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
    }

    Button {  // icon reset-info-delete-warning
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
        anchors.top: parent.top
        anchors.topMargin: 8
        enabled: (arrayValue !== undefined) 
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

     InspectorRow.IndicatorToolTip {
        visible: hoverHandler.hovered && severity !== -1
        issueString: issueMessage
        offset: validationIcon.x
        toolTipColor: tooltipColor
    }

    ToolTip { // tooltip input text
        id: errorMessage
        parent: iconAction
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
