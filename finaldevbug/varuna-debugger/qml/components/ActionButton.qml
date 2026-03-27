// -----------------------------------------------------------------
// File: qml/components/ActionButton.qml
// Phase: Phase 6 Step 1
// -----------------------------------------------------------------

import QtQuick

Item {
    id: actionBtn

    signal clicked()

    property string text: "Button"
    property bool filled: true
    property bool enabled: true
    property color buttonColor: Theme.primary

    implicitWidth: buttonLabel.implicitWidth + Math.round(48 * rootWindow.dp)
    implicitHeight: Math.round(44 * rootWindow.dp)

    opacity: {
        if (!actionBtn.enabled) return 1.0;
        if (mouseArea.pressed) return 0.85;
        return 1.0;
    }

    Behavior on opacity {
        NumberAnimation {
            duration: Settings.animationsEnabled ? 100 : 0
            easing.type: Easing.OutQuad
        }
    }

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: height / 2
        color: {
            if (!actionBtn.enabled) {
                return actionBtn.filled ? Theme.surfaceVariant : "transparent";
            }
            return actionBtn.filled ? actionBtn.buttonColor : "transparent";
        }
        border.width: actionBtn.filled ? 0 : Math.max(1, Math.round(1.5 * rootWindow.dp))
        border.color: actionBtn.enabled ? actionBtn.buttonColor : Theme.onSurfaceVariant
        clip: true

        Behavior on color {
            ColorAnimation { duration: Theme.animDuration }
        }
        Behavior on border.color {
            ColorAnimation { duration: Theme.animDuration }
        }

        // Ripple circle
        Rectangle {
            id: rippleCircle
            property real cx: 0
            property real cy: 0
            x: cx - width / 2
            y: cy - height / 2
            width: 0
            height: width
            radius: width / 2
            color: actionBtn.filled ? "#ffffff" : actionBtn.buttonColor
            opacity: 0
            visible: opacity > 0

            ParallelAnimation {
                id: rippleAnim

                NumberAnimation {
                    target: rippleCircle
                    property: "width"
                    from: 0
                    to: Math.max(bg.width, bg.height) * 2.5
                    duration: Settings.animationsEnabled ? 300 : 0
                    easing.type: Easing.OutQuad
                }

                SequentialAnimation {
                    NumberAnimation {
                        target: rippleCircle
                        property: "opacity"
                        from: 0.15
                        to: 0.15
                        duration: Settings.animationsEnabled ? 200 : 0
                    }
                    NumberAnimation {
                        target: rippleCircle
                        property: "opacity"
                        from: 0.15
                        to: 0
                        duration: Settings.animationsEnabled ? 100 : 0
                        easing.type: Easing.OutQuad
                    }
                }
            }
        }

        // Button text
        Text {
            id: buttonLabel
            anchors.centerIn: parent
            text: actionBtn.text
            font.family: "Noto Sans"
            font.pixelSize: Math.round(14 * rootWindow.sp)
            font.weight: Font.Medium
            color: {
                if (!actionBtn.enabled) return Theme.onSurfaceVariant;
                return actionBtn.filled ? "#ffffff" : actionBtn.buttonColor;
            }

            Behavior on color {
                ColorAnimation { duration: Theme.animDuration }
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: actionBtn.enabled
        cursorShape: actionBtn.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor

        onPressed: function(mouse) {
            if (actionBtn.enabled) {
                rippleCircle.cx = mouse.x;
                rippleCircle.cy = mouse.y;
                rippleAnim.stop();
                rippleCircle.width = 0;
                rippleCircle.opacity = 0;
                rippleAnim.start();
            }
        }

        onClicked: {
            actionBtn.clicked();
        }
    }
}

// -----------------------------------------------------------------
// File: qml/components/ActionButton.qml
// Phase: Phase 6 Step 1
// ----------------------------END----------------------------------
