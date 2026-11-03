import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: recorder

    signal recorded(string sequence)
    implicitWidth: 180
    implicitHeight: 36

    function begin() {
        captureDialog.open()
    }

    Dialog {
        id: captureDialog
        title: "Record shortcut"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Cancel
        onOpened: captureField.forceActiveFocus()

        contentItem: ColumnLayout {
            width: 340
            spacing: 10
            Label {
                text: "Press one key combination. The shortcut is saved immediately."
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            TextField {
                id: captureField
                readOnly: true
                placeholderText: "Waiting for a key combination…"
                Layout.fillWidth: true

                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Control
                            || event.key === Qt.Key_Shift
                            || event.key === Qt.Key_Alt
                            || event.key === Qt.Key_Meta) {
                        event.accepted = true
                        return
                    }
                    if (event.key === Qt.Key_Backspace && event.modifiers === Qt.NoModifier) {
                        captureDialog.close()
                        recorder.recorded("")
                        event.accepted = true
                        return
                    }

                    let keyName = ""
                    if (event.key >= Qt.Key_A && event.key <= Qt.Key_Z) {
                        keyName = String.fromCharCode(event.key)
                    } else if (event.key >= Qt.Key_0 && event.key <= Qt.Key_9) {
                        keyName = String.fromCharCode(event.key)
                    } else if (event.key >= Qt.Key_F1 && event.key <= Qt.Key_F35) {
                        keyName = "F" + (event.key - Qt.Key_F1 + 1)
                    } else {
                        const names = {}
                        names[Qt.Key_Space] = "Space"
                        names[Qt.Key_Tab] = "Tab"
                        names[Qt.Key_Return] = "Return"
                        names[Qt.Key_Enter] = "Enter"
                        names[Qt.Key_Escape] = "Esc"
                        names[Qt.Key_Insert] = "Ins"
                        names[Qt.Key_Delete] = "Del"
                        names[Qt.Key_Home] = "Home"
                        names[Qt.Key_End] = "End"
                        names[Qt.Key_PageUp] = "PgUp"
                        names[Qt.Key_PageDown] = "PgDown"
                        names[Qt.Key_Left] = "Left"
                        names[Qt.Key_Right] = "Right"
                        names[Qt.Key_Up] = "Up"
                        names[Qt.Key_Down] = "Down"
                        names[Qt.Key_Plus] = "Plus"
                        names[Qt.Key_Minus] = "Minus"
                        names[Qt.Key_Comma] = "Comma"
                        names[Qt.Key_Period] = "Period"
                        names[Qt.Key_Slash] = "Slash"
                        names[Qt.Key_Backslash] = "Backslash"
                        names[Qt.Key_BracketLeft] = "BracketLeft"
                        names[Qt.Key_BracketRight] = "BracketRight"
                        names[Qt.Key_Apostrophe] = "Apostrophe"
                        names[Qt.Key_Semicolon] = "Semicolon"
                        names[Qt.Key_Equal] = "Equal"
                        names[Qt.Key_QuoteLeft] = "QuoteLeft"
                        names[Qt.Key_Asterisk] = "Asterisk"
                        names[Qt.Key_Question] = "Question"
                        names[Qt.Key_Exclam] = "Exclam"
                        keyName = names[event.key] || ""
                    }

                    if (!keyName) {
                        event.accepted = true
                        return
                    }
                    const parts = []
                    if (event.modifiers & Qt.ControlModifier) parts.push("Ctrl")
                    if (event.modifiers & Qt.AltModifier) parts.push("Alt")
                    if (event.modifiers & Qt.ShiftModifier) parts.push("Shift")
                    if (event.modifiers & Qt.MetaModifier) parts.push("Meta")
                    parts.push(keyName)
                    const sequence = parts.join("+")
                    captureField.text = sequence
                    captureDialog.close()
                    recorder.recorded(sequence)
                    event.accepted = true
                }
            }
            Label {
                text: "Press Backspace without modifiers to clear the shortcut."
                color: "#a6aab5"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }
    }
}
