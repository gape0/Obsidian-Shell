import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "ObsidianStyle.js" as Style

Item {
    id: page

    property string pendingActionId: ""

    ScrollView {
        id: scrollArea
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
        width: scrollArea.availableWidth
        spacing: 14

        SettingsCard {
            Label {
                text: "Global shortcuts"
                color: Style.foreground
                font.pixelSize: 19
                font.weight: Font.Medium
            }
            Label {
                text: "Choose an action and record one key combination. Existing KDE shortcuts can also be changed here."
                color: Style.muted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                ComboBox {
                    id: actionPicker
                    model: obsidian.shortcutActions
                    textRole: "label"
                    Layout.fillWidth: true
                }
                Button {
                    text: "Record"
                    enabled: actionPicker.currentIndex >= 0
                    onClicked: {
                        page.pendingActionId = obsidian.shortcutActions[actionPicker.currentIndex].actionId
                        newShortcutRecorder.begin()
                    }
                }
                Button {
                    text: "Refresh"
                    onClicked: obsidian.refreshShortcuts()
                }
            }
            ShortcutRecorder {
                id: newShortcutRecorder
                width: 1
                height: 1
                opacity: 0
                onRecorded: function(sequence) {
                    if (page.pendingActionId)
                        obsidian.assignShortcut(page.pendingActionId, sequence)
                }
            }
        }

        SettingsCard {
            Label {
                text: "Configured shortcuts"
                color: Style.foreground
                font.pixelSize: 16
                font.weight: Font.Medium
            }
            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: "ACTION"
                    color: Style.muted
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                }
                Label {
                    text: "SHORTCUT"
                    color: Style.muted
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    Layout.preferredWidth: 200
                }
                Item { Layout.preferredWidth: 76 }
            }
            ListView {
                id: bindings
                model: obsidian.shortcutBindings
                clip: true
                spacing: 4
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 300)
                ScrollBar.vertical: ScrollBar {}
                delegate: Rectangle {
                    required property string actionId
                    required property string label
                    required property string shortcut
                    width: ListView.view.width
                    height: 48
                    radius: 6
                    color: Style.raised
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 8
                        Label {
                            text: label
                            color: Style.foreground
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Button {
                            text: shortcut
                            Layout.preferredWidth: 190
                            onClicked: rowRecorder.begin()
                        }
                        Button {
                            text: "Clear"
                            onClicked: obsidian.clearShortcut(actionId)
                        }
                    }
                    ShortcutRecorder {
                        id: rowRecorder
                        width: 1
                        height: 1
                        opacity: 0
                        onRecorded: function(sequence) {
                            obsidian.assignShortcut(actionId, sequence)
                        }
                    }
                }
                Label {
                    anchors.centerIn: parent
                    visible: bindings.count === 0
                    text: "No shortcuts are configured yet."
                    color: Style.muted
                }
            }
        }
        Item { Layout.fillHeight: true }
        }
    }
}
