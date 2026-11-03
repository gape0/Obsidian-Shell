import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "ObsidianStyle.js" as Style

ApplicationWindow {
    id: window
    width: 1080
    height: 780
    minimumWidth: 680
    minimumHeight: 560
    visible: true
    title: "Obsidian Wallpapers"
    color: Style.background

    property bool busy: false
    property string statusMessage: "Choose a folder to browse wallpapers."
    property bool statusIsError: false

    header: ToolBar {
        background: Rectangle { color: Style.surface }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            Label {
                text: "OBSIDIAN WALLPAPERS"
                color: Style.foreground
                font.pixelSize: 17
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
            Button {
                text: "Choose folder"
                enabled: !window.busy
                onClicked: folderDialog.open()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 14

        Label {
            text: "Pick your next desktop background."
            color: Style.foreground
            font.pixelSize: 25
            font.weight: Font.Medium
        }

        Label {
            text: obsidian.wallpaperDirectory || "No wallpaper folder selected"
            color: Style.muted
            elide: Text.ElideMiddle
            Layout.fillWidth: true
        }

        GridView {
            id: gallery
            model: obsidian.wallpaperChoices
            clip: true
            cellWidth: Math.max(220, Math.min(360, Math.floor(width / 3)))
            cellHeight: 246
            Layout.fillWidth: true
            Layout.fillHeight: true
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                required property string path
                required property string url
                required property string name
                width: gallery.cellWidth - 12
                height: gallery.cellHeight - 12
                radius: 10
                color: Style.surface
                border.width: path === obsidian.wallpaper ? 2 : 1
                border.color: path === obsidian.wallpaper ? Style.accent : Style.raised
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 7
                    spacing: 5

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Rectangle {
                            anchors.fill: parent
                            radius: 6
                            color: Style.raised
                        }
                        Image {
                            id: preview
                            anchors.fill: parent
                            asynchronous: true
                            cache: true
                            fillMode: Image.PreserveAspectCrop
                            source: url
                        }
                        Label {
                            anchors.centerIn: parent
                            visible: preview.status === Image.Error
                            text: "Preview unavailable"
                            color: Style.muted
                        }
                    }

                    Label {
                        text: name
                        color: Style.foreground
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }

                    Button {
                        text: path === obsidian.wallpaper ? "Current wallpaper" : "Set wallpaper"
                        highlighted: path !== obsidian.wallpaper
                        enabled: !window.busy && path !== obsidian.wallpaper
                        Layout.fillWidth: true
                        onClicked: {
                            window.busy = true
                            window.statusMessage = "Applying wallpaper…"
                            window.statusIsError = false
                            obsidian.applyWallpaper(path)
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: gallery.count === 0
                text: window.busy
                    ? "Loading wallpapers…"
                    : "No images found. Choose a folder containing PNG, JPEG, BMP, or WebP images."
                color: Style.muted
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                width: Math.min(parent.width - 48, 420)
            }
        }

        Label {
            text: window.statusMessage
            color: window.statusIsError ? Style.danger : Style.muted
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
    }

    FolderDialog {
        id: folderDialog
        title: "Choose a wallpaper folder"
        onAccepted: {
            window.busy = true
            window.statusMessage = "Loading wallpapers…"
            window.statusIsError = false
            obsidian.loadWallpaperDirectory(selectedFolder.toLocalFile())
        }
    }

    Connections {
        target: obsidian

        function onWallpaperChoicesChanged() {
            window.busy = false
            window.statusMessage = obsidian.wallpaperChoices.length
                ? obsidian.wallpaperChoices.length + " wallpapers"
                : "No supported images found in this folder."
            window.statusIsError = false
        }

        function onApplied(message) {
            window.busy = false
            window.statusMessage = message
            window.statusIsError = false
        }

        function onErrorOccurred(message) {
            window.busy = false
            window.statusMessage = message
            window.statusIsError = true
        }
    }

    Component.onCompleted: {
        if (obsidian.wallpaperDirectory)
            obsidian.loadWallpaperDirectory(obsidian.wallpaperDirectory)
        else
            folderDialog.open()
    }
}
