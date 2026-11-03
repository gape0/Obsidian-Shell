import QtQuick
import QtQuick.Layouts
import "ObsidianStyle.js" as Style

Item {
    id: card
    default property alias contentData: content.data
    implicitHeight: content.implicitHeight + 32
    Layout.fillWidth: true

    Rectangle {
        anchors.fill: parent
        radius: 12
        color: Style.surface
    }

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10
    }
}
