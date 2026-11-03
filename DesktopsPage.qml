import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "ObsidianStyle.js" as Style

Item {
    id: page
    property int currentCount: Math.max(1, obsidian.desktopCount)

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
                text: "Virtual desktops"
                color: Style.foreground
                font.pixelSize: 19
                font.weight: Font.Medium
            }
            Label {
                text: "Choose how many workspaces KDE Plasma keeps available."
                color: Style.muted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            RowLayout {
                Label { text: "Desktop count"; color: Style.foreground; Layout.fillWidth: true }
                SpinBox {
                    id: countEditor
                    from: 1
                    to: 10
                    value: page.currentCount
                }
                Button {
                    text: "Apply"
                    highlighted: true
                    onClicked: {
                        if (countEditor.value < page.currentCount)
                            removeDesktopDialog.open()
                        else
                            obsidian.setDesktopCount(countEditor.value)
                    }
                }
                Button {
                    text: "Refresh"
                    onClicked: obsidian.refreshDesktopCount()
                }
            }
            Label {
                text: "Reducing the count removes the last desktop(s). KDE may move windows from removed desktops."
                color: Style.muted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
        Item { Layout.fillHeight: true }
        }
    }

    Dialog {
        id: removeDesktopDialog
        title: "Remove virtual desktops?"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Cancel | Dialog.Ok
        contentItem: Label {
            text: "Reducing the count removes the last desktop(s). KDE may move windows on those desktops. Continue?"
            color: Style.foreground
            wrapMode: Text.WordWrap
            width: 360
            padding: 18
        }
        onAccepted: obsidian.setDesktopCount(countEditor.value)
    }

    Connections {
        target: obsidian
        function onDesktopCountChanged(count) {
            page.currentCount = count
            countEditor.value = count
        }
    }
}
