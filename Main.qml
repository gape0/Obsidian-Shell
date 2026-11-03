import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "ObsidianStyle.js" as Style

ApplicationWindow {
    id: window
    width: 900
    height: 720
    minimumWidth: 700
    minimumHeight: 580
    visible: true
    title: "Obsidian Settings"
    color: Style.background

    property color textColor: Style.foreground
    property color mutedColor: Style.muted
    property color accentColor: obsidian.accent
    property string statusMessage: "Ready"
    property bool statusIsError: false

    header: ToolBar {
        background: Rectangle { color: Style.surface }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            Label {
                text: "OBSIDIAN"
                color: window.textColor
                font.pixelSize: 18
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
            Label {
                text: "DESKTOP SETTINGS"
                color: window.mutedColor
                font.pixelSize: 11
                font.letterSpacing: 1.2
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        Label {
            text: "Make the desktop yours."
            color: window.textColor
            font.pixelSize: 27
            font.weight: Font.Medium
        }
        Label {
            text: "Configure shortcuts, desktops, connectivity, input, and Obsidian options."
            color: window.mutedColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        TabBar {
            id: tabs
            Layout.fillWidth: true
            background: Rectangle { color: "transparent" }
            TabButton { text: "Shortcuts" }
            TabButton { text: "Desktops" }
            TabButton { text: "Wi-Fi" }
            TabButton { text: "Bluetooth" }
            TabButton { text: "Input" }
            TabButton { text: "Obsidian" }
        }

        StackLayout {
            currentIndex: tabs.currentIndex
            Layout.fillWidth: true
            Layout.fillHeight: true

            ShortcutsPage {}
            DesktopsPage {}
            WifiPage {}
            BluetoothPage {}
            InputPage {
                onErrorMessage: function(message) {
                    window.statusMessage = message
                    window.statusIsError = true
                }
            }
            ObsidianPage {
                onErrorMessage: function(message) {
                    window.statusMessage = message
                    window.statusIsError = true
                }
            }
        }

        Label {
            text: window.statusMessage
            color: window.statusIsError ? "#ff8a80" : window.mutedColor
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    Connections {
        target: obsidian

        function onErrorOccurred(message) {
            window.statusMessage = message
            window.statusIsError = true
        }

        function onApplied(message) {
            window.statusMessage = message
            window.statusIsError = false
        }

        function onShortcutMessage(message) {
            window.statusMessage = message
            window.statusIsError = false
        }
    }

    Component.onCompleted: {
        obsidian.refreshShortcuts()
        obsidian.refreshDesktopCount()
        obsidian.refreshWifi()
        obsidian.refreshBluetooth()
        obsidian.refreshInputSettings()
    }
}
