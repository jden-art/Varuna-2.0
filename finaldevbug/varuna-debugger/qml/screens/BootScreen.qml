// -----------------------------------------------------------------
// File: qml/screens/BootScreen.qml
// Phase: Phase 6 Step 3 — Radial HUD Redesign
// Theme-synced: light / dark via ThemeManager
// -----------------------------------------------------------------

import QtQuick
import "../components"

Item {
    id: bootScreen

    // ─── Backend state ───
    property bool allPassed: false
    property bool anyFailed: false
    property bool viewReady: false

    // ─── Responsive design tokens ───
    readonly property real dp: rootWindow.dp
    readonly property real sp: rootWindow.sp
    readonly property real minDim: Math.min(width, height)
    readonly property real maxDim: Math.max(width, height)
    readonly property bool isLandscape: width > height * 1.15
    readonly property bool isSmallScreen: minDim < 380
    readonly property bool isTinyScreen: minDim < 280

    // ─── Footer height (used to centre HUD above it) ───
    readonly property real footerH: isLandscape ? Math.round(80 * dp)
    : Math.round(110 * dp)

    // ─── Layout geometry — centred above footer ───
    readonly property real availableH: height - footerH
    readonly property real hudCenterX: width * 0.5
    readonly property real hudCenterY: availableH * 0.5

    readonly property real ringRadius: minDim * (isTinyScreen ? 0.10
    : isSmallScreen ? 0.11 : 0.125)
    readonly property real nodeOrbitRadius: ringRadius + minDim
    * (isTinyScreen ? 0.125
    : isSmallScreen ? 0.14 : 0.175)
    readonly property real nodeSize: Math.round(Math.max(18, minDim * 0.058))
    readonly property int  nodeCount: checkModel.count

    // ─── Theme-reactive flag ───
    readonly property bool isDark: Theme.dark

    // ─── HUD palette (light / dark) ───
    readonly property color bgDeep:        isDark ? "#05070b" : "#f2f4f8"
    readonly property color bgMid:         isDark ? "#090d16" : "#e8ebf0"
    readonly property color hudDimLine:    isDark ? "#1a2240" : "#c8d0e0"
    readonly property color hudDeco:       isDark ? "#5974b3" : "#7882a0"
    readonly property color hudPrimary:    isDark ? "#6c8cff" : "#3b6ce8"
    readonly property color hudPrimaryDim: isDark ? "#3a5199" : "#94aed8"
    readonly property color hudSuccess:    isDark ? "#34d399" : "#1a8a4c"
    readonly property color hudSuccessDim: isDark ? "#1a7a55" : "#7cc0a0"
    readonly property color hudError:      isDark ? "#f87171" : "#d93025"
    readonly property color hudErrorDim:   isDark ? "#993333" : "#e8a0a0"
    readonly property color hudWarning:    isDark ? "#fbbf24" : "#e69500"
    readonly property color textPrimary:   isDark ? "#c8d0e0" : "#1a2030"
    readonly property color textSecondary: isDark ? "#8892a8" : "#5f6368"
    readonly property color textMuted:     isDark ? "#4a5470" : "#9aa0a6"

    readonly property color activeColor: allPassed ? hudSuccess
    : anyFailed ? hudError
    : hudPrimary
    readonly property color activeDim:   allPassed ? hudSuccessDim
    : anyFailed ? hudErrorDim
    : hudPrimaryDim

    property int  completedCount: 0
    property real completionRatio: checkModel.count > 0
    ? completedCount / checkModel.count : 0

    // ─── GPay overlay state ───
    property bool overlayActive: false
    property bool overlayIsPass: true

    onOverlayActiveChanged: {
        if (overlayActive) {
            gpayOverlay.circleProgress = 0;
            gpayOverlay.fillProgress   = 0;
            gpayOverlay.iconProgress   = 0;
            gpayOverlay.labelOpacity   = 0;
            gpayOverlay.overallScale   = 0.7;
            if (Settings.animationsEnabled) {
                gpaySequence.start();
            } else {
                gpayOverlay.circleProgress = 1;
                gpayOverlay.fillProgress   = 1;
                gpayOverlay.iconProgress   = 1;
                gpayOverlay.labelOpacity   = 1;
                gpayOverlay.overallScale   = 1;
                gpayOverlay.requestPaint();
            }
        }
    }

    // ─── Repaint everything on theme change ───
    Connections {
        target: Theme
        function onChanged() {
            bgCanvas.requestPaint();
            hudCanvas.requestPaint();
            radialLines.requestPaint();
            centerTextCanvas.requestPaint();
            nodeCanvas.requestPaint();
            if (gpayOverlay.visible) gpayOverlay.requestPaint();
        }
    }

    function recount() {
        var done = 0;
        for (var i = 0; i < checkModel.count; i++) {
            if (checkModel.get(i).checkStatus !== "WAITING") done++;
        }
        completedCount = done;
    }

    function evaluateResults() {
        var checks = Device.bootChecks;
        var prevAllPassed = allPassed;
        var prevAnyFailed = anyFailed;
        var allP = true;
        var anyF = false;
        for (var i = 0; i < checks.length; i++) {
            if (checks[i].status !== "PASS") allP = false;
            if (checks[i].status === "FAIL") anyF = true;
        }
        bootScreen.allPassed = allP;
        bootScreen.anyFailed = anyF;
        recount();
        if (!prevAllPassed && !prevAnyFailed) {
            if (allP || anyF) {
                overlayIsPass = allP;
                overlayActive = true;
            }
        }
    }

    ListModel { id: checkModel }

    Component.onCompleted: {
        var checks = Device.bootChecks;
        for (var i = 0; i < checks.length; i++) {
            checkModel.append({
                "checkName":   checks[i].name   !== undefined ? checks[i].name   : "",
                "checkLabel":  checks[i].label  !== undefined ? checks[i].label  : "",
                "checkStatus": checks[i].status !== undefined ? checks[i].status : "WAITING",
                "checkDetail": checks[i].detail !== undefined ? checks[i].detail : ""
            });
        }
        evaluateResults();
        readyDelay.start();
    }

    Timer {
        id: readyDelay
        interval: 80
        onTriggered: bootScreen.viewReady = true
    }

    Connections {
        target: Device
        function onBootChecksChanged() {
            var checks = Device.bootChecks;
            for (var i = 0; i < checks.length && i < checkModel.count; i++) {
                checkModel.set(i, {
                    "checkName":   checks[i].name   !== undefined ? checks[i].name   : "",
                    "checkLabel":  checks[i].label  !== undefined ? checks[i].label  : "",
                    "checkStatus": checks[i].status !== undefined ? checks[i].status : "WAITING",
                    "checkDetail": checks[i].detail !== undefined ? checks[i].detail : ""
                });
            }
            evaluateResults();
            hudCanvas.requestPaint();
            nodeCanvas.requestPaint();
            radialLines.requestPaint();
        }
    }

    // =====================================================================
    //  LAYER 0 — BACKGROUND (branches light vs dark)
    // =====================================================================
    Canvas {
        id: bgCanvas
        anchors.fill: parent
        z: -2
        renderStrategy: Canvas.Cooperative

        Component.onCompleted: requestPaint()
        onWidthChanged:  requestPaint()
        onHeightChanged: requestPaint()

        property var starData: {
            var stars = [];
            for (var i = 0; i < 60; i++) {
                stars.push({
                    x: Math.random(), y: Math.random(),
                           r: 0.3 + Math.random() * 0.7,
                           a: 0.04 + Math.random() * 0.14,
                           hue: 0.58 + Math.random() * 0.12
                });
            }
            return stars;
        }

        onPaint: {
            var ctx = getContext("2d");
            var w = width, h = height;
            ctx.clearRect(0, 0, w, h);
            var dark = isDark;

            // Base fill
            ctx.fillStyle = bgDeep;
            ctx.fillRect(0, 0, w, h);

            // Centre glow
            var cGrad = ctx.createRadialGradient(
                hudCenterX, hudCenterY, 0,
                hudCenterX, hudCenterY, maxDim * 0.65);
            if (dark) {
                cGrad.addColorStop(0.0,  Qt.rgba(0.06, 0.08, 0.16, 0.45));
                cGrad.addColorStop(0.35, Qt.rgba(0.03, 0.04, 0.09, 0.20));
            } else {
                cGrad.addColorStop(0.0,  Qt.rgba(0.82, 0.86, 0.95, 0.30));
                cGrad.addColorStop(0.35, Qt.rgba(0.90, 0.92, 0.96, 0.10));
            }
            cGrad.addColorStop(1.0, "transparent");
            ctx.fillStyle = cGrad;
            ctx.fillRect(0, 0, w, h);

            // Secondary glow (dark only)
            if (dark) {
                var wGrad = ctx.createRadialGradient(
                    w * 0.75, h * 0.7, 0, w * 0.75, h * 0.7, maxDim * 0.5);
                wGrad.addColorStop(0.0, Qt.rgba(0.08, 0.04, 0.14, 0.08));
                wGrad.addColorStop(1.0, "transparent");
                ctx.fillStyle = wGrad;
                ctx.fillRect(0, 0, w, h);
            }

            // Stars (dark mode only)
            if (dark) {
                var sd = starData;
                for (var i = 0; i < sd.length; i++) {
                    var s = sd[i];
                    var sr = s.r * dp;
                    ctx.beginPath();
                    ctx.arc(s.x * w, s.y * h, sr, 0, 2 * Math.PI);
                    var bri = 0.7 + s.hue * 0.3;
                    ctx.fillStyle = Qt.rgba(bri * 0.9, bri * 0.92, bri, s.a);
                    ctx.fill();
                }
            }

            // Scanlines
            var step = Math.max(2, Math.round(2.5 * dp));
            ctx.globalAlpha = dark ? 0.018 : 0.008;
            ctx.fillStyle = dark ? "#ffffff" : "#000000";
            for (var y = 0; y < h; y += step * 2) {
                ctx.fillRect(0, y, w, Math.max(1, 0.5 * dp));
            }
            ctx.globalAlpha = 1.0;

            // Vignette
            var vGrad = ctx.createRadialGradient(
                w * 0.5, h * 0.5, minDim * 0.25,
                w * 0.5, h * 0.5, maxDim * 0.75);
            vGrad.addColorStop(0.0, "transparent");
            if (dark) {
                vGrad.addColorStop(0.7, Qt.rgba(0.02, 0.03, 0.04, 0.15));
                vGrad.addColorStop(1.0, Qt.rgba(0.01, 0.02, 0.03, 0.45));
            } else {
                vGrad.addColorStop(0.7, Qt.rgba(0.70, 0.73, 0.78, 0.04));
                vGrad.addColorStop(1.0, Qt.rgba(0.60, 0.63, 0.68, 0.08));
            }
            ctx.fillStyle = vGrad;
            ctx.fillRect(0, 0, w, h);
        }
    }

    // =====================================================================
    //  STAR TWINKLE — dark mode only
    // =====================================================================
    Canvas {
        id: twinkleCanvas
        anchors.fill: parent
        z: -1
        renderStrategy: Canvas.Cooperative
        visible: Settings.animationsEnabled && viewReady && isDark

        property real tick: 0
        property var twinkleStars: {
            var ts = [];
            for (var i = 0; i < 12; i++) {
                ts.push({
                    x: Math.random(), y: Math.random(),
                        r: (0.4 + Math.random() * 0.6) * dp,
                        speed: 0.3 + Math.random() * 0.7,
                        phase: Math.random() * Math.PI * 2,
                        brightness: 0.7 + Math.random() * 0.3
                });
            }
            return ts;
        }

        Timer {
            running: twinkleCanvas.visible
            interval: 130; repeat: true
            onTriggered: { twinkleCanvas.tick += 0.13; twinkleCanvas.requestPaint(); }
        }

        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);
            var ts = twinkleStars;
            for (var i = 0; i < ts.length; i++) {
                var s = ts[i];
                var alpha = 0.06 + 0.18 * (0.5 + 0.5 * Math.sin(tick * s.speed + s.phase));
                var br = s.brightness;
                ctx.beginPath();
                ctx.arc(s.x * width, s.y * height, s.r, 0, 2 * Math.PI);
                ctx.fillStyle = Qt.rgba(br * 0.88, br * 0.91, br, alpha);
                ctx.fill();
            }
        }
    }

    // =====================================================================
    //  MAIN HUD CANVAS — rings, crosshairs, sweep
    //  Uses palette colour components for automatic light/dark adaptation
    // =====================================================================
    Canvas {
        id: hudCanvas
        anchors.fill: parent
        z: 1
        renderStrategy: Canvas.Cooperative

        property real spinA:      0
        property real spinB:      0
        property real innerSpin:  0
        property real sweepAngle: 0
        property real glowPulse:  0
        property real prog:       bootScreen.completionRatio
        property real reveal:     0
        property bool isResolved: allPassed || anyFailed

        Timer {
            id: hudTimer
            running: Settings.animationsEnabled && viewReady
            interval: 42; repeat: true
            onTriggered: {
                var dt = 0.042;
                if (!hudCanvas.isResolved) {
                    hudCanvas.spinA     = (hudCanvas.spinA + dt * 112.5) % 360;
                    hudCanvas.spinB     = (hudCanvas.spinB - dt * 72) % 360;
                    if (hudCanvas.spinB < 0) hudCanvas.spinB += 360;
                    hudCanvas.innerSpin = (hudCanvas.innerSpin - dt * 52) % 360;
                    if (hudCanvas.innerSpin < 0) hudCanvas.innerSpin += 360;
                    hudCanvas.sweepAngle = (hudCanvas.sweepAngle + dt * 85) % 360;
                }
                hudCanvas.glowPulse = 0.5 + 0.5 * Math.sin(Date.now() * 0.0014);
                hudCanvas.requestPaint();
            }
        }

        Component.onCompleted: {
            if (Settings.animationsEnabled) { revealAnim.start(); }
            else { reveal = 1.0; requestPaint(); }
        }

        NumberAnimation {
            id: revealAnim
            target: hudCanvas; property: "reveal"
            from: 0; to: 1; duration: 1100
            easing.type: Easing.OutCubic
        }

        onRevealChanged: if (!hudTimer.running) requestPaint()
        onProgChanged:   if (!hudTimer.running) requestPaint()

        onPaint: {
            var ctx = getContext("2d");
            var w = width, h = height;
            ctx.clearRect(0, 0, w, h);

            var rv = reveal;
            if (rv < 0.01) return;

            var cx = hudCenterX, cy = hudCenterY;
            var R = ringRadius, rInner = R * 0.62, rMid = (R + rInner) / 2;
            var dpv = dp;
            var dark = isDark;

            // Palette shortcuts for Canvas
            var pr = hudPrimary.r, pg = hudPrimary.g, pb = hudPrimary.b;
            var dr = hudDeco.r,    dg = hudDeco.g,    db = hudDeco.b;
            var sr = hudSuccess.r, sg = hudSuccess.g, sb = hudSuccess.b;
            var ar = activeColor.r, ag = activeColor.g, ab = activeColor.b;
            var glowMul = dark ? 1.0 : 0.35;

            ctx.save();
            ctx.globalAlpha = rv;

            // ── Tick marks ──
            var tickR   = R + 5 * dpv;
            var tickLen = 3 * dpv;
            ctx.lineWidth = Math.max(1, 0.5 * dpv);
            for (var ti = 0; ti < 72; ti++) {
                var ta    = (ti / 72) * 2 * Math.PI;
                var tLen  = (ti % 9 === 0) ? tickLen * 2.2 : tickLen;
                var tAlph = (ti % 9 === 0) ? (dark ? 0.14 : 0.22) : (dark ? 0.06 : 0.10);
                ctx.strokeStyle = Qt.rgba(dr, dg, db, tAlph);
                ctx.beginPath();
                ctx.moveTo(cx + Math.cos(ta) * tickR,
                           cy + Math.sin(ta) * tickR);
                ctx.lineTo(cx + Math.cos(ta) * (tickR + tLen),
                           cy + Math.sin(ta) * (tickR + tLen));
                ctx.stroke();
            }

            // ── Crosshair ──
            var crossStart = R * 0.18;
            var crossEnd   = R * 0.52;
            ctx.lineWidth = Math.max(1, 0.5 * dpv);
            var dirs = [[0,-1],[0,1],[-1,0],[1,0]];
            for (var d = 0; d < 4; d++) {
                ctx.strokeStyle = Qt.rgba(dr, dg, db, dark ? 0.06 : 0.10);
                ctx.beginPath();
                ctx.moveTo(cx + dirs[d][0] * crossStart, cy + dirs[d][1] * crossStart);
                ctx.lineTo(cx + dirs[d][0] * crossEnd,   cy + dirs[d][1] * crossEnd);
                ctx.stroke();
                var ex = cx + dirs[d][0] * crossEnd;
                var ey = cy + dirs[d][1] * crossEnd;
                var dd = 1.5 * dpv;
                ctx.fillStyle = Qt.rgba(dr, dg, db, dark ? 0.10 : 0.14);
                ctx.beginPath();
                ctx.moveTo(ex, ey - dd); ctx.lineTo(ex + dd, ey);
                ctx.lineTo(ex, ey + dd); ctx.lineTo(ex - dd, ey);
                ctx.closePath(); ctx.fill();
            }

            // ── Breathing glow ring ──
            var gAlpha = glowPulse * 0.15 * glowMul;
            var glowR = R + 8 * dpv;
            ctx.beginPath();
            ctx.arc(cx, cy, glowR, 0, 2 * Math.PI);
            ctx.strokeStyle = Qt.rgba(ar, ag, ab, gAlpha);
            ctx.lineWidth = Math.max(1, 1.8 * dpv);
            ctx.stroke();

            var bloomR = R + 14 * dpv;
            var bloomGrad = ctx.createRadialGradient(cx, cy, R, cx, cy, bloomR);
            bloomGrad.addColorStop(0, Qt.rgba(ar, ag, ab, gAlpha * (dark ? 0.4 : 0.15)));
            bloomGrad.addColorStop(1, "transparent");
            ctx.beginPath();
            ctx.arc(cx, cy, bloomR, 0, 2 * Math.PI);
            ctx.fillStyle = bloomGrad;
            ctx.fill();

            if (!isResolved) {
                // ── ACTIVE STATE ──

                // Outer ring track
                ctx.beginPath();
                ctx.arc(cx, cy, R, 0, 2 * Math.PI);
                ctx.strokeStyle = Qt.rgba(pr, pg, pb, dark ? 0.07 : 0.12);
                ctx.lineWidth = Math.max(1, 1.5 * dpv);
                ctx.stroke();

                // Primary arc
                var saA = (spinA - 90) * Math.PI / 180;
                ctx.beginPath();
                ctx.arc(cx, cy, R, saA, saA + Math.PI * 1.11);
                var arcGrad1 = ctx.createLinearGradient(
                    cx + Math.cos(saA) * R, cy + Math.sin(saA) * R,
                                                        cx + Math.cos(saA + Math.PI) * R, cy + Math.sin(saA + Math.PI) * R);
                arcGrad1.addColorStop(0,   Qt.rgba(pr, pg, pb, 0.15));
                arcGrad1.addColorStop(0.5, Qt.rgba(pr, pg, pb, 0.70));
                arcGrad1.addColorStop(1,   Qt.rgba(pr, pg, pb, 0.30));
                ctx.strokeStyle = arcGrad1;
                ctx.lineWidth = Math.max(1.5, 2 * dpv);
                ctx.lineCap = "round";
                ctx.stroke();

                // Secondary arc
                var saB = (spinB - 90) * Math.PI / 180;
                ctx.beginPath();
                ctx.arc(cx, cy, R, saB, saB + Math.PI * 0.5);
                ctx.strokeStyle = Qt.rgba(pr, pg, pb, dark ? 0.18 : 0.22);
                ctx.lineWidth = Math.max(1, 1.2 * dpv);
                ctx.lineCap = "round";
                ctx.stroke();

                // Inner ring track
                ctx.beginPath();
                ctx.arc(cx, cy, rInner, 0, 2 * Math.PI);
                ctx.strokeStyle = Qt.rgba(pr, pg, pb, dark ? 0.04 : 0.07);
                ctx.lineWidth = Math.max(1, dpv);
                ctx.stroke();

                // Inner ring arc
                var saI = (innerSpin - 90) * Math.PI / 180;
                ctx.beginPath();
                ctx.arc(cx, cy, rInner, saI, saI + Math.PI * 0.7);
                ctx.strokeStyle = Qt.rgba(pr, pg, pb, dark ? 0.20 : 0.25);
                ctx.lineWidth = Math.max(1, 0.8 * dpv);
                ctx.lineCap = "round";
                ctx.stroke();

                // Progress arc
                if (prog > 0.005) {
                    var pStart = -Math.PI / 2;
                    var pEnd   = pStart + 2 * Math.PI * prog;
                    ctx.beginPath();
                    ctx.arc(cx, cy, rMid, pStart, pEnd);
                    ctx.strokeStyle = Qt.rgba(sr, sg, sb, 0.25);
                    ctx.lineWidth = Math.max(2, 2.5 * dpv);
                    ctx.lineCap = "round";
                    ctx.stroke();
                    var headX = cx + Math.cos(pEnd) * rMid;
                    var headY = cy + Math.sin(pEnd) * rMid;
                    ctx.beginPath();
                    ctx.arc(headX, headY, 2 * dpv, 0, 2 * Math.PI);
                    ctx.fillStyle = Qt.rgba(sr, sg, sb, 0.6);
                    ctx.fill();
                }

                // Sweep wedge
                var sweepRad  = (sweepAngle - 90) * Math.PI / 180;
                var sweepSpan = 0.28;
                ctx.beginPath();
                ctx.moveTo(cx, cy);
                ctx.arc(cx, cy, R * 0.85, sweepRad - sweepSpan, sweepRad);
                ctx.closePath();
                ctx.fillStyle = Qt.rgba(pr, pg, pb, dark ? 0.025 : 0.018);
                ctx.fill();

                // Sweep leading edge
                ctx.beginPath();
                ctx.moveTo(cx + Math.cos(sweepRad) * rInner * 0.5,
                           cy + Math.sin(sweepRad) * rInner * 0.5);
                ctx.lineTo(cx + Math.cos(sweepRad) * R * 0.85,
                           cy + Math.sin(sweepRad) * R * 0.85);
                ctx.strokeStyle = Qt.rgba(pr, pg, pb, dark ? 0.08 : 0.06);
                ctx.lineWidth = Math.max(1, 0.5 * dpv);
                ctx.stroke();

            } else {
                // ── RESOLVED — quiet static rings ──
                var rc = allPassed ? hudSuccess : hudError;

                ctx.beginPath();
                ctx.arc(cx, cy, R, 0, 2 * Math.PI);
                ctx.strokeStyle = Qt.rgba(rc.r, rc.g, rc.b, dark ? 0.35 : 0.45);
                ctx.lineWidth = Math.max(1.5, 2 * dpv);
                ctx.stroke();

                ctx.beginPath();
                ctx.arc(cx, cy, rInner, 0, 2 * Math.PI);
                ctx.strokeStyle = Qt.rgba(rc.r, rc.g, rc.b, dark ? 0.10 : 0.15);
                ctx.lineWidth = Math.max(1, dpv);
                ctx.stroke();
            }

            // ── Dashed orbit path ──
            var oR = nodeOrbitRadius;
            ctx.setLineDash([3.5 * dpv, 7 * dpv]);
            ctx.beginPath();
            ctx.arc(cx, cy, oR, 0, 2 * Math.PI);
            ctx.strokeStyle = Qt.rgba(dr, dg, db, dark ? 0.045 : 0.10);
            ctx.lineWidth = Math.max(1, 0.5 * dpv);
            ctx.stroke();
            ctx.setLineDash([]);

            ctx.restore();
        }
    }

    // =====================================================================
    //  RADIAL CONNECTING LINES
    // =====================================================================
    Canvas {
        id: radialLines
        anchors.fill: parent
        z: 1
        renderStrategy: Canvas.Cooperative

        property real lineReveal: 0

        Component.onCompleted: {
            if (Settings.animationsEnabled) { lineRevealAnim.start(); }
            else { lineReveal = 1.0; }
            requestPaint();
        }

        NumberAnimation {
            id: lineRevealAnim
            target: radialLines; property: "lineReveal"
            from: 0; to: 1; duration: 1400
            easing.type: Easing.OutCubic
        }

        onLineRevealChanged: requestPaint()
        onWidthChanged:  requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);
            var rv = lineReveal;
            if (rv < 0.01) return;

            var cx = hudCenterX, cy = hudCenterY;
            var R = ringRadius, oR = nodeOrbitRadius;
            var n = checkModel.count, dpv = dp;
            var dark = isDark;
            if (n === 0) return;

            for (var i = 0; i < n; i++) {
                var angle  = (i / n) * 2 * Math.PI - Math.PI / 2;
                var startX = cx + Math.cos(angle) * (R + 6 * dpv);
                var startY = cy + Math.sin(angle) * (R + 6 * dpv);
                var endX   = cx + Math.cos(angle) * (oR - 12 * dpv);
                var endY   = cy + Math.sin(angle) * (oR - 12 * dpv);

                var stagger = Math.max(0, Math.min(1, (rv * (n + 2) - i) / 2.5));
                var curX = startX + (endX - startX) * stagger;
                var curY = startY + (endY - startY) * stagger;

                var st = checkModel.get(i).checkStatus;
                var lc, la;
                if (st === "PASS")      { lc = hudSuccess; la = dark ? 0.30 : 0.40; }
                else if (st === "FAIL") { lc = hudError;   la = dark ? 0.30 : 0.40; }
                else                    { lc = hudPrimary;  la = dark ? 0.06 : 0.10; }

                ctx.beginPath();
                ctx.moveTo(startX, startY);
                ctx.lineTo(curX, curY);
                ctx.strokeStyle = Qt.rgba(lc.r, lc.g, lc.b, la * stagger);
                ctx.lineWidth = Math.max(1, 0.6 * dpv);
                ctx.stroke();

                if (stagger > 0.95) {
                    var dd = 1.5 * dpv;
                    ctx.fillStyle = Qt.rgba(lc.r, lc.g, lc.b, la * 0.7);
                    ctx.beginPath();
                    ctx.moveTo(curX, curY - dd); ctx.lineTo(curX + dd, curY);
                    ctx.lineTo(curX, curY + dd); ctx.lineTo(curX - dd, curY);
                    ctx.closePath(); ctx.fill();
                }
            }
        }
    }

    // =====================================================================
    //  CENTRE TEXT — counter during active, hidden when resolved
    // =====================================================================
    Canvas {
        id: centerTextCanvas
        x: hudCenterX - width / 2
        y: hudCenterY - height / 2
        width:  ringRadius * 1.6
        height: ringRadius * 1.4
        z: 2
        renderStrategy: Canvas.Cooperative
        visible: !(allPassed || anyFailed)

        property real textReveal: 0

        Component.onCompleted: {
            if (Settings.animationsEnabled) { textRevealAnim.start(); }
            else { textReveal = 1.0; }
        }

        NumberAnimation {
            id: textRevealAnim
            target: centerTextCanvas; property: "textReveal"
            from: 0; to: 1; duration: 800
            easing.type: Easing.OutCubic
        }

        onTextRevealChanged: requestPaint()

        Connections {
            target: bootScreen
            function onCompletedCountChanged() { centerTextCanvas.requestPaint() }
        }

        onWidthChanged:  requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);
            var rv = textReveal;
            if (rv < 0.01) return;

            ctx.globalAlpha = rv;
            var cxL = width / 2, cyL = height / 2;

            ctx.textAlign    = "center";
            ctx.textBaseline = "middle";

            var counterSize = Math.round(Math.max(18, minDim * 0.06));
            ctx.font = "300 " + counterSize + "px 'Inter','Noto Sans',sans-serif";
            ctx.fillStyle = Qt.rgba(hudPrimary.r, hudPrimary.g,
                                    hudPrimary.b, 0.85);
            ctx.fillText(completedCount + "/" + checkModel.count, cxL, cyL - 3 * dp);

            var subSize = Math.round(Math.max(6, minDim * 0.017));
            ctx.font = "500 " + subSize + "px 'Inter','Noto Sans',sans-serif";
            ctx.fillStyle = Qt.rgba(textMuted.r, textMuted.g,
                                    textMuted.b, isDark ? 0.55 : 0.70);
            ctx.fillText("PREFLIGHT", cxL, cyL + counterSize * 0.55);

            ctx.globalAlpha = 1.0;
        }
    }

    // =====================================================================
    //  GPAY-STYLE RESULT OVERLAY
    // =====================================================================
    Canvas {
        id: gpayOverlay
        anchors.fill: parent
        z: 10
        renderStrategy: Canvas.Cooperative
        visible: overlayActive

        property real circleProgress: 0
        property real fillProgress:   0
        property real iconProgress:   0
        property real labelOpacity:   0
        property real overallScale:   0.7

        readonly property real circleR: ringRadius * 0.85

        onCircleProgressChanged: requestPaint()
        onFillProgressChanged:   requestPaint()
        onIconProgressChanged:   requestPaint()
        onLabelOpacityChanged:   requestPaint()
        onOverallScaleChanged:   requestPaint()
        onWidthChanged:  if (visible) requestPaint()
        onHeightChanged: if (visible) requestPaint()

        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);

            var cx  = hudCenterX, cy = hudCenterY;
            var R   = circleR, dpv = dp;
            var sc  = overallScale;
            var isP = overlayIsPass;
            var rc  = isP ? hudSuccess : hudError;
            var dark = isDark;

            ctx.save();
            ctx.translate(cx, cy);
            ctx.scale(sc, sc);
            ctx.translate(-cx, -cy);

            // Phase 1: Circle border draw-on
            if (circleProgress > 0.001) {
                var startAng = -Math.PI / 2;
                var endAng   = startAng + 2 * Math.PI * circleProgress;
                ctx.beginPath();
                ctx.arc(cx, cy, R, startAng, endAng);
                ctx.strokeStyle = Qt.rgba(rc.r, rc.g, rc.b, dark ? 0.85 : 0.90);
                ctx.lineWidth = Math.max(2.5, 3 * dpv);
                ctx.lineCap = "round";
                ctx.stroke();
            }

            // Phase 2: Fill bloom
            if (fillProgress > 0.001) {
                var fillR = R * fillProgress;
                var fg = ctx.createRadialGradient(cx, cy, 0, cx, cy, fillR);
                var fAlpha = fillProgress * (dark ? 0.14 : 0.10);
                fg.addColorStop(0,   Qt.rgba(rc.r, rc.g, rc.b, fAlpha));
                fg.addColorStop(0.7, Qt.rgba(rc.r, rc.g, rc.b, fAlpha * 0.4));
                fg.addColorStop(1,   "transparent");
                ctx.beginPath();
                ctx.arc(cx, cy, fillR, 0, 2 * Math.PI);
                ctx.fillStyle = fg;
                ctx.fill();
            }

            // Phase 3: Icon draw-on
            if (iconProgress > 0.001) {
                ctx.strokeStyle = Qt.rgba(rc.r, rc.g, rc.b, 0.92);
                ctx.lineWidth = Math.max(2.5, 3.5 * dpv);
                ctx.lineCap  = "round";
                ctx.lineJoin = "round";

                if (isP) {
                    var ir = R * 0.42;
                    var p0x = cx - ir * 0.65, p0y = cy + ir * 0.05;
                    var p1x = cx - ir * 0.05, p1y = cy + ir * 0.55;
                    var p2x = cx + ir * 0.75, p2y = cy - ir * 0.45;

                    var seg1 = Math.min(1, iconProgress / 0.45);
                    if (seg1 > 0) {
                        ctx.beginPath();
                        ctx.moveTo(p0x, p0y);
                        ctx.lineTo(p0x + (p1x - p0x) * seg1, p0y + (p1y - p0y) * seg1);
                        ctx.stroke();
                    }
                    var seg2 = Math.max(0, Math.min(1, (iconProgress - 0.3) / 0.7));
                    if (seg2 > 0) {
                        ctx.beginPath();
                        ctx.moveTo(p1x, p1y);
                        ctx.lineTo(p1x + (p2x - p1x) * seg2, p1y + (p2y - p1y) * seg2);
                        ctx.stroke();
                    }
                } else {
                    var xm = R * 0.30;
                    var s1x = Math.min(1, iconProgress / 0.5);
                    if (s1x > 0) {
                        ctx.beginPath();
                        ctx.moveTo(cx - xm, cy - xm);
                        ctx.lineTo(cx - xm + 2 * xm * s1x, cy - xm + 2 * xm * s1x);
                        ctx.stroke();
                    }
                    var s2x = Math.max(0, Math.min(1, (iconProgress - 0.35) / 0.65));
                    if (s2x > 0) {
                        ctx.beginPath();
                        ctx.moveTo(cx + xm, cy - xm);
                        ctx.lineTo(cx + xm - 2 * xm * s2x, cy - xm + 2 * xm * s2x);
                        ctx.stroke();
                    }
                }
            }

            // Phase 4: Label below circle
            if (labelOpacity > 0.01) {
                ctx.globalAlpha = labelOpacity;
                ctx.textAlign    = "center";
                ctx.textBaseline = "middle";
                var labelSize = Math.round(Math.max(8, minDim * 0.022));
                ctx.font = "600 " + labelSize + "px 'Inter','Noto Sans',sans-serif";
                ctx.fillStyle = Qt.rgba(rc.r, rc.g, rc.b, dark ? 0.80 : 0.90);
                ctx.fillText(isP ? "ALL SYSTEMS PASS" : "FAULT DETECTED",
                             cx, cy + R + 22 * dpv);
                ctx.globalAlpha = 1.0;
            }

            ctx.restore();
        }
    }

    // ─── GPay animation sequence ───
    SequentialAnimation {
        id: gpaySequence
        NumberAnimation {
            target: gpayOverlay; property: "overallScale"
            from: 0.7; to: 1.0; duration: 350
            easing.type: Easing.OutBack; easing.overshoot: 1.3
        }
        NumberAnimation {
            target: gpayOverlay; property: "circleProgress"
            from: 0; to: 1; duration: 500
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: gpayOverlay; property: "fillProgress"
            from: 0; to: 1; duration: 300
            easing.type: Easing.OutQuad
        }
        PauseAnimation { duration: 80 }
        NumberAnimation {
            target: gpayOverlay; property: "iconProgress"
            from: 0; to: 1; duration: 400
            easing.type: Easing.OutCubic
        }
        PauseAnimation { duration: 120 }
        NumberAnimation {
            target: gpayOverlay; property: "labelOpacity"
            from: 0; to: 1; duration: 350
            easing.type: Easing.OutCubic
        }
    }

    // =====================================================================
    //  ALL NODES — consolidated Canvas
    // =====================================================================
    Canvas {
        id: nodeCanvas
        anchors.fill: parent
        z: 3
        renderStrategy: Canvas.Cooperative

        property real tick: 0
        property var bursts: []
        property var pulses: []
        property var prevStatuses: []

        property var particleData: {
            var all = [];
            for (var n = 0; n < 16; n++) {
                var pa = [];
                for (var j = 0; j < 8; j++) {
                    pa.push({
                        angle: (j / 8) * 2 * Math.PI + (Math.random() - 0.5) * 0.5,
                            speed: 0.5 + Math.random() * 0.5,
                            size:  0.5 + Math.random() * 0.5
                    });
                }
                all.push(pa);
            }
            return all;
        }

        Component.onCompleted: {
            var b = [], p = [], ps = [];
            for (var i = 0; i < checkModel.count; i++) {
                b.push({ active: false, progress: 0, isPass: true });
                p.push({ active: false, progress: 0, isPass: true });
                ps.push(checkModel.get(i).checkStatus);
            }
            bursts = b; pulses = p; prevStatuses = ps;
            requestPaint();
        }

        function checkTransitions() {
            var ps = prevStatuses;
            var newPs = [];
            for (var i = 0; i < checkModel.count; i++) {
                var cur = checkModel.get(i).checkStatus;
                newPs.push(cur);
                if (i < ps.length && ps[i] === "WAITING" && cur !== "WAITING") {
                    var isP = (cur === "PASS");
                    bursts[i] = { active: true, progress: 0, isPass: isP };
                    pulses[i] = { active: true, progress: 0, isPass: isP };
                }
            }
            prevStatuses = newPs;
        }

        Connections {
            target: hudTimer
            function onTriggered() {
                nodeCanvas.tick += 0.042;
                var b = nodeCanvas.bursts, p = nodeCanvas.pulses;
                for (var i = 0; i < b.length; i++) {
                    if (b[i].active) {
                        b[i].progress += 0.042 * 1.3;
                        if (b[i].progress >= 1) b[i].active = false;
                    }
                    if (p[i] && p[i].active) {
                        p[i].progress += 0.042 * 1.5;
                        if (p[i].progress >= 1) p[i].active = false;
                    }
                }
                nodeCanvas.checkTransitions();
                nodeCanvas.requestPaint();
            }
        }

        Connections {
            target: checkModel
            function onDataChanged() {
                nodeCanvas.checkTransitions();
                nodeCanvas.requestPaint();
            }
        }

        onWidthChanged:  requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);

            var n = checkModel.count;
            if (n === 0) return;

            var cx = hudCenterX, cy = hudCenterY;
            var oR = nodeOrbitRadius, ns = nodeSize;
            var halfNs = ns / 2, t = tick, dpv = dp;
            var dark = isDark;

            var pr = hudPrimary.r, pg = hudPrimary.g, pb = hudPrimary.b;

            for (var i = 0; i < n; i++) {
                var angle = (i / n) * 2 * Math.PI - Math.PI / 2;
                var driftX = Math.sin(t * 0.35 + i * 1.8) * 1.5 * dpv;
                var driftY = Math.cos(t * 0.3  + i * 2.2) * 1.5 * dpv;
                var nx = cx + Math.cos(angle) * oR + driftX;
                var ny = cy + Math.sin(angle) * oR + driftY;

                var st = checkModel.get(i).checkStatus;
                var r  = halfNs * 0.76;
                var lw = Math.max(1, 1.3 * dpv);

                // Pulse ring
                if (i < pulses.length && pulses[i].active) {
                    var pp     = pulses[i].progress;
                    var pAlpha = (1 - pp) * 0.40;
                    var pScale = 1 + pp * 2.0;
                    var pc     = pulses[i].isPass ? hudSuccess : hudError;
                    ctx.beginPath();
                    ctx.arc(nx, ny, r * pScale, 0, 2 * Math.PI);
                    ctx.strokeStyle = Qt.rgba(pc.r, pc.g, pc.b, pAlpha);
                    ctx.lineWidth = Math.max(1, 0.8 * dpv);
                    ctx.stroke();
                }

                // Particle burst
                if (i < bursts.length && bursts[i].active) {
                    var bp = bursts[i].progress;
                    var bc = bursts[i].isPass ? hudSuccess : hudError;
                    var pd = particleData[i];
                    if (pd) {
                        for (var pi = 0; pi < pd.length; pi++) {
                            var part = pd[pi];
                            var dist = ns * 0.85 * bp * part.speed;
                            var px = nx + Math.cos(part.angle) * dist;
                            var py = ny + Math.sin(part.angle) * dist;
                            ctx.beginPath();
                            ctx.arc(px, py, part.size * dpv, 0, 2 * Math.PI);
                            ctx.fillStyle = Qt.rgba(bc.r, bc.g, bc.b, (1 - bp) * 0.65);
                            ctx.fill();
                        }
                    }
                }

                // Node body
                if (st === "WAITING") {
                    ctx.beginPath();
                    ctx.arc(nx, ny, r, 0, 2 * Math.PI);
                    ctx.strokeStyle = Qt.rgba(pr, pg, pb, dark ? 0.09 : 0.15);
                    ctx.lineWidth = lw; ctx.stroke();

                    var spinSpd = 300 + i * 35;
                    var nSpin = (t * spinSpd) % 360;
                    var sa = (nSpin - 90) * Math.PI / 180;
                    ctx.beginPath();
                    ctx.arc(nx, ny, r, sa, sa + Math.PI * 0.6);
                    ctx.strokeStyle = Qt.rgba(pr, pg, pb, dark ? 0.32 : 0.40);
                    ctx.lineWidth = lw; ctx.lineCap = "round"; ctx.stroke();

                    var dotA = 0.12 + 0.12 * Math.sin(t * 2.2 + i * 1.1);
                    ctx.beginPath();
                    ctx.arc(nx, ny, 1.2 * dpv, 0, 2 * Math.PI);
                    ctx.fillStyle = Qt.rgba(pr, pg, pb, dotA);
                    ctx.fill();

                } else if (st === "PASS") {
                    var gc = hudSuccess;

                    // Glow halo
                    var gg = ctx.createRadialGradient(nx, ny, 0, nx, ny, r * 1.4);
                    gg.addColorStop(0,   Qt.rgba(gc.r, gc.g, gc.b, dark ? 0.10 : 0.06));
                    gg.addColorStop(0.6, Qt.rgba(gc.r, gc.g, gc.b, dark ? 0.03 : 0.02));
                    gg.addColorStop(1,   "transparent");
                    ctx.beginPath(); ctx.arc(nx, ny, r * 1.4, 0, 2 * Math.PI);
                    ctx.fillStyle = gg; ctx.fill();

                    ctx.beginPath(); ctx.arc(nx, ny, r, 0, 2 * Math.PI);
                    ctx.strokeStyle = Qt.rgba(gc.r, gc.g, gc.b, dark ? 0.40 : 0.55);
                    ctx.lineWidth = lw; ctx.stroke();

                    ctx.beginPath(); ctx.arc(nx, ny, r * 0.80, 0, 2 * Math.PI);
                    ctx.fillStyle = Qt.rgba(gc.r, gc.g, gc.b, dark ? 0.06 : 0.04);
                    ctx.fill();

                    var cr = r * 0.45;
                    ctx.strokeStyle = Qt.rgba(gc.r, gc.g, gc.b, dark ? 0.82 : 0.90);
                    ctx.lineWidth = Math.max(1.2, 1.5 * dpv);
                    ctx.lineCap = "round"; ctx.lineJoin = "round";
                    ctx.beginPath();
                    ctx.moveTo(nx - cr * 0.6,  ny + cr * 0.05);
                    ctx.lineTo(nx - cr * 0.05, ny + cr * 0.48);
                    ctx.lineTo(nx + cr * 0.6,  ny - cr * 0.38);
                    ctx.stroke();

                } else {
                    var fc = hudError;

                    var fg2 = ctx.createRadialGradient(nx, ny, 0, nx, ny, r * 1.4);
                    fg2.addColorStop(0,   Qt.rgba(fc.r, fc.g, fc.b, dark ? 0.10 : 0.06));
                    fg2.addColorStop(0.6, Qt.rgba(fc.r, fc.g, fc.b, dark ? 0.03 : 0.02));
                    fg2.addColorStop(1,   "transparent");
                    ctx.beginPath(); ctx.arc(nx, ny, r * 1.4, 0, 2 * Math.PI);
                    ctx.fillStyle = fg2; ctx.fill();

                    ctx.beginPath(); ctx.arc(nx, ny, r, 0, 2 * Math.PI);
                    ctx.strokeStyle = Qt.rgba(fc.r, fc.g, fc.b, dark ? 0.40 : 0.55);
                    ctx.lineWidth = lw; ctx.stroke();

                    ctx.beginPath(); ctx.arc(nx, ny, r * 0.80, 0, 2 * Math.PI);
                    ctx.fillStyle = Qt.rgba(fc.r, fc.g, fc.b, dark ? 0.06 : 0.04);
                    ctx.fill();

                    var xo = r * 0.32;
                    ctx.strokeStyle = Qt.rgba(fc.r, fc.g, fc.b, dark ? 0.82 : 0.90);
                    ctx.lineWidth = Math.max(1.2, 1.5 * dpv);
                    ctx.lineCap = "round";
                    ctx.beginPath();
                    ctx.moveTo(nx - xo, ny - xo); ctx.lineTo(nx + xo, ny + xo); ctx.stroke();
                    ctx.beginPath();
                    ctx.moveTo(nx + xo, ny - xo); ctx.lineTo(nx - xo, ny + xo); ctx.stroke();
                }
            }
        }
    }

    // =====================================================================
    //  NODE LABELS
    // =====================================================================
    Repeater {
        id: labelRepeater
        model: checkModel
        z: 4

        delegate: Column {
            id: labelCol

            property real angle: nodeCount > 0
            ? (index / nodeCount) * 2 * Math.PI - Math.PI / 2 : 0
            property real labelMaxW: Math.max(40, Math.min(68, minDim * 0.14))

            property bool isTop:    Math.sin(angle) < -0.3
            property bool isBottom: Math.sin(angle) > 0.3
            property bool isRight:  Math.cos(angle) > 0.3
            property bool isLeft:   Math.cos(angle) < -0.3

            property real offsetDist: nodeSize * 0.55 + 5 * dp

            x: {
                var base = hudCenterX + Math.cos(angle) * nodeOrbitRadius;
                if (isRight && !isTop && !isBottom) return base + offsetDist;
                if (isLeft  && !isTop && !isBottom) return base - offsetDist - labelMaxW;
                return base - labelMaxW / 2;
            }
            y: {
                var base = hudCenterY + Math.sin(angle) * nodeOrbitRadius;
                if (isBottom) return base + offsetDist;
                if (isTop)    return base - offsetDist - height;
                return base - height / 2;
            }

            width: labelMaxW
            spacing: 1 * dp

            opacity: viewReady ? 1.0 : 0.0
            Behavior on opacity {
                NumberAnimation {
                    duration: Settings.animationsEnabled ? 800 : 0
                    easing.type: Easing.OutCubic
                }
            }

            Text {
                width: parent.width
                text: model.checkName
                font.family: "Inter"
                font.pixelSize: Math.round(Math.max(6, minDim * 0.016))
                font.weight: Font.DemiBold
                font.letterSpacing: 1.2 * sp
                font.capitalization: Font.AllUppercase
                color: model.checkStatus === "PASS" ? hudSuccess
                : model.checkStatus === "FAIL" ? hudError
                : textMuted
                opacity: model.checkStatus === "WAITING" ? (isDark ? 0.25 : 0.40) : 0.78
                horizontalAlignment: {
                    if (labelCol.isRight && !labelCol.isTop && !labelCol.isBottom)
                        return Text.AlignLeft;
                    if (labelCol.isLeft && !labelCol.isTop && !labelCol.isBottom)
                        return Text.AlignRight;
                    return Text.AlignHCenter;
                }
                elide: Text.ElideRight
                Behavior on color   { ColorAnimation  { duration: Settings.animationsEnabled ? 400 : 0 } }
                Behavior on opacity { NumberAnimation { duration: Settings.animationsEnabled ? 400 : 0 } }
            }

            Text {
                width: parent.width
                text: model.checkLabel
                font.family: "Inter"
                font.pixelSize: Math.round(Math.max(5, minDim * 0.012))
                font.weight: Font.Normal
                color: model.checkStatus === "PASS" ? hudSuccess
                : model.checkStatus === "FAIL" ? hudError
                : textMuted
                opacity: model.checkStatus === "WAITING" ? (isDark ? 0.15 : 0.30) : 0.40
                horizontalAlignment: {
                    if (labelCol.isRight && !labelCol.isTop && !labelCol.isBottom)
                        return Text.AlignLeft;
                    if (labelCol.isLeft && !labelCol.isTop && !labelCol.isBottom)
                        return Text.AlignRight;
                    return Text.AlignHCenter;
                }
                elide: Text.ElideRight
                visible: !isTinyScreen
                Behavior on color   { ColorAnimation  { duration: Settings.animationsEnabled ? 400 : 0 } }
                Behavior on opacity { NumberAnimation { duration: Settings.animationsEnabled ? 400 : 0 } }
            }
        }
    }

    // =====================================================================
    //  FOOTER
    // =====================================================================
    Item {
        id: footerArea
        anchors.bottom: parent.bottom
        anchors.left:   parent.left
        anchors.right:  parent.right
        height: footerH
        z: 5

        opacity: viewReady ? 1.0 : 0.0
        transform: Translate {
            id: footerShift
            y: viewReady ? 0 : 20 * dp
        }

        Behavior on opacity {
            NumberAnimation {
                duration: Settings.animationsEnabled ? 600 : 0
                easing.type: Easing.OutCubic
            }
        }

        Component.onCompleted: {
            if (Settings.animationsEnabled) { footerSlide.start(); }
            else { footerShift.y = 0; }
        }

        SequentialAnimation {
            id: footerSlide
            PauseAnimation { duration: 300 }
            NumberAnimation {
                target: footerShift; property: "y"
                from: 20 * dp; to: 0
                duration: 500; easing.type: Easing.OutCubic
            }
        }

        // Top separator
        Rectangle {
            anchors.top:   parent.top
            anchors.left:  parent.left
            anchors.right: parent.right
            anchors.leftMargin:  parent.width * 0.2
            anchors.rightMargin: parent.width * 0.2
            height: Math.max(1, 0.5 * dp)
            color: isDark ? Qt.rgba(0.35, 0.45, 0.65, 0.08)
            : Qt.rgba(0.55, 0.58, 0.65, 0.15)
        }

        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Math.round(12 * dp)
            width: Math.min(parent.width - 28 * dp, 400 * dp)
            spacing: 7 * dp

            // Status subtitle
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: allPassed  ? "ALL SYSTEMS NOMINAL"
                : anyFailed  ? "REVIEW FLAGGED SUBSYSTEMS"
                : "VERIFYING SUBSYSTEMS " + completedCount + "/" + checkModel.count
                font.family: "Inter"
                font.pixelSize: Math.round(Math.max(7, minDim * 0.018))
                font.weight: Font.Medium
                font.letterSpacing: 2.2 * sp
                font.capitalization: Font.AllUppercase
                color: allPassed ? hudSuccess
                : anyFailed ? hudError
                : textMuted
                horizontalAlignment: Text.AlignHCenter
                Behavior on color { ColorAnimation { duration: Settings.animationsEnabled ? 400 : 0 } }
            }

            // Warning line
            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                width: warnRow.width + 14 * dp
                height: anyFailed ? Math.round(20 * dp) : 0
                clip: true
                visible: height > 1
                Behavior on height {
                    NumberAnimation {
                        duration: Settings.animationsEnabled ? 350 : 0
                        easing.type: Easing.OutCubic
                    }
                }

                Row {
                    id: warnRow
                    anchors.centerIn: parent
                    spacing: 5 * dp

                    Rectangle {
                        width:  Math.round(6 * dp)
                        height: Math.round(6 * dp)
                        radius: Math.round(3 * dp)
                        color: hudWarning
                        anchors.verticalCenter: parent.verticalCenter
                        opacity: 0.9
                    }

                    Text {
                        text: "SUBSYSTEM FAULT DETECTED"
                        font.family: "Inter"
                        font.pixelSize: Math.round(Math.max(6, minDim * 0.014))
                        font.weight: Font.Medium
                        font.letterSpacing: 1.2 * sp
                        color: hudWarning
                        opacity: 0.75
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            // Continue button
            Item {
                id: btnOuter
                width: parent.width
                height: Math.round(Math.max(36, minDim * 0.08))
                anchors.horizontalCenter: parent.horizontalCenter

                // Glow pulse (allPassed)
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -2 * dp
                    radius: (parent.height / 2) + 2 * dp
                    color: "transparent"
                    border.width: Math.max(1, 1.2 * dp)
                    border.color: hudSuccess
                    visible: allPassed && Settings.animationsEnabled
                    opacity: 0
                    SequentialAnimation on opacity {
                        loops: Animation.Infinite
                        running: allPassed && Settings.animationsEnabled
                        NumberAnimation { from: 0; to: isDark ? 0.16 : 0.25; duration: 1500; easing.type: Easing.InOutSine }
                        NumberAnimation { from: isDark ? 0.16 : 0.25; to: 0; duration: 1500; easing.type: Easing.InOutSine }
                    }
                }

                Rectangle {
                    id: btnRect
                    anchors.fill: parent
                    radius: height / 2

                    color: isDark
                    ? (btnMouse.pressed ? Qt.rgba(0.20, 0.24, 0.32, 0.55)
                    : Qt.rgba(0.12, 0.15, 0.22, 0.38))
                    : (btnMouse.pressed ? Qt.rgba(0.82, 0.85, 0.90, 0.90)
                    : Qt.rgba(0.90, 0.92, 0.96, 0.75))

                    border.width: Math.max(1, 0.7 * dp)
                    border.color: Qt.rgba(activeColor.r, activeColor.g, activeColor.b,
                                          btnMouse.pressed
                                          ? (isDark ? 0.35 : 0.45)
                                          : (isDark ? 0.18 : 0.28))

                    Behavior on color        { ColorAnimation  { duration: Settings.animationsEnabled ? 150 : 0 } }
                    Behavior on border.color { ColorAnimation  { duration: Settings.animationsEnabled ? 300 : 0 } }
                    Behavior on scale        { NumberAnimation { duration: Settings.animationsEnabled ? 100 : 0; easing.type: Easing.OutCubic } }

                    // Inner highlight
                    Rectangle {
                        anchors.top: parent.top
                        anchors.topMargin: Math.max(1, 0.7 * dp)
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.width * 0.6
                        height: Math.max(1, 0.5 * dp)
                        radius: height / 2
                        color: Qt.rgba(1, 1, 1, isDark ? 0.04 : 0.40)
                    }

                    Text {
                        id: btnText
                        anchors.centerIn: parent
                        text: allPassed ? "ALL CLEAR  —  CONTINUE" : "CONTINUE"
                        font.family: "Inter"
                        font.pixelSize: Math.round(Math.max(8, minDim * 0.02))
                        font.weight: Font.Medium
                        font.letterSpacing: 1.8 * sp
                        font.capitalization: Font.AllUppercase
                        color: Qt.rgba(textPrimary.r, textPrimary.g, textPrimary.b,
                                       btnMouse.pressed ? 0.6 : 0.85)
                        Behavior on color { ColorAnimation { duration: Settings.animationsEnabled ? 100 : 0 } }
                    }

                    MouseArea {
                        id: btnMouse
                        anchors.fill: parent
                        onPressed:  btnRect.scale = 0.97
                        onReleased: btnRect.scale = 1.0
                        onCanceled: btnRect.scale = 1.0
                        onClicked: {
                            console.log("Boot screen: Continue clicked → Live screen.");
                            Device.markScreenCompleted(0);
                            bottomNav.currentIndex = 1;
                            bottomNav.navTapped(1);
                        }
                    }
                }
            }
        }
    }
}
