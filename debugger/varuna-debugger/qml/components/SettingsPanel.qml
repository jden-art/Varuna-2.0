// -----------------------------------------------------------------
// File : qml/components/SettingsPanel.qml
// Phase : Phase 3 Step 3
// -----------------------------------------------------------------

import QtQuick
import QtQuick.Controls

Item {
    id: root
    anchors.fill: parent
    visible: true
    z: 1000

    readonly property real dp: Math.max(1, Math.min(root.width / 1280, root.height / 720))
    readonly property real sp: dp * Settings.fontScale
    readonly property real panelHeight: root.height * 0.60
    readonly property real triggerZone: Math.round(40 * dp)
    property bool isOpen: false
    readonly property int animMs: Settings.animationsEnabled ? 250 : 0
    property bool aboutExpanded: false

    function open() {
        isOpen = true;
        panelAnim.to = 0;
        panelAnim.duration = animMs;
        panelAnim.start();
        scrimAnim.to = Theme.scrimOpacity;
        scrimAnim.duration = animMs;
        scrimAnim.start();
    }

    function close() {
        isOpen = false;
        panelAnim.to = -panelHeight;
        panelAnim.duration = animMs;
        panelAnim.start();
        scrimAnim.to = 0;
        scrimAnim.duration = animMs;
        scrimAnim.start();
    }

    // ==================== SCRIM ====================
    Rectangle {
        id: scrim
        anchors.fill: parent
        color: "#000000"
        opacity: 0

        NumberAnimation on opacity {
            id: scrimAnim
            running: false
            to: 0
            duration: 250
            easing.type: Easing.OutQuad
        }

        MouseArea {
            anchors.fill: parent
            enabled: root.isOpen
            onClicked: root.close()
        }
    }

    // ==================== PANEL BODY ====================
    Rectangle {
        id: panelBody
        width: parent.width
        height: root.panelHeight
        y: -root.panelHeight
        color: Theme.surface

        Behavior on color {
            ColorAnimation { duration: Theme.animDuration }
        }

        NumberAnimation on y {
            id: panelAnim
            running: false
            to: 0
            duration: 250
            easing.type: Easing.OutQuad
        }

        // Block clicks from passing through panel to scrim
        MouseArea {
            anchors.fill: parent
        }

        // Bottom rounded corners
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: Math.round(16 * root.dp)
            color: Theme.surface
            radius: Math.round(16 * root.dp)

            Behavior on color {
                ColorAnimation { duration: Theme.animDuration }
            }
        }

        // Drag handle
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Math.round(8 * root.dp)
            width: Math.round(32 * root.dp)
            height: Math.round(4 * root.dp)
            radius: Math.round(2 * root.dp)
            color: Theme.onSurfaceVariant
            z: 10

            Behavior on color {
                ColorAnimation { duration: Theme.animDuration }
            }
        }

        // ==================== HEADER ====================
        Item {
            id: headerArea
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: Math.round(48 * root.dp)
            z: 5

            Text {
                anchors.left: parent.left
                anchors.leftMargin: Math.round(16 * root.dp)
                anchors.verticalCenter: parent.verticalCenter
                text: "Settings"
                font.family: "Noto Sans"
                font.pixelSize: Math.round(18 * root.sp)
                font.weight: Font.Medium
                color: Theme.onBackground

                Behavior on color {
                    ColorAnimation { duration: Theme.animDuration }
                }
            }

            Rectangle {
                id: closeBtn
                anchors.right: parent.right
                anchors.rightMargin: Math.round(12 * root.dp)
                anchors.verticalCenter: parent.verticalCenter
                width: closeBtnLabel.implicitWidth + Math.round(24 * root.dp)
                height: Math.round(32 * root.dp)
                radius: Math.round(16 * root.dp)
                color: closeBtnMa.pressed ? Theme.primaryContainer : "transparent"

                Behavior on color {
                    ColorAnimation { duration: 100 }
                }

                Text {
                    id: closeBtnLabel
                    anchors.centerIn: parent
                    text: "Close"
                    font.family: "Noto Sans"
                    font.pixelSize: Math.round(14 * root.sp)
                    font.weight: Font.Medium
                    color: Theme.primary

                    Behavior on color {
                        ColorAnimation { duration: Theme.animDuration }
                    }
                }

                MouseArea {
                    id: closeBtnMa
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.close()
                }
            }

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
        }

        // ==================== SCROLLABLE SETTINGS LIST ====================
        Flickable {
            id: flickArea
            anchors.top: headerArea.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Math.round(24 * root.dp)
            clip: true
            contentHeight: settingsCol.height
            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds

            Column {
                id: settingsCol
                width: flickArea.width

                // ===== 1. DARK MODE (toggle) =====
                Item {
                    width: parent.width
                    height: Math.round(56 * root.dp)

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Settings.setDarkMode(!Settings.darkMode)
                    }

                    Rectangle {
                        id: dmIconBox
                        anchors.left: parent.left
                        anchors.leftMargin: Math.round(16 * root.dp)
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.round(24 * root.dp)
                        height: Math.round(24 * root.dp)
                        color: "transparent"

                        Rectangle {
                            anchors.centerIn: parent
                            width: Math.round(18 * root.dp)
                            height: Math.round(18 * root.dp)
                            radius: width / 2
                            color: "transparent"
                            border.color: Theme.onSurfaceVariant
                            border.width: Math.round(2 * root.dp)

                            Rectangle {
                                anchors.right: parent.right
                                anchors.rightMargin: -Math.round(3 * root.dp)
                                anchors.top: parent.top
                                anchors.topMargin: -Math.round(1 * root.dp)
                                width: Math.round(12 * root.dp)
                                height: Math.round(12 * root.dp)
                                radius: width / 2
                                color: Theme.surface

                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                            }
                        }
                    }

                    Column {
                        anchors.left: dmIconBox.right
                        anchors.leftMargin: Math.round(16 * root.dp)
                        anchors.right: dmToggleArea.left
                        anchors.rightMargin: Math.round(12 * root.dp)
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Math.round(2 * root.dp)

                        Text { text: "Dark mode"; font.family: "Noto Sans"; font.pixelSize: Math.round(14 * root.sp); color: Theme.onBackground; width: parent.width; elide: Text.ElideRight }
                        Text { text: "Reduce eye strain in low light"; font.family: "Noto Sans"; font.pixelSize: Math.round(11 * root.sp); color: Theme.onSurfaceVariant; width: parent.width; elide: Text.ElideRight }
                    }

                    Item {
                        id: dmToggleArea
                        anchors.right: parent.right
                        anchors.rightMargin: Math.round(16 * root.dp)
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.round(44 * root.dp)
                        height: Math.round(24 * root.dp)

                        Rectangle {
                            id: dmTrack
                            anchors.centerIn: parent
                            width: Math.round(36 * root.dp)
                            height: Math.round(20 * root.dp)
                            radius: height / 2
                            color: Settings.darkMode ? Theme.primary : Theme.onSurfaceVariant
                            opacity: Settings.darkMode ? 1.0 : 0.38
                            Behavior on color { ColorAnimation { duration: Settings.animationsEnabled ? 150 : 0 } }
                            Behavior on opacity { NumberAnimation { duration: Settings.animationsEnabled ? 150 : 0 } }
                        }
                        Rectangle {
                            width: Math.round(16 * root.dp); height: Math.round(16 * root.dp); radius: width / 2; color: "#ffffff"
                            anchors.verticalCenter: parent.verticalCenter
                            x: Settings.darkMode ? (dmTrack.x + dmTrack.width - width - Math.round(2 * root.dp)) : (dmTrack.x + Math.round(2 * root.dp))
                            Behavior on x { NumberAnimation { duration: Settings.animationsEnabled ? 150 : 0; easing.type: Easing.OutQuad } }
                        }
                    }
                }

                // divider
                Rectangle { width: parent.width - Math.round(32 * root.dp); height: 1; anchors.horizontalCenter: parent.horizontalCenter; color: Theme.border; opacity: 0.5 }

                // ===== 2. ANIMATIONS (toggle, self-exempt) =====
                Item {
                    width: parent.width
                    height: Math.round(56 * root.dp)

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Settings.setAnimationsEnabled(!Settings.animationsEnabled)
                    }

                    Rectangle {
                        id: anIconBox
                        anchors.left: parent.left
                        anchors.leftMargin: Math.round(16 * root.dp)
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.round(24 * root.dp)
                        height: Math.round(24 * root.dp)
                        color: "transparent"

                        Row {
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: Math.round(2 * root.dp)
                            Rectangle { width: Math.round(3 * root.dp); height: Math.round(8 * root.dp); color: Theme.onSurfaceVariant; radius: 1 }
                            Rectangle { width: Math.round(3 * root.dp); height: Math.round(14 * root.dp); color: Theme.onSurfaceVariant; radius: 1 }
                            Rectangle { width: Math.round(3 * root.dp); height: Math.round(6 * root.dp); color: Theme.onSurfaceVariant; radius: 1 }
                            Rectangle { width: Math.round(3 * root.dp); height: Math.round(11 * root.dp); color: Theme.onSurfaceVariant; radius: 1 }
                        }
                    }

                    Column {
                        anchors.left: anIconBox.right
                        anchors.leftMargin: Math.round(16 * root.dp)
                        anchors.right: anToggleArea.left
                        anchors.rightMargin: Math.round(12 * root.dp)
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Math.round(2 * root.dp)

                        Text { text: "Animations"; font.family: "Noto Sans"; font.pixelSize: Math.round(14 * root.sp); color: Theme.onBackground; width: parent.width; elide: Text.ElideRight }
                        Text { text: "Disable for better performance"; font.family: "Noto Sans"; font.pixelSize: Math.round(11 * root.sp); color: Theme.onSurfaceVariant; width: parent.width; elide: Text.ElideRight }
                    }

                    Item {
                        id: anToggleArea
                        anchors.right: parent.right
                        anchors.rightMargin: Math.round(16 * root.dp)
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.round(44 * root.dp)
                        height: Math.round(24 * root.dp)

                        Rectangle {
                            id: anTrack
                            anchors.centerIn: parent
                            width: Math.round(36 * root.dp); height: Math.round(20 * root.dp); radius: height / 2
                            color: Settings.animationsEnabled ? Theme.primary : Theme.onSurfaceVariant
                            opacity: Settings.animationsEnabled ? 1.0 : 0.38
                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on opacity { NumberAnimation { duration: 150 } }
                        }
                        Rectangle {
                            width: Math.round(16 * root.dp); height: Math.round(16 * root.dp); radius: width / 2; color: "#ffffff"
                            anchors.verticalCenter: parent.verticalCenter
                            x: Settings.animationsEnabled ? (anTrack.x + anTrack.width - width - Math.round(2 * root.dp)) : (anTrack.x + Math.round(2 * root.dp))
                            Behavior on x { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } }
                        }
                    }
                }

                Rectangle { width: parent.width - Math.round(32 * root.dp); height: 1; anchors.horizontalCenter: parent.horizontalCenter; color: Theme.border; opacity: 0.5 }

                // ===== 3. TEXT SIZE (segmented) =====
                Item {
                    width: parent.width
                    height: Math.round(56 * root.dp)

                    Rectangle {
                        id: tsIconBox
                        anchors.left: parent.left
                        anchors.leftMargin: Math.round(16 * root.dp)
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.round(24 * root.dp)
                        height: Math.round(24 * root.dp)
                        color: "transparent"

                        Text { anchors.centerIn: parent; text: "Aa"; font.family: "Noto Sans"; font.pixelSize: Math.round(13 * root.dp); font.weight: Font.Medium; color: Theme.onSurfaceVariant }
                    }

                    Column {
                        anchors.left: tsIconBox.right
                        anchors.leftMargin: Math.round(16 * root.dp)
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Math.round(2 * root.dp)

                        Text { text: "Text size"; font.family: "Noto Sans"; font.pixelSize: Math.round(14 * root.sp); color: Theme.onBackground }
                        Text { text: "Adjust for readability"; font.family: "Noto Sans"; font.pixelSize: Math.round(11 * root.sp); color: Theme.onSurfaceVariant }
                    }

                    Row {
                        anchors.right: parent.right
                        anchors.rightMargin: Math.round(16 * root.dp)
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: -1

                        Rectangle {
                            width: Math.round(52 * root.dp); height: Math.round(28 * root.dp)
                            radius: Math.round(14 * root.dp)
                            color: Math.abs(Settings.fontScale - 0.85) < 0.01 ? Theme.primaryContainer : "transparent"
                            border.color: Math.abs(Settings.fontScale - 0.85) < 0.01 ? Theme.primary : Theme.border; border.width: 1
                            Text { anchors.centerIn: parent; text: "Small"; font.family: "Noto Sans"; font.pixelSize: Math.round(10 * root.sp); font.weight: Font.Medium; color: Math.abs(Settings.fontScale - 0.85) < 0.01 ? Theme.primary : Theme.onBackground }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Settings.setFontScale(0.85) }
                        }
                        Rectangle {
                            width: Math.round(56 * root.dp); height: Math.round(28 * root.dp)
                            radius: 0
                            color: Math.abs(Settings.fontScale - 1.0) < 0.01 ? Theme.primaryContainer : "transparent"
                            border.color: Math.abs(Settings.fontScale - 1.0) < 0.01 ? Theme.primary : Theme.border; border.width: 1
                            Text { anchors.centerIn: parent; text: "Default"; font.family: "Noto Sans"; font.pixelSize: Math.round(10 * root.sp); font.weight: Font.Medium; color: Math.abs(Settings.fontScale - 1.0) < 0.01 ? Theme.primary : Theme.onBackground }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Settings.setFontScale(1.0) }
                        }
                        Rectangle {
                            width: Math.round(52 * root.dp); height: Math.round(28 * root.dp)
                            radius: Math.round(14 * root.dp)
                            color: Math.abs(Settings.fontScale - 1.15) < 0.01 ? Theme.primaryContainer : "transparent"
                            border.color: Math.abs(Settings.fontScale - 1.15) < 0.01 ? Theme.primary : Theme.border; border.width: 1
                            Text { anchors.centerIn: parent; text: "Large"; font.family: "Noto Sans"; font.pixelSize: Math.round(10 * root.sp); font.weight: Font.Medium; color: Math.abs(Settings.fontScale - 1.15) < 0.01 ? Theme.primary : Theme.onBackground }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Settings.setFontScale(1.15) }
                        }
                    }
                }

                Rectangle { width: parent.width - Math.round(32 * root.dp); height: 1; anchors.horizontalCenter: parent.horizontalCenter; color: Theme.border; opacity: 0.5 }

                // ===== 4. BRIGHTNESS (slider) =====
                Item {
                    width: parent.width
                    height: Math.round(56 * root.dp)

                    Rectangle {
                        id: brIconBox
                        anchors.left: parent.left
                        anchors.leftMargin: Math.round(16 * root.dp)
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.round(24 * root.dp)
                        height: Math.round(24 * root.dp)
                        color: "transparent"

                        Rectangle {
                            anchors.centerIn: parent
                            width: Math.round(18 * root.dp); height: Math.round(18 * root.dp); radius: width / 2
                            color: Theme.onSurfaceVariant
                            Rectangle { anchors.centerIn: parent; width: Math.round(8 * root.dp); height: Math.round(8 * root.dp); radius: width / 2; color: Theme.surface; Behavior on color { ColorAnimation { duration: Theme.animDuration } } }
                        }
                    }

                    Column {
                        anchors.left: brIconBox.right
                        anchors.leftMargin: Math.round(16 * root.dp)
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Math.round(2 * root.dp)

                        Text { text: "Brightness"; font.family: "Noto Sans"; font.pixelSize: Math.round(14 * root.sp); color: Theme.onBackground }
                        Text { text: Settings.brightness + "%"; font.family: "Noto Sans"; font.pixelSize: Math.round(11 * root.sp); color: Theme.onSurfaceVariant }
                    }

                    Slider {
                        id: brSlider
                        anchors.right: parent.right
                        anchors.rightMargin: Math.round(16 * root.dp)
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.round(140 * root.dp)
                        from: 10; to: 100; stepSize: 1
                        value: Settings.brightness
                        onMoved: Settings.setBrightness(Math.round(value))

                        background: Rectangle {
                            x: brSlider.leftPadding
                            y: brSlider.topPadding + brSlider.availableHeight / 2 - Math.round(2 * root.dp)
                            width: brSlider.availableWidth; height: Math.round(4 * root.dp); radius: Math.round(2 * root.dp)
                            color: Theme.onSurfaceVariant; opacity: 0.3
                            Rectangle { width: brSlider.visualPosition * parent.width; height: parent.height; radius: parent.radius; color: Theme.primary }
                        }
                        handle: Rectangle {
                            x: brSlider.leftPadding + brSlider.visualPosition * brSlider.availableWidth - Math.round(10 * root.dp)
                            y: brSlider.topPadding + brSlider.availableHeight / 2 - Math.round(10 * root.dp)
                            width: Math.round(20 * root.dp); height: Math.round(20 * root.dp); radius: width / 2; color: Theme.primary
                        }
                    }
                }

                Rectangle { width: parent.width - Math.round(32 * root.dp); height: 1; anchors.horizontalCenter: parent.horizontalCenter; color: Theme.border; opacity: 0.5 }

                // ===== 5. SERIAL PORT (segmented) =====
                Item {
                    width: parent.width
                    height: Math.round(56 * root.dp)

                    Rectangle {
                        id: spIconBox
                        anchors.left: parent.left
                        anchors.leftMargin: Math.round(16 * root.dp)
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.round(24 * root.dp)
                        height: Math.round(24 * root.dp)
                        color: "transparent"

                        Rectangle {
                            anchors.centerIn: parent
                            width: Math.round(18 * root.dp); height: Math.round(12 * root.dp); radius: Math.round(2 * root.dp)
                            color: "transparent"; border.color: Theme.onSurfaceVariant; border.width: Math.round(2 * root.dp)
                        }
                    }

                    Column {
                        anchors.left: spIconBox.right
                        anchors.leftMargin: Math.round(16 * root.dp)
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Math.round(2 * root.dp)

                        Text { text: "Serial port"; font.family: "Noto Sans"; font.pixelSize: Math.round(14 * root.sp); color: Theme.onBackground }
                        Text { text: "Override auto-detection"; font.family: "Noto Sans"; font.pixelSize: Math.round(11 * root.sp); color: Theme.onSurfaceVariant }
                    }

                    Row {
                        anchors.right: parent.right
                        anchors.rightMargin: Math.round(16 * root.dp)
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: -1

                        Rectangle {
                            width: Math.round(42 * root.dp); height: Math.round(28 * root.dp)
                            radius: Math.round(14 * root.dp)
                            color: Settings.serialPortOverride === "auto" ? Theme.primaryContainer : "transparent"
                            border.color: Settings.serialPortOverride === "auto" ? Theme.primary : Theme.border; border.width: 1
                            Text { anchors.centerIn: parent; text: "Auto"; font.family: "Noto Sans"; font.pixelSize: Math.round(9 * root.sp); font.weight: Font.Medium; color: Settings.serialPortOverride === "auto" ? Theme.primary : Theme.onBackground }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Settings.setSerialPortOverride("auto") }
                        }
                        Rectangle {
                            width: Math.round(44 * root.dp); height: Math.round(28 * root.dp)
                            radius: 0
                            color: Settings.serialPortOverride === "/dev/ttyUSB0" ? Theme.primaryContainer : "transparent"
                            border.color: Settings.serialPortOverride === "/dev/ttyUSB0" ? Theme.primary : Theme.border; border.width: 1
                            Text { anchors.centerIn: parent; text: "USB0"; font.family: "Noto Sans"; font.pixelSize: Math.round(9 * root.sp); font.weight: Font.Medium; color: Settings.serialPortOverride === "/dev/ttyUSB0" ? Theme.primary : Theme.onBackground }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Settings.setSerialPortOverride("/dev/ttyUSB0") }
                        }
                        Rectangle {
                            width: Math.round(46 * root.dp); height: Math.round(28 * root.dp)
                            radius: 0
                            color: Settings.serialPortOverride === "/dev/ttyACM0" ? Theme.primaryContainer : "transparent"
                            border.color: Settings.serialPortOverride === "/dev/ttyACM0" ? Theme.primary : Theme.border; border.width: 1
                            Text { anchors.centerIn: parent; text: "ACM0"; font.family: "Noto Sans"; font.pixelSize: Math.round(9 * root.sp); font.weight: Font.Medium; color: Settings.serialPortOverride === "/dev/ttyACM0" ? Theme.primary : Theme.onBackground }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Settings.setSerialPortOverride("/dev/ttyACM0") }
                        }
                        Rectangle {
                            width: Math.round(36 * root.dp); height: Math.round(28 * root.dp)
                            radius: Math.round(14 * root.dp)
                            color: Settings.serialPortOverride === "/dev/ttyS0" ? Theme.primaryContainer : "transparent"
                            border.color: Settings.serialPortOverride === "/dev/ttyS0" ? Theme.primary : Theme.border; border.width: 1
                            Text { anchors.centerIn: parent; text: "S0"; font.family: "Noto Sans"; font.pixelSize: Math.round(9 * root.sp); font.weight: Font.Medium; color: Settings.serialPortOverride === "/dev/ttyS0" ? Theme.primary : Theme.onBackground }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Settings.setSerialPortOverride("/dev/ttyS0") }
                        }
                    }
                }

                Rectangle { width: parent.width - Math.round(32 * root.dp); height: 1; anchors.horizontalCenter: parent.horizontalCenter; color: Theme.border; opacity: 0.5 }

                // ===== 6. DATA LOGGING (toggle) =====
                Item {
                    width: parent.width
                    height: Math.round(56 * root.dp)

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Settings.setDataLoggingEnabled(!Settings.dataLoggingEnabled)
                    }

                    Rectangle {
                        id: dlIconBox
                        anchors.left: parent.left
                        anchors.leftMargin: Math.round(16 * root.dp)
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.round(24 * root.dp)
                        height: Math.round(24 * root.dp)
                        color: "transparent"

                        Rectangle {
                            anchors.centerIn: parent
                            width: Math.round(16 * root.dp); height: Math.round(18 * root.dp); radius: Math.round(2 * root.dp)
                            color: "transparent"; border.color: Theme.onSurfaceVariant; border.width: Math.round(2 * root.dp)
                        }
                    }

                    Column {
                        anchors.left: dlIconBox.right
                        anchors.leftMargin: Math.round(16 * root.dp)
                        anchors.right: dlToggleArea.left
                        anchors.rightMargin: Math.round(12 * root.dp)
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Math.round(2 * root.dp)

                        Text { text: "Log raw serial data"; font.family: "Noto Sans"; font.pixelSize: Math.round(14 * root.sp); color: Theme.onBackground; width: parent.width; elide: Text.ElideRight }
                        Text { text: "Save incoming data to file"; font.family: "Noto Sans"; font.pixelSize: Math.round(11 * root.sp); color: Theme.onSurfaceVariant; width: parent.width; elide: Text.ElideRight }
                    }

                    Item {
                        id: dlToggleArea
                        anchors.right: parent.right
                        anchors.rightMargin: Math.round(16 * root.dp)
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.round(44 * root.dp)
                        height: Math.round(24 * root.dp)

                        Rectangle {
                            id: dlTrack
                            anchors.centerIn: parent
                            width: Math.round(36 * root.dp); height: Math.round(20 * root.dp); radius: height / 2
                            color: Settings.dataLoggingEnabled ? Theme.primary : Theme.onSurfaceVariant
                            opacity: Settings.dataLoggingEnabled ? 1.0 : 0.38
                            Behavior on color { ColorAnimation { duration: Settings.animationsEnabled ? 150 : 0 } }
                            Behavior on opacity { NumberAnimation { duration: Settings.animationsEnabled ? 150 : 0 } }
                        }
                        Rectangle {
                            width: Math.round(16 * root.dp); height: Math.round(16 * root.dp); radius: width / 2; color: "#ffffff"
                            anchors.verticalCenter: parent.verticalCenter
                            x: Settings.dataLoggingEnabled ? (dlTrack.x + dlTrack.width - width - Math.round(2 * root.dp)) : (dlTrack.x + Math.round(2 * root.dp))
                            Behavior on x { NumberAnimation { duration: Settings.animationsEnabled ? 150 : 0; easing.type: Easing.OutQuad } }
                        }
                    }
                }

                Rectangle { width: parent.width - Math.round(32 * root.dp); height: 1; anchors.horizontalCenter: parent.horizontalCenter; color: Theme.border; opacity: 0.5 }

                // ===== 7. CHART HISTORY (segmented) =====
                Item {
                    width: parent.width
                    height: Math.round(56 * root.dp)

                    Rectangle {
                        id: chIconBox
                        anchors.left: parent.left
                        anchors.leftMargin: Math.round(16 * root.dp)
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.round(24 * root.dp)
                        height: Math.round(24 * root.dp)
                        color: "transparent"

                        Row {
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: Math.round(2 * root.dp)
                            Rectangle { width: Math.round(4 * root.dp); height: Math.round(6 * root.dp); color: Theme.onSurfaceVariant }
                            Rectangle { width: Math.round(4 * root.dp); height: Math.round(12 * root.dp); color: Theme.onSurfaceVariant }
                            Rectangle { width: Math.round(4 * root.dp); height: Math.round(18 * root.dp); color: Theme.onSurfaceVariant }
                        }
                    }

                    Column {
                        anchors.left: chIconBox.right
                        anchors.leftMargin: Math.round(16 * root.dp)
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Math.round(2 * root.dp)

                        Text { text: "Chart history"; font.family: "Noto Sans"; font.pixelSize: Math.round(14 * root.sp); color: Theme.onBackground }
                        Text { text: "Rolling chart duration"; font.family: "Noto Sans"; font.pixelSize: Math.round(11 * root.sp); color: Theme.onSurfaceVariant }
                    }

                    Row {
                        anchors.right: parent.right
                        anchors.rightMargin: Math.round(16 * root.dp)
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: -1

                        Rectangle {
                            width: Math.round(48 * root.dp); height: Math.round(28 * root.dp)
                            radius: Math.round(14 * root.dp)
                            color: Settings.chartHistorySeconds === 60 ? Theme.primaryContainer : "transparent"
                            border.color: Settings.chartHistorySeconds === 60 ? Theme.primary : Theme.border; border.width: 1
                            Text { anchors.centerIn: parent; text: "1 min"; font.family: "Noto Sans"; font.pixelSize: Math.round(10 * root.sp); font.weight: Font.Medium; color: Settings.chartHistorySeconds === 60 ? Theme.primary : Theme.onBackground }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Settings.setChartHistorySeconds(60) }
                        }
                        Rectangle {
                            width: Math.round(48 * root.dp); height: Math.round(28 * root.dp)
                            radius: 0
                            color: Settings.chartHistorySeconds === 180 ? Theme.primaryContainer : "transparent"
                            border.color: Settings.chartHistorySeconds === 180 ? Theme.primary : Theme.border; border.width: 1
                            Text { anchors.centerIn: parent; text: "3 min"; font.family: "Noto Sans"; font.pixelSize: Math.round(10 * root.sp); font.weight: Font.Medium; color: Settings.chartHistorySeconds === 180 ? Theme.primary : Theme.onBackground }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Settings.setChartHistorySeconds(180) }
                        }
                        Rectangle {
                            width: Math.round(48 * root.dp); height: Math.round(28 * root.dp)
                            radius: Math.round(14 * root.dp)
                            color: Settings.chartHistorySeconds === 300 ? Theme.primaryContainer : "transparent"
                            border.color: Settings.chartHistorySeconds === 300 ? Theme.primary : Theme.border; border.width: 1
                            Text { anchors.centerIn: parent; text: "5 min"; font.family: "Noto Sans"; font.pixelSize: Math.round(10 * root.sp); font.weight: Font.Medium; color: Settings.chartHistorySeconds === 300 ? Theme.primary : Theme.onBackground }
                            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: Settings.setChartHistorySeconds(300) }
                        }
                    }
                }

                Rectangle { width: parent.width - Math.round(32 * root.dp); height: 1; anchors.horizontalCenter: parent.horizontalCenter; color: Theme.border; opacity: 0.5 }

                // ===== 8. ABOUT (expandable) =====
                Item {
                    width: parent.width
                    height: Math.round(56 * root.dp) + (root.aboutExpanded ? aboutInfoCol.height + Math.round(8 * root.dp) : 0)
                    clip: true

                    Behavior on height {
                        NumberAnimation { duration: Settings.animationsEnabled ? 200 : 0; easing.type: Easing.OutQuad }
                    }

                    MouseArea {
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: Math.round(56 * root.dp)
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.aboutExpanded = !root.aboutExpanded
                    }

                    Rectangle {
                        id: abIconBox
                        anchors.left: parent.left
                        anchors.leftMargin: Math.round(16 * root.dp)
                        y: Math.round(16 * root.dp)
                        width: Math.round(24 * root.dp)
                        height: Math.round(24 * root.dp)
                        color: "transparent"

                        Rectangle {
                            anchors.centerIn: parent
                            width: Math.round(18 * root.dp); height: Math.round(18 * root.dp); radius: width / 2
                            color: "transparent"; border.color: Theme.onSurfaceVariant; border.width: Math.round(2 * root.dp)
                            Text { anchors.centerIn: parent; text: "i"; font.family: "Noto Sans"; font.pixelSize: Math.round(12 * root.dp); font.weight: Font.Bold; color: Theme.onSurfaceVariant }
                        }
                    }

                    Text {
                        anchors.left: abIconBox.right
                        anchors.leftMargin: Math.round(16 * root.dp)
                        y: Math.round(18 * root.dp)
                        text: "About"
                        font.family: "Noto Sans"
                        font.pixelSize: Math.round(14 * root.sp)
                        color: Theme.onBackground
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: Math.round(16 * root.dp)
                        y: Math.round(18 * root.dp)
                        text: root.aboutExpanded ? "\u25B2" : "\u25BC"
                        font.family: "Noto Sans"
                        font.pixelSize: Math.round(12 * root.dp)
                        color: Theme.onSurfaceVariant
                    }

                    Column {
                        id: aboutInfoCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: Math.round(56 * root.dp)
                        anchors.rightMargin: Math.round(16 * root.dp)
                        y: Math.round(56 * root.dp)
                        spacing: Math.round(6 * root.dp)
                        visible: root.aboutExpanded

                        Text { text: "VARUNA Debugger v" + Settings.appVersion; font.family: "Noto Sans"; font.pixelSize: Math.round(12 * root.sp); font.weight: Font.Medium; color: Theme.onBackground }
                        Text { text: "Python: " + Settings.pythonVersion; font.family: "Noto Sans Mono"; font.pixelSize: Math.round(11 * root.sp); color: Theme.onSurfaceVariant }
                        Text { text: "PySide6: " + Settings.pyside6Version; font.family: "Noto Sans Mono"; font.pixelSize: Math.round(11 * root.sp); color: Theme.onSurfaceVariant }
                        Text { text: "Qt: " + Settings.qtVersion; font.family: "Noto Sans Mono"; font.pixelSize: Math.round(11 * root.sp); color: Theme.onSurfaceVariant }
                        Text { text: "Platform: " + Settings.platformName; font.family: "Noto Sans Mono"; font.pixelSize: Math.round(11 * root.sp); color: Theme.onSurfaceVariant }
                        Text { text: "Display: " + Settings.displayResolution; font.family: "Noto Sans Mono"; font.pixelSize: Math.round(11 * root.sp); color: Theme.onSurfaceVariant }
                    }
                }
            }
        }
    }

    // ==================== SWIPE DETECTOR ====================
    MouseArea {
        id: swipeDetector
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.triggerZone
        enabled: !root.isOpen
        z: 999

        property real startY: 0
        property bool tracking: false

        onPressed: function(mouse) {
            startY = mouse.y;
            tracking = true;
            panelAnim.stop();
            scrimAnim.stop();
        }

        onPositionChanged: function(mouse) {
            if (!tracking) return;
            var mapped = mapToItem(root, mouse.x, mouse.y);
            var startMapped = mapToItem(root, 0, startY);
            var delta = mapped.y - startMapped.y;
            if (delta > 0) {
                var progress = Math.min(delta / root.panelHeight, 1.0);
                panelBody.y = -root.panelHeight + (root.panelHeight * progress);
                scrim.opacity = Theme.scrimOpacity * progress;
            }
        }

        onReleased: function(mouse) {
            if (!tracking) return;
            tracking = false;
            var mapped = mapToItem(root, mouse.x, mouse.y);
            var startMapped = mapToItem(root, 0, startY);
            var delta = mapped.y - startMapped.y;
            var progress = delta / root.panelHeight;
            if (progress > 0.3) {
                root.open();
            } else {
                root.close();
            }
        }
    }
}

// -----------------------------------------------------------------
// File : qml/components/SettingsPanel.qml
// Phase : Phase 3 Step 3
// ----------------------------END----------------------------------
