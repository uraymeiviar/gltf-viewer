pragma Singleton

import QtQuick

Item {
    property color foreground: "#f4f4f4"
    property color background: "#2b2b2b"
    property color backgroundColor: "#2b2b2b"
    property color accentColor: "#87c540"
    property color accent: "#87c540"
    property color primary: "#373737"
    property color backgroundDimColor: "#99fafafa"
    property color buttonColor: "#3FCCCCCC" // color of "raisedButtonColorDark", not completely sure here
    property color buttonDisabledColor: "#1EFFFFFF" // color of "raisedButtonDisabledColorDark"
    property color hintTextColor: "#4CFFFFFF"
    property color primaryHighlightedTextColor: "#FFFFFF"
    property color highlightedCheckedButtonColor: "#FFFFFF" // TODO this is the accent color but lighter, better pick it from old Gestaltor
    property color highlightedButtonColor: "#87c540"
    property color primaryTextColor: "#FFFFFF"
    property color secondaryTextColor: "#B2FFFFFF"
    property color textSelectionColor: "#6687c540" // alpha of 0.4
    property color highlightedRippleColor: "#32FFFFFF" // alpha is 50, we interpreted this as being 50 out of 255
    property color rippleColor: "#20FFFFFF"
    property color listHighlightColor: "#1EFFFFFF"
    property color dialogColor: "#FF424242"
    property color frameColor: "#4CFFFFFF"
    property color dividerColor: "#1EFFFFFF"
    property color sliderDisabledColor: "#FF616161"
    property color scrollBarPressedColor: "#80FFFFFF"
    property color scrollBarHoveredColor: "#60FFFFFF"
    property color scrollBarColor: "#40FFFFFF"
    property color spinBoxDisabledIconColor: "#FF666666"
    property color placeholderTextColor: "#FFFFFF" // TODO this does not exist in qquickmaterialstyle.cpp
    property color toolTextColor: "#FFFFFF"
    property color toolBarColor: "#373737"
    property color tooltipColor: "#2b2b2b" // most likely is the background color, but might be different
    property color switchCheckedTrackColor: "#8087c540"
    property color switchUncheckedTrackColor: "#4CFFFFFF"
    property color switchDisabledTrackColor: "#19FFFFFF"
    property color switchCheckedHandleColor: "#87c540"
    property color switchUncheckedHandleColor: "#BDBDBD"
    property color switchDisabledHandleColor: "#424242"
    property color comboBoxHighlightColor: "#838383"

    property color dark0: "#212121"
    property color dark05: "#272727"
    property color dark1: "#2b2b2b"
    property color dark2: "#373737"
    property color dark205: "#3b3b3b"
    property color dark3: "#424242"
    property color dark305: "#494949"
    property color dark4: "#4d4d4d"
    property color dark5: "#666666"

    property color lightgreen: "#9CCD63"
    property color green: "#87C540"
    property color green3: "#73A43A"
    property color darkgreen: "#72A339"
    property color darkgreen2: "#44592B"
    property color darkgreen3: "#3F4834"

    property color light0: "#ffffff"
    property color light1: "#f4f4f4"
    property color light3: "#d3d3d3"
    property color light4: "#909090"
    property color light5: "#BCBCBC"

    property color red: "#FF3434"
    property color red1: "#F85E5E"
    property color red2: "#642626"
    property color red3: "#F05C5C"
}

// TODO find out the correct colors for all variables that are #FFFFFF
// try to find out the formula that relates these colors to the accent, primary, foreground and background colors if possible
// TODO check all qml files for #FFFFFF and replace it with a coloriable
// TODO in Button.qml:65, ComboBox.qml:59/143 Material.background/foreground is being set. Why is that?
