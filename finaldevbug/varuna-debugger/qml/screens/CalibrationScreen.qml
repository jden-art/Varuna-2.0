// -----------------------------------------------------------------
// File: qml/screens/CalibrationScreen.qml
// Phase: Phase 13 (added RECALIBRATE command button, in-progress
//         spinner overlay, Commander signal handlers, recal timeout)
// -----------------------------------------------------------------

import QtQuick
import "../components"

Item {
    id: calibrationScreen

    readonly property real margin: Math.round(12 * rootWindow.dp)
    readonly property real gap:    Math.round(8  * rootWindow.dp)
    readonly property bool wideMode: calibrationScreen.width > Math.round(900 * rootWindow.dp)

    property bool configRequested:    false
    property bool recalRequested:     false
    property bool recalInProgress:    false

    // ═══════════════ STABILITY TEST STATE ═══════════════

    property bool stabilityRunning:       false
    property bool stabilityComplete:      false
    property int  stabilityElapsed:       0
    readonly property int stabilityDuration: 30
    property real stabilityDeviation:     0.0
    property real stabilityFinalDeviation: 0.0

    property var tiltSamples: []

    function startStabilityTest() {
        stabilityRunning       = true;
        stabilityComplete      = false;
        stabilityElapsed       = 0;
        stabilityDeviation     = 0.0;
        stabilityFinalDeviation = 0.0;
        tiltSamples            = [];
        stabilityTimer.start();
        console.log("CalibrationScreen: Stability test started.");
    }

    function stopStabilityTest() {
        stabilityTimer.stop();
        stabilityRunning = false;
        console.log("CalibrationScreen: Stability test stopped.");
    }

    function computeStdDev(samples) {
        if (samples.length < 2) return 0.0;
        var sum = 0.0;
        for (var i = 0; i < samples.length; i++) sum += samples[i];
        var mean = sum / samples.length;
        var sqSum = 0.0;
        for (var j = 0; j < samples.length; j++) {
            var diff = samples[j] - mean;
            sqSum += diff * diff;
        }
        return Math.sqrt(sqSum / (samples.length - 1));
    }

    function stabilityResultColor() {
        if (stabilityFinalDeviation < 0.5)  return Theme.secondary;
        if (stabilityFinalDeviation < 1.5)  return Theme.warning;
        return Theme.error;
    }

    function stabilityResultText() {
        if (stabilityFinalDeviation < 0.5)  return "Excellent stability";
        if (stabilityFinalDeviation < 1.0)  return "Good stability";
        if (stabilityFinalDeviation < 1.5)  return "Marginal stability";
        return "Poor stability — recalibrate";
    }

    function stabilityResultBadge() {
        if (stabilityFinalDeviation < 0.5)  return "PASS";
        if (stabilityFinalDeviation < 1.5)  return "WARN";
        return "FAIL";
    }

    Timer {
        id: stabilityTimer
        interval: 1000
        repeat: true
        onTriggered: {
            if (!calibrationScreen.stabilityRunning) return;
            var tiltMag = Math.sqrt(
                Device.correctedTiltX * Device.correctedTiltX +
                Device.correctedTiltY * Device.correctedTiltY
            );
            calibrationScreen.tiltSamples.push(tiltMag);
            calibrationScreen.stabilityDeviation =
                calibrationScreen.computeStdDev(calibrationScreen.tiltSamples);
            calibrationScreen.stabilityElapsed += 1;
            if (calibrationScreen.stabilityElapsed >= calibrationScreen.stabilityDuration) {
                calibrationScreen.stabilityFinalDeviation = calibrationScreen.stabilityDeviation;
                calibrationScreen.stabilityRunning   = false;
                calibrationScreen.stabilityComplete  = true;
                stabilityTimer.stop();
                console.log("CalibrationScreen: Stability test complete. Deviation: " +
                    calibrationScreen.stabilityFinalDeviation.toFixed(3) + "°");
            }
        }
    }

    // ═══════════════ CALIBRATION MODEL ═══════════════

    ListModel { id: calModel }

    Component.onCompleted: {
        var rows = Device.calibrationRows;
        for (var i = 0; i < rows.length; i++) {
            calModel.append({
                "itemName":   rows[i].name,
                "itemResult": rows[i].result,
                "itemValue":  rows[i].value
            });
        }
        Device.markScreenCompleted(2);
        requestConfig();
        startStabilityTest();
    }

    // ─── GETCONFIG on load ───────────────────────────────────────────────

    function requestConfig() {
        if (!configRequested) {
            configRequested = true;
            console.log("CalibrationScreen: Sending GETCONFIG command.");
            Commander.sendCommand("GETCONFIG");
            configTimeoutTimer.start();
        }
    }

    Timer {
        id: configTimeoutTimer
        interval: 3000
        onTriggered: {
            if (!Device.calDataReceived) {
                console.log("CalibrationScreen: GETCONFIG timeout — using simulated calibration data.");
                Device.requestCalibrationData();
            }
        }
    }

    // ─── RECALIBRATE command ─────────────────────────────────────────────

    function sendRecalibrate() {
        console.log("CalibrationScreen: Sending RECALIBRATE command.");
        recalRequested   = true;
        recalInProgress  = true;
        Device.resetCalibrationData();     // clears checklist to WAITING + sets spinner flag
        stopStabilityTest();               // stability test no longer valid mid-recal
        Commander.sendCommand("RECALIBRATE");
        recalTimeoutTimer.start();
    }

    // If RECALIBRATE is confirmed but CONFIG data doesn't arrive within 3 s,
    // fall back to requesting simulated config (same as GETCONFIG timeout).
    Timer {
        id: recalTimeoutTimer
        interval: 5000
        onTriggered: {
            if (!Device.calDataReceived) {
                console.log("CalibrationScreen: RECALIBRATE config timeout — using simulated data.");
                Device.requestCalibrationData();
            }
            calibrationScreen.recalInProgress = false;
            // Re-run stability test with fresh data
            calibrationScreen.startStabilityTest();
        }
    }

    // ─── Commander signal handlers ───────────────────────────────────────

    Connections {
        target: Commander

        function onCommandConfirmed(cmd) {
            if (cmd === "RECALIBRATE") {
                console.log("CalibrationScreen: RECALIBRATE confirmed — waiting for CONFIG response.");
                // recalTimeoutTimer is already running; Device will call setCalibrationData
                // when the CONFIG status message arrives, which clears recalInProgress.
            }
            if (cmd === "GETCONFIG") {
                configTimeoutTimer.stop();
            }
        }

        function onCommandFailed(cmd, reason) {
            if (cmd === "RECALIBRATE") {
                console.log("CalibrationScreen: RECALIBRATE failed: " + reason);
                calibrationScreen.recalInProgress = false;
                recalTimeoutTimer.stop();
                rootWindow.showToast("Recalibrate failed — " + reason, Theme.error);
                // Repopulate with whatever was there before
                Device.requestCalibrationData();
                calibrationScreen.startStabilityTest();
            }
            if (cmd === "GETCONFIG") {
                console.log("CalibrationScreen: GETCONFIG failed: " + reason);
                Device.requestCalibrationData();
            }
        }
    }

    // ─── Calibration data update ─────────────────────────────────────────

    Connections {
        target: Device

        function onCalibrationDataChanged() {
            console.log("CalibrationScreen: Calibration data updated.");
            var rows = Device.calibrationRows;
            calModel.clear();
            for (var i = 0; i < rows.length; i++) {
                calModel.append({
                    "itemName":   rows[i].name,
                    "itemResult": rows[i].result,
                    "itemValue":  rows[i].value
                });
            }
            // Once real data comes back, clear the in-progress flags and
            // re-run stability test (only if we were recalibrating)
            if (calibrationScreen.recalInProgress && Device.calDataReceived) {
                calibrationScreen.recalInProgress = false;
                recalTimeoutTimer.stop();
                calibrationScreen.startStabilityTest();
                rootWindow.showToast("Recalibration complete", Theme.secondary);
            }
        }
    }

    // ═══════════════ LAYOUT ═══════════════

    Item {
        id: contentArea
        anchors.fill: parent
        anchors.margins: calibrationScreen.margin

        // ─── LEFT PANEL: calibration checklist ──────────────────────────

        Item {
            id: leftPanel
            anchors.top:  parent.top
            anchors.left: parent.left
            width:  calibrationScreen.wideMode
                        ? (parent.width - calibrationScreen.gap) / 2
                        : parent.width
            height: calibrationScreen.wideMode
                        ? parent.height
                        : (parent.height - calibrationScreen.gap) / 2

            Rectangle {
                anchors.fill: parent
                radius:       Math.round(12 * rootWindow.dp)
                color:        Theme.surface
                border.width: 1
                border.color: Theme.border

                Behavior on color       { ColorAnimation { duration: Theme.animDuration } }
                Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                Column {
                    anchors.fill:    parent
                    anchors.margins: Math.round(12 * rootWindow.dp)

                    // Header row
                    Item {
                        width:  parent.width
                        height: Math.round(36 * rootWindow.dp)

                        Text {
                            anchors.left:            parent.left
                            anchors.verticalCenter:  parent.verticalCenter
                            text:       "Calibration results"
                            font.family:    "Noto Sans"
                            font.pixelSize: Math.round(15 * rootWindow.sp)
                            font.weight:    Font.Medium
                            color:      Theme.onBackground
                            Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                        }

                        // Pass count badge
                        Text {
                            anchors.right:           parent.right
                            anchors.verticalCenter:  parent.verticalCenter
                            text: {
                                var passed = 0, total = calModel.count;
                                for (var i = 0; i < total; i++)
                                    if (calModel.get(i).itemResult === "PASS") passed++;
                                return passed + "/" + total;
                            }
                            font.family:    "Noto Sans Mono"
                            font.pixelSize: Math.round(12 * rootWindow.sp)
                            color: {
                                var passed = 0, failed = 0, total = calModel.count;
                                for (var i = 0; i < total; i++) {
                                    var r = calModel.get(i).itemResult;
                                    if (r === "PASS") passed++;
                                    if (r === "FAIL") failed++;
                                }
                                if (passed === total && total > 0) return Theme.secondary;
                                if (failed > 0)                    return Theme.error;
                                return Theme.onSurfaceVariant;
                            }
                            Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                        }

                        Rectangle {
                            anchors.left:   parent.left
                            anchors.right:  parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color:  Theme.border
                            opacity: 0.5
                        }
                    }

                    // Checklist
                    Flickable {
                        width:  parent.width
                        height: parent.height
                                - Math.round(36 * rootWindow.dp)
                                - recalBtnRow.height
                        contentHeight:      checkCol.height
                        flickableDirection: Flickable.VerticalFlick
                        boundsBehavior:     Flickable.StopAtBounds
                        clip: true

                        Column {
                            id: checkCol
                            width: parent.width

                            Repeater {
                                model: calModel

                                CheckRow {
                                    width:  checkCol.width
                                    name:   model.itemName
                                    result: model.itemResult
                                    measuredValue: model.itemValue

                                    opacity: model.itemResult === "WAITING" ? 0.7 : 1.0
                                    Behavior on opacity {
                                        NumberAnimation {
                                            duration: Settings.animationsEnabled ? 300 : 0
                                            easing.type: Easing.OutQuad
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // ── RECALIBRATE button row ───────────────────────────
                    Item {
                        id: recalBtnRow
                        width:  parent.width
                        height: Math.round(56 * rootWindow.dp)

                        Row {
                            anchors.centerIn: parent
                            spacing: Math.round(8 * rootWindow.dp)

                            // Recalibrate button
                            ActionButton {
                                id: recalBtn
                                text: calibrationScreen.recalInProgress
                                          ? "Recalibrating\u2026"
                                          : "Recalibrate device"
                                filled:      true
                                buttonColor: Theme.primary
                                width:  Math.round(180 * rootWindow.dp)
                                height: Math.round(40 * rootWindow.dp)
                                // Disable while: in-progress, another cmd in-flight,
                                // stability test is running, or not connected
                                enabled: !calibrationScreen.recalInProgress
                                         && !calibrationScreen.stabilityRunning
                                         && Device.connected

                                onClicked: {
                                    calibrationScreen.sendRecalibrate();
                                }

                                // Small spinner dot inside button while recal running
                                Rectangle {
                                    visible: calibrationScreen.recalInProgress
                                             && Settings.animationsEnabled
                                    anchors.right:        parent.right
                                    anchors.rightMargin:  Math.round(12 * rootWindow.dp)
                                    anchors.verticalCenter: parent.verticalCenter
                                    width:  Math.round(8 * rootWindow.dp)
                                    height: Math.round(8 * rootWindow.dp)
                                    radius: width / 2
                                    color:  "#ffffff"
                                    opacity: 0.0

                                    SequentialAnimation on opacity {
                                        running: calibrationScreen.recalInProgress
                                                 && Settings.animationsEnabled
                                        loops: Animation.Infinite
                                        NumberAnimation { to: 1.0; duration: 400 }
                                        NumberAnimation { to: 0.2; duration: 400 }
                                    }
                                }
                            }

                            // Refresh read button (GETCONFIG without resetting)
                            ActionButton {
                                text:        "Refresh read"
                                filled:      false
                                buttonColor: Theme.primary
                                width:  Math.round(120 * rootWindow.dp)
                                height: Math.round(40 * rootWindow.dp)
                                enabled: !calibrationScreen.recalInProgress
                                         && Device.connected

                                onClicked: {
                                    console.log("CalibrationScreen: Sending GETCONFIG (refresh).");
                                    Commander.sendCommand("GETCONFIG");
                                    configTimeoutTimer.restart();
                                }
                            }
                        }
                    }
                }
            }
        }

        // ─── RIGHT PANEL: static stability test ─────────────────────────

        Item {
            id: rightPanel
            anchors.top:       calibrationScreen.wideMode ? parent.top   : leftPanel.bottom
            anchors.topMargin: calibrationScreen.wideMode ? 0            : calibrationScreen.gap
            anchors.left:      calibrationScreen.wideMode ? leftPanel.right : parent.left
            anchors.leftMargin: calibrationScreen.wideMode ? calibrationScreen.gap : 0
            anchors.right:  parent.right
            anchors.bottom: parent.bottom

            Rectangle {
                anchors.fill: parent
                radius:       Math.round(12 * rootWindow.dp)
                color:        Theme.surface
                border.width: 1
                border.color: Theme.border

                Behavior on color       { ColorAnimation { duration: Theme.animDuration } }
                Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                Flickable {
                    anchors.fill:    parent
                    anchors.margins: Math.round(12 * rootWindow.dp)
                    contentHeight:   rightCol.height
                    flickableDirection: Flickable.VerticalFlick
                    boundsBehavior:  Flickable.StopAtBounds
                    clip: true

                    Column {
                        id: rightCol
                        width: parent.width
                        spacing: Math.round(8 * rootWindow.dp)

                        // ── Header ──────────────────────────────────────
                        Item {
                            width:  parent.width
                            height: Math.round(36 * rootWindow.dp)

                            Text {
                                anchors.left:           parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                text:       "Static stability test"
                                font.family:    "Noto Sans"
                                font.pixelSize: Math.round(15 * rootWindow.sp)
                                font.weight:    Font.Medium
                                color: Theme.onBackground
                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                            }

                            Text {
                                anchors.right:          parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                text: {
                                    if (calibrationScreen.recalInProgress) return "Paused";
                                    if (calibrationScreen.stabilityComplete) return "Complete";
                                    if (calibrationScreen.stabilityRunning)
                                        return calibrationScreen.stabilityElapsed + "/"
                                               + calibrationScreen.stabilityDuration + "s";
                                    return "Ready";
                                }
                                font.family:    "Noto Sans Mono"
                                font.pixelSize: Math.round(11 * rootWindow.sp)
                                color: {
                                    if (calibrationScreen.recalInProgress) return Theme.onSurfaceVariant;
                                    if (calibrationScreen.stabilityComplete) return Theme.secondary;
                                    if (calibrationScreen.stabilityRunning)  return Theme.primary;
                                    return Theme.onSurfaceVariant;
                                }
                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                            }

                            Rectangle {
                                anchors.left:   parent.left
                                anchors.right:  parent.right
                                anchors.bottom: parent.bottom
                                height: 1; color: Theme.border; opacity: 0.5
                            }
                        }

                        // ── Subtitle ─────────────────────────────────────
                        Text {
                            width: parent.width
                            text: calibrationScreen.recalInProgress
                                      ? "Recalibration in progress — please wait\u2026"
                                      : "Keep buoy still on flat surface."
                            font.family:    "Noto Sans"
                            font.pixelSize: Math.round(12 * rootWindow.sp)
                            color: calibrationScreen.recalInProgress
                                       ? Theme.warning
                                       : Theme.onSurfaceVariant
                            wrapMode: Text.WordWrap
                            Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                        }

                        // ── Progress bar ──────────────────────────────────
                        Item {
                            width:  parent.width
                            height: Math.round(8 * rootWindow.dp)

                            Rectangle {
                                anchors.left:           parent.left
                                anchors.right:          parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                height: Math.round(8 * rootWindow.dp)
                                radius: Math.round(4 * rootWindow.dp)
                                color:  Theme.surfaceVariant
                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }

                                Rectangle {
                                    anchors.left:   parent.left
                                    anchors.top:    parent.top
                                    anchors.bottom: parent.bottom
                                    width: {
                                        if (calibrationScreen.recalInProgress) return 0;
                                        if (calibrationScreen.stabilityComplete) return parent.width;
                                        if (!calibrationScreen.stabilityRunning) return 0;
                                        return parent.width * Math.min(1.0,
                                            calibrationScreen.stabilityElapsed /
                                            calibrationScreen.stabilityDuration);
                                    }
                                    radius: parent.radius
                                    color: calibrationScreen.stabilityComplete
                                               ? calibrationScreen.stabilityResultColor()
                                               : Theme.primary

                                    Behavior on width {
                                        NumberAnimation {
                                            duration: Settings.animationsEnabled ? 300 : 0
                                            easing.type: Easing.InOutQuad
                                        }
                                    }
                                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                }
                            }
                        }

                        // ── Live deviation readout ────────────────────────
                        Item {
                            width:  parent.width
                            height: Math.round(32 * rootWindow.dp)

                            Text {
                                anchors.left:           parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                text:       "Standard deviation:"
                                font.family:    "Noto Sans"
                                font.pixelSize: Math.round(12 * rootWindow.sp)
                                color: Theme.onSurfaceVariant
                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                            }

                            Text {
                                anchors.right:          parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                text: {
                                    if (calibrationScreen.recalInProgress) return "\u2014";
                                    if (!calibrationScreen.stabilityRunning
                                            && !calibrationScreen.stabilityComplete) return "\u2014";
                                    if (calibrationScreen.stabilityComplete)
                                        return calibrationScreen.stabilityFinalDeviation.toFixed(3) + "\u00B0";
                                    return calibrationScreen.stabilityDeviation.toFixed(3) + "\u00B0";
                                }
                                font.family:    "Noto Sans Mono"
                                font.pixelSize: Math.round(14 * rootWindow.sp)
                                font.weight:    Font.Medium
                                color: calibrationScreen.stabilityComplete
                                           ? calibrationScreen.stabilityResultColor()
                                           : Theme.onBackground
                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                            }
                        }

                        Rectangle {
                            width: parent.width; height: 1
                            color: Theme.border; opacity: 0.3
                        }

                        // ── Result area ───────────────────────────────────
                        Item {
                            width:  parent.width
                            height: Math.round(120 * rootWindow.dp)

                            // Recalibration in-progress overlay
                            Column {
                                anchors.centerIn: parent
                                spacing: Math.round(6 * rootWindow.dp)
                                visible: calibrationScreen.recalInProgress

                                Item {
                                    width:  Math.round(48 * rootWindow.dp)
                                    height: Math.round(48 * rootWindow.dp)
                                    anchors.horizontalCenter: parent.horizontalCenter

                                    Canvas {
                                        id: recalSpinner
                                        anchors.fill: parent
                                        property real spinAngle: 0
                                        property color spinColor: Theme.warning

                                        onSpinColorChanged: requestPaint()

                                        NumberAnimation on spinAngle {
                                            from: 0; to: 360; duration: 900
                                            loops: Animation.Infinite
                                            running: calibrationScreen.recalInProgress
                                                     && Settings.animationsEnabled
                                        }

                                        onSpinAngleChanged: requestPaint()

                                        onPaint: {
                                            var ctx = getContext("2d");
                                            ctx.clearRect(0, 0, width, height);
                                            var cx = width / 2, cy = height / 2;
                                            var r  = Math.min(cx, cy) * 0.7;
                                            var lw = Math.max(2, Math.round(3 * rootWindow.dp));

                                            ctx.beginPath();
                                            ctx.arc(cx, cy, r, 0, 2 * Math.PI);
                                            ctx.strokeStyle = Qt.rgba(
                                                spinColor.r, spinColor.g, spinColor.b, 0.15);
                                            ctx.lineWidth = lw;
                                            ctx.stroke();

                                            ctx.beginPath();
                                            var srad = (spinAngle - 90) * Math.PI / 180;
                                            ctx.arc(cx, cy, r, srad, srad + 1.5 * Math.PI);
                                            ctx.strokeStyle = spinColor;
                                            ctx.lineWidth = lw;
                                            ctx.lineCap = "round";
                                            ctx.stroke();
                                        }
                                    }
                                }

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "Recalibrating ESP32-S3\u2026"
                                    font.family:    "Noto Sans"
                                    font.pixelSize: Math.round(12 * rootWindow.sp)
                                    color: Theme.warning
                                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                }
                            }

                            // Stability test running spinner
                            Column {
                                anchors.centerIn: parent
                                spacing: Math.round(6 * rootWindow.dp)
                                visible: calibrationScreen.stabilityRunning
                                         && !calibrationScreen.stabilityComplete
                                         && !calibrationScreen.recalInProgress

                                Item {
                                    width:  Math.round(48 * rootWindow.dp)
                                    height: Math.round(48 * rootWindow.dp)
                                    anchors.horizontalCenter: parent.horizontalCenter

                                    Canvas {
                                        id: waitIcon
                                        anchors.fill: parent
                                        property real  spinAngle: 0
                                        property color spinColor: Theme.primary

                                        onSpinColorChanged: requestPaint()

                                        NumberAnimation on spinAngle {
                                            from: 0; to: 360; duration: 1200
                                            loops: Animation.Infinite
                                            running: calibrationScreen.stabilityRunning
                                                     && Settings.animationsEnabled
                                        }

                                        onSpinAngleChanged: requestPaint()

                                        onPaint: {
                                            var ctx = getContext("2d");
                                            ctx.clearRect(0, 0, width, height);
                                            var cx = width / 2, cy = height / 2;
                                            var r  = Math.min(cx, cy) * 0.7;
                                            var lw = Math.max(2, Math.round(3 * rootWindow.dp));

                                            ctx.beginPath();
                                            ctx.arc(cx, cy, r, 0, 2 * Math.PI);
                                            ctx.strokeStyle = Qt.rgba(
                                                spinColor.r, spinColor.g, spinColor.b, 0.15);
                                            ctx.lineWidth = lw;
                                            ctx.stroke();

                                            ctx.beginPath();
                                            var srad = (spinAngle - 90) * Math.PI / 180;
                                            ctx.arc(cx, cy, r, srad, srad + 1.5 * Math.PI);
                                            ctx.strokeStyle = spinColor;
                                            ctx.lineWidth = lw;
                                            ctx.lineCap = "round";
                                            ctx.stroke();
                                        }
                                    }
                                }

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "Collecting samples\u2026 "
                                          + calibrationScreen.tiltSamples.length + "/30"
                                    font.family:    "Noto Sans"
                                    font.pixelSize: Math.round(12 * rootWindow.sp)
                                    color: Theme.onSurfaceVariant
                                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                }
                            }

                            // Idle — not started yet
                            Column {
                                anchors.centerIn: parent
                                spacing: Math.round(6 * rootWindow.dp)
                                visible: !calibrationScreen.stabilityRunning
                                         && !calibrationScreen.stabilityComplete
                                         && !calibrationScreen.recalInProgress

                                Item {
                                    width:  Math.round(48 * rootWindow.dp)
                                    height: Math.round(48 * rootWindow.dp)
                                    anchors.horizontalCenter: parent.horizontalCenter

                                    Canvas {
                                        id: idleIcon
                                        anchors.fill: parent
                                        property color iconColor: Theme.onSurfaceVariant
                                        onIconColorChanged: requestPaint()
                                        Component.onCompleted: requestPaint()

                                        onPaint: {
                                            var ctx = getContext("2d");
                                            ctx.clearRect(0, 0, width, height);
                                            var cx = width / 2, cy = height / 2;
                                            var r  = Math.min(cx, cy) * 0.7;
                                            var lw = Math.max(2, Math.round(3 * rootWindow.dp));

                                            ctx.beginPath();
                                            ctx.arc(cx, cy, r, 0, 2 * Math.PI);
                                            ctx.strokeStyle = Qt.rgba(
                                                iconColor.r, iconColor.g, iconColor.b, 0.3);
                                            ctx.lineWidth = lw;
                                            ctx.stroke();

                                            ctx.beginPath();
                                            ctx.arc(cx, cy, r * 0.15, 0, 2 * Math.PI);
                                            ctx.fillStyle = Qt.rgba(
                                                iconColor.r, iconColor.g, iconColor.b, 0.4);
                                            ctx.fill();
                                        }
                                    }
                                }

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "Waiting to start\u2026"
                                    font.family:    "Noto Sans"
                                    font.pixelSize: Math.round(12 * rootWindow.sp)
                                    color: Theme.onSurfaceVariant
                                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                }
                            }

                            // Complete — show result
                            Column {
                                anchors.centerIn: parent
                                spacing: Math.round(6 * rootWindow.dp)
                                visible: calibrationScreen.stabilityComplete
                                         && !calibrationScreen.recalInProgress

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: calibrationScreen.stabilityFinalDeviation.toFixed(3)
                                          + "\u00B0"
                                    font.family:    "Noto Sans"
                                    font.pixelSize: Math.round(28 * rootWindow.sp)
                                    font.weight:    Font.DemiBold
                                    color: calibrationScreen.stabilityResultColor()
                                    scale: calibrationScreen.stabilityComplete ? 1.0 : 0.9
                                    Behavior on scale {
                                        NumberAnimation {
                                            duration: Settings.animationsEnabled ? 400 : 0
                                            easing.type: Easing.OutBack; easing.overshoot: 1.2
                                        }
                                    }
                                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                }

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text:  calibrationScreen.stabilityResultText()
                                    font.family:    "Noto Sans"
                                    font.pixelSize: Math.round(12 * rootWindow.sp)
                                    color: calibrationScreen.stabilityResultColor()
                                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                }

                                StatusBadge {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    status: calibrationScreen.stabilityResultBadge()
                                    scale: calibrationScreen.stabilityComplete ? 1.0 : 0.8
                                    Behavior on scale {
                                        NumberAnimation {
                                            duration: Settings.animationsEnabled ? 300 : 0
                                            easing.type: Easing.OutBack; easing.overshoot: 1.2
                                        }
                                    }
                                }
                            }
                        }

                        // ── Rerun / Start stability test button ───────────
                        Item {
                            width:  parent.width
                            height: Math.round(48 * rootWindow.dp)
                            visible: !calibrationScreen.recalInProgress
                                     && (calibrationScreen.stabilityComplete
                                         || (!calibrationScreen.stabilityRunning
                                             && !calibrationScreen.stabilityComplete))

                            ActionButton {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width:  Math.round(160 * rootWindow.dp)
                                height: Math.round(40 * rootWindow.dp)
                                text: calibrationScreen.stabilityComplete
                                          ? "Rerun test"
                                          : "Start test"
                                filled:      false
                                buttonColor: Theme.primary
                                enabled:     !calibrationScreen.stabilityRunning

                                onClicked: {
                                    console.log("CalibrationScreen: "
                                        + (calibrationScreen.stabilityComplete
                                               ? "Rerunning" : "Starting")
                                        + " stability test.");
                                    calibrationScreen.startStabilityTest();
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
