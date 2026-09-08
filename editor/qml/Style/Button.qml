/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls 2 module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick
import QtQuick.Templates as T
import QtQuick.Controls.impl
import QtQuick.Controls.Material
import Style.impl

T.Button {
    id: control
    property var backgroundColor: "#424242"

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    topInset: 0
    bottomInset: 0
    padding: 14
    verticalPadding: padding - 4
    spacing: 6

    font.weight: Font.DemiBold
    icon.width: 24
    icon.height: 24
    icon.color: !enabled ? Colors.hintTextColor :
        flat && highlighted ? Colors.accentColor :
        highlighted ? Colors.primaryHighlightedTextColor : Colors.foreground

    Material.elevation: flat ? control.down || control.hovered ? 2 : 0
                             : control.down ? 8 : 2
    Material.background: flat ? "transparent" : undefined

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display

        icon: control.icon
        text: control.text
        font: control.font
        color: !control.enabled ? Colors.hintTextColor :
            control.flat && control.highlighted ? Colors.accentColor :
            control.highlighted ? Colors.primaryHighlightedTextColor : Colors.foreground
    }

    background: Rectangle {
        implicitWidth: 64
        implicitHeight: control.Material.buttonHeight

        radius: 2
        color: !control.enabled ? Colors.buttonDisabledColor :
                control.highlighted ? (control.checked ? Colors.highlightedCheckedButtonColor :
                Colors.highlightedButtonColor) : backgroundColor == undefined ? Colors.buttonColor : backgroundColor

        PaddedRectangle {
            y: parent.height - 4
            width: parent.width
            height: 4
            radius: 2
            topPadding: -2
            clip: true
            visible: control.checkable && (!control.highlighted || control.flat)
            color: control.checked && control.enabled ? Colors.accentColor : Colors.secondaryTextColor
        }

        // The layer is disabled when the button color is transparent so you can do
        // Colors.background: "transparent" and get a proper flat button without needing
        // to set Material.elevation as well
        layer.enabled: control.enabled && Colors.buttonColor.a > 0
        layer.effect: ElevationEffect {
            elevation: control.Material.elevation
        }

        Ripple {
            clipRadius: 2
            width: parent.width
            height: parent.height
            pressed: control.pressed
            anchor: control
            active: control.down || control.visualFocus || control.hovered
            color: control.flat && control.highlighted ? Colors.highlightedRippleColor : Colors.rippleColor
        }
    }

    Rectangle {
        id: rejection
        radius: 2
        border.color: Colors.red
        border.width: 2
        x: -border.width / 2
        y: -border.width / 2
        implicitWidth: background.width + border.width
        implicitHeight: background.height + border.width
        color: background.color
        opacity: 0.0
    }

    function reject() {
        rejectAnimation.start();
    }

    PropertyAnimation {
        id: rejectAnimation
        target: rejection
        properties: "opacity"
        from: 1.0
        to: 0.0
        easing.type: Easing.InQuad
        duration: 500
    }
}
