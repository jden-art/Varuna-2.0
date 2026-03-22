import QtQuick

Item {
    id: toastRoot

    height: Math.round(44 * rootWindow.dp)

    property string message: ""
    property color toastColor: Theme.primary
    property bool _showing: false

    function show(msg, clr) {
        message = msg;
        if (clr !== undefined && clr !== null) {
            toastColor = clr;
        } else {
            toastColor = Theme.primary;
        }
        _showing = true;
        dismissTimer.restart();
    }

    function hide() {
        _showing = false;
        dismissTimer.stop();
    }

    Timer {
        id: dismissTimer
        interval: 3000
        onTriggered: toastRoot._showing = false
    }

    Rectangle {
        id: toastPill
        anchors.horizontalCenter: parent.horizontalCenter
        width: toastContentRow.implicitWidth + Math.round(32 * rootWindow.dp)
        height: Math.round(40 * rootWindow.dp)
        radius: height / 2
        color: toastRoot.toastColor

        y: toastRoot._showing ? 0 : Math.round(60 * rootWindow.dp)
        opacity: toastRoot._showing ? 1.0 : 0.0

        Behavior on y {
            NumberAnimation {
                duration: Settings.animationsEnabled ? 250 : 0
                easing.type: Easing.OutCubic
            }
        }

        Behavior on opacity {
            NumberAnimation {
                duration: Settings.animationsEnabled ? 200 : 0
                easing.type: Easing.OutQuad
            }
        }

        Behavior on color {
            ColorAnimation { duration: Theme.animDuration }
        }

        layer.enabled: true
        layer.effect: Item {}

        Rectangle {
            anchors.fill: parent
            anchors.topMargin: Math.round(2 * rootWindow.dp)
            radius: parent.radius
            color: "#000000"
            opacity: 0.15
            z: -1
        }

        Row {
            id: toastContentRow
            anchors.centerIn: parent
            spacing: Math.round(8 * rootWindow.dp)

            Rectangle {
                width: Math.round(6 * rootWindow.dp)
                height: Math.round(6 * rootWindow.dp)
                radius: width / 2
                color: "#ffffff"
                opacity: 0.9
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: toastRoot.message
                font.family: "Noto Sans"
                font.pixelSize: Math.round(13 * rootWindow.sp)
                font.weight: Font.Medium
                color: "#ffffff"
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
