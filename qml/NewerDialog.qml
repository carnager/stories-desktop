import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Stories
import Stories.Native

Popup {
    id: root
    readonly property var place: Core.newer
    anchors.centerIn: Overlay.overlay
    width: 430
    modal: true
    padding: 24
    visible: place.title !== undefined
    closePolicy: Popup.NoAutoClose
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.5) }
    background: Rectangle { radius: 14; color: Theme.surface; border.width: 1; border.color: Theme.line }

    contentItem: ColumnLayout {
        spacing: 12
        Text { text: "Newer position on another device"; color: Theme.text; font.pixelSize: 19; font.weight: Font.Bold; Layout.fillWidth: true; wrapMode: Text.Wrap }
        Text {
            Layout.fillWidth: true
            text: {
                const where = root.place.finished ? "finished"
                    : [root.place.chapter, Theme.clock(root.place.bookMs ?? 0)].filter(x => x).join(" · ")
                return `“${root.place.title ?? ""}” was last heard at ${where}. You're at ${Theme.clock(Core.position)}.`
            }
            color: Theme.secondary
            font.pixelSize: 14
            wrapMode: Text.Wrap
        }
        RowLayout {
            Layout.topMargin: 6
            Item { Layout.fillWidth: true }
            Pill { text: "Stay here"; onClicked: Core.declineNewer() }
            Pill { primary: true; text: "Go there"; onClicked: Core.acceptNewer() }
        }
    }
}
