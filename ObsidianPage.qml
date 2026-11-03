import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "AppSettings.js" as AppSettings
import "ObsidianStyle.js" as Style

Item {
    id: page
    signal errorMessage(string message)

    function save() {
        try {
            let settings = Object.assign({}, obsidian.settingsData)
            settings.accent = accentField.text.trim()
            settings.background = backgroundField.text.trim()
            settings.foreground = foregroundField.text.trim()
            settings.font_size = fontSize.value
            settings.kitty_enabled = kittyEnabled.checked
            settings.fish_enabled = fishEnabled.checked
            settings.wallpaper = wallpaperField.text.trim()
            let generated = AppSettings.build(settings)
            obsidian.applySettings(JSON.stringify(generated.settings), generated.kitty, generated.fish)
        } catch (error) {
            page.errorMessage(error.message)
        }
    }

    ScrollView {
        id: scrollArea
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
        width: scrollArea.availableWidth
        spacing: 12

        SettingsCard {
            Label {
                text: "Appearance"
                color: Style.foreground
                font.pixelSize: 19
                font.weight: Font.Medium
            }
            GridLayout {
                columns: 2
                rowSpacing: 10
                columnSpacing: 14
                Layout.fillWidth: true
                Label { text: "Accent"; color: Style.foreground }
                TextField { id: accentField; text: obsidian.accent; placeholderText: "#8ab4f8"; Layout.fillWidth: true }
                Label { text: "Background"; color: Style.foreground }
                TextField { id: backgroundField; text: obsidian.background; placeholderText: "#111318"; Layout.fillWidth: true }
                Label { text: "Foreground"; color: Style.foreground }
                TextField { id: foregroundField; text: obsidian.foreground; placeholderText: "#e6e8ef"; Layout.fillWidth: true }
                Label { text: "Kitty font size"; color: Style.foreground }
                SpinBox { id: fontSize; from: 6; to: 32; value: obsidian.fontSize }
            }
            Label {
                text: "Use six-digit hex colors. These appearance values are also used by the managed Kitty theme."
                color: Style.muted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        SettingsCard {
            Label {
                text: "Integrations"
                color: Style.foreground
                font.pixelSize: 16
                font.weight: Font.Medium
            }
            CheckBox {
                id: kittyEnabled
                text: "Manage Kitty theme"
                checked: obsidian.kittyEnabled
            }
            CheckBox {
                id: fishEnabled
                text: "Use Obsidian Fish prompt"
                checked: obsidian.fishEnabled
            }
            Label {
                text: "Existing configuration files are backed up before Obsidian manages them."
                color: Style.muted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        SettingsCard {
            Label {
                text: "Wallpaper"
                color: Style.foreground
                font.pixelSize: 16
                font.weight: Font.Medium
            }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: wallpaperField
                    text: obsidian.wallpaper
                    placeholderText: "Choose an image file"
                    Layout.fillWidth: true
                }
                Button { text: "Browse"; onClicked: wallpaperDialog.open() }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button {
                text: "Apply changes"
                highlighted: true
                onClicked: page.save()
            }
        }
        Item { Layout.fillHeight: true }
        }
    }

    FileDialog {
        id: wallpaperDialog
        title: "Choose wallpaper"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Images (*.png *.jpg *.jpeg *.webp *.bmp)"]
        onAccepted: {
            obsidian.setWallpaperFile(selectedFile.toString())
            wallpaperField.text = obsidian.wallpaper
        }
    }
}
