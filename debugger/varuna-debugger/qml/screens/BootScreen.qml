// -----------------------------------------------------------------
// File: qml/screens/BootScreen.qml
// Phase: Phase 6 Step 3
// -----------------------------------------------------------------

import QtQuick
import "../components"

Item {
    id: bootScreen

    readonly property real margin: Math.round(12 * rootWindow.dp)
    readonly property real gap: Math.round(10 * rootWindow.dp)
    readonly property real iconSize: Math.round(36 * rootWindow.dp)
    readonly property real btnAreaH: Math.round(80 * rootWindow.dp)

    property bool allPassed: false
    property bool anyFailed: false

    function evaluateResults() {
        var checks = Device.bootChecks;
        var allP = true;
        var anyF = false;
        for (var i = 0; i < checks.length; i++) {
            if (checks[i].status !== "PASS") allP = false;
            if (checks[i].status === "FAIL") anyF = true;
        }
        bootScreen.allPassed = allP;
        bootScreen.anyFailed = anyF;
    }

    ListModel {
        id: checkModel
    }

    Component.onCompleted: {
        var checks = Device.bootChecks;
        for (var i = 0; i < checks.length; i++) {
            checkModel.append({
                "checkName": checks[i].name !== undefined ? checks[i].name : "",
                "checkLabel": checks[i].label !== undefined ? checks[i].label : "",
                "checkStatus": checks[i].status !== undefined ? checks[i].status : "WAITING",
                "checkDetail": checks[i].detail !== undefined ? checks[i].detail : ""
            });
        }
        evaluateResults();
    }

    Connections {
        target: Device
        function onBootChecksChanged() {
            var checks = Device.bootChecks;
            for (var i = 0; i < checks.length && i < checkModel.count; i++) {
                checkModel.set(i, {
                    "checkName": checks[i].name !== undefined ? checks[i].name : "",
                    "checkLabel": checks[i].label !== undefined ? checks[i].label : "",
                    "checkStatus": checks[i].status !== undefined ? checks[i].status : "WAITING",
                    "checkDetail": checks[i].detail !== undefined ? checks[i].detail : ""
                });
            }
            evaluateResults();
        }
    }

    // ===== GRID AREA =====
    Item {
        id: gridArea
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: buttonArea.top
        anchors.margins: bootScreen.margin

        Grid {
            id: checkGrid
            anchors.fill: parent
            columns: 2
            spacing: bootScreen.gap

            Repeater {
                model: checkModel

                delegate: Rectangle {
                    id: cardDelegate

                    width: (checkGrid.width - bootScreen.gap) / 2
                    height: (checkGrid.height - 2 * bootScreen.gap) / 3

                    radius: Math.round(12 * rootWindow.dp)
                    color: Theme.surface
                    border.width: Math.max(1, Math.round(1.5 * rootWindow.dp))
                    border.color: Theme.border

                    property string prevStatus: "WAITING"
                    property color flashColor: "transparent"

                    Behavior on color {
                        ColorAnimation { duration: Theme.animDuration }
                    }

                    // Border flash on status change
                    onFlashColorChanged: {
                        if (flashColor !== "transparent" && flashColor !== Qt.rgba(0,0,0,0)) {
                            border.color = flashColor;
                        }
                    }

                    SequentialAnimation {
                        id: borderFlashAnim
                        property color targetColor: Theme.secondary

                        ColorAnimation {
                            target: cardDelegate
                            property: "border.color"
                            to: borderFlashAnim.targetColor
                            duration: 1
                        }
                        PauseAnimation {
                            duration: Settings.animationsEnabled ? 400 : 0
                        }
                        ColorAnimation {
                            target: cardDelegate
                            property: "border.color"
                            to: Theme.border
                            duration: Settings.animationsEnabled ? 300 : 0
                            easing.type: Easing.OutQuad
                        }
                    }

                    // Detect status change from WAITING to result
                    Connections {
                        target: checkModel.get(index) !== null ? checkModel : null
                        function onDataChanged() {
                            var current = model.checkStatus;
                            if (cardDelegate.prevStatus === "WAITING" && current !== "WAITING") {
                                // Fire border flash
                                borderFlashAnim.targetColor = (current === "PASS") ? Theme.secondary : Theme.error;
                                borderFlashAnim.start();

                                // Fire icon scale animation
                                if (Settings.animationsEnabled) {
                                    iconScaleAnim.start();
                                }
                            }
                            cardDelegate.prevStatus = current;
                        }
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: Math.round(6 * rootWindow.dp)

                        // ===== ICON AREA =====
                        Item {
                            width: bootScreen.iconSize
                            height: bootScreen.iconSize
                            anchors.horizontalCenter: parent.horizontalCenter

                            scale: 1.0

                            SequentialAnimation {
                                id: iconScaleAnim

                                NumberAnimation {
                                    target: iconCanvas.parent
                                    property: "scale"
                                    from: 0.85
                                    to: 1.08
                                    duration: Settings.animationsEnabled ? 180 : 0
                                    easing.type: Easing.OutQuad
                                }
                                NumberAnimation {
                                    target: iconCanvas.parent
                                    property: "scale"
                                    from: 1.08
                                    to: 1.0
                                    duration: Settings.animationsEnabled ? 120 : 0
                                    easing.type: Easing.InOutQuad
                                }
                            }

                            Canvas {
                                id: iconCanvas
                                anchors.fill: parent

                                property string cStatus: model.checkStatus
                                property real spinAngle: 0
                                property color colPass: Theme.secondary
                                property color colFail: Theme.error
                                property color colWait: Theme.primary

                                onCStatusChanged: requestPaint()
                                onColPassChanged: requestPaint()
                                onColFailChanged: requestPaint()
                                onColWaitChanged: requestPaint()

                                Component.onCompleted: requestPaint()

                                NumberAnimation on spinAngle {
                                    from: 0
                                    to: 360
                                    duration: 1200
                                    loops: Animation.Infinite
                                    running: iconCanvas.cStatus === "WAITING" && Settings.animationsEnabled
                                }

                                onSpinAngleChanged: {
                                    if (cStatus === "WAITING") {
                                        requestPaint();
                                    }
                                }

                                onPaint: {
                                    var ctx = getContext("2d");
                                    ctx.clearRect(0, 0, width, height);

                                    var cx = width / 2;
                                    var cy = height / 2;
                                    var r = Math.min(cx, cy) * 0.82;
                                    var lw = Math.max(2, Math.round(3 * rootWindow.dp));

                                    if (cStatus === "WAITING") {
                                        ctx.beginPath();
                                        ctx.arc(cx, cy, r, 0, 2 * Math.PI);
                                        ctx.strokeStyle = Qt.rgba(colWait.r, colWait.g, colWait.b, 0.15);
                                        ctx.lineWidth = lw;
                                        ctx.stroke();

                                        ctx.beginPath();
                                        var srad = (spinAngle - 90) * Math.PI / 180;
                                        ctx.arc(cx, cy, r, srad, srad + 1.5 * Math.PI);
                                        ctx.strokeStyle = colWait;
                                        ctx.lineWidth = lw;
                                        ctx.lineCap = "round";
                                        ctx.stroke();

                                    } else if (cStatus === "PASS") {
                                        ctx.beginPath();
                                        ctx.arc(cx, cy, r, 0, 2 * Math.PI);
                                        ctx.fillStyle = colPass;
                                        ctx.fill();

                                        ctx.beginPath();
                                        ctx.moveTo(width * 0.28, height * 0.50);
                                        ctx.lineTo(width * 0.43, height * 0.66);
                                        ctx.lineTo(width * 0.72, height * 0.35);
                                        ctx.strokeStyle = "#ffffff";
                                        ctx.lineWidth = lw + 0.5;
                                        ctx.lineCap = "round";
                                        ctx.lineJoin = "round";
                                        ctx.stroke();

                                    } else if (cStatus === "FAIL") {
                                        ctx.beginPath();
                                        ctx.arc(cx, cy, r, 0, 2 * Math.PI);
                                        ctx.fillStyle = colFail;
                                        ctx.fill();

                                        var off = r * 0.45;
                                        ctx.strokeStyle = "#ffffff";
                                        ctx.lineWidth = lw + 0.5;
                                        ctx.lineCap = "round";

                                        ctx.beginPath();
                                        ctx.moveTo(cx - off, cy - off);
                                        ctx.lineTo(cx + off, cy + off);
                                        ctx.stroke();

                                        ctx.beginPath();
                                        ctx.moveTo(cx + off, cy - off);
                                        ctx.lineTo(cx - off, cy + off);
                                        ctx.stroke();
                                    }
                                }
                            }
                        }

                        // ===== SENSOR NAME =====
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: model.checkName
                            font.family: "Noto Sans"
                            font.pixelSize: Math.round(15 * rootWindow.sp)
                            font.weight: Font.Medium
                            color: Theme.onBackground
                            horizontalAlignment: Text.AlignHCenter

                            Behavior on color {
                                ColorAnimation { duration: Theme.animDuration }
                            }
                        }

                        // ===== DESCRIPTION =====
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: model.checkLabel
                            font.family: "Noto Sans"
                            font.pixelSize: Math.round(12 * rootWindow.sp)
                            color: Theme.onSurfaceVariant
                            horizontalAlignment: Text.AlignHCenter

                            Behavior on color {
                                ColorAnimation { duration: Theme.animDuration }
                            }
                        }

                        // ===== STATUS BADGE =====
                        StatusBadge {
                            anchors.horizontalCenter: parent.horizontalCenter
                            status: model.checkStatus
                            scale: model.checkStatus !== "WAITING" ? 1.0 : 0.9

                            Behavior on scale {
                                NumberAnimation {
                                    duration: Settings.animationsEnabled ? 300 : 0
                                    easing.type: Easing.OutBack
                                    easing.overshoot: 1.2
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ===== BUTTON AREA =====
    Item {
        id: buttonArea
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: bootScreen.btnAreaH

        Column {
            anchors.centerIn: parent
            width: parent.width * 0.7
            spacing: Math.round(6 * rootWindow.dp)

            // Warning text (only visible if any failed)
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "One or more subsystems failed. Review before deploying."
                font.family: "Noto Sans"
                font.pixelSize: Math.round(12 * rootWindow.sp)
                color: Theme.warning
                visible: bootScreen.anyFailed
                horizontalAlignment: Text.AlignHCenter
                width: parent.width
                wrapMode: Text.WordWrap

                Behavior on color {
                    ColorAnimation { duration: Theme.animDuration }
                }
            }

            // Continue button
            ActionButton {
                id: continueBtn
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width
                height: Math.round(48 * rootWindow.dp)
                filled: true
                enabled: true
                buttonColor: bootScreen.allPassed ? Theme.secondary : Theme.primary
                text: bootScreen.allPassed ? "All systems nominal — Continue" : "Continue to live readout"

                onClicked: {
                    console.log("Boot screen: Continue button clicked, navigating to Live screen.");
                    Device.markScreenCompleted(0);
                    bottomNav.currentIndex = 1;
                    bottomNav.navTapped(1);
                }
            }
        }
    }
}

// -----------------------------------------------------------------
// File: qml/screens/BootScreen.qml
// Phase: Phase 6 Step 3
// ----------------------------END----------------------------------
