// -----------------------------------------------------------------
// File: qml/components/StatusBadge.qml
// Phase: Phase 6 Step 1
// -----------------------------------------------------------------

import QtQuick

Rectangle {
    id: badge

    property string text: ""
    property string status: "WAITING"

    readonly property color statusColor: {
        if (status === "PASS") return Theme.secondary;
        if (status === "WARN") return Theme.warning;
        if (status === "FAIL") return Theme.error;
        return Theme.onSurfaceVariant;
    }

    width: badgeText.implicitWidth + Math.round(24 * rootWindow.dp)
    height: Math.round(22 * rootWindow.dp)
    radius: Math.round(11 * rootWindow.dp)
    color: statusColor
    opacity: status === "WAITING" ? 0.5 : 1.0

    Behavior on color {
        ColorAnimation { duration: Theme.animDuration }
    }

    Behavior on opacity {
        NumberAnimation {
            duration: Settings.animationsEnabled ? 300 : 0
        }
    }

    Behavior on scale {
        NumberAnimation {
            duration: Settings.animationsEnabled ? 300 : 0
            easing.type: Easing.OutBack
            easing.overshoot: 1.2
        }
    }

    Text {
        id: badgeText
        anchors.centerIn: parent
        text: badge.text !== "" ? badge.text : badge.status
        font.family: "Noto Sans"
        font.pixelSize: Math.round(10 * rootWindow.sp)
        font.weight: Font.Medium
        color: {
            if (badge.status === "WAITING") return Theme.onBackground;
            if (badge.status === "WARN") return "#202124";
            return "#ffffff";
        }

        Behavior on color {
            ColorAnimation { duration: Theme.animDuration }
        }
    }
}

// -----------------------------------------------------------------
// File: qml/components/StatusBadge.qml
// Phase: Phase 6 Step 1
// ----------------------------END----------------------------------
