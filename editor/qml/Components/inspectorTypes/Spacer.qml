import QtQuick

import "../../Colors.js" as Colors

Item { 
    // the component takes the space between 2 elements and changes the layout of a row 
    id: spacer
    width: 10
    height: 2

    property var leftItem: null  // left element 
    property var rightItem: null  // right element 
    property var elementWidth: 1  // element space needed 
    
    anchors.left: leftItem.right
    anchors.right: rightItem.left
    anchors.top: mainRow.top

    // based on the width available, the row will change its height 
    onWidthChanged: {
        if (spacer.width < elementWidth) {
            mainRow.heightRow = 50 
        } else if (spacer.width >= elementWidth) {
            mainRow.heightRow = 30 
        }
    }

}