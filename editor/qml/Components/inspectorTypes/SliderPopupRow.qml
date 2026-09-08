import QtQuick 2.15
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.15

import Model 1.0 as Model
import "../../Colors.js" as Colors


// row
Rectangle {   //container
    id: mainRow
    color:  Colors.dark2
    height: 60
    implicitHeight: 60


    // imported for each row 
    property string labelTitle: "" 
    property var isPercentage: false

    // value 
    property var valueNum: 90  
    property var minValue: 1  
    property var maxValue: 100
    property var stepSize: 1
    
    Text { // label name 
        id: labelText
        text: labelTitle
        anchors.left: parent.left
        anchors.leftMargin: 5
        anchors.top: parent.top
        anchors.topMargin: 5
        color: Colors.light1 
    }

    TextField {  // actual input
        id: inputBox
        placeholderText: "90"
        text: {
            let locale = Qt.locale()
            let precision = 0 
            let text = valueNum === undefined ? "" : JSON.stringify(valueNum)
            if (text.includes(".")) {
                precision = text.slice(text.indexOf(".") + 1).length
                text = Number(valueNum).toLocaleString(locale, "f", precision)
            }
            return text
        } 
        validator: DoubleValidator{bottom: minValue; top: maxValue;}
        autoScroll: false
        anchors.top: parent.top
        anchors.topMargin: 5
        anchors.right: parent.right
        anchors.rightMargin: 25 
        implicitHeight: 20
        font.pixelSize: 12
        placeholderTextColor: Colors.light4
        leftPadding: 2
        color: Colors.light1 
        implicitWidth: 70
        Rectangle {
            z: -1
            anchors.fill: parent
            color: Colors.dark1
        }
        onEditingFinished: {
            let locale = Qt.locale()
            valueNum = Number.fromLocaleString(locale, text);
        }
    }

    Label { // "%" placeholder on the right of the input 
        id: inputLabel
        visible: isPercentage
        width: 15
        anchors.top: parent.top
        anchors.topMargin: 5
        anchors.right: parent.right
        anchors.rightMargin: 5
        text: "%"
        font.pixelSize: 11
        color: Colors.light1
    }

    Slider { 
        id: sliderInput
        anchors.top: inputLabel.bottom
        anchors.topMargin: 5
        anchors.right: parent.right
        anchors.rightMargin: 20
        anchors.left: parent.left
        anchors.leftMargin: 5
        width: 200
        from: minValue
        to: maxValue
        stepSize: mainRow.stepSize
        value: valueNum !== undefined ? valueNum : 0 
        onMoved: {
            valueNum = sliderInput.value
        }  
    }

    Label { // min value for slider 
        id: minLabel
        width: 15
        anchors.top: sliderInput.bottom
        anchors.topMargin: -5
        anchors.left: sliderInput.left
        text: minValue
        font.pixelSize: 10
        color: Colors.light4
    }

    Label { // max value for slider 
        id: maxLabel
        width: 15
        anchors.top: sliderInput.bottom
        anchors.topMargin: -5
        anchors.right: sliderInput.right
        anchors.rightMargin: -5
        text: maxValue
        font.pixelSize: 10
        color: Colors.light4
    }
}