// -----------------------------------------------------------------
// File: qml/screens/ThresholdScreen.qml
// Phase: Phase 13 (added "Apply to device" SETTHRESH button,
//         "Reset to defaults" RESETTHRESH button, Commander signal
//         handlers, send-in-progress state)
// -----------------------------------------------------------------

import QtQuick
import "../components"

Item {
    id: thresholdScreen

    readonly property real margin: Math.round(12 * rootWindow.dp)
    readonly property real gap:    Math.round(8  * rootWindow.dp)

    property string alertValue:   ""
    property string warningValue: ""
    property string dangerValue:  ""

    property string alertError:   ""
    property string warningError: ""
    property string dangerError:  ""
    property bool   hasValidationErrors: false

    property string currentAlertDevice:   "Reading\u2026"
    property string currentWarningDevice: "Reading\u2026"
    property string currentDangerDevice:  "Reading\u2026"

    property bool thresholdsLoaded:   false
    property bool threshRequested:    false

    // NEW — track in-flight commands so buttons disable correctly
    property bool setThreshInProgress:   false
    property bool resetThreshInProgress: false

    onAlertValueChanged:   validate()
    onWarningValueChanged: validate()
    onDangerValueChanged:  validate()

    // ─── Validation ──────────────────────────────────────────────────────

    function validate() {
        var aErr = "", wErr = "", dErr = "";

        var aStr = alertValue.trim();
        var wStr = warningValue.trim();
        var dStr = dangerValue.trim();

        var a = parseFloat(aStr);
        var w = parseFloat(wStr);
        var d = parseFloat(dStr);

        if (aStr.length > 0 && (isNaN(a) || a <= 0))
            aErr = "Must be a positive number";
        if (wStr.length > 0 && (isNaN(w) || w <= 0))
            wErr = "Must be a positive number";
        if (dStr.length > 0 && (isNaN(d) || d <= 0))
            dErr = "Must be a positive number";

        if (aStr.length > 0 && wStr.length > 0 && aErr === "" && wErr === "") {
            if (a >= w) {
                aErr = "Alert must be less than warning";
                wErr = "Warning must be greater than alert";
            }
        }

        if (wStr.length > 0 && dStr.length > 0
                && (wErr === "" || wErr === "Warning must be greater than alert")
                && dErr === "") {
            if (w >= d) {
                if (wErr === "") wErr = "Warning must be less than danger";
                dErr = "Danger must be greater than warning";
            }
        }

        alertError   = aErr;
        warningError = wErr;
        dangerError  = dErr;
        hasValidationErrors = (aErr !== "" || wErr !== "" || dErr !== "");
    }

    // ─── All three fields filled and valid ───────────────────────────────

    readonly property bool canApply:
        alertValue.trim().length > 0
        && warningValue.trim().length > 0
        && dangerValue.trim().length > 0
        && !hasValidationErrors

    // ─── Lifecycle ───────────────────────────────────────────────────────

    Component.onCompleted: {
        Device.markScreenCompleted(4);
        requestThresholds();
    }

    function requestThresholds() {
        if (!threshRequested) {
            threshRequested = true;
            console.log("ThresholdScreen: Sending GETTHRESH command.");
            Commander.sendCommand("GETTHRESH");
            threshTimeoutTimer.start();
        }
    }

    Timer {
        id: threshTimeoutTimer
        interval: 3000
        onTriggered: {
            if (!Device.thresholdDataReceived) {
                console.log("ThresholdScreen: GETTHRESH timeout — using simulated data.");
                Device.requestThresholdData();
            }
        }
    }

    // ─── Device data update ──────────────────────────────────────────────

    Connections {
        target: Device
        function onThresholdDataChanged() {
            console.log("ThresholdScreen: Threshold data updated from device.");
            thresholdScreen.currentAlertDevice   = Device.thresholdAlert.toFixed(1);
            thresholdScreen.currentWarningDevice = Device.thresholdWarning.toFixed(1);
            thresholdScreen.currentDangerDevice  = Device.thresholdDanger.toFixed(1);
            thresholdScreen.thresholdsLoaded     = true;
        }
    }

    // ─── Commander signal handlers ───────────────────────────────────────

    Connections {
        target: Commander

        function onCommandConfirmed(cmd) {
            if (cmd.indexOf("SETTHRESH") === 0) {
                thresholdScreen.setThreshInProgress = false;
                rootWindow.showToast("Thresholds applied to device", Theme.secondary);
            }
            if (cmd === "RESETTHRESH") {
                thresholdScreen.resetThreshInProgress = false;
                // After reset, re-fetch so the "current on device" column refreshes.
                // Give device 500 ms to settle then request.
                resetFetchTimer.start();
                rootWindow.showToast("Thresholds reset to device defaults", Theme.secondary);
            }
            if (cmd === "GETTHRESH") {
                threshTimeoutTimer.stop();
            }
        }

        function onCommandFailed(cmd, reason) {
            if (cmd.indexOf("SETTHRESH") === 0) {
                thresholdScreen.setThreshInProgress = false;
                rootWindow.showToast("Apply failed \u2014 " + reason, Theme.error);
            }
            if (cmd === "RESETTHRESH") {
                thresholdScreen.resetThreshInProgress = false;
                rootWindow.showToast("Reset failed \u2014 " + reason, Theme.error);
            }
            if (cmd === "GETTHRESH") {
                Device.requestThresholdData();
            }
        }
    }

    // After RESETTHRESH confirmed, re-fetch current device values
    Timer {
        id: resetFetchTimer
        interval: 500
        onTriggered: {
            thresholdScreen.threshRequested = false;
            thresholdScreen.requestThresholds();
        }
    }

    // ═══════════════ LAYOUT ═══════════════

    Flickable {
        id: mainFlick
        anchors.fill:    parent
        anchors.margins: thresholdScreen.margin
        contentHeight:   mainCol.height
        contentWidth:    width
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior:  Flickable.StopAtBounds
        clip: true

        Column {
            id: mainCol
            width: mainFlick.width
            spacing: thresholdScreen.gap

            // ═══════════════ HEADER ROW ═══════════════

            Item {
                width:  parent.width
                height: Math.round(28 * rootWindow.dp)

                Text {
                    anchors.left:           parent.left
                    anchors.leftMargin:     Math.round(4 * rootWindow.dp)
                    anchors.verticalCenter: parent.verticalCenter
                    text:       "Threshold configuration"
                    font.family:    "Noto Sans"
                    font.pixelSize: Math.round(15 * rootWindow.sp)
                    font.weight:    Font.Medium
                    color: Theme.onBackground
                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                }

                Row {
                    anchors.right:          parent.right
                    anchors.rightMargin:    Math.round(4 * rootWindow.dp)
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: parent.width * 0.15

                    Text {
                        text:       "Current on device"
                        font.family:    "Noto Sans"
                        font.pixelSize: Math.round(12 * rootWindow.sp)
                        color: Theme.onSurfaceVariant
                        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                    }

                    Text {
                        text:       "Set new value"
                        font.family:    "Noto Sans"
                        font.pixelSize: Math.round(12 * rootWindow.sp)
                        color: Theme.onSurfaceVariant
                        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                    }
                }
            }

            // ═══════════════ ALERT ROW ═══════════════

            Rectangle {
                width: parent.width
                height: alertCardCol.height + Math.round(24 * rootWindow.dp)
                radius: Math.round(12 * rootWindow.dp)
                color:  Theme.surface
                border.width: 1
                border.color: Theme.border
                Behavior on color       { ColorAnimation { duration: Theme.animDuration } }
                Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                Column {
                    id: alertCardCol
                    anchors.left:    parent.left
                    anchors.right:   parent.right
                    anchors.top:     parent.top
                    anchors.margins: Math.round(12 * rootWindow.dp)
                    spacing: Math.round(4 * rootWindow.dp)

                    Row {
                        width: parent.width
                        spacing: 0

                        Column {
                            width: parent.width * 0.35
                            spacing: Math.round(4 * rootWindow.dp)

                            Text {
                                text: "Alert threshold"
                                font.family:    "Noto Sans"
                                font.pixelSize: Math.round(14 * rootWindow.sp)
                                font.weight:    Font.Medium
                                color: Theme.onBackground
                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                            }

                            Text {
                                text: "cm"
                                font.family:    "Noto Sans"
                                font.pixelSize: Math.round(11 * rootWindow.sp)
                                color: Theme.onSurfaceVariant
                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                            }
                        }

                        Rectangle {
                            width: 1; height: Math.round(48 * rootWindow.dp)
                            color: Theme.border
                            anchors.verticalCenter: parent.verticalCenter
                            Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                        }

                        Item {
                            width:  parent.width * 0.3
                            height: Math.round(48 * rootWindow.dp)

                            Rectangle {
                                anchors.fill:    parent
                                anchors.margins: Math.round(6 * rootWindow.dp)
                                radius: Math.round(6 * rootWindow.dp)
                                color:  Theme.surfaceVariant
                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }

                                Text {
                                    anchors.centerIn: parent
                                    text: thresholdScreen.currentAlertDevice
                                    font.family:    "Noto Sans Mono"
                                    font.pixelSize: Math.round(20 * rootWindow.sp)
                                    color: thresholdScreen.thresholdsLoaded
                                               ? Theme.onBackground
                                               : Theme.onSurfaceVariant
                                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                }
                            }
                        }

                        Item {
                            width:  parent.width * 0.35 - 1
                            height: Math.round(48 * rootWindow.dp)

                            Rectangle {
                                anchors.fill:    parent
                                anchors.margins: Math.round(6 * rootWindow.dp)
                                radius: Math.round(8 * rootWindow.dp)
                                color:  Theme.surfaceVariant
                                border.width: 1
                                border.color: {
                                    if (thresholdScreen.alertError !== ""
                                            && thresholdScreen.alertValue.length > 0)
                                        return Theme.error;
                                    return Theme.border;
                                }
                                Behavior on color       { ColorAnimation { duration: Theme.animDuration } }
                                Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                                Text {
                                    anchors.centerIn: parent
                                    text: thresholdScreen.alertValue.length > 0
                                              ? thresholdScreen.alertValue
                                              : "Tap to set"
                                    font.family:    "Noto Sans Mono"
                                    font.pixelSize: Math.round(16 * rootWindow.sp)
                                    color: thresholdScreen.alertValue.length > 0
                                               ? Theme.onBackground
                                               : Theme.onSurfaceVariant
                                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: numKeypad.open("alert", thresholdScreen.alertValue)
                                }
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        text:    thresholdScreen.alertError
                        font.family:    "Noto Sans"
                        font.pixelSize: Math.round(10 * rootWindow.sp)
                        color:   Theme.error
                        visible: thresholdScreen.alertError !== ""
                        wrapMode: Text.WordWrap
                        leftPadding: parent.width * 0.35 + Math.round(6 * rootWindow.dp)
                        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                    }
                }
            }

            // ═══════════════ WARNING ROW ═══════════════

            Rectangle {
                width: parent.width
                height: warningCardCol.height + Math.round(24 * rootWindow.dp)
                radius: Math.round(12 * rootWindow.dp)
                color:  Theme.surface
                border.width: 1
                border.color: Theme.border
                Behavior on color       { ColorAnimation { duration: Theme.animDuration } }
                Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                Column {
                    id: warningCardCol
                    anchors.left:    parent.left
                    anchors.right:   parent.right
                    anchors.top:     parent.top
                    anchors.margins: Math.round(12 * rootWindow.dp)
                    spacing: Math.round(4 * rootWindow.dp)

                    Row {
                        width: parent.width
                        spacing: 0

                        Column {
                            width: parent.width * 0.35
                            spacing: Math.round(4 * rootWindow.dp)

                            Text {
                                text: "Warning threshold"
                                font.family:    "Noto Sans"
                                font.pixelSize: Math.round(14 * rootWindow.sp)
                                font.weight:    Font.Medium
                                color: Theme.warning
                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                            }

                            Text {
                                text: "cm"
                                font.family:    "Noto Sans"
                                font.pixelSize: Math.round(11 * rootWindow.sp)
                                color: Theme.onSurfaceVariant
                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                            }
                        }

                        Rectangle {
                            width: 1; height: Math.round(48 * rootWindow.dp)
                            color: Theme.border
                            anchors.verticalCenter: parent.verticalCenter
                            Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                        }

                        Item {
                            width:  parent.width * 0.3
                            height: Math.round(48 * rootWindow.dp)

                            Rectangle {
                                anchors.fill:    parent
                                anchors.margins: Math.round(6 * rootWindow.dp)
                                radius: Math.round(6 * rootWindow.dp)
                                color:  Theme.surfaceVariant
                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }

                                Text {
                                    anchors.centerIn: parent
                                    text: thresholdScreen.currentWarningDevice
                                    font.family:    "Noto Sans Mono"
                                    font.pixelSize: Math.round(20 * rootWindow.sp)
                                    color: thresholdScreen.thresholdsLoaded
                                               ? Theme.onBackground
                                               : Theme.onSurfaceVariant
                                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                }
                            }
                        }

                        Item {
                            width:  parent.width * 0.35 - 1
                            height: Math.round(48 * rootWindow.dp)

                            Rectangle {
                                anchors.fill:    parent
                                anchors.margins: Math.round(6 * rootWindow.dp)
                                radius: Math.round(8 * rootWindow.dp)
                                color:  Theme.surfaceVariant
                                border.width: 1
                                border.color: {
                                    if (thresholdScreen.warningError !== ""
                                            && thresholdScreen.warningValue.length > 0)
                                        return Theme.error;
                                    return Theme.border;
                                }
                                Behavior on color       { ColorAnimation { duration: Theme.animDuration } }
                                Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                                Text {
                                    anchors.centerIn: parent
                                    text: thresholdScreen.warningValue.length > 0
                                              ? thresholdScreen.warningValue
                                              : "Tap to set"
                                    font.family:    "Noto Sans Mono"
                                    font.pixelSize: Math.round(16 * rootWindow.sp)
                                    color: thresholdScreen.warningValue.length > 0
                                               ? Theme.onBackground
                                               : Theme.onSurfaceVariant
                                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: numKeypad.open("warning", thresholdScreen.warningValue)
                                }
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        text:    thresholdScreen.warningError
                        font.family:    "Noto Sans"
                        font.pixelSize: Math.round(10 * rootWindow.sp)
                        color:   Theme.error
                        visible: thresholdScreen.warningError !== ""
                        wrapMode: Text.WordWrap
                        leftPadding: parent.width * 0.35 + Math.round(6 * rootWindow.dp)
                        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                    }
                }
            }

            // ═══════════════ DANGER ROW ═══════════════

            Rectangle {
                width: parent.width
                height: dangerCardCol.height + Math.round(24 * rootWindow.dp)
                radius: Math.round(12 * rootWindow.dp)
                color:  Theme.surface
                border.width: 1
                border.color: Theme.border
                Behavior on color       { ColorAnimation { duration: Theme.animDuration } }
                Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                Column {
                    id: dangerCardCol
                    anchors.left:    parent.left
                    anchors.right:   parent.right
                    anchors.top:     parent.top
                    anchors.margins: Math.round(12 * rootWindow.dp)
                    spacing: Math.round(4 * rootWindow.dp)

                    Row {
                        width: parent.width
                        spacing: 0

                        Column {
                            width: parent.width * 0.35
                            spacing: Math.round(4 * rootWindow.dp)

                            Text {
                                text: "Danger threshold"
                                font.family:    "Noto Sans"
                                font.pixelSize: Math.round(14 * rootWindow.sp)
                                font.weight:    Font.Medium
                                color: Theme.error
                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                            }

                            Text {
                                text: "cm"
                                font.family:    "Noto Sans"
                                font.pixelSize: Math.round(11 * rootWindow.sp)
                                color: Theme.onSurfaceVariant
                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                            }
                        }

                        Rectangle {
                            width: 1; height: Math.round(48 * rootWindow.dp)
                            color: Theme.border
                            anchors.verticalCenter: parent.verticalCenter
                            Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                        }

                        Item {
                            width:  parent.width * 0.3
                            height: Math.round(48 * rootWindow.dp)

                            Rectangle {
                                anchors.fill:    parent
                                anchors.margins: Math.round(6 * rootWindow.dp)
                                radius: Math.round(6 * rootWindow.dp)
                                color:  Theme.surfaceVariant
                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }

                                Text {
                                    anchors.centerIn: parent
                                    text: thresholdScreen.currentDangerDevice
                                    font.family:    "Noto Sans Mono"
                                    font.pixelSize: Math.round(20 * rootWindow.sp)
                                    color: thresholdScreen.thresholdsLoaded
                                               ? Theme.onBackground
                                               : Theme.onSurfaceVariant
                                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                }
                            }
                        }

                        Item {
                            width:  parent.width * 0.35 - 1
                            height: Math.round(48 * rootWindow.dp)

                            Rectangle {
                                anchors.fill:    parent
                                anchors.margins: Math.round(6 * rootWindow.dp)
                                radius: Math.round(8 * rootWindow.dp)
                                color:  Theme.surfaceVariant
                                border.width: 1
                                border.color: {
                                    if (thresholdScreen.dangerError !== ""
                                            && thresholdScreen.dangerValue.length > 0)
                                        return Theme.error;
                                    return Theme.border;
                                }
                                Behavior on color       { ColorAnimation { duration: Theme.animDuration } }
                                Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                                Text {
                                    anchors.centerIn: parent
                                    text: thresholdScreen.dangerValue.length > 0
                                              ? thresholdScreen.dangerValue
                                              : "Tap to set"
                                    font.family:    "Noto Sans Mono"
                                    font.pixelSize: Math.round(16 * rootWindow.sp)
                                    color: thresholdScreen.dangerValue.length > 0
                                               ? Theme.onBackground
                                               : Theme.onSurfaceVariant
                                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: numKeypad.open("danger", thresholdScreen.dangerValue)
                                }
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        text:    thresholdScreen.dangerError
                        font.family:    "Noto Sans"
                        font.pixelSize: Math.round(10 * rootWindow.sp)
                        color:   Theme.error
                        visible: thresholdScreen.dangerError !== ""
                        wrapMode: Text.WordWrap
                        leftPadding: parent.width * 0.35 + Math.round(6 * rootWindow.dp)
                        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                    }
                }
            }

            // ═══════════════ ACTION ROW ═══════════════
            // Apply to device (SETTHRESH) + Reset to defaults (RESETTHRESH)

            Rectangle {
                width: parent.width
                height: actionRow.height + Math.round(24 * rootWindow.dp)
                radius: Math.round(12 * rootWindow.dp)
                color:  Theme.surface
                border.width: 1
                border.color: Theme.border
                Behavior on color       { ColorAnimation { duration: Theme.animDuration } }
                Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                Column {
                    id: actionRow
                    anchors.left:    parent.left
                    anchors.right:   parent.right
                    anchors.top:     parent.top
                    anchors.margins: Math.round(12 * rootWindow.dp)
                    spacing: Math.round(10 * rootWindow.dp)

                    // Section title
                    Text {
                        text: "Send to device"
                        font.family:    "Noto Sans"
                        font.pixelSize: Math.round(13 * rootWindow.sp)
                        font.weight:    Font.Medium
                        color: Theme.onSurfaceVariant
                        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                    }

                    Rectangle {
                        width: parent.width; height: 1
                        color: Theme.border; opacity: 0.5
                    }

                    // Hint: what will be sent
                    Text {
                        width: parent.width
                        visible: thresholdScreen.canApply
                        text: "Will send: SETTHRESH="
                              + thresholdScreen.alertValue.trim()
                              + "," + thresholdScreen.warningValue.trim()
                              + "," + thresholdScreen.dangerValue.trim()
                        font.family:    "Noto Sans Mono"
                        font.pixelSize: Math.round(11 * rootWindow.sp)
                        color: Theme.onSurfaceVariant
                        wrapMode: Text.NoWrap
                        elide: Text.ElideRight
                        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                    }

                    // Button row
                    Row {
                        width: parent.width
                        spacing: Math.round(8 * rootWindow.dp)

                        // ── APPLY TO DEVICE ──────────────────────────────
                        ActionButton {
                            id: applyBtn
                            text: thresholdScreen.setThreshInProgress
                                      ? "Sending\u2026"
                                      : "Apply to device"
                            filled:      true
                            buttonColor: Theme.primary
                            width:  Math.round(180 * rootWindow.dp)
                            height: Math.round(44 * rootWindow.dp)
                            enabled: thresholdScreen.canApply
                                     && !thresholdScreen.setThreshInProgress
                                     && !thresholdScreen.resetThreshInProgress
                                     && Device.connected

                            onClicked: {
                                var a = thresholdScreen.alertValue.trim();
                                var w = thresholdScreen.warningValue.trim();
                                var d = thresholdScreen.dangerValue.trim();
                                var cmd = "SETTHRESH=" + a + "," + w + "," + d;
                                console.log("ThresholdScreen: " + cmd);

                                thresholdScreen.setThreshInProgress = true;

                                // Update DeviceModel immediately so the rest of
                                // the app sees the new values even if the device
                                // is slow to confirm.
                                Device.setThresholdData(
                                    parseFloat(a), parseFloat(w), parseFloat(d));

                                Commander.sendCommand(cmd);
                            }
                        }

                        // ── RESET TO DEVICE DEFAULTS ─────────────────────
                        ActionButton {
                            id: resetBtn
                            text: thresholdScreen.resetThreshInProgress
                                      ? "Resetting\u2026"
                                      : "Reset to defaults"
                            filled:      false
                            buttonColor: Theme.warning
                            width:  Math.round(160 * rootWindow.dp)
                            height: Math.round(44 * rootWindow.dp)
                            enabled: !thresholdScreen.setThreshInProgress
                                     && !thresholdScreen.resetThreshInProgress
                                     && Device.connected

                            onClicked: {
                                console.log("ThresholdScreen: Sending RESETTHRESH command.");
                                thresholdScreen.resetThreshInProgress = true;
                                // Clear local model so "current on device" shows
                                // a loading state until device replies
                                Device.resetThresholds();
                                thresholdScreen.currentAlertDevice   = "Reading\u2026";
                                thresholdScreen.currentWarningDevice  = "Reading\u2026";
                                thresholdScreen.currentDangerDevice   = "Reading\u2026";
                                thresholdScreen.thresholdsLoaded      = false;
                                Commander.sendCommand("RESETTHRESH");
                            }
                        }
                    }

                    // Status hint
                    Text {
                        width: parent.width
                        visible: !Device.connected
                        text: "Connect a device to send commands."
                        font.family:    "Noto Sans"
                        font.pixelSize: Math.round(11 * rootWindow.sp)
                        color: Theme.onSurfaceVariant
                        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                    }
                }
            }
        }
    }

    // ═══════════════ NUMERIC KEYPAD ═══════════════

    NumericKeypad {
        id: numKeypad

        onConfirmed: function(value) {
            if (numKeypad.targetField === "alert") {
                thresholdScreen.alertValue = value;
            } else if (numKeypad.targetField === "warning") {
                thresholdScreen.warningValue = value;
            } else if (numKeypad.targetField === "danger") {
                thresholdScreen.dangerValue = value;
            }
            console.log("ThresholdScreen: " + numKeypad.targetField + " set to " + value);
        }

        onCancelled: {
            console.log("ThresholdScreen: Keypad cancelled for " + numKeypad.targetField);
        }
    }
}
