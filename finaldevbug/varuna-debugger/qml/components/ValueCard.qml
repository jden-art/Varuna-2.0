// -----------------------------------------------------------------
// File: qml/components/ValueCard.qml
// Phase: Phase 7 Step 2
// -----------------------------------------------------------------

import QtQuick

Rectangle {
    id: valueCard

    property string label: ""
    property string value: "--"
    property string unit: ""
    property string subLabel: ""
    property color statusColor: Theme.onBackground
    property bool showBottomBar: false
    property real bottomBarPercent: 0.0
    property color bottomBarColor: Theme.primary

    radius: Math.round(12 * rootWindow.dp)
    color: Theme.surface
    border.width: 1
    border.color: Theme.border

    Behavior on color {
        ColorAnimation { duration: Theme.animDuration }
    }
    Behavior on border.color {
        ColorAnimation { duration: Theme.animDuration }
    }

    // Label (top-left)
    Text {
        id: labelText
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: Math.round(10 * rootWindow.dp)
        anchors.leftMargin: Math.round(12 * rootWindow.dp)
        anchors.rightMargin: Math.round(12 * rootWindow.dp)
        text: valueCard.label
        font.family: "Noto Sans"
        font.pixelSize: Math.round(12 * rootWindow.sp)
        font.weight: Font.Medium
        color: Theme.onSurfaceVariant
        elide: Text.ElideRight

        Behavior on color {
            ColorAnimation { duration: Theme.animDuration }
        }
    }

    // Value + unit (centered)
    Row {
        id: valueRow
        anchors.centerIn: parent
        anchors.verticalCenterOffset: Math.round(2 * rootWindow.dp)
        spacing: Math.round(3 * rootWindow.dp)

        Text {
            id: valueText
            text: valueCard.value
            font.family: "Noto Sans"
            font.pixelSize: Math.round(26 * rootWindow.sp)
            font.weight: Font.DemiBold
            color: valueCard.statusColor
            visible: valueCard.value !== ""

            Behavior on color {
                ColorAnimation { duration: Theme.animDuration }
            }
        }

        Text {
            text: valueCard.unit
            font.family: "Noto Sans"
            font.pixelSize: Math.round(14 * rootWindow.sp)
            font.weight: Font.Normal
            color: Theme.onSurfaceVariant
            anchors.baseline: valueText.baseline
            visible: valueCard.unit !== "" && valueCard.value !== ""

            Behavior on color {
                ColorAnimation { duration: Theme.animDuration }
            }
        }
    }

    // Sub-label (bottom-left)
    Text {
        id: subLabelText
        anchors.bottom: bottomBar.visible ? bottomBar.top : parent.bottom
        anchors.bottomMargin: bottomBar.visible ? Math.round(6 * rootWindow.dp) : Math.round(10 * rootWindow.dp)
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Math.round(12 * rootWindow.dp)
        anchors.rightMargin: Math.round(12 * rootWindow.dp)
        text: valueCard.subLabel
        font.family: "Noto Sans"
        font.pixelSize: Math.round(11 * rootWindow.sp)
        color: Theme.onSurfaceVariant
        elide: Text.ElideRight
        visible: valueCard.subLabel !== ""

        Behavior on color {
            ColorAnimation { duration: Theme.animDuration }
        }
    }

    // Bottom bar
    Rectangle {
        id: bottomBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Math.round(12 * rootWindow.dp)
        anchors.rightMargin: Math.round(12 * rootWindow.dp)
        anchors.bottomMargin: Math.round(8 * rootWindow.dp)
        height: Math.round(4 * rootWindow.dp)
        radius: Math.round(2 * rootWindow.dp)
        color: Theme.surfaceVariant
        visible: valueCard.showBottomBar

        Behavior on color {
            ColorAnimation { duration: Theme.animDuration }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width * Math.max(0, Math.min(1, valueCard.bottomBarPercent))
            radius: parent.radius
            color: valueCard.bottomBarColor

            Behavior on width {
                NumberAnimation {
                    duration: Settings.animationsEnabled ? 300 : 0
                    easing.type: Easing.InOutQuad
                }
            }
            Behavior on color {
                ColorAnimation { duration: Theme.animDuration }
            }
        }
    }
}

// -----------------------------------------------------------------
// File: qml/components/ValueCard.qml
// Phase: Phase 7 Step 2
// ----------------------------END----------------------------------
