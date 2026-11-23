import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Stories
import Stories.Native

Popup {
    id: root
    property string joined: ""
    readonly property string link: "stories://connect?url=" + encodeURIComponent(Core.shareUrl)

    anchors.centerIn: Overlay.overlay
    width: 400
    modal: true
    focus: true
    padding: 24
    Overlay.modal: Rectangle { color: Qt.rgba(0, 0, 0, 0.5) }
    background: Rectangle { radius: 14; color: Theme.surface; border.width: 1; border.color: Theme.line }
    onOpened: joined = ""

    Connections {
        target: Core
        function onDeviceSignedIn(device) {
            if (root.opened)
                root.joined = device
        }
    }

    contentItem: ColumnLayout {
        spacing: 12
        Text { text: "Add phone"; color: Theme.text; font.pixelSize: 22; font.weight: Font.Bold }
        Text {
            Layout.fillWidth: true
            text: root.joined ? `“${root.joined}” is connected.`
                : Core.shareUrl ? "Scan this with your phone's camera, then enter the password in Stories."
                : "This server has no address a phone can reach. Set public_url in its config."
            color: root.joined ? Theme.done : Theme.secondary
            font.pixelSize: 14
            wrapMode: Text.Wrap
        }
        Image {
            visible: !root.joined && Core.shareUrl.length > 0
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 260
            Layout.preferredHeight: 260
            source: Core.shareUrl ? "image://qr/" + encodeURIComponent(root.link) : ""
            smooth: false
            fillMode: Image.PreserveAspectFit
        }
        Text {
            visible: Core.shareUrl.length > 0
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: "Or enter this address: " + Core.shareUrl
            color: Theme.muted
            font.pixelSize: 13
            wrapMode: Text.WrapAnywhere
        }
        RowLayout {
            Item { Layout.fillWidth: true }
            Pill { primary: true; text: root.joined ? "Done" : "Close"; onClicked: root.close() }
        }
    }
}
