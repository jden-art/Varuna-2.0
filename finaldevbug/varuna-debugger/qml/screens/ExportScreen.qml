import QtQuick
import "../components"

Item {
    id: exportScreen

    readonly property real margin: Math.round(12 * rootWindow.dp)
    readonly property real gap: Math.round(8 * rootWindow.dp)

    property string engineerName: ""
    property string engineerId: ""

    property bool exporting: false
    property bool exportDone: false
    property string pdfPath: ""
    property string jsonPath: ""
    property string qrBase64: ""
    property bool uploadAttempted: false
    property bool uploadSuccess: false

    readonly property bool canExport: engineerName.trim().length > 0 && engineerId.trim().length > 0 && !exporting

    readonly property bool sessionIncomplete: {
        var sc = Device.screensCompleted;
        if (!sc || sc.length < 7) return true;
        for (var i = 1; i <= 4; i++) {
            if (!sc[i]) return true;
        }
        return false;
    }

    readonly property var missingScreens: {
        var names = ["Boot", "Live", "Cal", "Conn", "Config", "Verdict", "Export"];
        var missing = [];
        var sc = Device.screensCompleted;
        if (!sc || sc.length < 7) return ["Live", "Cal", "Conn", "Config"];
        for (var i = 1; i <= 4; i++) {
            if (!sc[i]) missing.push(names[i]);
        }
        return missing;
    }

    Component.onCompleted: {
        Device.markScreenCompleted(6);
    }

    Connections {
        target: Exporter

        function onExportStarted() {
            exportScreen.exporting = true;
            exportScreen.exportDone = false;
            console.log("ExportScreen: Export started.");
        }

        function onExportComplete(pdf, jsonFile, qr) {
            exportScreen.exporting = false;
            exportScreen.exportDone = true;
            exportScreen.pdfPath = pdf;
            exportScreen.jsonPath = jsonFile;
            exportScreen.qrBase64 = qr;
            rootWindow.showToast("Report exported successfully", Theme.secondary);
            console.log("ExportScreen: Export complete. PDF=" + pdf + " JSON=" + jsonFile);
        }

        function onExportFailed(reason) {
            exportScreen.exporting = false;
            exportScreen.exportDone = false;
            rootWindow.showToast("Export failed \u2014 " + reason, Theme.error);
            console.log("ExportScreen: Export failed: " + reason);
        }
    }

    Flickable {
        id: mainFlick
        anchors.fill: parent
        anchors.margins: exportScreen.margin
        contentHeight: mainCol.height
        contentWidth: width
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Column {
            id: mainCol
            width: mainFlick.width
            spacing: exportScreen.gap

            Rectangle {
                width: parent.width
                height: warningCol.height + Math.round(24 * rootWindow.dp)
                radius: Math.round(12 * rootWindow.dp)
                color: Theme.surface
                border.width: 1
                border.color: Theme.warning
                visible: exportScreen.sessionIncomplete

                Behavior on color { ColorAnimation { duration: Theme.animDuration } }

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: Math.round(4 * rootWindow.dp)
                    color: Theme.warning
                    radius: Math.round(12 * rootWindow.dp)

                    Rectangle {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: Math.round(2 * rootWindow.dp)
                        color: Theme.warning
                    }
                }

                Column {
                    id: warningCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Math.round(12 * rootWindow.dp)
                    anchors.leftMargin: Math.round(20 * rootWindow.dp)
                    spacing: Math.round(6 * rootWindow.dp)

                    Text {
                        text: "Incomplete session"
                        font.family: "Noto Sans"
                        font.pixelSize: Math.round(14 * rootWindow.sp)
                        font.weight: Font.Medium
                        color: Theme.warning
                        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                    }

                    Text {
                        width: parent.width
                        text: "The following screens have not been visited:"
                        font.family: "Noto Sans"
                        font.pixelSize: Math.round(12 * rootWindow.sp)
                        color: Theme.onSurfaceVariant
                        wrapMode: Text.WordWrap
                        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                    }

                    Repeater {
                        model: exportScreen.missingScreens

                        Row {
                            spacing: Math.round(6 * rootWindow.dp)

                            Rectangle {
                                width: Math.round(6 * rootWindow.dp)
                                height: Math.round(6 * rootWindow.dp)
                                radius: width / 2
                                color: Theme.warning
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Text {
                                text: modelData
                                font.family: "Noto Sans"
                                font.pixelSize: Math.round(12 * rootWindow.sp)
                                color: Theme.onBackground
                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                            }
                        }
                    }

                    Text {
                        width: parent.width
                        text: "You can still export, but the report may be incomplete."
                        font.family: "Noto Sans"
                        font.pixelSize: Math.round(11 * rootWindow.sp)
                        color: Theme.onSurfaceVariant
                        wrapMode: Text.WordWrap
                        topPadding: Math.round(2 * rootWindow.dp)
                        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: formCol.height + Math.round(24 * rootWindow.dp)
                radius: Math.round(12 * rootWindow.dp)
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                Column {
                    id: formCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Math.round(12 * rootWindow.dp)
                    spacing: Math.round(12 * rootWindow.dp)

                    Text {
                        text: "Engineer details"
                        font.family: "Noto Sans"
                        font.pixelSize: Math.round(15 * rootWindow.sp)
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

                    Column {
                        width: parent.width
                        spacing: Math.round(4 * rootWindow.dp)

                        Text {
                            text: "Engineer name"
                            font.family: "Noto Sans"
                            font.pixelSize: Math.round(12 * rootWindow.sp)
                            color: Theme.onSurfaceVariant
                            Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                        }

                        Rectangle {
                            width: parent.width
                            height: Math.round(48 * rootWindow.dp)
                            radius: Math.round(8 * rootWindow.dp)
                            color: Theme.surfaceVariant
                            border.width: 1
                            border.color: Theme.border

                            Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                            Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: Math.round(12 * rootWindow.dp)
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
                                anchors.rightMargin: Math.round(12 * rootWindow.dp)
                                text: exportScreen.engineerName.length > 0 ? exportScreen.engineerName : "Tap to enter name"
                                font.family: "Noto Sans"
                                font.pixelSize: Math.round(15 * rootWindow.sp)
                                color: exportScreen.engineerName.length > 0 ? Theme.onBackground : Theme.onSurfaceVariant
                                elide: Text.ElideRight
                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                enabled: !exportScreen.exporting
                                onClicked: {
                                    textKeypad.open("name", exportScreen.engineerName);
                                }
                            }
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: Math.round(4 * rootWindow.dp)

                        Text {
                            text: "Engineer ID / Badge number"
                            font.family: "Noto Sans"
                            font.pixelSize: Math.round(12 * rootWindow.sp)
                            color: Theme.onSurfaceVariant
                            Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                        }

                        Rectangle {
                            width: parent.width
                            height: Math.round(48 * rootWindow.dp)
                            radius: Math.round(8 * rootWindow.dp)
                            color: Theme.surfaceVariant
                            border.width: 1
                            border.color: Theme.border

                            Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                            Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: Math.round(12 * rootWindow.dp)
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.right: parent.right
                                anchors.rightMargin: Math.round(12 * rootWindow.dp)
                                text: exportScreen.engineerId.length > 0 ? exportScreen.engineerId : "Tap to enter ID"
                                font.family: "Noto Sans"
                                font.pixelSize: Math.round(15 * rootWindow.sp)
                                color: exportScreen.engineerId.length > 0 ? Theme.onBackground : Theme.onSurfaceVariant
                                elide: Text.ElideRight
                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                enabled: !exportScreen.exporting
                                onClicked: {
                                    textKeypad.open("id", exportScreen.engineerId);
                                }
                            }
                        }
                    }
                }
            }

            Item {
                width: parent.width
                height: Math.round(52 * rootWindow.dp)
                visible: !exportScreen.exportDone

                ActionButton {
                    anchors.fill: parent
                    text: exportScreen.exporting ? "Exporting\u2026" : "Export report"
                    filled: true
                    enabled: exportScreen.canExport
                    buttonColor: Theme.primary
                    visible: !exportScreen.exporting

                    onClicked: {
                        console.log("ExportScreen: Export button clicked.");
                        Exporter.exportReport(exportScreen.engineerName.trim(), exportScreen.engineerId.trim());
                    }
                }

                Item {
                    anchors.centerIn: parent
                    width: Math.round(32 * rootWindow.dp)
                    height: Math.round(32 * rootWindow.dp)
                    visible: exportScreen.exporting

                    Canvas {
                        id: exportSpinner
                        anchors.fill: parent
                        property real spinAngle: 0
                        property color spinColor: Theme.primary

                        onSpinColorChanged: requestPaint()

                        NumberAnimation on spinAngle {
                            from: 0; to: 360; duration: 1200
                            loops: Animation.Infinite
                            running: exportScreen.exporting && Settings.animationsEnabled
                        }

                        onSpinAngleChanged: requestPaint()

                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.clearRect(0, 0, width, height);
                            var cx = width / 2;
                            var cy = height / 2;
                            var r = Math.min(cx, cy) * 0.8;
                            var lw = Math.max(2, Math.round(3 * rootWindow.dp));

                            ctx.beginPath();
                            ctx.arc(cx, cy, r, 0, 2 * Math.PI);
                            ctx.strokeStyle = Qt.rgba(spinColor.r, spinColor.g, spinColor.b, 0.15);
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
            }

            Rectangle {
                width: parent.width
                height: resultsCol.height + Math.round(24 * rootWindow.dp)
                radius: Math.round(12 * rootWindow.dp)
                color: Theme.surface
                border.width: 1
                border.color: Theme.secondary
                visible: exportScreen.exportDone

                Behavior on color { ColorAnimation { duration: Theme.animDuration } }

                Column {
                    id: resultsCol
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Math.round(12 * rootWindow.dp)
                    spacing: Math.round(8 * rootWindow.dp)

                    Text {
                        text: "Export complete"
                        font.family: "Noto Sans"
                        font.pixelSize: Math.round(15 * rootWindow.sp)
                        font.weight: Font.Medium
                        color: Theme.secondary
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
                        spacing: Math.round(8 * rootWindow.dp)

                        Rectangle {
                            width: Math.round(20 * rootWindow.dp)
                            height: Math.round(20 * rootWindow.dp)
                            radius: width / 2
                            color: Theme.secondary
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                anchors.centerIn: parent
                                text: "\u2713"
                                font.family: "Noto Sans"
                                font.pixelSize: Math.round(12 * rootWindow.sp)
                                font.weight: Font.Bold
                                color: "#ffffff"
                            }
                        }

                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Math.round(2 * rootWindow.dp)

                            Text {
                                text: "PDF saved"
                                font.family: "Noto Sans"
                                font.pixelSize: Math.round(12 * rootWindow.sp)
                                font.weight: Font.Medium
                                color: Theme.onBackground
                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                            }

                            Text {
                                text: exportScreen.pdfPath
                                font.family: "Noto Sans Mono"
                                font.pixelSize: Math.round(10 * rootWindow.sp)
                                color: Theme.onSurfaceVariant
                                elide: Text.ElideMiddle
                                width: mainCol.width - Math.round(60 * rootWindow.dp)
                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                            }
                        }
                    }

                    Row {
                        width: parent.width
                        spacing: Math.round(8 * rootWindow.dp)

                        Rectangle {
                            width: Math.round(20 * rootWindow.dp)
                            height: Math.round(20 * rootWindow.dp)
                            radius: width / 2
                            color: Theme.secondary
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                anchors.centerIn: parent
                                text: "\u2713"
                                font.family: "Noto Sans"
                                font.pixelSize: Math.round(12 * rootWindow.sp)
                                font.weight: Font.Bold
                                color: "#ffffff"
                            }
                        }

                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Math.round(2 * rootWindow.dp)

                            Text {
                                text: "JSON saved"
                                font.family: "Noto Sans"
                                font.pixelSize: Math.round(12 * rootWindow.sp)
                                font.weight: Font.Medium
                                color: Theme.onBackground
                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                            }

                            Text {
                                text: exportScreen.jsonPath
                                font.family: "Noto Sans Mono"
                                font.pixelSize: Math.round(10 * rootWindow.sp)
                                color: Theme.onSurfaceVariant
                                elide: Text.ElideMiddle
                                width: mainCol.width - Math.round(60 * rootWindow.dp)
                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                            }
                        }
                    }
                }
            }

            Item {
                width: parent.width
                height: Math.round(180 * rootWindow.dp)
                visible: exportScreen.exportDone && exportScreen.qrBase64.length > 0

                Column {
                    anchors.centerIn: parent
                    spacing: Math.round(8 * rootWindow.dp)

                    Image {
                        id: qrImage
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Math.round(140 * rootWindow.dp)
                        height: Math.round(140 * rootWindow.dp)
                        source: exportScreen.qrBase64.length > 0 ? ("data:image/png;base64," + exportScreen.qrBase64) : ""
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Scan to store record offline"
                        font.family: "Noto Sans"
                        font.pixelSize: Math.round(11 * rootWindow.sp)
                        color: Theme.onSurfaceVariant
                        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                    }
                }
            }

            Item {
                width: parent.width
                height: Math.round(52 * rootWindow.dp)
                visible: exportScreen.exportDone

                Row {
                    anchors.centerIn: parent
                    spacing: Math.round(12 * rootWindow.dp)

                    ActionButton {
                        text: "Upload now"
                        filled: false
                        enabled: !exportScreen.uploadAttempted
                        buttonColor: Theme.primary
                        width: Math.round(140 * rootWindow.dp)
                        height: Math.round(44 * rootWindow.dp)

                        onClicked: {
                            exportScreen.uploadAttempted = true;
                            exportScreen.uploadSuccess = false;
                            console.log("ExportScreen: Upload attempted (simulated failure — no network).");
                            rootWindow.showToast("No network \u2014 transfer via USB", Theme.warning);
                        }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: exportScreen.uploadAttempted ? "No network \u2014 transfer via USB" : ""
                        font.family: "Noto Sans"
                        font.pixelSize: Math.round(12 * rootWindow.sp)
                        color: Theme.warning
                        visible: exportScreen.uploadAttempted && !exportScreen.uploadSuccess
                        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                    }
                }
            }

            Item {
                width: parent.width
                height: Math.round(20 * rootWindow.dp)
            }
        }
    }

    TextKeypad {
        id: textKeypad

        onConfirmed: function(value) {
            if (textKeypad.targetField === "name") {
                exportScreen.engineerName = value;
            } else if (textKeypad.targetField === "id") {
                exportScreen.engineerId = value;
            }
            console.log("ExportScreen: " + textKeypad.targetField + " set to \"" + value + "\"");
        }

        onCancelled: {
            console.log("ExportScreen: Text keypad cancelled for " + textKeypad.targetField);
        }
    }
}
