import QtQuick
import QtQuick.Controls.Basic
import Stories
import Stories.Native

Item {
    id: root
    required property string bookId
    required property string title
    required property string author
    required property string cover
    required property string status
    required property real fraction
    required property string line
    signal open(string id)

    readonly property bool isCurrent: Core.current.id === bookId

    HoverHandler { id: hover }
    TapHandler { onTapped: root.open(root.bookId) }

    Column {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 7

        Item {
            width: parent.width
            height: width
            scale: hover.hovered ? 1.025 : 1
            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }

            Cover {
                anchors.fill: parent
                source: root.cover
                title: root.title
                author: root.author
                opacity: root.status === "finished" ? 0.55 : 1
            }
            Rectangle {
                anchors.fill: parent
                color: "transparent"
                border.width: root.isCurrent ? 2 : 0
                border.color: Theme.accent
            }
            Rectangle {
                visible: root.status === "listening"
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 7 }
                height: 4
                radius: 2
                color: Qt.rgba(0, 0, 0, 0.5)
                Rectangle {
                    width: parent.width * Math.max(0.03, root.fraction)
                    height: parent.height
                    radius: 2
                    color: Theme.accent
                }
            }
            Rectangle {
                visible: root.status === "finished"
                anchors { right: parent.right; top: parent.top; margins: 7 }
                width: 22; height: 22; radius: 11
                color: Theme.done
                Icon { anchors.centerIn: parent; name: "check"; size: 15; color: "#10180a" }
            }
            IconButton {
                visible: hover.hovered
                anchors.centerIn: parent
                icon_: root.isCurrent && Core.playing ? "pause" : "play"
                iconSize: 26
                filled: true
                onClicked: root.isCurrent ? Core.toggle() : Core.play(root.bookId)
            }
        }
        Text {
            width: parent.width
            text: root.title
            color: root.status === "finished" ? Theme.secondary : Theme.text
            font.pixelSize: 14
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            text: root.author
            color: Theme.muted
            font.pixelSize: 12
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            text: root.line
            color: root.status === "new" ? Theme.accent : root.status === "finished" ? Theme.done : Theme.secondary
            font.pixelSize: 12
            elide: Text.ElideRight
        }
    }
}
