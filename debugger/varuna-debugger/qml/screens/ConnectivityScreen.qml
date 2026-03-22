// -----------------------------------------------------------------
// File : qml/screens/ConnectivityScreen.qml
// Phase : Phase 9 Step 3
// -----------------------------------------------------------------

import QtQuick
import "../components"

Item {
    id: connectivityScreen

    readonly property real margin: Math.round(12 * rootWindow.dp)
    readonly property real gap: Math.round(8 * rootWindow.dp)

    property string selectedApn: "airtelgprs.com"
    property bool gprsTestRun: false
    property bool pingInProgress: false
    property bool gprsInProgress: false
    property bool reinitInProgress: false
    property string txDisplay: "\u2014"
    property string rxDisplay: "\u2014"

    ListModel {
        id: txrxMessages
    }

    function addMessage(prefix, text) {
        txrxMessages.append({ msgPrefix: prefix, msgText: text });
        while (txrxMessages.count > 5) {
            txrxMessages.remove(0);
        }
        msgScrollTimer.restart();
    }

    function msgColor(prefix) {
        if (prefix === "TX") return Theme.primary;
        if (prefix === "RX") return Theme.secondary;
        if (prefix === "ERROR") return Theme.error;
        if (prefix === "WARN") return Theme.warning;
        return Theme.onSurfaceVariant;
    }

    Timer {
        id: msgScrollTimer
        interval: 50
        onTriggered: {
            if (msgFlick.contentHeight > msgFlick.height) {
                msgFlick.contentY = msgFlick.contentHeight - msgFlick.height;
            }
        }
    }

    Component.onCompleted: {
        Device.markScreenCompleted(3);
        toastDelayTimer.start();
    }

    Timer {
        id: toastDelayTimer
        interval: 400
        onTriggered: {
            rootWindow.showToast("Connectivity diagnostics ready", Theme.primary);
        }
    }

    // ═══════════════ COMMANDER SIGNALS ═══════════════

    Connections {
        target: Commander

        function onCommandSent(cmd) {
            if (cmd === "PING") {
                connectivityScreen.txDisplay = "PING\\n \u2192 5 bytes";
                connectivityScreen.addMessage("TX", "PING (5 bytes)");
            } else if (cmd === "TESTGPRS") {
                connectivityScreen.txDisplay = "TESTGPRS\\n \u2192 9 bytes";
                connectivityScreen.addMessage("TX", "TESTGPRS (9 bytes)");
            } else if (cmd.indexOf("SETAPN") === 0) {
                var apnBytes = cmd.length + 1;
                connectivityScreen.txDisplay = cmd + "\\n \u2192 " + apnBytes + " bytes";
                connectivityScreen.addMessage("TX", cmd + " (" + apnBytes + " bytes)");
            } else if (cmd === "REINITSIM") {
                connectivityScreen.txDisplay = "REINITSIM\\n \u2192 10 bytes";
                connectivityScreen.addMessage("TX", "REINITSIM (10 bytes)");
            }
        }

        function onCommandConfirmed(cmd) {
            if (cmd === "PING") {
                connectivityScreen.pingInProgress = false;
                connectivityScreen.rxDisplay = "PONG \u2190 4 bytes";
                connectivityScreen.addMessage("RX", "PONG (confirmed)");
                txrxBorderFlash.flashColor = Theme.secondary;
                txrxBorderFlash.start();
            } else if (cmd === "TESTGPRS") {
                connectivityScreen.gprsInProgress = false;
                connectivityScreen.addMessage("RX", "GPRS result received");
                Device.simulateGprsResult();
                connectivityScreen.gprsTestRun = true;
                if (Device.gprsTestPassed) {
                    rootWindow.showToast("GPRS connection successful", Theme.secondary);
                } else {
                    rootWindow.showToast("GPRS connection failed", Theme.error);
                }
            } else if (cmd.indexOf("SETAPN") === 0) {
                connectivityScreen.addMessage("RX", "APN updated (confirmed)");
                rootWindow.showToast("APN set to " + connectivityScreen.selectedApn, Theme.secondary);
            } else if (cmd === "REINITSIM") {
                connectivityScreen.reinitInProgress = false;
                connectivityScreen.addMessage("RX", "SIM reinitialised (confirmed)");
                rootWindow.showToast("SIM reinitialised successfully", Theme.secondary);
            }
        }

        function onCommandFailed(cmd, reason) {
            if (cmd === "PING") {
                connectivityScreen.pingInProgress = false;
                connectivityScreen.rxDisplay = "No response";
                connectivityScreen.addMessage("ERROR", "PING timed out: " + reason);
                txrxBorderFlash.flashColor = Theme.error;
                txrxBorderFlash.start();
                rootWindow.showToast("PING failed \u2014 " + reason, Theme.error);
            } else if (cmd === "TESTGPRS") {
                connectivityScreen.gprsInProgress = false;
                connectivityScreen.addMessage("ERROR", "GPRS failed: " + reason);
                Device.setGprsTestFailed();
                connectivityScreen.gprsTestRun = true;
                rootWindow.showToast("GPRS connection failed", Theme.error);
            } else if (cmd.indexOf("SETAPN") === 0) {
                connectivityScreen.addMessage("ERROR", "SETAPN failed: " + reason);
                rootWindow.showToast("APN update failed \u2014 " + reason, Theme.error);
            } else if (cmd === "REINITSIM") {
                connectivityScreen.reinitInProgress = false;
                connectivityScreen.addMessage("ERROR", "REINITSIM failed: " + reason);
                rootWindow.showToast("SIM reinit failed \u2014 " + reason, Theme.error);
            }
        }
    }

    // ═══════════════ BORDER FLASH ANIMATION ═══════════════

    SequentialAnimation {
        id: txrxBorderFlash
        property color flashColor: Theme.secondary

        ColorAnimation {
            target: txrxCard
            property: "border.color"
            to: txrxBorderFlash.flashColor
            duration: 1
        }
        PauseAnimation {
            duration: Settings.animationsEnabled ? 400 : 0
        }
        ColorAnimation {
            target: txrxCard
            property: "border.color"
            to: Theme.border
            duration: Settings.animationsEnabled ? 300 : 0
            easing.type: Easing.OutQuad
        }
    }

    // ═══════════════ LAYOUT ═══════════════

    Flickable {
        id: mainFlick
        anchors.fill: parent
        anchors.margins: connectivityScreen.margin
        contentHeight: mainCol.height
        contentWidth: width
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Column {
            id: mainCol
            width: mainFlick.width
            spacing: connectivityScreen.gap

            // ═══════════════ TX/RX CARD ═══════════════

            Rectangle {
                id: txrxCard
                width: parent.width
                height: txrxCol.height + Math.round(24 * rootWindow.dp)
                radius: Math.round(12 * rootWindow.dp)
                color: Theme.surface
                border.width: Math.max(1, Math.round(1.5 * rootWindow.dp))
                border.color: Theme.border

                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                Column {
                    id: txrxCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Math.round(12 * rootWindow.dp)
                    spacing: Math.round(8 * rootWindow.dp)

                    Text {
                        text: "Serial TX/RX"
                        font.family: "Noto Sans"
                        font.pixelSize: Math.round(14 * rootWindow.sp)
                        font.weight: Font.Medium
                        color: Theme.onBackground
                        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: Theme.border
                        opacity: 0.5
                    }

                    Row {
                        width: parent.width
                        spacing: Math.round(12 * rootWindow.dp)

                        ActionButton {
                            id: pingBtn
                            text: "Send PING"
                            filled: true
                            enabled: Device.connected && !connectivityScreen.pingInProgress
                            buttonColor: Theme.primary
                            width: Math.round(140 * rootWindow.dp)
                            height: Math.round(40 * rootWindow.dp)
                            onClicked: {
                                connectivityScreen.pingInProgress = true;
                                connectivityScreen.rxDisplay = "\u2014";
                                Commander.sendCommand("PING");
                                console.log("ConnectivityScreen: PING sent.");
                            }
                        }

                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Math.round(4 * rootWindow.dp)

                            Row {
                                spacing: Math.round(4 * rootWindow.dp)
                                Text {
                                    text: "TX:"
                                    font.family: "Noto Sans Mono"
                                    font.pixelSize: Math.round(11 * rootWindow.sp)
                                    color: Theme.onSurfaceVariant
                                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                }
                                Text {
                                    text: connectivityScreen.txDisplay
                                    font.family: "Noto Sans Mono"
                                    font.pixelSize: Math.round(11 * rootWindow.sp)
                                    color: Theme.onBackground
                                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                }
                            }

                            Row {
                                spacing: Math.round(4 * rootWindow.dp)
                                Text {
                                    text: "RX:"
                                    font.family: "Noto Sans Mono"
                                    font.pixelSize: Math.round(11 * rootWindow.sp)
                                    color: Theme.onSurfaceVariant
                                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                }
                                Text {
                                    text: connectivityScreen.rxDisplay
                                    font.family: "Noto Sans Mono"
                                    font.pixelSize: Math.round(11 * rootWindow.sp)
                                    color: Theme.onBackground
                                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: Math.round(80 * rootWindow.dp)
                        radius: Math.round(6 * rootWindow.dp)
                        color: Theme.surfaceVariant

                        Behavior on color { ColorAnimation { duration: Theme.animDuration } }

                        Flickable {
                            id: msgFlick
                            anchors.fill: parent
                            anchors.margins: Math.round(6 * rootWindow.dp)
                            contentHeight: msgCol.height
                            flickableDirection: Flickable.VerticalFlick
                            boundsBehavior: Flickable.StopAtBounds
                            clip: true

                            Column {
                                id: msgCol
                                width: parent.width
                                spacing: Math.round(2 * rootWindow.dp)

                                Text {
                                    width: parent.width
                                    text: "No messages yet"
                                    font.family: "Noto Sans Mono"
                                    font.pixelSize: Math.round(10 * rootWindow.sp)
                                    color: Theme.onSurfaceVariant
                                    horizontalAlignment: Text.AlignHCenter
                                    topPadding: Math.round(20 * rootWindow.dp)
                                    visible: txrxMessages.count === 0

                                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                }

                                Repeater {
                                    model: txrxMessages

                                    Row {
                                        width: msgCol.width
                                        spacing: Math.round(6 * rootWindow.dp)

                                        Rectangle {
                                            width: prefixLabel.implicitWidth + Math.round(8 * rootWindow.dp)
                                            height: Math.round(16 * rootWindow.dp)
                                            radius: Math.round(3 * rootWindow.dp)
                                            color: connectivityScreen.msgColor(model.msgPrefix)
                                            opacity: 0.2
                                            anchors.verticalCenter: parent.verticalCenter

                                            Text {
                                                id: prefixLabel
                                                anchors.centerIn: parent
                                                text: model.msgPrefix
                                                font.family: "Noto Sans Mono"
                                                font.pixelSize: Math.round(9 * rootWindow.sp)
                                                font.weight: Font.Medium
                                                color: connectivityScreen.msgColor(model.msgPrefix)
                                            }
                                        }

                                        Text {
                                            text: model.msgText
                                            font.family: "Noto Sans Mono"
                                            font.pixelSize: Math.round(10 * rootWindow.sp)
                                            color: connectivityScreen.msgColor(model.msgPrefix)
                                            elide: Text.ElideRight
                                            width: parent.width - prefixLabel.implicitWidth - Math.round(22 * rootWindow.dp)
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ═══════════════ GPRS CARD ═══════════════

            Rectangle {
                id: gprsCard
                width: parent.width
                height: gprsCol.height + Math.round(24 * rootWindow.dp)
                radius: Math.round(12 * rootWindow.dp)
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                Column {
                    id: gprsCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Math.round(12 * rootWindow.dp)
                    spacing: Math.round(8 * rootWindow.dp)

                    Text {
                        text: "GPRS / Server"
                        font.family: "Noto Sans"
                        font.pixelSize: Math.round(14 * rootWindow.sp)
                        font.weight: Font.Medium
                        color: Theme.onBackground
                        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: Theme.border
                        opacity: 0.5
                    }

                    Row {
                        width: parent.width
                        spacing: Math.round(12 * rootWindow.dp)

                        ActionButton {
                            id: gprsTestBtn
                            text: "Test connection"
                            filled: true
                            enabled: Device.connected && !connectivityScreen.gprsInProgress
                            buttonColor: Theme.primary
                            width: Math.round(160 * rootWindow.dp)
                            height: Math.round(40 * rootWindow.dp)
                            onClicked: {
                                connectivityScreen.gprsInProgress = true;
                                Commander.sendCommand("TESTGPRS");
                                console.log("ConnectivityScreen: TESTGPRS sent.");
                            }
                        }

                        Flow {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Math.round(6 * rootWindow.dp)
                            width: parent.width - gprsTestBtn.width - Math.round(12 * rootWindow.dp)

                            Rectangle {
                                width: statusChipContent.implicitWidth + Math.round(16 * rootWindow.dp)
                                height: Math.round(26 * rootWindow.dp)
                                radius: Math.round(13 * rootWindow.dp)
                                color: {
                                    if (!Device.gprsTestComplete) return "transparent";
                                    return Device.gprsTestPassed ? Theme.secondary : Theme.error;
                                }
                                border.width: Device.gprsTestComplete ? 0 : 1
                                border.color: Theme.border

                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                                Text {
                                    id: statusChipContent
                                    anchors.centerIn: parent
                                    text: {
                                        if (!Device.gprsTestComplete) return "Status: \u2014";
                                        return Device.gprsTestPassed ? "PASS" : "FAIL";
                                    }
                                    font.family: "Noto Sans Mono"
                                    font.pixelSize: Math.round(10 * rootWindow.sp)
                                    font.weight: Device.gprsTestComplete ? Font.Medium : Font.Normal
                                    color: Device.gprsTestComplete ? "#ffffff" : Theme.onSurfaceVariant

                                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                }
                            }

                            Rectangle {
                                width: httpChipContent.implicitWidth + Math.round(16 * rootWindow.dp)
                                height: Math.round(26 * rootWindow.dp)
                                radius: Math.round(13 * rootWindow.dp)
                                color: "transparent"
                                border.width: 1
                                border.color: Device.gprsTestComplete ? Theme.secondary : Theme.border

                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                                Text {
                                    id: httpChipContent
                                    anchors.centerIn: parent
                                    text: Device.gprsTestComplete ? ("HTTP: " + Device.gprsHttpCode) : "HTTP: \u2014"
                                    font.family: "Noto Sans Mono"
                                    font.pixelSize: Math.round(10 * rootWindow.sp)
                                    color: Device.gprsTestComplete ? Theme.onBackground : Theme.onSurfaceVariant

                                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                }
                            }

                            Rectangle {
                                width: rttChipContent.implicitWidth + Math.round(16 * rootWindow.dp)
                                height: Math.round(26 * rootWindow.dp)
                                radius: Math.round(13 * rootWindow.dp)
                                color: "transparent"
                                border.width: 1
                                border.color: Device.gprsTestComplete ? Theme.secondary : Theme.border

                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                                Text {
                                    id: rttChipContent
                                    anchors.centerIn: parent
                                    text: Device.gprsTestComplete ? ("RTT: " + Device.gprsRttMs + "ms") : "RTT: \u2014"
                                    font.family: "Noto Sans Mono"
                                    font.pixelSize: Math.round(10 * rootWindow.sp)
                                    color: Device.gprsTestComplete ? Theme.onBackground : Theme.onSurfaceVariant

                                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                }
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        text: "Current RSSI: " + Device.simSignalRSSI + " \u2014 " + Device.rssiQuality
                        font.family: "Noto Sans"
                        font.pixelSize: Math.round(12 * rootWindow.sp)
                        color: Theme.warning
                        visible: Device.gprsTestComplete && !Device.gprsTestPassed

                        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                    }

                    ActionButton {
                        id: reinitSimBtn
                        text: "Reinitialise SIM"
                        filled: false
                        enabled: connectivityScreen.gprsTestRun && !connectivityScreen.reinitInProgress
                        buttonColor: Theme.primary
                        width: Math.round(160 * rootWindow.dp)
                        height: Math.round(36 * rootWindow.dp)
                        onClicked: {
                            connectivityScreen.reinitInProgress = true;
                            Commander.sendCommand("REINITSIM");
                            console.log("ConnectivityScreen: REINITSIM sent.");
                        }
                    }
                }
            }

            // ═══════════════ APN CARD ═══════════════

            Rectangle {
                id: apnCard
                width: parent.width
                height: apnCol.height + Math.round(24 * rootWindow.dp)
                radius: Math.round(12 * rootWindow.dp)
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                Column {
                    id: apnCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Math.round(12 * rootWindow.dp)
                    spacing: Math.round(8 * rootWindow.dp)

                    Text {
                        text: "APN Configuration"
                        font.family: "Noto Sans"
                        font.pixelSize: Math.round(14 * rootWindow.sp)
                        font.weight: Font.Medium
                        color: Theme.onBackground
                        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                    }

                    Rectangle {
                        width: parent.width
                        height: 1
                        color: Theme.border
                        opacity: 0.5
                    }

                    Text {
                        text: connectivityScreen.selectedApn
                        font.family: "Noto Sans Mono"
                        font.pixelSize: Math.round(18 * rootWindow.sp)
                        color: Theme.onBackground
                        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                    }

                    Row {
                        spacing: Math.round(8 * rootWindow.dp)

                        Repeater {
                            model: ListModel {
                                ListElement { apnName: "airtelgprs.com" }
                                ListElement { apnName: "internet" }
                                ListElement { apnName: "bsnlnet" }
                            }

                            Rectangle {
                                required property string apnName
                                required property int index

                                property bool isSelected: connectivityScreen.selectedApn === apnName

                                width: apnChipText.implicitWidth + Math.round(20 * rootWindow.dp)
                                height: Math.round(36 * rootWindow.dp)
                                radius: Math.round(8 * rootWindow.dp)
                                color: isSelected ? Theme.primaryContainer : "transparent"
                                border.width: 1
                                border.color: isSelected ? Theme.primary : Theme.border

                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                                Text {
                                    id: apnChipText
                                    anchors.centerIn: parent
                                    text: parent.apnName
                                    font.family: "Noto Sans Mono"
                                    font.pixelSize: Math.round(11 * rootWindow.sp)
                                    font.weight: Font.Medium
                                    color: parent.isSelected ? Theme.primary : Theme.onBackground
                                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (connectivityScreen.selectedApn !== parent.apnName) {
                                            connectivityScreen.selectedApn = parent.apnName;
                                            Commander.sendCommand("SETAPN=" + parent.apnName);
                                            console.log("ConnectivityScreen: APN change requested: " + parent.apnName);
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        text: "Verify this matches the SIM card in the device."
                        font.family: "Noto Sans"
                        font.pixelSize: Math.round(11 * rootWindow.sp)
                        color: Theme.onSurfaceVariant
                        wrapMode: Text.WordWrap
                        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                    }
                }
            }
        }
    }
}

// -----------------------------------------------------------------
// File : qml/screens/ConnectivityScreen.qml
// Phase : Phase 9 Step 3
// ----------------------------END----------------------------------
