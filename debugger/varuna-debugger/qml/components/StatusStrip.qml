// -----------------------------------------------------------------
// File: qml/components/StatusStrip.qml
// Phase: Phase 6 Step 1
// -----------------------------------------------------------------

import QtQuick

Item {
    id: statusStrip
    width: parent.width
    height: Math.round(36 * dp)

    readonly property real dp: Math.max(1, Math.min(parent.width / 1280, parent.height / 720))
    readonly property real sp: dp * Settings.fontScale

    property string screenTitle: "Boot check"
    property int currentScreenIndex: 0

    property int sessionSeconds: 0

    Timer {
        id: sessionTimer
        interval: 1000
        running: true
        repeat: true
        onTriggered: statusStrip.sessionSeconds += 1
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.surface

        Behavior on color {
            ColorAnimation { duration: Theme.animDuration }
        }

        // Bottom border
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.border

            Behavior on color {
                ColorAnimation { duration: Theme.animDuration }
            }
        }

        // Left: App name / Screen title
        Row {
            anchors.left: parent.left
            anchors.leftMargin: Math.round(12 * statusStrip.dp)
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0

            Text {
                text: "VARUNA"
                font.family: "Noto Sans"
                font.pixelSize: Math.round(13 * statusStrip.sp)
                font.weight: Font.DemiBold
                color: Theme.onSurfaceVariant

                Behavior on color {
                    ColorAnimation { duration: Theme.animDuration }
                }
            }

            Text {
                text: " / "
                font.family: "Noto Sans"
                font.pixelSize: Math.round(13 * statusStrip.sp)
                color: Theme.onSurfaceVariant

                Behavior on color {
                    ColorAnimation { duration: Theme.animDuration }
                }
            }

            Text {
                text: statusStrip.screenTitle
                font.family: "Noto Sans"
                font.pixelSize: Math.round(13 * statusStrip.sp)
                font.weight: Font.Medium
                color: Theme.onBackground

                Behavior on color {
                    ColorAnimation { duration: Theme.animDuration }
                }
            }
        }

        // Right side items
        Row {
            anchors.right: parent.right
            anchors.rightMargin: Math.round(12 * statusStrip.dp)
            anchors.verticalCenter: parent.verticalCenter
            spacing: Math.round(12 * statusStrip.dp)

            // Connection indicator
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: Math.round(4 * statusStrip.dp)

                // Pulse ring (behind the dot)
                Item {
                    width: Math.round(18 * statusStrip.dp)
                    height: Math.round(18 * statusStrip.dp)
                    anchors.verticalCenter: parent.verticalCenter

                    // Expanding ring
                    Rectangle {
                        id: pulseRing
                        anchors.centerIn: parent
                        width: Math.round(8 * statusStrip.dp)
                        height: width
                        radius: width / 2
                        color: "transparent"
                        border.width: Math.round(1.5 * statusStrip.dp)
                        border.color: Device.connected ? Theme.secondary : "transparent"
                        opacity: 0
                        visible: Device.connected

                        SequentialAnimation {
                            id: ringPulseAnim
                            running: Device.connected && Settings.animationsEnabled
                            loops: Animation.Infinite
                            alwaysRunToEnd: false

                            onRunningChanged: {
                                if (!running) {
                                    pulseRing.width = Math.round(8 * statusStrip.dp);
                                    pulseRing.opacity = 0;
                                }
                            }

                            ParallelAnimation {
                                NumberAnimation {
                                    target: pulseRing
                                    property: "width"
                                    from: Math.round(8 * statusStrip.dp)
                                    to: Math.round(18 * statusStrip.dp)
                                    duration: 1200
                                    easing.type: Easing.OutQuad
                                }
                                NumberAnimation {
                                    target: pulseRing
                                    property: "opacity"
                                    from: 0.7
                                    to: 0
                                    duration: 1200
                                    easing.type: Easing.OutQuad
                                }
                            }

                            PauseAnimation {
                                duration: 400
                            }
                        }
                    }

                    // Solid dot
                    Rectangle {
                        id: connDot
                        anchors.centerIn: parent
                        width: Math.round(8 * statusStrip.dp)
                        height: Math.round(8 * statusStrip.dp)
                        radius: width / 2
                        color: Device.connected ? Theme.secondary : Theme.error

                        Behavior on color {
                            ColorAnimation { duration: Theme.animDuration }
                        }
                    }
                }

                Text {
                    text: Device.connected ? "LIVE" : "NO DATA"
                    font.family: "Noto Sans"
                    font.pixelSize: Math.round(10 * statusStrip.sp)
                    font.weight: Font.Medium
                    color: Device.connected ? Theme.secondary : Theme.error
                    anchors.verticalCenter: parent.verticalCenter

                    Behavior on color {
                        ColorAnimation { duration: Theme.animDuration }
                    }
                }
            }

            // RSSI mini indicator
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: Math.round(2 * statusStrip.dp)
                height: Math.round(16 * statusStrip.dp)

                Repeater {
                    model: 4

                    Item {
                        width: Math.round(3 * statusStrip.dp)
                        height: Math.round(16 * statusStrip.dp)

                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: Math.round((4 + index * 4) * statusStrip.dp)
                            radius: 1

                            property int rssiVal: Device.simSignalRSSI
                            property var thresholds: [1, 10, 18, 25]
                            property bool barLit: rssiVal >= thresholds[index]

                            color: {
                                if (!barLit) return Theme.onSurfaceVariant;
                                if (rssiVal >= 20) return Theme.secondary;
                                if (rssiVal >= 10) return Theme.warning;
                                return Theme.error;
                            }
                            opacity: barLit ? 1.0 : 0.3

                            Behavior on color {
                                ColorAnimation { duration: Theme.animDuration }
                            }
                            Behavior on opacity {
                                NumberAnimation { duration: Theme.animDuration }
                            }
                        }
                    }
                }
            }

            // Session timer
            Text {
                text: {
                    var mins = Math.floor(statusStrip.sessionSeconds / 60);
                    var secs = statusStrip.sessionSeconds % 60;
                    var mStr = mins < 10 ? "0" + mins : "" + mins;
                    var sStr = secs < 10 ? "0" + secs : "" + secs;
                    return mStr + ":" + sStr;
                }
                font.family: "Noto Sans Mono"
                font.pixelSize: Math.round(12 * statusStrip.sp)
                color: Theme.onSurfaceVariant
                anchors.verticalCenter: parent.verticalCenter

                Behavior on color {
                    ColorAnimation { duration: Theme.animDuration }
                }
            }

            // Screen progress dots
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: Math.round(4 * statusStrip.dp)

                Repeater {
                    model: 7

                    Rectangle {
                        width: Math.round(6 * statusStrip.dp)
                        height: Math.round(6 * statusStrip.dp)
                        radius: width / 2
                        color: {
                            if (index === statusStrip.currentScreenIndex) {
                                return Theme.primary;
                            } else if (Device.screensCompleted[index]) {
                                return Theme.secondary;
                            } else {
                                return "transparent";
                            }
                        }
                        border.width: (index === statusStrip.currentScreenIndex || Device.screensCompleted[index]) ? 0 : 1
                        border.color: Theme.onSurfaceVariant
                        opacity: (index === statusStrip.currentScreenIndex || Device.screensCompleted[index]) ? 1.0 : 0.3

                        Behavior on color {
                            ColorAnimation { duration: Theme.animDuration }
                        }
                    }
                }
            }
        }
    }
}

// -----------------------------------------------------------------
// File: qml/components/StatusStrip.qml
// Phase: Phase 6 Step 1
// ----------------------------END----------------------------------
