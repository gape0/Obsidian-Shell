import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "InputSettings.js" as InputSettings
import "ObsidianStyle.js" as Style

Item {
    id: page
    signal errorMessage(string message)
    property var availableLayouts: []
    property var scrollFactors: [0.1, 0.3, 0.5, 0.75, 1, 1.5, 2, 3, 4, 5, 7, 9, 12, 15, 20]

    function configuredLayouts() {
        let result = []
        for (let i = 0; i < layoutsModel.count; ++i)
            result.push(layoutsModel.get(i))
        return result
    }

    function applyDeviceValue(sysName, key, value) {
        let change = {}
        change[key] = value
        obsidian.setInputDeviceProperties(sysName, JSON.stringify(change))
    }

    function updateData(data) {
        let prepared = InputSettings.prepareData(data)
        page.availableLayouts = prepared.availableKeyboardLayouts
        layoutsModel.clear()
        for (let i = 0; i < prepared.keyboardLayouts.length; ++i)
            layoutsModel.append(prepared.keyboardLayouts[i])
        miceModel.clear()
        for (let j = 0; j < prepared.mouseDevices.length; ++j)
            miceModel.append(prepared.mouseDevices[j])
        touchpadsModel.clear()
        for (let k = 0; k < prepared.touchpadDevices.length; ++k)
            touchpadsModel.append(prepared.touchpadDevices[k])
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
                text: "Keyboard layouts"
                color: Style.foreground
                font.pixelSize: 19
                font.weight: Font.Medium
            }
            Label {
                text: "Manage the XKB layouts enabled for your KDE Plasma session."
                color: Style.muted
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                ComboBox {
                    id: layoutPicker
                    model: page.availableLayouts
                    textRole: "label"
                    Layout.fillWidth: true
                }
                Button {
                    text: "Add"
                    enabled: layoutPicker.currentIndex >= 0
                    onClicked: {
                        try {
                            let next = InputSettings.addKeyboardLayout(
                                page.configuredLayouts(),
                                page.availableLayouts[layoutPicker.currentIndex]
                            )
                            obsidian.saveKeyboardLayouts(JSON.stringify(next))
                        } catch (error) {
                            page.errorMessage(error.message)
                        }
                    }
                }
                Button {
                    text: "Refresh"
                    onClicked: obsidian.refreshInputSettings()
                }
            }
            ListView {
                id: layoutList
                model: layoutsModel
                clip: true
                spacing: 4
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 150)
                delegate: Rectangle {
                    required property string label
                    width: ListView.view.width
                    height: 40
                    radius: 6
                    color: Style.raised
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 8
                        Label {
                            text: label
                            color: Style.foreground
                            Layout.fillWidth: true
                        }
                        Button {
                            text: "Remove"
                            onClicked: {
                                try {
                                    let next = InputSettings.removeKeyboardLayout(
                                        page.configuredLayouts(), index
                                    )
                                    obsidian.saveKeyboardLayouts(JSON.stringify(next))
                                } catch (error) {
                                    page.errorMessage(error.message)
                                }
                            }
                        }
                    }
                }
                Label {
                    anchors.centerIn: parent
                    visible: layoutList.count === 0
                    text: "No keyboard layouts configured."
                    color: Style.muted
                }
            }
        }

        SettingsCard {
            Label {
                text: "Mouse"
                color: Style.foreground
                font.pixelSize: 16
                font.weight: Font.Medium
            }
            Repeater {
                model: miceModel
                delegate: ColumnLayout {
                    required property string sysName
                    required property string name
                    required property real pointerAcceleration
                    required property bool supportsPointerAcceleration
                    required property bool supportsAdaptiveAcceleration
                    required property bool supportsFlatAcceleration
                    required property bool adaptiveAcceleration
                    Layout.fillWidth: true
                    Label { text: name; color: Style.muted; font.weight: Font.Medium }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Pointer speed"; color: Style.foreground }
                        Slider {
                            from: -1
                            to: 1
                            stepSize: 0.05
                            value: pointerAcceleration
                            enabled: supportsPointerAcceleration
                            Layout.fillWidth: true
                            onMoved: page.applyDeviceValue(sysName, "pointerAcceleration", value)
                        }
                    }
                    RowLayout {
                        visible: supportsAdaptiveAcceleration || supportsFlatAcceleration
                        Label { text: "Acceleration"; color: Style.foreground; Layout.fillWidth: true }
                        RadioButton {
                            text: "Adaptive"
                            checked: adaptiveAcceleration
                            enabled: supportsAdaptiveAcceleration
                            onClicked: page.applyDeviceValue(sysName, "pointerAccelerationProfileAdaptive", true)
                        }
                        RadioButton {
                            text: "Flat"
                            checked: !adaptiveAcceleration
                            enabled: supportsFlatAcceleration
                            onClicked: page.applyDeviceValue(sysName, "pointerAccelerationProfileFlat", true)
                        }
                    }
                }
            }
            Label {
                visible: miceModel.count === 0
                text: "No configurable mouse devices detected."
                color: Style.muted
            }
        }

        SettingsCard {
            Label {
                text: "Touchpad"
                color: Style.foreground
                font.pixelSize: 16
                font.weight: Font.Medium
            }
            Repeater {
                model: touchpadsModel
                delegate: ColumnLayout {
                    required property string sysName
                    required property string name
                    required property real pointerAcceleration
                    required property bool supportsPointerAcceleration
                    required property bool tapToClick
                    required property int tapFingerCount
                    required property bool supportsNaturalScroll
                    required property bool naturalScroll
                    required property real scrollFactor
                    Layout.fillWidth: true
                    Label { text: name; color: Style.muted; font.weight: Font.Medium }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Pointer speed"; color: Style.foreground }
                        Slider {
                            from: -1
                            to: 1
                            stepSize: 0.05
                            value: pointerAcceleration
                            enabled: supportsPointerAcceleration
                            Layout.fillWidth: true
                            onMoved: page.applyDeviceValue(sysName, "pointerAcceleration", value)
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "Scroll speed"; color: Style.foreground; Layout.fillWidth: true }
                        ComboBox {
                            model: page.scrollFactors
                            currentIndex: Math.max(0, page.scrollFactors.indexOf(scrollFactor))
                            onActivated: page.applyDeviceValue(sysName, "scrollFactor", page.scrollFactors[currentIndex])
                        }
                    }
                    CheckBox {
                        text: "Tap to click"
                        checked: tapToClick
                        enabled: tapFingerCount > 0
                        onClicked: page.applyDeviceValue(sysName, "tapToClick", checked)
                    }
                    CheckBox {
                        text: "Natural scrolling"
                        checked: naturalScroll
                        enabled: supportsNaturalScroll
                        onClicked: page.applyDeviceValue(sysName, "naturalScroll", checked)
                    }
                }
            }
            Label {
                visible: touchpadsModel.count === 0
                text: "No configurable touchpads detected."
                color: Style.muted
            }
        }
        Item { Layout.fillHeight: true }
        }
    }

    ListModel { id: layoutsModel }
    ListModel { id: miceModel }
    ListModel { id: touchpadsModel }

    Connections {
        target: obsidian
        function onInputSettingsDataChanged(data) {
            page.updateData(data)
        }
    }
}
