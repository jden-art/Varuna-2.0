// -----------------------------------------------------------------
// File : qml/components/NumericKeypad.qml
// Phase : Phase 10 Step 1
// -----------------------------------------------------------------

import QtQuick

Item {
    id: keypadRoot
    anchors.fill: parent
    visible: _isOpen || scrimFade.running || panelSlide.running
    z: 1800

    property bool _isOpen: false
    property string currentValue: ""
    property string targetField: ""
    property int maxLength: 8

    signal confirmed(string value)
    signal cancelled()

    function open(fieldId, initialValue) {
        targetField = fieldId;
        currentValue = initialValue !== undefined && initialValue !== null ? initialValue : "";
        _isOpen = true;
        scrimFade.to = Theme.scrimOpacity;
        scrimFade.duration = Settings.animationsEnabled ? 200 : 0;
        scrimFade.start();
        panelSlide.to = keypadRoot.height - panelBody.height;
        panelSlide.duration = Settings.animationsEnabled ? 250 : 0;
        panelSlide.start();
    }

    function close() {
        _isOpen = false;
        scrimFade.to = 0;
        scrimFade.duration = Settings.animationsEnabled ? 200 : 0;
        scrimFade.start();
        panelSlide.to = keypadRoot.height;
        panelSlide.duration = Settings.animationsEnabled ? 200 : 0;
        panelSlide.start();
    }

    function appendChar(ch) {
        if (currentValue.length >= maxLength) return;
        if (ch === "." && currentValue.indexOf(".") >= 0) return;
        if (ch === "." && currentValue.length === 0) {
            currentValue = "0.";
            return;
        }
        currentValue = currentValue + ch;
    }

    function backspace() {
        if (currentValue.length > 0) {
            currentValue = currentValue.substring(0, currentValue.length - 1);
        }
    }

    function confirmInput() {
        confirmed(currentValue);
        close();
    }

    function cancelInput() {
        cancelled();
        close();
    }

    // ═══════════════ SCRIM ═══════════════

    Rectangle {
        id: scrim
        anchors.fill: parent
        color: "#000000"
        opacity: 0

        NumberAnimation on opacity {
            id: scrimFade
            running: false
            to: 0
            duration: 200
            easing.type: Easing.OutQuad
        }

        MouseArea {
            anchors.fill: parent
            enabled: keypadRoot._isOpen
            onClicked: keypadRoot.cancelInput()
        }
    }

    // ═══════════════ PANEL ═══════════════

    Rectangle {
        id: panelBody
        width: parent.width
        height: Math.round(220 * rootWindow.dp)
        y: parent.height
        color: Theme.surface

        Behavior on color { ColorAnimation { duration: Theme.animDuration } }

        NumberAnimation on y {
            id: panelSlide
            running: false
            to: 0
            duration: 250
            easing.type: Easing.OutCubic
        }

        // Block clicks from passing to scrim
        MouseArea {
            anchors.fill: parent
        }

        // Top rounded corners
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: Math.round(16 * rootWindow.dp)
            color: Theme.surface
            radius: Math.round(16 * rootWindow.dp)

            Behavior on color { ColorAnimation { duration: Theme.animDuration } }
        }

        // Drag handle
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: Math.round(6 * rootWindow.dp)
            width: Math.round(32 * rootWindow.dp)
            height: Math.round(4 * rootWindow.dp)
            radius: Math.round(2 * rootWindow.dp)
            color: Theme.onSurfaceVariant
            opacity: 0.5

            Behavior on color { ColorAnimation { duration: Theme.animDuration } }
        }

        Column {
            anchors.fill: parent
            anchors.topMargin: Math.round(16 * rootWindow.dp)
            anchors.leftMargin: Math.round(12 * rootWindow.dp)
            anchors.rightMargin: Math.round(12 * rootWindow.dp)
            anchors.bottomMargin: Math.round(8 * rootWindow.dp)
            spacing: Math.round(8 * rootWindow.dp)

            // ═══════════════ DISPLAY ROW ═══════════════

            Rectangle {
                width: parent.width
                height: Math.round(40 * rootWindow.dp)
                radius: Math.round(8 * rootWindow.dp)
                color: Theme.surfaceVariant
                border.width: 1
                border.color: Theme.primary

                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                Row {
                    anchors.centerIn: parent
                    spacing: 0

                    Text {
                        text: keypadRoot.currentValue.length > 0 ? keypadRoot.currentValue : "0"
                        font.family: "Noto Sans Mono"
                        font.pixelSize: Math.round(22 * rootWindow.sp)
                        font.weight: Font.Medium
                        color: keypadRoot.currentValue.length > 0 ? Theme.onBackground : Theme.onSurfaceVariant

                        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                    }

                    Rectangle {
                        id: cursor
                        width: Math.round(2 * rootWindow.dp)
                        height: Math.round(24 * rootWindow.dp)
                        color: Theme.primary
                        anchors.verticalCenter: parent.verticalCenter

                        SequentialAnimation on opacity {
                            loops: Animation.Infinite
                            running: keypadRoot._isOpen

                            NumberAnimation {
                                from: 1.0
                                to: 0.0
                                duration: 500
                            }
                            NumberAnimation {
                                from: 0.0
                                to: 1.0
                                duration: 500
                            }
                        }
                    }
                }
            }

            // ═══════════════ KEY GRID ═══════════════

            Grid {
                id: keyGrid
                width: parent.width
                columns: 4
                spacing: Math.round(6 * rootWindow.dp)

                readonly property real keyW: (width - 3 * spacing) / 4
                readonly property real keyH: Math.round(42 * rootWindow.dp)

                Repeater {
                    model: ["7", "8", "9", "BS", "4", "5", "6", ".", "1", "2", "3", "0"]

                    Rectangle {
                        required property string modelData
                        required property int index

                        width: keyGrid.keyW
                        height: keyGrid.keyH
                        radius: Math.round(8 * rootWindow.dp)
                        color: {
                            if (modelData === "BS") return keyMa.pressed ? Theme.error : Theme.surfaceVariant;
                            return keyMa.pressed ? Theme.primaryContainer : Theme.surfaceVariant;
                        }

                        Behavior on color { ColorAnimation { duration: 80 } }

                        Text {
                            anchors.centerIn: parent
                            text: {
                                if (modelData === "BS") return "\u232B";
                                return modelData;
                            }
                            font.family: modelData === "BS" ? "Noto Sans" : "Noto Sans Mono"
                            font.pixelSize: {
                                if (modelData === "BS") return Math.round(20 * rootWindow.sp);
                                return Math.round(18 * rootWindow.sp);
                            }
                            font.weight: Font.Medium
                            color: {
                                if (modelData === "BS") return keyMa.pressed ? "#ffffff" : Theme.error;
                                return Theme.onBackground;
                            }

                            Behavior on color { ColorAnimation { duration: 80 } }
                        }

                        MouseArea {
                            id: keyMa
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (modelData === "BS") {
                                    keypadRoot.backspace();
                                } else {
                                    keypadRoot.appendChar(modelData);
                                }
                            }
                        }
                    }
                }
            }

            // ═══════════════ CONFIRM BUTTON ═══════════════

            Rectangle {
                width: parent.width
                height: Math.round(42 * rootWindow.dp)
                radius: Math.round(20 * rootWindow.dp)
                color: confirmMa.pressed ? Qt.darker(Theme.primary, 1.1) : Theme.primary

                Behavior on color { ColorAnimation { duration: 80 } }

                Text {
                    anchors.centerIn: parent
                    text: "\u2713 Confirm"
                    font.family: "Noto Sans"
                    font.pixelSize: Math.round(15 * rootWindow.sp)
                    font.weight: Font.Medium
                    color: "#ffffff"
                }

                MouseArea {
                    id: confirmMa
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: keypadRoot.confirmInput()
                }
            }
        }
    }
}

// -----------------------------------------------------------------
// File : qml/components/NumericKeypad.qml
// Phase : Phase 10 Step 1
// ----------------------------END----------------------------------
