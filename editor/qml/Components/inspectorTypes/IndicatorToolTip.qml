import QtQuick
import QtQuick.Shapes 1.4
import QtQuick.Controls 2.15

import "../../Colors.js" as Colors

ToolTip { // tooltip input text
        property var offset: undefined
        required property var toolTipColor
        property string issueString

        property var shapeSize: 14

        y: -1* implicitHeight - shapeSize / 2

        // Left aligned (9 is padding lower values do not work in Inspector)
        x: 9
        delay: 1000
        padding: 4
        contentItem: Text {  
            text: issueMessage
            font.pixelSize: 12
            color: Colors.dark2
            wrapMode: Text.Wrap
        }
        background: Item {
            Rectangle {
                anchors.fill: parent
                color: toolTipColor
                radius: 3
            }

            Shape {
                id: arrow
                width: shapeSize
                height: shapeSize
                x: offset - shapeSize/2 - padding
                y: parent.height - shapeSize/2 
                visible: shapeSize > 0

                ShapePath {
                    strokeWidth: 1
                    fillColor: toolTipColor
                    strokeColor: toolTipColor
                    fillRule: ShapePath.WindingFill
                    startX: 0; startY: shapeSize/2
                    PathLine { x: shapeSize; y: shapeSize/2 }
                    PathLine { x: shapeSize/2; y: shapeSize }
                    PathLine { x: 0; y: shapeSize/2 }
                }
            }
        }
    }