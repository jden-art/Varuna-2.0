import QtQuick

Rectangle {
    id: root

    property var dataModel: []
    property var secondaryDataModel: []

    property color lineColor: Theme.primary
    property color secondaryLineColor: Theme.tertiary
    property real lineWidth: Math.max(1.8, 1.8 * rootWindow.dp)
    property string unit: ""

    property real yMin: NaN
    property real yMax: NaN

    property real thresholdValue: NaN
    property color thresholdColor: Theme.warning
    property string thresholdLabel: ""

    property string title: ""
    property string legendPrimary: ""
    property string legendSecondary: ""

    property bool rssiMode: false
    property color rssiLow: Theme.error
    property color rssiMid: Theme.warning
    property color rssiHigh: Theme.secondary

    property real samplesInView: 900
    readonly property real minSamples: 20
    readonly property real maxSamples: 3600
    readonly property int totalSamples: dataModel ? dataModel.length : 0

    property real cYMin: 0
    property real cYMax: 1

    readonly property real yAxisW: Math.round(40 * rootWindow.dp)
    readonly property real headH: Math.round(38 * rootWindow.dp)
    readonly property real dp: rootWindow.dp
    readonly property real sp: rootWindow.sp

    implicitHeight: Math.round(220 * dp)

    radius: Math.round(10 * dp)
    color: Theme.dark ? "#111318" : "#f4f5f7"
    border.width: 1
    border.color: Theme.dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.08)

    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
    Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

    readonly property color textPrimary: Theme.dark ? "#e2e4e9" : "#202124"
    readonly property color textSecondary: Theme.dark ? "#4a4f5e" : "#9aa0a6"
    readonly property color gridLine: Theme.dark ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(0, 0, 0, 0.06)
    readonly property color gridLineSub: Theme.dark ? Qt.rgba(1, 1, 1, 0.035) : Qt.rgba(0, 0, 0, 0.04)
    readonly property color dotRing: Theme.dark ? "#111318" : "#f4f5f7"
    readonly property color tagBgActive: Theme.dark ? Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.12) : Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.1)
    readonly property color tagBgInactive: Theme.dark ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(0, 0, 0, 0.04)
    readonly property color tagBorderActive: Theme.dark ? Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.4) : Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.3)
    readonly property color tagBorderInactive: Theme.dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.08)
    readonly property color tooltipBg: Theme.dark ? Qt.rgba(0.067, 0.075, 0.094, 0.95) : Qt.rgba(1, 1, 1, 0.95)
    readonly property color tooltipBorder: Theme.dark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.12)
    readonly property color zoomBtnBg: Theme.dark ? Qt.rgba(0.043, 0.047, 0.059, 0.8) : Qt.rgba(1, 1, 1, 0.85)
    readonly property color zoomBtnBorder: Theme.dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.1)
    readonly property color scrollTrack: Theme.dark ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(0, 0, 0, 0.06)
    readonly property color scrollThumbColor: Theme.dark ? Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.35) : Qt.rgba(Theme.primary.r, Theme.primary.g, Theme.primary.b, 0.4)

    function _calcContentWidth(siv, total, viewW) {
        if (viewW <= 0) return viewW;
        if (total <= siv || total <= 1) return viewW;
        return Math.max(viewW, total * (viewW / siv));
    }

    function _calcPPS(contentW, total, siv) {
        if (total <= 1) return 0;
        var pts = total <= siv ? siv - 1 : total - 1;
        if (pts <= 0) return 0;
        return contentW / pts;
    }

    Rectangle {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: root.headH
        color: "transparent"
        z: 10

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 1
            color: root.gridLine
        }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: Math.round(14 * dp)
            anchors.verticalCenter: parent.verticalCenter
            spacing: Math.round(10 * dp)

            Text {
                text: root.title
                font.family: "Noto Sans"
                font.pixelSize: Math.round(12 * sp)
                font.weight: Font.Medium
                color: root.textPrimary
            }

            Text {
                id: nowLabel
                font.family: "Noto Sans Mono"
                font.pixelSize: Math.round(11 * sp)
                color: root.lineColor
                text: "\u2014"
            }
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: Math.round(14 * dp)
            anchors.verticalCenter: parent.verticalCenter
            spacing: Math.round(10 * dp)

            Row {
                spacing: Math.round(4 * dp)
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    text: "\u2191"
                    font.family: "Noto Sans Mono"
                    font.pixelSize: Math.round(10 * sp)
                    color: root.textSecondary
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    id: hiLabel
                    font.family: "Noto Sans Mono"
                    font.pixelSize: Math.round(10 * sp)
                    color: Theme.secondary
                    text: "\u2014"
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Row {
                spacing: Math.round(4 * dp)
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    text: "\u2193"
                    font.family: "Noto Sans Mono"
                    font.pixelSize: Math.round(10 * sp)
                    color: root.textSecondary
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    id: loLabel
                    font.family: "Noto Sans Mono"
                    font.pixelSize: Math.round(10 * sp)
                    color: Theme.error
                    text: "\u2014"
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Rectangle {
                width: t15Text.implicitWidth + Math.round(16 * dp)
                height: Math.round(20 * dp)
                radius: Math.round(4 * dp)
                anchors.verticalCenter: parent.verticalCenter
                color: root.samplesInView <= 900 ? root.tagBgActive : root.tagBgInactive
                border.width: 1
                border.color: root.samplesInView <= 900 ? root.tagBorderActive : root.tagBorderInactive
                Behavior on color { ColorAnimation { duration: 150 } }
                Behavior on border.color { ColorAnimation { duration: 150 } }
                Text {
                    id: t15Text
                    anchors.centerIn: parent
                    text: "15m"
                    font.family: "Noto Sans Mono"
                    font.pixelSize: Math.round(10 * sp)
                    color: root.samplesInView <= 900 ? Theme.primary : root.textSecondary
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.samplesInView = 900;
                        flickArea.autoScroll = true;
                        root._onDataChanged();
                    }
                }
            }

            Rectangle {
                width: t60Text.implicitWidth + Math.round(16 * dp)
                height: Math.round(20 * dp)
                radius: Math.round(4 * dp)
                anchors.verticalCenter: parent.verticalCenter
                color: root.samplesInView > 900 ? root.tagBgActive : root.tagBgInactive
                border.width: 1
                border.color: root.samplesInView > 900 ? root.tagBorderActive : root.tagBorderInactive
                Behavior on color { ColorAnimation { duration: 150 } }
                Behavior on border.color { ColorAnimation { duration: 150 } }
                Text {
                    id: t60Text
                    anchors.centerIn: parent
                    text: "1h"
                    font.family: "Noto Sans Mono"
                    font.pixelSize: Math.round(10 * sp)
                    color: root.samplesInView > 900 ? Theme.primary : root.textSecondary
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.samplesInView = 3600;
                        flickArea.autoScroll = true;
                        root._onDataChanged();
                    }
                }
            }
        }
    }

    Column {
        anchors.left: parent.left
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        width: root.yAxisW
        z: 5

        Repeater {
            model: 5
            Text {
                required property int index
                width: root.yAxisW
                horizontalAlignment: Text.AlignRight
                rightPadding: Math.round(6 * dp)
                y: {
                    var totalH = root.height - root.headH;
                    var usable = totalH - Math.round(18 * dp);
                    return Math.round(6 * dp) + (usable / 4) * index - font.pixelSize / 2;
                }
                font.family: "Noto Sans Mono"
                font.pixelSize: Math.round(9 * sp)
                color: root.textSecondary
                text: {
                    var val = root.cYMax - ((root.cYMax - root.cYMin) / 4) * index;
                    return root.fmtVal(val) + root.unit;
                }
            }
        }
    }

    Rectangle {
        x: root.yAxisW
        y: root.headH
        width: 1
        height: root.height - root.headH
        color: root.gridLine
        z: 5
    }

    Item {
        id: chartBody
        anchors.left: parent.left
        anchors.leftMargin: root.yAxisW
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        clip: true

        PinchArea {
            id: pinchArea
            anchors.fill: parent
            enabled: root.totalSamples > 20

            property real startSamples: root.samplesInView

            onPinchStarted: {
                startSamples = root.samplesInView;
                longPressTimer.stop();
                dragArea.crosshairActive = false;
                crosshairCanvas.requestPaint();
                tooltip.visible = false;
            }

            onPinchUpdated: function(pinch) {
                var newSiv = Math.max(root.minSamples, Math.min(root.maxSamples, startSamples / pinch.scale));
                root.samplesInView = newSiv;
                flickArea.autoScroll = false;
                root._onDataChanged();
            }

            onPinchFinished: {
                if (flickArea.contentX >= flickArea.contentWidth - flickArea.width - 4 * dp) {
                    flickArea.autoScroll = true;
                }
            }

            MouseArea {
                id: dragArea
                anchors.fill: parent

                property bool _dragging: false
                property bool _longPressed: false
                property bool _dragMoved: false
                property real _startX: 0
                property real _startScrollX: 0
                property bool crosshairActive: false
                property int crosshairIdx: -1

                cursorShape: {
                    if (crosshairActive) return Qt.CrossCursor;
                    if (_dragging && _dragMoved) return Qt.ClosedHandCursor;
                    return Qt.OpenHandCursor;
                }

                acceptedButtons: Qt.LeftButton
                hoverEnabled: false

                onPressed: function(mouse) {
                    _dragging = true;
                    _dragMoved = false;
                    _longPressed = false;
                    _startX = mouse.x;
                    _startScrollX = flickArea.contentX;
                    longPressTimer.restart();
                }

                onPositionChanged: function(mouse) {
                    if (!_dragging) return;

                    if (_longPressed || crosshairActive) {
                        updateCrosshair(mouse.x);
                        crosshairCanvas.requestPaint();
                        return;
                    }

                    var dx = mouse.x - _startX;
                    if (Math.abs(dx) > 3 * dp) {
                        _dragMoved = true;
                        longPressTimer.stop();
                        flickArea.autoScroll = false;
                        var newX = _startScrollX - dx;
                        var maxScroll = Math.max(0, flickArea.contentWidth - flickArea.width);
                        flickArea.contentX = Math.max(0, Math.min(maxScroll, newX));
                    }
                }

                onReleased: {
                    _dragging = false;
                    longPressTimer.stop();

                    if (crosshairActive) {
                        crosshairActive = false;
                        _longPressed = false;
                        crosshairCanvas.requestPaint();
                        tooltip.visible = false;
                    }

                    var maxScroll = Math.max(0, flickArea.contentWidth - flickArea.width);
                    if (maxScroll <= 0 || flickArea.contentX >= maxScroll - 4 * dp) {
                        flickArea.autoScroll = true;
                    }
                }

                onCanceled: {
                    _dragging = false;
                    longPressTimer.stop();
                    crosshairActive = false;
                    _longPressed = false;
                    crosshairCanvas.requestPaint();
                    tooltip.visible = false;
                }

                Timer {
                    id: longPressTimer
                    interval: 300
                    onTriggered: {
                        if (dragArea._dragging && !dragArea._dragMoved) {
                            dragArea._longPressed = true;
                            dragArea.crosshairActive = true;
                            dragArea.updateCrosshair(dragArea.mouseX);
                            crosshairCanvas.requestPaint();
                        }
                    }
                }

                function updateCrosshair(mx) {
                    if (!root.dataModel || root.dataModel.length === 0) return;
                    var localX = mx + flickArea.contentX;
                    var n = root.totalSamples;
                    var pps = root._calcPPS(flickArea.contentWidth, n, root.samplesInView);
                    if (pps <= 0) return;
                    crosshairIdx = Math.max(0, Math.min(n - 1, Math.round(localX / pps)));
                }

                onWheel: function(wheel) {
                    var factor = wheel.angleDelta.y > 0 ? 0.8 : 1.25;
                    var oldSiv = root.samplesInView;
                    var newSiv = Math.max(root.minSamples, Math.min(root.maxSamples, oldSiv * factor));

                    if (Math.abs(newSiv - oldSiv) < 0.5) return;

                    var viewW = chartBody.width;
                    if (viewW <= 0) return;

                    var n = root.totalSamples;
                    var oldCW = root._calcContentWidth(oldSiv, n, viewW);
                    var newCW = root._calcContentWidth(newSiv, n, viewW);

                    var mouseViewX = wheel.x;
                    var mouseContentX = flickArea.contentX + mouseViewX;

                    var fraction = oldCW > 0 ? mouseContentX / oldCW : 0;
                    var newContentX = fraction * newCW - mouseViewX;

                    var maxScroll = Math.max(0, newCW - viewW);
                    newContentX = Math.max(0, Math.min(maxScroll, newContentX));

                    root.samplesInView = newSiv;
                    flickArea.autoScroll = false;
                    flickArea.contentX = newContentX;
                    root._onDataChanged();

                    if (maxScroll <= 0 || newContentX >= maxScroll - 4 * dp) {
                        flickArea.autoScroll = true;
                    }
                }
            }
        }

        Flickable {
            id: flickArea
            anchors.fill: parent
            contentHeight: height
            contentWidth: root._calcContentWidth(root.samplesInView, root.totalSamples, chartBody.width)
            flickableDirection: Flickable.HorizontalFlick
            boundsBehavior: Flickable.StopAtBounds
            interactive: false

            property bool autoScroll: true

            Canvas {
                id: plotCanvas
                width: flickArea.contentWidth
                height: flickArea.contentHeight
                renderStrategy: Canvas.Cooperative

                onPaint: {
                    var ctx = getContext("2d");
                    var w = width;
                    var h = height;
                    ctx.clearRect(0, 0, w, h);

                    var data = root.dataModel;
                    var n = data ? data.length : 0;

                    if (n < 2) {
                        ctx.fillStyle = root.textSecondary;
                        ctx.font = Math.round(11 * sp) + "px 'Noto Sans Mono'";
                        ctx.textAlign = "center";
                        ctx.textBaseline = "middle";
                        ctx.fillText("Waiting for data\u2026", w / 2, h / 2);
                        return;
                    }

                    var data2 = (root.secondaryDataModel && root.secondaryDataModel.length > 1) ? root.secondaryDataModel : null;
                    var mn = root.cYMin;
                    var mx = root.cYMax;
                    var yr = (mx - mn) || 1;
                    var pps = root._calcPPS(w, n, root.samplesInView);
                    var padPx = 7 * dp;

                    function toY(v) {
                        return h - ((v - mn) / yr) * (h - 2 * padPx) - padPx;
                    }

                    ctx.strokeStyle = root.gridLine;
                    ctx.lineWidth = 1;
                    for (var gi = 0; gi <= 4; gi++) {
                        var gy = Math.round(h / 4 * gi) + 0.5;
                        ctx.beginPath();
                        ctx.moveTo(0, gy);
                        ctx.lineTo(w, gy);
                        ctx.stroke();
                    }

                    var interval = root.timeInterval(root.samplesInView);
                    ctx.strokeStyle = root.gridLineSub;
                    ctx.setLineDash([2 * dp, 5 * dp]);
                    ctx.lineWidth = 1;
                    for (var ti = interval; ti < n; ti += interval) {
                        var tx = Math.round(ti * pps) + 0.5;
                        ctx.beginPath();
                        ctx.moveTo(tx, 0);
                        ctx.lineTo(tx, h - 14 * dp);
                        ctx.stroke();
                    }
                    ctx.setLineDash([]);

                    ctx.fillStyle = root.textSecondary;
                    ctx.font = Math.round(8 * sp) + "px 'Noto Sans Mono'";
                    ctx.textAlign = "center";
                    ctx.textBaseline = "bottom";
                    for (var tl = interval; tl < n; tl += interval) {
                        ctx.fillText(root.fmtTime(tl), tl * pps, h - 1 * dp);
                    }

                    if (root.rssiMode) {
                        drawRssiZones(ctx, w, h, mn, yr, toY);
                    }

                    if (!root.rssiMode) {
                        var rgb = root.hexToRgb(root.lineColor);
                        var grad = ctx.createLinearGradient(0, 0, 0, h);
                        grad.addColorStop(0, "rgba(" + rgb + ",0.18)");
                        grad.addColorStop(1, "rgba(" + rgb + ",0)");
                        ctx.fillStyle = grad;
                        ctx.beginPath();
                        ctx.moveTo(0, h);
                        for (var fi = 0; fi < n; fi++) {
                            ctx.lineTo(fi * pps, toY(data[fi]));
                        }
                        ctx.lineTo((n - 1) * pps, h);
                        ctx.closePath();
                        ctx.fill();
                    }

                    if (data2 && data2.length >= 2) {
                        ctx.strokeStyle = root.secondaryLineColor;
                        ctx.lineWidth = 1.5 * dp;
                        ctx.lineJoin = "round";
                        ctx.globalAlpha = 0.7;
                        ctx.beginPath();
                        for (var si = 0; si < data2.length; si++) {
                            if (si === 0) ctx.moveTo(si * pps, toY(data2[si]));
                            else ctx.lineTo(si * pps, toY(data2[si]));
                        }
                        ctx.stroke();
                        ctx.globalAlpha = 1.0;
                    }

                    if (root.rssiMode) {
                        drawRssiLine(ctx, data, n, pps, toY);
                    } else {
                        ctx.strokeStyle = root.lineColor;
                        ctx.lineWidth = root.lineWidth;
                        ctx.lineJoin = "round";
                        ctx.lineCap = "round";
                        ctx.beginPath();
                        for (var pi = 0; pi < n; pi++) {
                            if (pi === 0) ctx.moveTo(pi * pps, toY(data[pi]));
                            else ctx.lineTo(pi * pps, toY(data[pi]));
                        }
                        ctx.stroke();
                    }

                    if (!root.rssiMode && root.samplesInView <= 80) {
                        ctx.fillStyle = root.lineColor;
                        for (var di = 0; di < n; di++) {
                            ctx.beginPath();
                            ctx.arc(di * pps, toY(data[di]), 2.5 * dp, 0, 2 * Math.PI);
                            ctx.fill();
                        }
                    }

                    if (n >= 1 && flickArea.autoScroll) {
                        var lx = (n - 1) * pps;
                        var ly = toY(data[n - 1]);
                        var col = root.rssiMode ? root.rssiColor(data[n - 1]) : root.lineColor;
                        var rgb2 = root.hexToRgb(col);
                        ctx.beginPath();
                        ctx.arc(lx, ly, 10 * dp, 0, 2 * Math.PI);
                        ctx.fillStyle = "rgba(" + rgb2 + ",0.12)";
                        ctx.fill();
                        ctx.beginPath();
                        ctx.arc(lx, ly, 4 * dp, 0, 2 * Math.PI);
                        ctx.fillStyle = col;
                        ctx.fill();
                        ctx.strokeStyle = root.dotRing;
                        ctx.lineWidth = 1.5 * dp;
                        ctx.stroke();
                    }

                    if (!isNaN(root.thresholdValue) && root.thresholdValue >= mn && root.thresholdValue <= mx) {
                        ctx.setLineDash([6 * dp, 4 * dp]);
                        ctx.strokeStyle = root.thresholdColor;
                        ctx.lineWidth = 1 * dp;
                        ctx.beginPath();
                        ctx.moveTo(0, toY(root.thresholdValue));
                        ctx.lineTo(w, toY(root.thresholdValue));
                        ctx.stroke();
                        ctx.setLineDash([]);
                    }
                }

                function drawRssiZones(ctx, w, h, mn, yr, toY) {
                    var zones = [
                        { lo: -99, hi: 10, c: Theme.dark ? "rgba(245,91,91,0.05)" : "rgba(217,48,37,0.05)" },
                        { lo: 10, hi: 15, c: Theme.dark ? "rgba(245,166,35,0.05)" : "rgba(249,171,0,0.05)" },
                        { lo: 15, hi: 99, c: Theme.dark ? "rgba(62,207,109,0.04)" : "rgba(30,142,62,0.04)" }
                    ];
                    for (var zi = 0; zi < zones.length; zi++) {
                        var z = zones[zi];
                        var y1 = Math.max(0, toY(Math.min(z.hi, root.cYMax)));
                        var y2 = Math.min(h, toY(Math.max(z.lo, root.cYMin)));
                        if (y2 > y1) {
                            ctx.fillStyle = z.c;
                            ctx.fillRect(0, y1, w, y2 - y1);
                        }
                    }
                    ctx.setLineDash([2 * dp, 4 * dp]);
                    ctx.lineWidth = 0.5;
                    var thresholds = [10, 15];
                    for (var rti = 0; rti < thresholds.length; rti++) {
                        if (thresholds[rti] > root.cYMin && thresholds[rti] < root.cYMax) {
                            ctx.strokeStyle = Theme.dark ? "rgba(255,255,255,0.07)" : "rgba(0,0,0,0.08)";
                            ctx.beginPath();
                            ctx.moveTo(0, toY(thresholds[rti]));
                            ctx.lineTo(w, toY(thresholds[rti]));
                            ctx.stroke();
                        }
                    }
                    ctx.setLineDash([]);
                }

                function drawRssiLine(ctx, data, n, pps, toY) {
                    ctx.lineWidth = root.lineWidth;
                    ctx.lineJoin = "round";
                    ctx.lineCap = "round";
                    for (var i = 1; i < n; i++) {
                        ctx.strokeStyle = root.rssiColor((data[i - 1] + data[i]) / 2);
                        ctx.beginPath();
                        ctx.moveTo((i - 1) * pps, toY(data[i - 1]));
                        ctx.lineTo(i * pps, toY(data[i]));
                        ctx.stroke();
                    }
                }
            }
        }

        Canvas {
            id: crosshairCanvas
            anchors.fill: parent
            renderStrategy: Canvas.Cooperative
            z: 10

            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);

                if (!dragArea.crosshairActive || dragArea.crosshairIdx < 0) {
                    tooltip.visible = false;
                    return;
                }

                var data = root.dataModel;
                if (!data || data.length === 0) return;

                var n = data.length;
                var idx = dragArea.crosshairIdx;
                if (idx >= n) return;

                var pps = root._calcPPS(flickArea.contentWidth, n, root.samplesInView);
                var xView = idx * pps - flickArea.contentX;

                if (xView < -10 * dp || xView > width + 10 * dp) {
                    tooltip.visible = false;
                    return;
                }

                var mn = root.cYMin;
                var mx = root.cYMax;
                var yr = (mx - mn) || 1;
                var padPx = 7 * dp;
                var val = data[idx];
                var yPos = height - ((val - mn) / yr) * (height - 2 * padPx) - padPx;

                ctx.strokeStyle = Theme.dark ? "rgba(255,255,255,0.15)" : "rgba(0,0,0,0.12)";
                ctx.lineWidth = 1;
                ctx.setLineDash([3 * dp, 4 * dp]);
                ctx.beginPath();
                ctx.moveTo(xView, 0);
                ctx.lineTo(xView, height);
                ctx.stroke();
                ctx.beginPath();
                ctx.moveTo(0, yPos);
                ctx.lineTo(width, yPos);
                ctx.stroke();
                ctx.setLineDash([]);

                var col = root.rssiMode ? root.rssiColor(val) : root.lineColor;
                ctx.beginPath();
                ctx.arc(xView, yPos, 5 * dp, 0, 2 * Math.PI);
                ctx.fillStyle = col;
                ctx.fill();
                ctx.strokeStyle = root.dotRing;
                ctx.lineWidth = 1.5 * dp;
                ctx.stroke();

                tooltip.valueText = root.fmtVal(val) + root.unit;
                tooltip.timeText = "t\u2212" + root.fmtTime(n - 1 - idx);
                if (root.secondaryDataModel && idx < root.secondaryDataModel.length) {
                    tooltip.secondaryText = "\u2192 " + root.fmtVal(root.secondaryDataModel[idx]) + root.unit;
                } else {
                    tooltip.secondaryText = "";
                }

                var tipX = xView + 12 * dp;
                var tipY = yPos - 8 * dp;
                if (tipX + tooltip.width > width - 10 * dp) {
                    tipX = xView - tooltip.width - 10 * dp;
                }
                if (tipY < 4 * dp) tipY = 4 * dp;
                if (tipY + tooltip.height > height - 4 * dp) {
                    tipY = height - tooltip.height - 4 * dp;
                }
                tooltip.x = tipX;
                tooltip.y = tipY;
                tooltip.visible = true;
            }
        }

        Rectangle {
            id: tooltip
            visible: false
            z: 20
            width: tipCol.implicitWidth + Math.round(20 * dp)
            height: tipCol.implicitHeight + Math.round(14 * dp)
            radius: Math.round(6 * dp)
            color: root.tooltipBg
            border.width: 1
            border.color: root.tooltipBorder

            property string valueText: ""
            property string secondaryText: ""
            property string timeText: ""

            Column {
                id: tipCol
                anchors.centerIn: parent
                spacing: Math.round(1 * dp)

                Text {
                    text: tooltip.valueText
                    font.family: "Noto Sans Mono"
                    font.pixelSize: Math.round(13 * sp)
                    font.weight: Font.Medium
                    color: root.textPrimary
                }
                Text {
                    text: tooltip.secondaryText
                    font.family: "Noto Sans Mono"
                    font.pixelSize: Math.round(11 * sp)
                    color: root.secondaryLineColor
                    visible: text !== ""
                }
                Text {
                    text: tooltip.timeText
                    font.family: "Noto Sans Mono"
                    font.pixelSize: Math.round(10 * sp)
                    color: root.textSecondary
                    topPadding: Math.round(3 * dp)
                }
            }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Math.round(3 * dp)
            anchors.left: parent.left
            anchors.right: parent.right
            height: Math.round(2 * dp)
            radius: height / 2
            color: root.scrollTrack
            z: 8
            visible: flickArea.contentWidth > flickArea.width + 4

            Rectangle {
                height: parent.height
                radius: height / 2
                color: root.scrollThumbColor
                x: {
                    if (flickArea.contentWidth <= flickArea.width) return 0;
                    var tw = Math.max(24 * dp, parent.width * (flickArea.width / flickArea.contentWidth));
                    var scrollRange = flickArea.contentWidth - flickArea.width;
                    if (scrollRange <= 0) return 0;
                    return (flickArea.contentX / scrollRange) * (parent.width - tw);
                }
                width: {
                    if (flickArea.contentWidth <= flickArea.width) return parent.width;
                    return Math.max(24 * dp, parent.width * (flickArea.width / flickArea.contentWidth));
                }
            }
        }

        Row {
            anchors.top: parent.top
            anchors.topMargin: Math.round(7 * dp)
            anchors.right: parent.right
            anchors.rightMargin: Math.round(8 * dp)
            spacing: Math.round(3 * dp)
            z: 15

            Repeater {
                model: [
                    { label: "+", action: "zi" },
                    { label: "\u2212", action: "zo" },
                    { label: "\u27F7", action: "fit" }
                ]

                Rectangle {
                    required property var modelData
                    required property int index
                    width: Math.round(22 * dp)
                    height: width
                    radius: Math.round(5 * dp)
                    color: zma.pressed ? root.tagBgActive : root.zoomBtnBg
                    border.width: 1
                    border.color: zma.pressed ? Theme.primary : root.zoomBtnBorder
                    Behavior on color { ColorAnimation { duration: 120 } }

                    Text {
                        anchors.centerIn: parent
                        text: modelData.label
                        font.family: "Noto Sans Mono"
                        font.pixelSize: modelData.action === "fit" ? Math.round(10 * sp) : Math.round(13 * sp)
                        color: zma.pressed ? Theme.primary : root.textSecondary
                    }

                    MouseArea {
                        id: zma
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (modelData.action === "zi") root.zoomIn();
                            else if (modelData.action === "zo") root.zoomOut();
                            else root.fitAll();
                        }
                    }
                }
            }
        }

        Row {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Math.round(20 * dp)
            anchors.left: parent.left
            anchors.leftMargin: Math.round(6 * dp)
            spacing: Math.round(10 * dp)
            z: 8
            visible: root.legendPrimary !== "" || root.legendSecondary !== "" || root.rssiMode

            Repeater {
                model: root.rssiMode ? [
                    { label: "<10", clr: root.rssiLow },
                    { label: "10\u201315", clr: root.rssiMid },
                    { label: ">15", clr: root.rssiHigh }
                ] : []

                Row {
                    required property var modelData
                    spacing: Math.round(4 * dp)

                    Rectangle {
                        width: Math.round(6 * dp)
                        height: width
                        radius: width / 2
                        color: modelData.clr
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: modelData.label
                        font.family: "Noto Sans Mono"
                        font.pixelSize: Math.round(9 * sp)
                        color: root.textSecondary
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            Row {
                spacing: Math.round(4 * dp)
                visible: !root.rssiMode && root.legendPrimary !== ""

                Rectangle {
                    width: Math.round(12 * dp)
                    height: Math.round(2 * dp)
                    radius: 1
                    color: root.lineColor
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: root.legendPrimary
                    font.family: "Noto Sans Mono"
                    font.pixelSize: Math.round(9 * sp)
                    color: root.textSecondary
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Row {
                spacing: Math.round(4 * dp)
                visible: !root.rssiMode && root.legendSecondary !== ""

                Rectangle {
                    width: Math.round(12 * dp)
                    height: Math.round(2 * dp)
                    radius: 1
                    color: root.secondaryLineColor
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: root.legendSecondary
                    font.family: "Noto Sans Mono"
                    font.pixelSize: Math.round(9 * sp)
                    color: root.textSecondary
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }

    Connections {
        target: Device
        function onChartDataUpdated() {
            root._onDataChanged();
        }
    }

    function _onDataChanged() {
        computeYRange();
        plotCanvas.requestPaint();
        updateNowStats();
        if (flickArea.autoScroll) {
            var maxScroll = Math.max(0, flickArea.contentWidth - flickArea.width);
            if (maxScroll > 0) {
                flickArea.contentX = maxScroll;
            }
        }
    }

    function computeYRange() {
        var data = root.dataModel;
        if (!data || data.length === 0) {
            root.cYMin = 0;
            root.cYMax = 1;
            return;
        }
        var mn = isNaN(root.yMin) ? Infinity : root.yMin;
        var mx = isNaN(root.yMax) ? -Infinity : root.yMax;
        if (isNaN(root.yMin) || isNaN(root.yMax)) {
            for (var i = 0; i < data.length; i++) {
                if (data[i] < mn) mn = data[i];
                if (data[i] > mx) mx = data[i];
            }
            var data2 = root.secondaryDataModel;
            if (data2) {
                for (var j = 0; j < data2.length; j++) {
                    if (data2[j] < mn) mn = data2[j];
                    if (data2[j] > mx) mx = data2[j];
                }
            }
            var rng = mx - mn;
            if (rng < 0.001) rng = 1;
            var pad = rng * 0.1;
            if (isNaN(root.yMin)) mn -= pad;
            if (isNaN(root.yMax)) mx += pad;
        }
        root.cYMin = mn;
        root.cYMax = mx;
    }

    function updateNowStats() {
        var data = root.dataModel;
        if (!data || data.length === 0) {
            nowLabel.text = "\u2014";
            hiLabel.text = "\u2014";
            loLabel.text = "\u2014";
            return;
        }
        var last = data[data.length - 1];
        var mn = data[0];
        var mx = data[0];
        for (var i = 1; i < data.length; i++) {
            if (data[i] < mn) mn = data[i];
            if (data[i] > mx) mx = data[i];
        }
        nowLabel.text = fmtVal(last) + root.unit;
        hiLabel.text = fmtVal(mx) + root.unit;
        loLabel.text = fmtVal(mn) + root.unit;
    }

    function fmtVal(v) {
        if (isNaN(v)) return "\u2014";
        var a = Math.abs(v);
        if (a >= 100) return v.toFixed(0);
        if (a >= 10) return v.toFixed(1);
        return v.toFixed(2);
    }

    function fmtTime(sec) {
        if (sec < 0) sec = 0;
        if (sec < 60) return sec + "s";
        var m = Math.floor(sec / 60);
        var s = sec % 60;
        if (m < 60) return m + "m" + (s ? (s < 10 ? "0" : "") + s + "s" : "");
        return Math.floor(m / 60) + "h" + (m % 60 ? (m % 60) + "m" : "");
    }

    function timeInterval(v) {
        if (v <= 60) return 10;
        if (v <= 120) return 30;
        if (v <= 300) return 60;
        if (v <= 900) return 300;
        if (v <= 1800) return 600;
        return 1200;
    }

    function rssiColor(v) {
        if (v >= 15) return Theme.secondary;
        if (v >= 10) return Theme.warning;
        return Theme.error;
    }

    function hexToRgb(hex) {
        if (typeof hex !== "string") {
            return Math.round(hex.r * 255) + "," + Math.round(hex.g * 255) + "," + Math.round(hex.b * 255);
        }
        var result = /^#?([a-fA-F0-9]{2})([a-fA-F0-9]{2})([a-fA-F0-9]{2})/.exec(hex);
        if (result) {
            return parseInt(result[1], 16) + "," + parseInt(result[2], 16) + "," + parseInt(result[3], 16);
        }
        return "91,140,245";
    }

    function zoomIn() {
        var newSiv = Math.max(minSamples, samplesInView / 1.5);
        samplesInView = newSiv;
        flickArea.autoScroll = false;
        _onDataChanged();
        var maxScroll = Math.max(0, flickArea.contentWidth - flickArea.width);
        if (maxScroll <= 0 || flickArea.contentX >= maxScroll - 4 * dp) {
            flickArea.autoScroll = true;
        }
    }

    function zoomOut() {
        var newSiv = Math.min(maxSamples, samplesInView * 1.5);
        samplesInView = newSiv;
        _onDataChanged();
        var maxScroll = Math.max(0, flickArea.contentWidth - flickArea.width);
        if (maxScroll > 0) {
            flickArea.contentX = Math.min(flickArea.contentX, maxScroll);
        }
        if (maxScroll <= 0 || flickArea.contentX >= maxScroll - 4 * dp) {
            flickArea.autoScroll = true;
        }
    }

    function fitAll() {
        samplesInView = Math.max(minSamples, totalSamples);
        flickArea.autoScroll = true;
        flickArea.contentX = 0;
        _onDataChanged();
    }

    onDataModelChanged: _onDataChanged()
    onSecondaryDataModelChanged: _onDataChanged()
    Component.onCompleted: _onDataChanged()
}
