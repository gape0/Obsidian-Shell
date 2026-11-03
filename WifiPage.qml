import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "ObsidianStyle.js" as Style

Item {
    id: page
    property bool busy: false
    property string pendingAccessPoint: ""
    property string pendingConnection: ""
    property string pendingSsid: ""

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
                        text: "Wi-Fi"
                        color: Style.foreground
                        font.pixelSize: 19
                        font.weight: Font.Medium
                    }
                    Label {
                        text: obsidian.wifiData.hasAdapter
                            ? (obsidian.wifiData.enabled
                                ? "Connect to nearby networks and manage saved ones."
                                : "Wi-Fi is currently turned off.")
                            : "No Wi-Fi adapter is available."
                        color: Style.muted
                        wrapMode: Text.WordWrap
                    }
                }
                Switch {
                    id: wifiSwitch
                    text: "On"
                    checked: obsidian.wifiData.enabled
                    enabled: obsidian.wifiData.hasAdapter
                        && obsidian.wifiData.hardwareEnabled && !page.busy
                    onClicked: {
                        page.busy = true
                        obsidian.setWifiEnabled(checked)
                    }
                }
                Button {
                    text: page.busy ? "Working…" : "Scan"
                    enabled: obsidian.wifiData.enabled
                        && obsidian.wifiData.hasAdapter && !page.busy
                    onClicked: {
                        page.busy = true
                        obsidian.scanWifi()
                    }
                }
            }
            Label {
                visible: !obsidian.wifiData.hardwareEnabled && obsidian.wifiData.hasAdapter
                text: "Wi-Fi is disabled by the hardware switch."
                color: Style.muted
            }
        }

        SettingsCard {
            Label {
                text: "Connected network"
                color: Style.foreground
                font.pixelSize: 16
                font.weight: Font.Medium
            }
            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: obsidian.wifiData.connectedSsid || "Not connected"
                    color: Style.muted
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Button {
                    text: "Disconnect"
                    enabled: !!obsidian.wifiData.connectedSsid && !page.busy
                    onClicked: {
                        page.busy = true
                        obsidian.disconnectWifi()
                    }
                }
            }
        }

        SettingsCard {
            Label {
                text: "Available networks"
                color: Style.foreground
                font.pixelSize: 16
                font.weight: Font.Medium
            }
            ListView {
                id: availableNetworks
                model: obsidian.wifiData.available
                clip: true
                spacing: 4
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 230)
                ScrollBar.vertical: ScrollBar {}
                delegate: Rectangle {
                    required property string accessPointPath
                    required property string ssid
                    required property string security
                    required property int strength
                    required property bool saved
                    required property bool connected
                    required property bool requiresPassword
                    required property bool connectable
                    width: ListView.view.width
                    height: 56
                    radius: 6
                    color: Style.raised
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 8
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label {
                                text: ssid
                                color: Style.foreground
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Label {
                                text: security + " · " + strength + "%"
                                    + (saved ? " · Saved" : "")
                                    + (connected ? " · Connected" : "")
                                color: Style.muted
                                font.pixelSize: 11
                            }
                        }
                        Button {
                            text: connected ? "Connected" : (connectable ? "Connect" : "Unsupported")
                            enabled: !connected && connectable && !page.busy
                            onClicked: {
                                if (requiresPassword && !saved) {
                                    page.pendingAccessPoint = accessPointPath
                                    page.pendingSsid = ssid
                                    passwordDialog.open()
                                } else {
                                    page.busy = true
                                    obsidian.connectWifi(accessPointPath, "")
                                }
                            }
                        }
                    }
                }
                Label {
                    anchors.centerIn: parent
                    visible: availableNetworks.count === 0
                    text: obsidian.wifiData.enabled ? "No networks found. Scan to check again."
                                                    : "Turn Wi-Fi on to see nearby networks."
                    color: Style.muted
                }
            }
        }

        SettingsCard {
            Label {
                text: "Saved networks"
                color: Style.foreground
                font.pixelSize: 16
                font.weight: Font.Medium
            }
            ListView {
                id: savedNetworks
                model: obsidian.wifiData.saved
                clip: true
                spacing: 4
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 180)
                ScrollBar.vertical: ScrollBar {}
                delegate: Rectangle {
                    required property string path
                    required property string name
                    required property bool connected
                    width: ListView.view.width
                    height: 48
                    radius: 6
                    color: Style.raised
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 8
                        Label {
                            text: name + (connected ? " · Connected" : "")
                            color: Style.foreground
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Button {
                            text: "Connect"
                            enabled: !connected && !page.busy
                            onClicked: {
                                page.busy = true
                                obsidian.connectSavedWifi(path)
                            }
                        }
                        Button {
                            text: "Forget"
                            enabled: !page.busy
                            onClicked: {
                                page.pendingConnection = path
                                page.pendingSsid = name
                                forgetDialog.open()
                            }
                        }
                    }
                }
                Label {
                    anchors.centerIn: parent
                    visible: savedNetworks.count === 0
                    text: "No saved Wi-Fi networks."
                    color: Style.muted
                }
            }
        }
    }
    }

    Dialog {
        id: passwordDialog
        title: "Connect to " + page.pendingSsid
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Cancel | Dialog.Ok
        onAccepted: {
            page.busy = true
            obsidian.connectWifi(page.pendingAccessPoint, passwordField.text)
            passwordField.clear()
        }
        onRejected: passwordField.clear()
        contentItem: ColumnLayout {
            width: 340
            spacing: 10
            Label {
                text: "Enter the Wi-Fi password. NetworkManager will store the connection profile."
                color: Style.foreground
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            TextField {
                id: passwordField
                echoMode: TextInput.Password
                placeholderText: "Password"
                Layout.fillWidth: true
            }
        }
    }

    Dialog {
        id: forgetDialog
        title: "Forget saved network?"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Cancel | Dialog.Ok
        onAccepted: {
            page.busy = true
            obsidian.forgetSavedWifi(page.pendingConnection)
        }
        contentItem: Label {
            text: "Remove “" + page.pendingSsid + "” from NetworkManager's saved profiles?"
            color: Style.foreground
            wrapMode: Text.WordWrap
            width: 340
            padding: 18
        }
    }

    Connections {
        target: obsidian
        function onWifiDataChanged(data) {
            page.busy = false
            wifiSwitch.checked = data.enabled
        }
        function onErrorOccurred(message) {
            page.busy = false
            wifiSwitch.checked = obsidian.wifiData.enabled
        }
    }
}
