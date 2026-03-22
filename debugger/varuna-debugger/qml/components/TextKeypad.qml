import QtQuick

Item {
    id: textKeypadRoot
    anchors.fill: parent
    visible: _isOpen || scrimFade.running || panelSlide.running
    z: 1900

    property bool _isOpen: false
    property string currentValue: ""
    property string targetField: ""
    property int maxLength: 50
    property bool shiftActive: false

    signal confirmed(string value)
    signal cancelled()

    function open(fieldId, initialValue) {
        targetField = fieldId;
        currentValue = initialValue !== undefined && initialValue !== null ? initialValue : "";
        shiftActive = false;
        _isOpen = true;
        scrimFade.to = Theme.scrimOpacity;
        scrimFade.duration = Settings.animationsEnabled ? 200 : 0;
        scrimFade.start();
        panelSlide.to = textKeypadRoot.height - panelBody.height;
        panelSlide.duration = Settings.animationsEnabled ? 250 : 0;
        panelSlide.start();
    }

    function close() {
        _isOpen = false;
        scrimFade.to = 0;
        scrimFade.duration = Settings.animationsEnabled ? 200 : 0;
        scrimFade.start();
        panelSlide.to = textKeypadRoot.height;
        panelSlide.duration = Settings.animationsEnabled ? 200 : 0;
        panelSlide.start();
    }

    function appendChar(ch) {
        if (currentValue.length >= maxLength) return;
        if (shiftActive) {
            currentValue = currentValue + ch.toUpperCase();
            shiftActive = false;
        } else {
            currentValue = currentValue + ch;
        }
    }

    function backspace() {
        if (currentValue.length > 0) {
            currentValue = currentValue.substring(0, currentValue.length - 1);
        }
    }

    function appendSpace() {
        if (currentValue.length >= maxLength) return;
        currentValue = currentValue + " ";
    }

    function confirmInput() {
        confirmed(currentValue);
        close();
    }

    function cancelInput() {
        cancelled();
        close();
    }

    function toggleShift() {
        shiftActive = !shiftActive;
    }

    readonly property var row1Keys: ["q", "w", "e", "r", "t", "y", "u", "i", "o", "p"]
    readonly property var row2Keys: ["a", "s", "d", "f", "g", "h", "j", "k", "l"]
    readonly property var row3Keys: ["z", "x", "c", "v", "b", "n", "m"]

    Rectangle {
        id: scrim
        anchors.fill: parent
        color: "#000000"
        opacity: 0

        NumberAnimation on opacity {
            id: scrimFade
            running: false
            to: 0
            duration: 200
            easing.type: Easing.OutQuad
        }

        MouseArea {
            anchors.fill: parent
            enabled: textKeypadRoot._isOpen
            onClicked: textKeypadRoot.cancelInput()
        }
    }

    Rectangle {
        id: panelBody
        width: parent.width
        height: Math.round(240 * rootWindow.dp)
        y: parent.height
        color: Theme.surface

        Behavior on color { ColorAnimation { duration: Theme.animDuration } }

        NumberAnimation on y {
            id: panelSlide
            running: false
            to: 0
            duration: 250
            easing.type: Easing.OutCubic
        }

        MouseArea {
            anchors.fill: parent
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: Math.round(16 * rootWindow.dp)
            color: Theme.surface
            radius: Math.round(16 * rootWindow.dp)
            Behavior on color { ColorAnimation { duration: Theme.animDuration } }
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: Math.round(6 * rootWindow.dp)
            width: Math.round(32 * rootWindow.dp)
            height: Math.round(4 * rootWindow.dp)
            radius: Math.round(2 * rootWindow.dp)
            color: Theme.onSurfaceVariant
            opacity: 0.5
            Behavior on color { ColorAnimation { duration: Theme.animDuration } }
        }

        Column {
            anchors.fill: parent
            anchors.topMargin: Math.round(16 * rootWindow.dp)
            anchors.leftMargin: Math.round(4 * rootWindow.dp)
            anchors.rightMargin: Math.round(4 * rootWindow.dp)
            anchors.bottomMargin: Math.round(6 * rootWindow.dp)
            spacing: Math.round(4 * rootWindow.dp)

            Rectangle {
                width: parent.width - Math.round(8 * rootWindow.dp)
                height: Math.round(36 * rootWindow.dp)
                anchors.horizontalCenter: parent.horizontalCenter
                radius: Math.round(8 * rootWindow.dp)
                color: Theme.surfaceVariant
                border.width: 1
                border.color: Theme.primary

                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                Behavior on border.color { ColorAnimation { duration: Theme.animDuration } }

                Row {
                    anchors.centerIn: parent
                    spacing: 0

                    Text {
                        text: textKeypadRoot.currentValue.length > 0 ? textKeypadRoot.currentValue : "Type here\u2026"
                        font.family: "Noto Sans"
                        font.pixelSize: Math.round(16 * rootWindow.sp)
                        font.weight: Font.Normal
                        color: textKeypadRoot.currentValue.length > 0 ? Theme.onBackground : Theme.onSurfaceVariant
                        elide: Text.ElideLeft
                        maximumLineCount: 1

                        Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                    }

                    Rectangle {
                        width: Math.round(2 * rootWindow.dp)
                        height: Math.round(20 * rootWindow.dp)
                        color: Theme.primary
                        anchors.verticalCenter: parent.verticalCenter

                        SequentialAnimation on opacity {
                            loops: Animation.Infinite
                            running: textKeypadRoot._isOpen
                            NumberAnimation { from: 1.0; to: 0.0; duration: 500 }
                            NumberAnimation { from: 0.0; to: 1.0; duration: 500 }
                        }
                    }
                }
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Math.round(3 * rootWindow.dp)

                Repeater {
                    model: textKeypadRoot.row1Keys

                    Rectangle {
                        width: Math.round(34 * rootWindow.dp)
                        height: Math.round(38 * rootWindow.dp)
                        radius: Math.round(6 * rootWindow.dp)
                        color: r1ma.pressed ? Theme.primaryContainer : Theme.surfaceVariant
                        Behavior on color { ColorAnimation { duration: 80 } }

                        Text {
                            anchors.centerIn: parent
                            text: textKeypadRoot.shiftActive ? modelData.toUpperCase() : modelData
                            font.family: "Noto Sans"
                            font.pixelSize: Math.round(15 * rootWindow.sp)
                            font.weight: Font.Normal
                            color: Theme.onBackground
                        }

                        MouseArea {
                            id: r1ma
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: textKeypadRoot.appendChar(modelData)
                        }
                    }
                }
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Math.round(3 * rootWindow.dp)

                Repeater {
                    model: textKeypadRoot.row2Keys

                    Rectangle {
                        width: Math.round(36 * rootWindow.dp)
                        height: Math.round(38 * rootWindow.dp)
                        radius: Math.round(6 * rootWindow.dp)
                        color: r2ma.pressed ? Theme.primaryContainer : Theme.surfaceVariant
                        Behavior on color { ColorAnimation { duration: 80 } }

                        Text {
                            anchors.centerIn: parent
                            text: textKeypadRoot.shiftActive ? modelData.toUpperCase() : modelData
                            font.family: "Noto Sans"
                            font.pixelSize: Math.round(15 * rootWindow.sp)
                            font.weight: Font.Normal
                            color: Theme.onBackground
                        }

                        MouseArea {
                            id: r2ma
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: textKeypadRoot.appendChar(modelData)
                        }
                    }
                }
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Math.round(3 * rootWindow.dp)

                Rectangle {
                    width: Math.round(48 * rootWindow.dp)
                    height: Math.round(38 * rootWindow.dp)
                    radius: Math.round(6 * rootWindow.dp)
                    color: textKeypadRoot.shiftActive ? Theme.primaryContainer : (shiftMa.pressed ? Theme.primaryContainer : Theme.surfaceVariant)
                    border.width: textKeypadRoot.shiftActive ? 1 : 0
                    border.color: Theme.primary
                    Behavior on color { ColorAnimation { duration: 80 } }

                    Text {
                        anchors.centerIn: parent
                        text: "\u21E7"
                        font.family: "Noto Sans"
                        font.pixelSize: Math.round(18 * rootWindow.sp)
                        font.weight: Font.Medium
                        color: textKeypadRoot.shiftActive ? Theme.primary : Theme.onBackground
                    }

                    MouseArea {
                        id: shiftMa
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: textKeypadRoot.toggleShift()
                    }
                }

                Repeater {
                    model: textKeypadRoot.row3Keys

                    Rectangle {
                        width: Math.round(34 * rootWindow.dp)
                        height: Math.round(38 * rootWindow.dp)
                        radius: Math.round(6 * rootWindow.dp)
                        color: r3ma.pressed ? Theme.primaryContainer : Theme.surfaceVariant
                        Behavior on color { ColorAnimation { duration: 80 } }

                        Text {
                            anchors.centerIn: parent
                            text: textKeypadRoot.shiftActive ? modelData.toUpperCase() : modelData
                            font.family: "Noto Sans"
                            font.pixelSize: Math.round(15 * rootWindow.sp)
                            font.weight: Font.Normal
                            color: Theme.onBackground
                        }

                        MouseArea {
                            id: r3ma
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: textKeypadRoot.appendChar(modelData)
                        }
                    }
                }

                Rectangle {
                    width: Math.round(48 * rootWindow.dp)
                    height: Math.round(38 * rootWindow.dp)
                    radius: Math.round(6 * rootWindow.dp)
                    color: bsMa.pressed ? Theme.error : Theme.surfaceVariant
                    Behavior on color { ColorAnimation { duration: 80 } }

                    Text {
                        anchors.centerIn: parent
                        text: "\u232B"
                        font.family: "Noto Sans"
                        font.pixelSize: Math.round(18 * rootWindow.sp)
                        color: bsMa.pressed ? "#ffffff" : Theme.error
                    }

                    MouseArea {
                        id: bsMa
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: textKeypadRoot.backspace()
                    }
                }
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Math.round(4 * rootWindow.dp)

                Rectangle {
                    width: Math.round(80 * rootWindow.dp)
                    height: Math.round(38 * rootWindow.dp)
                    radius: Math.round(6 * rootWindow.dp)
                    color: numMa.pressed ? Theme.primaryContainer : Theme.surfaceVariant
                    Behavior on color { ColorAnimation { duration: 80 } }

                    Text {
                        anchors.centerIn: parent
                        text: "123"
                        font.family: "Noto Sans Mono"
                        font.pixelSize: Math.round(14 * rootWindow.sp)
                        font.weight: Font.Medium
                        color: Theme.onBackground
                    }

                    property bool numMode: false

                    MouseArea {
                        id: numMa
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            parent.numMode = !parent.numMode;
                        }
                    }
                }

                Rectangle {
                    width: Math.round(200 * rootWindow.dp)
                    height: Math.round(38 * rootWindow.dp)
                    radius: Math.round(6 * rootWindow.dp)
                    color: spaceMa.pressed ? Theme.primaryContainer : Theme.surfaceVariant
                    Behavior on color { ColorAnimation { duration: 80 } }

                    Text {
                        anchors.centerIn: parent
                        text: "space"
                        font.family: "Noto Sans"
                        font.pixelSize: Math.round(13 * rootWindow.sp)
                        color: Theme.onSurfaceVariant
                    }

                    MouseArea {
                        id: spaceMa
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: textKeypadRoot.appendSpace()
                    }
                }

                Rectangle {
                    width: Math.round(80 * rootWindow.dp)
                    height: Math.round(38 * rootWindow.dp)
                    radius: Math.round(20 * rootWindow.dp)
                    color: enterMa.pressed ? Qt.darker(Theme.primary, 1.1) : Theme.primary
                    Behavior on color { ColorAnimation { duration: 80 } }

                    Text {
                        anchors.centerIn: parent
                        text: "\u2713 Done"
                        font.family: "Noto Sans"
                        font.pixelSize: Math.round(13 * rootWindow.sp)
                        font.weight: Font.Medium
                        color: "#ffffff"
                    }

                    MouseArea {
                        id: enterMa
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: textKeypadRoot.confirmInput()
                    }
                }
            }
        }
    }
}
