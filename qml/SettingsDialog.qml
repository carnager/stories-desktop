import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Stories
import Stories.Native

Popup {
    id: root
    signal addPhone()
    anchors.centerIn: Overlay.overlay
    width: Math.min(500, (parent?.width ?? 500) - 40)
    modal: true
    focus: true
    padding: 24
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.5) }
    background: Rectangle { radius: 14; color: Theme.surface; border.width: 1; border.color: Theme.line }
    onOpened: folder.text = Core.localLibrary

    contentItem: ColumnLayout {
        spacing: 8
        Text { text: "Settings"; color: Theme.text; font.pixelSize: 22; font.weight: Font.Bold; Layout.bottomMargin: 8 }

        Text { text: "Server"; color: Theme.secondary; font.pixelSize: 13; font.weight: Font.Bold }
        Text { text: Core.serverUrl; color: Theme.text; font.pixelSize: 15 }
        RowLayout {
            Layout.topMargin: 4
            spacing: 8
            Pill { icon_: "phone"; text: "Add phone"; onClicked: { root.close(); root.addPhone() } }
            Pill { text: "Sign out"; onClicked: { root.close(); Core.logout() } }
        }

        Text {
            Layout.topMargin: 16
            text: "Local copy of the library"
            color: Theme.secondary
            font.pixelSize: 13
            font.weight: Font.Bold
        }
        Text {
            Layout.fillWidth: true
            text: "Files found here with the same path and size play directly instead of streaming."
            color: Theme.muted
            font.pixelSize: 13
            wrapMode: Text.Wrap
        }
        TextField {
            id: folder
            Layout.fillWidth: true
            implicitHeight: 40
            placeholderText: "Optional, e.g. /home/you/audiobooks"
            placeholderTextColor: Theme.muted
            color: Theme.text
            selectionColor: Theme.accent
            selectedTextColor: Theme.onAccent
            font.pixelSize: 14
            background: Rectangle {
                radius: 8
                color: Theme.bg
                border.width: folder.activeFocus ? 2 : 1
                border.color: folder.activeFocus ? Theme.accent : Theme.line
            }
            onEditingFinished: Core.localLibrary = text
        }
        RowLayout {
            Layout.topMargin: 14
            Item { Layout.fillWidth: true }
            Pill { primary: true; text: "Done"; onClicked: { Core.localLibrary = folder.text; root.close() } }
        }
    }
}
