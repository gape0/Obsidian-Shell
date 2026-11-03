import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "ObsidianStyle.js" as Style

Item {
    id: page
    property bool busy: false
    property string pendingDevicePath: ""
    property string pendingDeviceName: ""

    ScrollView {
        id: scrollArea
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
        width: scrollArea.availableWidth
        spacing: 12

        SettingsCard {
            RowLayout {
                Layout.fillWidth: true
                ColumnLayout {
                    Layout.fillWidth: true
                    Label {
                        text: "Bluetooth"
                        color: Style.foreground
                        font.pixelSize: 19
                        font.weight: Font.Medium
                    }
                    Label {
                        text: obsidian.bluetoothData.hasAdapter
                            ? "Pair and manage devices with your Bluetooth adapter."
                            : "No Bluetooth adapter is available."
                        color: Style.muted
                        wrapMode: Text.WordWrap
                    }
                }
                Switch {
                    id: bluetoothSwitch
                    text: "On"
                    checked: obsidian.bluetoothData.powered
                    enabled: obsidian.bluetoothData.hasAdapter && !page.busy
                    onClicked: {
                        page.busy = true
                        obsidian.setBluetoothPowered(checked)
                    }
                }
                Button {
                    text: page.busy ? "Working…" : "Scan"
                    enabled: obsidian.bluetoothData.powered
                        && obsidian.bluetoothData.hasAdapter && !page.busy
                    onClicked: {
                        page.busy = true
                        obsidian.scanBluetooth()
                    }
                }
            }
        }

        SettingsCard {
            Label {
                text: "Nearby devices"
                color: Style.foreground
                font.pixelSize: 16
                font.weight: Font.Medium
            }
            ListView {
                id: nearbyDevices
                model: obsidian.bluetoothData.nearby
                clip: true
                spacing: 4
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 220)
                ScrollBar.vertical: ScrollBar {}
                delegate: Rectangle {
                    required property string path
                    required property string name
                    required property string address
                    required property int rssi
                    width: ListView.view.width
                    height: 54
                    radius: 6
                    color: Style.raised
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 8
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label { text: name; color: Style.foreground; elide: Text.ElideRight }
                            Label { text: address + (rssi ? " · " + rssi + " dBm" : ""); color: Style.muted; font.pixelSize: 11 }
                        }
                        Button {
                            text: "Pair"
                            enabled: obsidian.bluetoothData.powered && !page.busy
                            onClicked: {
                                page.busy = true
                                obsidian.pairBluetoothDevice(path)
                            }
                        }
                    }
                }
                Label {
                    anchors.centerIn: parent
                    visible: nearbyDevices.count === 0
                    text: obsidian.bluetoothData.powered ? "No unpaired devices found. Scan to check again."
                                                         : "Turn Bluetooth on to scan."
                    color: Style.muted
                }
            }
        }

        SettingsCard {
            Label {
                text: "Paired devices"
                color: Style.foreground
                font.pixelSize: 16
                font.weight: Font.Medium
            }
            ListView {
                id: pairedDevices
                model: obsidian.bluetoothData.paired
                clip: true
                spacing: 4
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 220)
                ScrollBar.vertical: ScrollBar {}
                delegate: Rectangle {
                    required property string path
                    required property string name
                    required property string address
                    required property bool connected
                    width: ListView.view.width
                    height: 54
                    radius: 6
                    color: Style.raised
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 8
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label { text: name; color: Style.foreground; elide: Text.ElideRight }
                            Label { text: address + (connected ? " · Connected" : " · Paired"); color: Style.muted; font.pixelSize: 11 }
                        }
                        Button {
                            text: connected ? "Disconnect" : "Connect"
                            enabled: !page.busy
                            onClicked: {
                                page.busy = true
                                if (connected)
                                    obsidian.disconnectBluetoothDevice(path)
                                else
                                    obsidian.connectBluetoothDevice(path)
                            }
                        }
                        Button {
                            text: "Forget"
                            enabled: !page.busy
                            onClicked: {
                                page.pendingDevicePath = path
                                page.pendingDeviceName = name
                                forgetDialog.open()
                            }
                        }
                    }
                }
                Label {
                    anchors.centerIn: parent
                    visible: pairedDevices.count === 0
                    text: "No paired Bluetooth devices."
                    color: Style.muted
                }
            }
        }
        Item { Layout.fillHeight: true }
        }
    }

    Dialog {
        id: forgetDialog
        title: "Forget paired device?"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Cancel | Dialog.Ok
        onAccepted: {
            page.busy = true
            obsidian.forgetBluetoothDevice(page.pendingDevicePath)
        }
        contentItem: Label {
            text: "Remove “" + page.pendingDeviceName + "” and its pairing information?"
            color: Style.foreground
            wrapMode: Text.WordWrap
            width: 340
            padding: 18
        }
    }

    Connections {
        target: obsidian
        function onBluetoothDataChanged(data) {
            page.busy = false
            bluetoothSwitch.checked = data.powered
        }
        function onErrorOccurred(message) {
            page.busy = false
            bluetoothSwitch.checked = obsidian.bluetoothData.powered
        }
    }
}
