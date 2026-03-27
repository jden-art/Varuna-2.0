import QtQuick

Item {
    id: checkRow

    property string name: ""
    property string detail: ""
    property string result: "WAITING"
    property string measuredValue: ""

    readonly property color resultColor: {
        if (result === "PASS") return Theme.secondary;
        if (result === "WARN") return Theme.warning;
        if (result === "FAIL") return Theme.error;
        return Theme.onSurfaceVariant;
    }

    implicitWidth: parent ? parent.width : 300
    implicitHeight: Math.round(48 * rootWindow.dp)

    Rectangle {
        anchors.fill: parent
        color: "transparent"

        Rectangle {
            id: leftBar
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: Math.round(3 * rootWindow.dp)
            radius: Math.round(1.5 * rootWindow.dp)
            color: checkRow.resultColor

            Behavior on color {
                ColorAnimation { duration: Theme.animDuration }
            }
        }

        Text {
            id: nameText
            anchors.left: leftBar.right
            anchors.leftMargin: Math.round(12 * rootWindow.dp)
            anchors.top: parent.top
            anchors.topMargin: checkRow.measuredValue !== "" ? Math.round(7 * rootWindow.dp) : 0
            anchors.right: badgeItem.left
            anchors.rightMargin: Math.round(8 * rootWindow.dp)
            anchors.verticalCenter: checkRow.measuredValue === "" ? parent.verticalCenter : undefined
            text: checkRow.name
            font.family: "Noto Sans"
            font.pixelSize: Math.round(13 * rootWindow.sp)
            font.weight: Font.Medium
            color: Theme.onBackground
            elide: Text.ElideRight

            Behavior on color {
                ColorAnimation { duration: Theme.animDuration }
            }
        }

        Text {
            id: valueText
            anchors.left: leftBar.right
            anchors.leftMargin: Math.round(12 * rootWindow.dp)
            anchors.top: nameText.bottom
            anchors.topMargin: Math.round(2 * rootWindow.dp)
            anchors.right: badgeItem.left
            anchors.rightMargin: Math.round(8 * rootWindow.dp)
            text: checkRow.measuredValue
            font.family: "Noto Sans Mono"
            font.pixelSize: Math.round(11 * rootWindow.sp)
            color: Theme.onSurfaceVariant
            elide: Text.ElideRight
            visible: checkRow.measuredValue !== ""

            Behavior on color {
                ColorAnimation { duration: Theme.animDuration }
            }
        }

        Item {
            id: badgeItem
            anchors.right: parent.right
            anchors.rightMargin: Math.round(8 * rootWindow.dp)
            anchors.verticalCenter: parent.verticalCenter
            width: badge.width
            height: badge.height

            StatusBadge {
                id: badge
                status: checkRow.result
                scale: checkRow.result !== "WAITING" ? 1.0 : 0.9

                Behavior on scale {
                    NumberAnimation {
                        duration: Settings.animationsEnabled ? 300 : 0
                        easing.type: Easing.OutBack
                        easing.overshoot: 1.2
                    }
                }
            }
        }

        Rectangle {
            anchors.left: leftBar.right
            anchors.leftMargin: Math.round(12 * rootWindow.dp)
            anchors.right: parent.right
            anchors.rightMargin: Math.round(8 * rootWindow.dp)
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.border
            opacity: 0.4

            Behavior on color {
                ColorAnimation { duration: Theme.animDuration }
            }
        }
    }
}
