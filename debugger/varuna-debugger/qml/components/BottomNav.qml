// -----------------------------------------------------------------
// File: qml/components/BottomNav.qml
// Phase: Phase 13 (switched from Rectangle-drawn icons to SvgIcon)
//
// To swap any icon: replace the .svg file in assets/icons/.
// The icon name maps directly: navIcons[0] = "boot" → assets/icons/boot.svg
// -----------------------------------------------------------------

import QtQuick

Item {
    id: bottomNav
    width:  parent.width
    height: Math.round(56 * dp)

    readonly property real dp: Math.max(1, Math.min(parent.width / 1280, parent.height / 720))
    readonly property real sp: dp * Settings.fontScale

    property int currentIndex: 0
    signal navTapped(int index)

    readonly property var navLabels: ["Boot", "Live", "Cal", "Conn", "Config", "Verdict", "Export"]
    readonly property var navTitles: [
        "Boot check", "Live sensors", "Calibration",
        "Connectivity", "Thresholds", "Verdict", "Export report"
    ]

    // Icon name for each tab — must match filename in assets/icons/ (without .svg)
    readonly property var navIcons: ["boot", "live", "cal", "conn", "thresh", "verdict", "export"]

    Rectangle {
        anchors.fill: parent
        color: Theme.surface

        Behavior on color { ColorAnimation { duration: Theme.animDuration } }

        // Top border
        Rectangle {
            anchors.left:  parent.left
            anchors.right: parent.right
            anchors.top:   parent.top
            height: 1
            color:  Theme.border
            Behavior on color { ColorAnimation { duration: Theme.animDuration } }
        }

        Row {
            anchors.fill: parent
            anchors.topMargin: 1

            Repeater {
                model: 7

                Item {
                    id: navItem
                    width:  bottomNav.width / 7
                    height: parent.height

                    property bool isActive: bottomNav.currentIndex === index

                    // Ripple state
                    property real rippleX: width  / 2
                    property real rippleY: height / 2
                    property real rippleRadius: 0
                    property real rippleOpacity: 0

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor

                        onPressed: function(mouse) {
                            navItem.rippleX = mouse.x;
                            navItem.rippleY = mouse.y;
                            rippleAnim.restart();
                        }

                        onClicked: {
                            if (bottomNav.currentIndex !== index) {
                                bottomNav.currentIndex = index;
                                bottomNav.navTapped(index);
                            }
                        }
                    }

                    // Ripple effect
                    SequentialAnimation {
                        id: rippleAnim
                        NumberAnimation {
                            target:   navItem
                            property: "rippleRadius"
                            from: 0
                            to:   navItem.width * 0.7
                            duration: Settings.animationsEnabled ? 300 : 0
                            easing.type: Easing.OutQuad
                        }
                        NumberAnimation {
                            target:   navItem
                            property: "rippleOpacity"
                            from: 0.12
                            to:   0
                            duration: Settings.animationsEnabled ? 200 : 0
                        }
                        ScriptAction {
                            script: { navItem.rippleRadius = 0; navItem.rippleOpacity = 0; }
                        }
                    }

                    Rectangle {
                        x:       navItem.rippleX - navItem.rippleRadius
                        y:       navItem.rippleY - navItem.rippleRadius
                        width:   navItem.rippleRadius * 2
                        height:  navItem.rippleRadius * 2
                        radius:  navItem.rippleRadius
                        color:   Theme.primary
                        opacity: navItem.rippleOpacity
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: Math.round(2 * bottomNav.dp)

                        // Icon + pill
                        Item {
                            width:  Math.round(48 * bottomNav.dp)
                            height: Math.round(28 * bottomNav.dp)
                            anchors.horizontalCenter: parent.horizontalCenter

                            // Active pill indicator
                            Rectangle {
                                anchors.centerIn: parent
                                width:   Math.round(48 * bottomNav.dp)
                                height:  Math.round(28 * bottomNav.dp)
                                radius:  Math.round(14 * bottomNav.dp)
                                color:   navItem.isActive ? Theme.primaryContainer : "transparent"
                                visible: navItem.isActive

                                Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                            }

                            // SVG icon
                            SvgIcon {
                                anchors.centerIn: parent
                                name:  bottomNav.navIcons[index]
                                size:  Math.round(20 * bottomNav.dp)
                                color: navItem.isActive ? Theme.primary : Theme.onSurfaceVariant

                                // Smooth color transition via the SvgIcon's internal Behavior
                            }
                        }

                        // Label
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text:       bottomNav.navLabels[index]
                            font.family:    "Noto Sans"
                            font.pixelSize: Math.round(9 * bottomNav.sp)
                            font.weight:    navItem.isActive ? Font.Medium : Font.Normal
                            color: navItem.isActive ? Theme.primary : Theme.onSurfaceVariant

                            Behavior on color { ColorAnimation { duration: Theme.animDuration } }
                        }
                    }
                }
            }
        }
    }
}
