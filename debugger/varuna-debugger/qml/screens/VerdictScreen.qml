// -----------------------------------------------------------------
// File: qml/screens/VerdictScreen.qml
// Phase: Phase 13 Step 1 (BUGFIX: merged duplicate Component.onCompleted)
// -----------------------------------------------------------------

import QtQuick
import "../components"

Item {
    id: verdictScreen

    readonly property real margin: Math.round(12 * rootWindow.dp)
    readonly property real gap: Math.round(8 * rootWindow.dp)
    readonly property bool wideMode: verdictScreen.width > Math.round(900 * rootWindow.dp)

    property bool verdictReady: false
    property bool bannerVisible: false

    // ─── FIXED: was two separate Component.onCompleted blocks (QML error:
    //     "Property value set multiple times" at line 379:5).
    //     Merged into one single handler. ───────────────────────────────────
    Component.onCompleted: {
        Device.markScreenCompleted(5);
        Device.generateVerdict();
        verdictReadyTimer.start();
        populateConfigSummary();
    }

    Timer {
        id: verdictReadyTimer
        interval: 300
        onTriggered: {
            verdictScreen.verdictReady = true;
            bannerShowTimer.start();
        }
    }

    Timer {
        id: bannerShowTimer
        interval: 200
        onTriggered: {
            verdictScreen.bannerVisible = true;
        }
    }

    Connections {
        target: Device
        function onVerdictChanged() {
            if (Device.verdictGenerated) {
                verdictScreen.verdictReady = true;
            }
        }
    }

    function verdictBannerColor() {
        var v = Device.overallVerdict;
        if (v === "DEPLOY") return Theme.secondary;
        if (v === "CAUTION") return Theme.warning;
        return Theme.error;
    }

    function verdictBannerText() {
        var v = Device.overallVerdict;
        if (v === "DEPLOY") return "DEPLOY";
        if (v === "CAUTION") return "DEPLOY WITH CAUTION";
        return "DO NOT DEPLOY";
    }

    function verdictBannerTextColor() {
        var v = Device.overallVerdict;
        if (v === "CAUTION") return "#202124";
        return "#ffffff";
    }

    Item {
        id: contentArea
        anchors.fill: parent
        anchors.margins: verdictScreen.margin

        Item {
            id: leftPanel
            anchors.top: parent.top
            anchors.left: parent.left
            width: verdictScreen.wideMode ? (parent.width - verdictScreen.gap) * 0.55 : parent.width
            height: verdictScreen.wideMode ? parent.height : (parent.height - verdictScreen.gap) * 0.6

            Rectangle {
                anchors.fill: parent
                radius: Math.round(12 * rootWindow.dp)
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                Column {
                    anchors.fill: parent
                    anchors.margins: Math.round(12 * rootWindow.dp)

                    Item {
                        width: parent.width
                        height: Math.round(36 * rootWindow.dp)

                        Text {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Deployment checklist"
                            font.family: "Noto Sans"
                            font.pixelSize: Math.round(15 * rootWindow.sp)
                            font.weight: Font.Medium
                            color: Theme.onBackground
                            Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                        }

                        Text {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            text: {
                                if (!Device.verdictGenerated) return "";
                                var checks = Device.verdictChecks;
                                var passed = 0;
                                for (var i = 0; i < checks.length; i++) {
                                    if (checks[i].result === "PASS") passed++;
                                }
                                return passed + "/" + checks.length;
                            }
                            font.family: "Noto Sans Mono"
                            font.pixelSize: Math.round(12 * rootWindow.sp)
                            color: {
                                if (!Device.verdictGenerated) return Theme.onSurfaceVariant;
                                var v = Device.overallVerdict;
                                if (v === "DEPLOY") return Theme.secondary;
                                if (v === "CAUTION") return Theme.warning;
                                return Theme.error;
                            }
                            Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color: Theme.border
                            opacity: 0.5
                        }
                    }

                    Flickable {
                        id: checkListFlick
                        width: parent.width
                        height: parent.height - Math.round(36 * rootWindow.dp) - bannerArea.height
                        contentHeight: checkListCol.height
                        flickableDirection: Flickable.VerticalFlick
                        boundsBehavior: Flickable.StopAtBounds
                        clip: true

                        Column {
                            id: checkListCol
                            width: parent.width

                            Repeater {
                                model: Device.verdictGenerated ? Device.verdictChecks.length : 0

                                CheckRow {
                                    width: checkListCol.width
                                    height: Math.round(42 * rootWindow.dp)
                                    name: Device.verdictChecks[index].name
                                    result: Device.verdictChecks[index].result
                                    measuredValue: Device.verdictChecks[index].detail

                                    opacity: verdictScreen.verdictReady ? 1.0 : 0.0

                                    Behavior on opacity {
                                        NumberAnimation {
                                            duration: Settings.animationsEnabled ? 250 : 0
                                            easing.type: Easing.OutQuad
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Item {
                        id: bannerArea
                        width: parent.width
                        height: Math.round(56 * rootWindow.dp)

                        Rectangle {
                            id: verdictBanner
                            anchors.centerIn: parent
                            width: parent.width
                            height: Math.round(48 * rootWindow.dp)
                            radius: Math.round(20 * rootWindow.dp)
                            color: verdictScreen.verdictBannerColor()
                            visible: Device.verdictGenerated

                            opacity: verdictScreen.bannerVisible ? 1.0 : 0.0
                            scale: verdictScreen.bannerVisible ? 1.0 : 0.9

                            Behavior on opacity {
                                NumberAnimation {
                                    duration: Settings.animationsEnabled ? 400 : 0
                                    easing.type: Easing.OutQuad
                                }
                            }

                            Behavior on scale {
                                NumberAnimation {
                                    duration: Settings.animationsEnabled ? 400 : 0
                                    easing.type: Easing.OutBack
                                    easing.overshoot: 1.2
                                }
                            }

                            Behavior on color {
                                ColorAnimation { duration: Theme.animDuration }
                            }

                            Text {
                                anchors.centerIn: parent
                                text: verdictScreen.verdictBannerText()
                                font.family: "Noto Sans"
                                font.pixelSize: {
                                    var v = Device.overallVerdict;
                                    if (v === "DEPLOY") return Math.round(20 * rootWindow.sp);
                                    return Math.round(18 * rootWindow.sp);
                                }
                                font.weight: Font.Bold
                                color: verdictScreen.verdictBannerTextColor()
                            }
                        }
                    }
                }
            }
        }

        Item {
            id: rightPanel
            anchors.top: verdictScreen.wideMode ? parent.top : leftPanel.bottom
            anchors.topMargin: verdictScreen.wideMode ? 0 : verdictScreen.gap
            anchors.left: verdictScreen.wideMode ? leftPanel.right : parent.left
            anchors.leftMargin: verdictScreen.wideMode ? verdictScreen.gap : 0
            anchors.right: parent.right
            anchors.bottom: parent.bottom

            Rectangle {
                anchors.fill: parent
                radius: Math.round(12 * rootWindow.dp)
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                Column {
                    anchors.fill: parent
                    anchors.margins: Math.round(12 * rootWindow.dp)

                    Item {
                        width: parent.width
                        height: Math.round(36 * rootWindow.dp)

                        Text {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Configuration summary"
                            font.family: "Noto Sans"
                            font.pixelSize: Math.round(15 * rootWindow.sp)
                            font.weight: Font.Medium
                            color: Theme.onBackground
                            Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color: Theme.border
                            opacity: 0.5
                        }
                    }

                    Flickable {
                        id: configFlick
                        width: parent.width
                        height: parent.height - Math.round(36 * rootWindow.dp) - reportBtnArea.height
                        contentHeight: configCol.height
                        flickableDirection: Flickable.VerticalFlick
                        boundsBehavior: Flickable.StopAtBounds
                        clip: true

                        Column {
                            id: configCol
                            width: parent.width

                            Repeater {
                                id: configRepeater

                                model: ListModel {
                                    id: configModel
                                }

                                Rectangle {
                                    width: configCol.width
                                    height: Math.round(26 * rootWindow.dp)
                                    color: index % 2 === 0 ? Theme.surface : Theme.surfaceVariant
                                    radius: 0

                                    Behavior on color { ColorAnimation { duration: Theme.animDuration } }

                                    Row {
                                        anchors.fill: parent
                                        anchors.leftMargin: Math.round(8 * rootWindow.dp)
                                        anchors.rightMargin: Math.round(8 * rootWindow.dp)

                                        Text {
                                            width: parent.width * 0.45
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: model.cfgLabel
                                            font.family: "Noto Sans"
                                            font.pixelSize: Math.round(11 * rootWindow.sp)
                                            color: Theme.onSurfaceVariant
                                            elide: Text.ElideRight
                                            Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                        }

                                        Text {
                                            width: parent.width * 0.55
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: model.cfgValue
                                            font.family: "Noto Sans Mono"
                                            font.pixelSize: Math.round(11 * rootWindow.sp)
                                            color: Theme.onBackground
                                            elide: Text.ElideRight
                                            horizontalAlignment: Text.AlignRight
                                            Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Item {
                        id: reportBtnArea
                        width: parent.width
                        height: Math.round(56 * rootWindow.dp)

                        ActionButton {
                            anchors.centerIn: parent
                            width: parent.width
                            height: Math.round(44 * rootWindow.dp)
                            text: "Generate deployment report"
                            filled: true
                            enabled: Device.verdictGenerated
                            buttonColor: Theme.primary

                            onClicked: {
                                console.log("VerdictScreen: Navigate to Export screen.");
                                rootWindow.navigateToScreen(6);
                            }
                        }
                    }
                }
            }
        }
    }

    function populateConfigSummary() {
        configModel.clear();
        configModel.append({ cfgLabel: "Alert threshold",    cfgValue: Device.thresholdDataReceived ? Device.thresholdAlert.toFixed(1)   + " cm" : "\u2014" });
        configModel.append({ cfgLabel: "Warning threshold",  cfgValue: Device.thresholdDataReceived ? Device.thresholdWarning.toFixed(1) + " cm" : "\u2014" });
        configModel.append({ cfgLabel: "Danger threshold",   cfgValue: Device.thresholdDataReceived ? Device.thresholdDanger.toFixed(1)  + " cm" : "\u2014" });
        configModel.append({ cfgLabel: "OLP length",         cfgValue: Device.olpLength.toFixed(1) + " cm" });
        configModel.append({ cfgLabel: "HC-SR04 mount height", cfgValue: Device.horizontalDist.toFixed(1) + " cm" });
        configModel.append({ cfgLabel: "APN",                cfgValue: "airtelgprs.com" });
        configModel.append({ cfgLabel: "Server URL",         cfgValue: "http://varuna-server.in/api..." });
        configModel.append({ cfgLabel: "GPS coordinates",    cfgValue: Device.latitude.toFixed(6) + ", " + Device.longitude.toFixed(6) });
        configModel.append({ cfgLabel: "Battery at verdict", cfgValue: Device.batteryPercent.toFixed(1) + "%" });
        configModel.append({ cfgLabel: "Timestamp (IST)",    cfgValue: Device.dateTimeString !== "" ? Device.dateTimeString : "\u2014" });
    }

    // Live refresh: update config summary whenever CSV data changes
    Connections {
        id: csvConn
        target: Device
        function onCsvDataChanged() {
            if (verdictScreen.verdictReady) {
                verdictScreen.populateConfigSummary();
            }
        }
    }
}
