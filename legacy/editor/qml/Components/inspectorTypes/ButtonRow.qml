
import QtQuick 2.15
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.15

import Model 1.0 as Model

import "../../Colors.js" as Colors


// actual row item

Rectangle {   // the actual row
    id: mainRow
    color:  Colors.dark3
    width: parent.width
    
    // imported for each row 
    property var modelArray


    Flow {
        id: buttonList
        anchors.top: parent.top 
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: 5
        anchors.leftMargin: 10 
        anchors.rightMargin: 10 
        anchors.bottomMargin: 5 
        spacing: 10

        onPositioningComplete: {
            mainRow.height = height + anchors.topMargin + anchors.bottomMargin
        }

        Repeater {
            model: modelArray 
            delegate: Button {
                id: rootButton
                flat: true
                text: model.modelData.btnText
                font.pixelSize: 12
                padding: 10
                hoverEnabled: true 
                //enabled: model.modelData.btnEnabled ? model.modelData.btnEnabled : true 
                contentItem: Label {
                    text: rootButton.text
                    font: rootButton.font
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                }
                background: Rectangle {
                    color: rootButton.hovered ? Colors.dark305 : Colors.dark4
                }
                onClicked: {
                    if (model.modelData.popupTitle) {
                        const globalPosButton = rootButton.mapToGlobal(0,rootButton.height)
                        const globalPosRect = mainRow.mapToGlobal(0,0)
                        applicationData.showPopup(model.modelData.popupModel, model.modelData.popupTitle, model.modelData.popupHeight, 380, globalPosRect.x, globalPosButton.y)
                    } else if (model.modelData.btnOperation !== ""){
	                    let parameters = {};
                        if (model.modelData.parameters) {
                            parameters =model.modelData.parameters
                        }
	                    parameters["path"] = "/gltf" + pathPanel;

	                    Model.Operations.execute(model.modelData.btnOperation, parameters).catch((message) => {
                            rootButton.reject();
                            errorMessage.parent = rootButton;
                            errorMessage.show(message, 3000);
                        });
                    }else if (model.modelData.btnFunction !== undefined){
                        model.modelData.btnFunction()
                    }
                }
            }
        }
    }

    ToolTip { // tooltip input text
        id: errorMessage
        parent: buttonList
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
