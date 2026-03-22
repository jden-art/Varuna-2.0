// -----------------------------------------------------------------
// File: qml/screens/LiveScreen.qml
// Phase: Phase 7 Step 3
// -----------------------------------------------------------------

import QtQuick
import "../components"

Item {
    id: liveScreen

    readonly property real margin: Math.round(12 * rootWindow.dp)
    readonly property real gap: Math.round(8 * rootWindow.dp)
    readonly property bool wideMode: liveScreen.width > Math.round(900 * rootWindow.dp)

    readonly property int gridCols: wideMode ? 4 : 2
    readonly property int gridRows: wideMode ? 2 : 4
    readonly property real cardW: (scrollContent.width - 2 * margin - (gridCols - 1) * gap) / gridCols
    readonly property real cardH: Math.round(Math.max(80 * rootWindow.dp, 100 * rootWindow.dp))
    readonly property real chartH: Math.round(220 * rootWindow.dp)

    Component.onCompleted: {
        Device.markScreenCompleted(1);
    }

    Flickable {
        id: mainFlick
        anchors.fill: parent
        contentWidth: parent.width
        contentHeight: scrollContent.height
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Item {
            id: scrollContent
            width: mainFlick.width
            height: cardsGrid.height + chartsCol.height + margin * 3 + gap

            Grid {
                id: cardsGrid
                x: liveScreen.margin
                y: liveScreen.margin
                width: parent.width - 2 * liveScreen.margin
                columns: liveScreen.gridCols
                spacing: liveScreen.gap

                Item {
                    width: liveScreen.cardW; height: liveScreen.cardH
                    ValueCard {
                        anchors.fill: parent; label: "Water height"; value: Device.waterHeight.toFixed(1); unit: "cm"; statusColor: Theme.onBackground
                        subLabel: { var t = Device.waterHeightTrend; if (t === "rising") return "\u25B2 Rising"; if (t === "falling") return "\u25BC Falling"; return "\u2500 Stable"; }
                    }
                    Item {
                        anchors.right: parent.right; anchors.top: parent.top; anchors.rightMargin: Math.round(10 * rootWindow.dp); anchors.topMargin: Math.round(8 * rootWindow.dp)
                        width: Math.round(24 * rootWindow.dp); height: Math.round(24 * rootWindow.dp)
                        Canvas {
                            anchors.fill: parent
                            property string trend: Device.waterHeightTrend; property color upColor: Theme.error; property color downColor: Theme.primary; property color stableColor: Theme.onSurfaceVariant
                            onTrendChanged: requestPaint(); onUpColorChanged: requestPaint(); onDownColorChanged: requestPaint(); onStableColorChanged: requestPaint(); Component.onCompleted: requestPaint()
                            onPaint: {
                                var ctx = getContext("2d"); ctx.clearRect(0, 0, width, height);
                                var cx = width / 2; var cy = height / 2; var lw = Math.max(2, rootWindow.dp * 2.5); ctx.lineCap = "round"; ctx.lineJoin = "round";
                                if (trend === "rising") { ctx.fillStyle = upColor; ctx.beginPath(); ctx.moveTo(cx, cy - 7 * rootWindow.dp); ctx.lineTo(cx + 6 * rootWindow.dp, cy + 3 * rootWindow.dp); ctx.lineTo(cx - 6 * rootWindow.dp, cy + 3 * rootWindow.dp); ctx.closePath(); ctx.fill(); ctx.strokeStyle = upColor; ctx.lineWidth = lw; ctx.beginPath(); ctx.moveTo(cx, cy + 3 * rootWindow.dp); ctx.lineTo(cx, cy + 8 * rootWindow.dp); ctx.stroke(); }
                                else if (trend === "falling") { ctx.fillStyle = downColor; ctx.beginPath(); ctx.moveTo(cx, cy + 7 * rootWindow.dp); ctx.lineTo(cx + 6 * rootWindow.dp, cy - 3 * rootWindow.dp); ctx.lineTo(cx - 6 * rootWindow.dp, cy - 3 * rootWindow.dp); ctx.closePath(); ctx.fill(); ctx.strokeStyle = downColor; ctx.lineWidth = lw; ctx.beginPath(); ctx.moveTo(cx, cy - 3 * rootWindow.dp); ctx.lineTo(cx, cy - 8 * rootWindow.dp); ctx.stroke(); }
                                else { ctx.strokeStyle = stableColor; ctx.lineWidth = lw; ctx.beginPath(); ctx.moveTo(cx - 7 * rootWindow.dp, cy); ctx.lineTo(cx + 7 * rootWindow.dp, cy); ctx.stroke(); }
                            }
                        }
                    }
                }

                ValueCard { width: liveScreen.cardW; height: liveScreen.cardH; label: "Tilt angle"; value: Device.theta.toFixed(1); unit: "\u00B0"; statusColor: Theme.onBackground; subLabel: "X: " + Device.correctedTiltX.toFixed(1) + "\u00B0  Y: " + Device.correctedTiltY.toFixed(1) + "\u00B0" }

                ValueCard {
                    width: liveScreen.cardW; height: liveScreen.cardH; label: "Battery"; value: Device.batteryPercent.toFixed(1); unit: "%"
                    statusColor: Device.batteryPercent > 80 ? Theme.secondary : (Device.batteryPercent > 20 ? Theme.warning : Theme.error)
                    showBottomBar: true; bottomBarPercent: Device.batteryPercent / 100
                    bottomBarColor: Device.batteryPercent > 80 ? Theme.secondary : (Device.batteryPercent > 20 ? Theme.warning : Theme.error)
                }

                ValueCard {
                    width: liveScreen.cardW; height: liveScreen.cardH; label: "SIM RSSI"; value: Device.simSignalRSSI + ""; unit: ""
                    statusColor: Device.simSignalRSSI >= 20 ? Theme.secondary : (Device.simSignalRSSI >= 10 ? Theme.warning : Theme.error)
                    subLabel: Device.rssiQuality
                    Row {
                        anchors.right: parent.right; anchors.top: parent.top; anchors.rightMargin: Math.round(12 * rootWindow.dp); anchors.topMargin: Math.round(10 * rootWindow.dp)
                        spacing: Math.round(2 * rootWindow.dp); height: Math.round(16 * rootWindow.dp)
                        Repeater {
                            model: 5
                            Item { width: Math.round(3 * rootWindow.dp); height: Math.round(16 * rootWindow.dp)
                                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: Math.round((3 + index * 3) * rootWindow.dp); radius: 1
                                    property int rv: Device.simSignalRSSI; property var th: [1, 7, 13, 20, 26]; property bool lit: rv >= th[index]
                                    color: !lit ? Theme.onSurfaceVariant : (rv >= 20 ? Theme.secondary : (rv >= 10 ? Theme.warning : Theme.error)); opacity: lit ? 1.0 : 0.25
                                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                }
                            }
                        }
                    }
                }

                ValueCard { width: liveScreen.cardW; height: liveScreen.cardH; label: "Pressure"; value: Device.currentPressure.toFixed(1); unit: "hPa"; statusColor: Theme.onBackground; subLabel: "\u0394 " + Device.pressureDeviation.toFixed(2) + " hPa" }
                ValueCard { width: liveScreen.cardW; height: liveScreen.cardH; label: "Temperature"; value: Device.currentTemperature.toFixed(1); unit: "\u00B0C"; statusColor: Theme.onBackground; subLabel: "BMP280 " + (Device.bmpAvailable ? "active" : "offline") }

                ValueCard {
                    id: gpsCard; width: liveScreen.cardW; height: liveScreen.cardH; label: "GPS"; value: ""; unit: ""; statusColor: Theme.onBackground; subLabel: Device.gpsCoordinates
                    Text { anchors.centerIn: parent; anchors.verticalCenterOffset: Math.round(2 * rootWindow.dp); text: Device.gpsFixValid ? ("FIX \u2014 " + Device.gpsSatellites + " sats") : ("NO FIX \u2014 " + Device.gpsSatellites + " sats"); font.family: "Noto Sans"; font.pixelSize: Math.round(17 * rootWindow.sp); font.weight: Font.DemiBold; color: Device.gpsFixValid ? Theme.secondary : Theme.warning; Behavior on color { ColorAnimation { duration: Theme.animDuration } } }
                    Item {
                        anchors.right: parent.right; anchors.top: parent.top; anchors.rightMargin: Math.round(10 * rootWindow.dp); anchors.topMargin: Math.round(8 * rootWindow.dp); width: Math.round(20 * rootWindow.dp); height: Math.round(20 * rootWindow.dp)
                        Rectangle { anchors.centerIn: parent; width: Math.round(14 * rootWindow.dp); height: Math.round(14 * rootWindow.dp); radius: width / 2; color: "transparent"; border.width: Math.round(1.5 * rootWindow.dp); border.color: Device.gpsFixValid ? Theme.secondary : Theme.onSurfaceVariant; Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }
                        Rectangle { anchors.centerIn: parent; width: Math.round(4 * rootWindow.dp); height: Math.round(4 * rootWindow.dp); radius: width / 2; color: Device.gpsFixValid ? Theme.secondary : Theme.onSurfaceVariant; Behavior on color { ColorAnimation { duration: Theme.animDuration } } }
                        }
                    }
                }
            }

            Column {
                id: chartsCol
                x: liveScreen.margin; y: cardsGrid.y + cardsGrid.height + liveScreen.gap
                width: parent.width - 2 * liveScreen.margin
                spacing: Math.round(8 * rootWindow.dp)

                MiniChart {
                    width: parent.width; height: liveScreen.chartH
                    dataModel: Device.chartWaterHeight; lineColor: Theme.primary
                    title: "Water Height"; unit: "cm"
                    thresholdValue: 60; thresholdColor: Theme.warning
                }

                MiniChart {
                    width: parent.width; height: liveScreen.chartH
                    dataModel: Device.chartTiltX; lineColor: "#06b6d4"
                    secondaryDataModel: Device.chartTiltY; secondaryLineColor: Theme.tertiary
                    title: "Tilt"; unit: "\u00B0"
                    legendPrimary: "Tilt X"; legendSecondary: "Tilt Y"
                }

                MiniChart {
                    width: parent.width; height: liveScreen.chartH
                    dataModel: Device.chartRSSI; lineColor: Theme.secondary
                    title: "RSSI"; unit: ""
                    yMin: 0; yMax: 31
                    rssiMode: true; rssiLow: Theme.error; rssiMid: Theme.warning; rssiHigh: Theme.secondary
                }
            }
        }
    }
}

// -----------------------------------------------------------------
// File: qml/screens/LiveScreen.qml
// Phase: Phase 7 Step 3
// ----------------------------END----------------------------------
