import QtQuick
import QtQuick.Controls
import "components"
import "screens"

ApplicationWindow {
    id: rootWindow
    visible: true
    width: 1280
    height: 720
    minimumWidth: 800
    minimumHeight: 480
    title: "VARUNA Debugger v" + Settings.appVersion
    color: Theme.background

    Behavior on color {
        ColorAnimation { duration: Theme.animDuration }
    }

    readonly property real dp: Math.max(1, Math.min(rootWindow.width / 1280, rootWindow.height / 720))
    readonly property real sp: dp * Settings.fontScale

    property int previousNavIndex: 0

    readonly property var screenComponents: [
        bootScreenComp,
        liveScreenComp,
        calibrationScreenComp,
        connectivityScreenComp,
        thresholdScreenComp,
        verdictScreenComp,
        exportScreenComp
    ]

    Component { id: bootScreenComp; BootScreen {} }
    Component { id: liveScreenComp; LiveScreen {} }
    Component { id: calibrationScreenComp; CalibrationScreen {} }
    Component { id: connectivityScreenComp; ConnectivityScreen {} }
    Component { id: thresholdScreenComp; ThresholdScreen {} }
    Component { id: verdictScreenComp; VerdictScreen {} }
    Component { id: exportScreenComp; ExportScreen {} }

    function navigateToScreen(index) {
        if (index < 0 || index > 6) return;
        if (index === bottomNav.currentIndex) return;
        previousNavIndex = bottomNav.currentIndex;
        bottomNav.currentIndex = index;
        screenStack.replace(screenComponents[index]);
    }

    function showToast(message, color) {
        toastNotification.show(message, color);
    }

    StatusStrip {
        id: statusStrip
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        z: 10
        screenTitle: bottomNav.navTitles[bottomNav.currentIndex]
        currentScreenIndex: bottomNav.currentIndex
    }

    StackView {
        id: screenStack
        anchors.top: statusStrip.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: serialLogStrip.top
        z: 1
        clip: true
        initialItem: bootScreenComp

        pushEnter: Transition {
            PropertyAnimation {
                property: "x"
                from: screenStack.width
                to: 0
                duration: Settings.animationsEnabled ? 300 : 0
                easing.type: Easing.OutCubic
            }
        }

        pushExit: Transition {
            ParallelAnimation {
                PropertyAnimation {
                    property: "x"
                    from: 0
                    to: -screenStack.width * 0.3
                    duration: Settings.animationsEnabled ? 300 : 0
                    easing.type: Easing.OutCubic
                }
                PropertyAnimation {
                    property: "opacity"
                    from: 1.0
                    to: 0.8
                    duration: Settings.animationsEnabled ? 300 : 0
                    easing.type: Easing.OutCubic
                }
            }
        }

        popEnter: Transition {
            PropertyAnimation {
                property: "x"
                from: -screenStack.width * 0.3
                to: 0
                duration: Settings.animationsEnabled ? 300 : 0
                easing.type: Easing.OutCubic
            }
        }

        popExit: Transition {
            PropertyAnimation {
                property: "x"
                from: 0
                to: screenStack.width
                duration: Settings.animationsEnabled ? 300 : 0
                easing.type: Easing.OutCubic
            }
        }

        replaceEnter: Transition {
            PropertyAnimation {
                property: "x"
                from: rootWindow.previousNavIndex < bottomNav.currentIndex ? screenStack.width : -screenStack.width * 0.3
                to: 0
                duration: Settings.animationsEnabled ? 300 : 0
                easing.type: Easing.OutCubic
            }
        }

        replaceExit: Transition {
            ParallelAnimation {
                PropertyAnimation {
                    property: "x"
                    from: 0
                    to: rootWindow.previousNavIndex < bottomNav.currentIndex ? -screenStack.width * 0.3 : screenStack.width
                    duration: Settings.animationsEnabled ? 300 : 0
                    easing.type: Easing.OutCubic
                }
                PropertyAnimation {
                    property: "opacity"
                    from: 1.0
                    to: 0.8
                    duration: Settings.animationsEnabled ? 300 : 0
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    ToastNotification {
        id: toastNotification
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: serialLogStrip.top
        anchors.bottomMargin: Math.round(8 * rootWindow.dp)
        z: 1500
    }

    SerialLogStrip {
        id: serialLogStrip
        anchors.bottom: bottomNav.top
        anchors.left: parent.left
        anchors.right: parent.right
        z: 10
    }

    BottomNav {
        id: bottomNav
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        z: 10
        currentIndex: 0
        onNavTapped: function(index) {
            console.log("Nav tapped: " + navTitles[index] + " (index " + index + ")");
            rootWindow.previousNavIndex = bottomNav.currentIndex;
            screenStack.replace(rootWindow.screenComponents[index]);
        }
    }

    SettingsPanel {
        id: settingsPanel
        anchors.fill: parent
        z: 2000
    }

    Component.onCompleted: {
        console.log("Main.qml loaded successfully.");
    }
}
