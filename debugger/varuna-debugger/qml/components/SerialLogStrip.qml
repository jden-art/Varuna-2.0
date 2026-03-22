// -----------------------------------------------------------------
// File: qml/components/SerialLogStrip.qml
// Phase: Phase 5 Step 4
// -----------------------------------------------------------------

import QtQuick

Item {
    id: logStrip
    width: parent.width
    height: logStrip.expanded ? Math.round(120 * dp) : Math.round(28 * dp)
    clip: true

    Behavior on height {
        NumberAnimation {
            duration: Settings.animationsEnabled ? 200 : 0
            easing.type: Easing.OutQuad
        }
    }

    readonly property real dp: Math.max(1, Math.min(parent.width / 1280, parent.height / 720))
    readonly property real sp: dp * Settings.fontScale
    property bool expanded: false

    // Live log entries from DeviceModel
    property var logEntries: Device.logEntries

    // Fallback: show a placeholder if no entries yet
    property var displayEntries: {
        if (logEntries && logEntries.length > 0) {
            return logEntries;
        }
        return [{"prefix": "STATUS", "message": "Waiting for device data..."}];
    }

    function prefixColor(prefix) {
        if (prefix === "ERROR") return Theme.error;
        if (prefix === "WARNING") return Theme.warning;
        if (prefix === "FLOOD") return Theme.tertiary;
        return Theme.onSurfaceVariant;
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.surface

        Behavior on color {
            ColorAnimation { duration: Theme.animDuration }
        }

        // Top border
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: Theme.border

            Behavior on color {
                ColorAnimation { duration: Theme.animDuration }
            }
        }

        // Collapsed view: single line + chevron
        Item {
            id: collapsedView
            anchors.top: parent.top
            anchors.topMargin: 1
            anchors.left: parent.left
            anchors.right: parent.right
            height: Math.round(27 * logStrip.dp)
            visible: true

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: logStrip.expanded = !logStrip.expanded
            }

            // Log icon (small terminal-like icon)
            Rectangle {
                id: logIcon
                anchors.left: parent.left
                anchors.leftMargin: Math.round(8 * logStrip.dp)
                anchors.verticalCenter: parent.verticalCenter
                width: Math.round(14 * logStrip.dp)
                height: Math.round(10 * logStrip.dp)
                radius: Math.round(2 * logStrip.dp)
                color: "transparent"
                border.color: Theme.onSurfaceVariant
                border.width: 1

                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: Math.round(2 * logStrip.dp)
                    anchors.top: parent.top
                    anchors.topMargin: Math.round(2 * logStrip.dp)
                    width: Math.round(4 * logStrip.dp)
                    height: 1
                    color: Theme.onSurfaceVariant
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: Math.round(2 * logStrip.dp)
                    anchors.top: parent.top
                    anchors.topMargin: Math.round(5 * logStrip.dp)
                    width: Math.round(6 * logStrip.dp)
                    height: 1
                    color: Theme.onSurfaceVariant
                }
            }

            // Latest log line
            Text {
                anchors.left: logIcon.right
                anchors.leftMargin: Math.round(6 * logStrip.dp)
                anchors.right: chevronArea.left
                anchors.rightMargin: Math.round(4 * logStrip.dp)
                anchors.verticalCenter: parent.verticalCenter
                text: {
                    var entries = logStrip.displayEntries;
                    var last = entries[entries.length - 1];
                    return last.prefix + ": " + last.message;
                }
                font.family: "Noto Sans Mono"
                font.pixelSize: Math.round(10 * logStrip.sp)
                color: {
                    var entries = logStrip.displayEntries;
                    var last = entries[entries.length - 1];
                    return logStrip.prefixColor(last.prefix);
                }
                elide: Text.ElideRight
                clip: true
            }

            // Chevron
            Item {
                id: chevronArea
                anchors.right: parent.right
                anchors.rightMargin: Math.round(8 * logStrip.dp)
                anchors.verticalCenter: parent.verticalCenter
                width: Math.round(12 * logStrip.dp)
                height: Math.round(12 * logStrip.dp)

                Text {
                    anchors.centerIn: parent
                    text: logStrip.expanded ? "\u25BC" : "\u25B2"
                    font.family: "Noto Sans"
                    font.pixelSize: Math.round(8 * logStrip.dp)
                    color: Theme.onSurfaceVariant
                }
            }
        }

        // Expanded view: scrollable log list
        Flickable {
            id: logListFlick
            anchors.top: collapsedView.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Math.round(2 * logStrip.dp)
            clip: true
            contentHeight: logListCol.height
            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds
            visible: logStrip.expanded

            // Auto-scroll to bottom when new entries arrive
            onContentHeightChanged: {
                if (contentHeight > height) {
                    contentY = contentHeight - height;
                }
            }

            Column {
                id: logListCol
                width: parent.width

                Repeater {
                    model: logStrip.displayEntries.length

                    Item {
                        width: logListCol.width
                        height: Math.round(18 * logStrip.dp)

                        Rectangle {
                            anchors.fill: parent
                            color: index % 2 === 0 ? "transparent" : Theme.surfaceVariant
                            opacity: index % 2 === 0 ? 1.0 : 0.3
                        }

                        Row {
                            anchors.left: parent.left
                            anchors.leftMargin: Math.round(8 * logStrip.dp)
                            anchors.right: parent.right
                            anchors.rightMargin: Math.round(8 * logStrip.dp)
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Math.round(6 * logStrip.dp)

                            // Line number
                            Text {
                                text: (index + 1) + ""
                                font.family: "Noto Sans Mono"
                                font.pixelSize: Math.round(9 * logStrip.sp)
                                color: Theme.onSurfaceVariant
                                opacity: 0.5
                                width: Math.round(14 * logStrip.dp)
                                horizontalAlignment: Text.AlignRight
                            }

                            // Prefix badge
                            Rectangle {
                                width: prefixBadgeText.implicitWidth + Math.round(8 * logStrip.dp)
                                height: Math.round(14 * logStrip.dp)
                                radius: Math.round(3 * logStrip.dp)
                                color: logStrip.prefixColor(logStrip.displayEntries[index].prefix)
                                opacity: 0.2
                                anchors.verticalCenter: parent.verticalCenter

                                Text {
                                    id: prefixBadgeText
                                    anchors.centerIn: parent
                                    text: logStrip.displayEntries[index].prefix
                                    font.family: "Noto Sans Mono"
                                    font.pixelSize: Math.round(8 * logStrip.sp)
                                    font.weight: Font.Medium
                                    color: logStrip.prefixColor(logStrip.displayEntries[index].prefix)
                                }
                            }

                            // Message
                            Text {
                                text: logStrip.displayEntries[index].message
                                font.family: "Noto Sans Mono"
                                font.pixelSize: Math.round(9 * logStrip.sp)
                                color: logStrip.prefixColor(logStrip.displayEntries[index].prefix)
                                elide: Text.ElideRight
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }
        }
    }
}

// -----------------------------------------------------------------
// File: qml/components/SerialLogStrip.qml
// Phase: Phase 5 Step 4
// ----------------------------END----------------------------------
