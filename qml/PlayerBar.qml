import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Stories
import Stories.Native

Rectangle {
    id: root
    signal expand()
    signal chooseSpeed(Item anchor)
    signal chooseSleep(Item anchor)

    readonly property var book: Core.current
    readonly property var chapterInfo: book.chapters?.[Core.chapter]

    implicitHeight: book.id ? 84 : 0
    visible: book.id !== undefined
    color: Theme.surface

    Rectangle { width: parent.width; height: 1; color: Theme.line }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 14

        Item {
            Layout.preferredWidth: 280
            Layout.minimumWidth: 150
            Layout.fillHeight: true
            HoverHandler { cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: root.expand() }
            Cover {
                id: art
                anchors.verticalCenter: parent.verticalCenter
                width: 60; height: 60
                source: root.book.cover ?? ""
                title: root.book.title ?? ""
                author: root.book.author ?? ""
            }
            Column {
                anchors { left: art.right; right: parent.right; leftMargin: 12; verticalCenter: parent.verticalCenter }
                spacing: 2
                Text { width: parent.width; text: root.book.title ?? ""; color: Theme.text; font.pixelSize: 14; font.weight: Font.Bold; elide: Text.ElideRight }
                Text { width: parent.width; text: root.chapterInfo?.title ?? ""; color: Theme.secondary; font.pixelSize: 12; elide: Text.ElideRight }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 0
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 4
                IconButton { icon_: "previous"; tip: "Previous chapter"; onClicked: Core.previousChapter() }
                IconButton { icon_: "back"; tip: "Back 15 seconds"; onClicked: Core.skip(-15000) }
                IconButton {
                    icon_: Core.playing ? "pause" : "play"
                    iconSize: 26
                    filled: true
                    tip: Core.buffering ? "Buffering…" : ""
                    onClicked: Core.toggle()
                }
                IconButton { icon_: "forward"; tip: "Forward 30 seconds"; onClicked: Core.skip(30000) }
                IconButton { icon_: "next"; tip: "Next chapter"; onClicked: Core.nextChapter() }
            }
            RowLayout {
                spacing: 10
                Text { text: Theme.clock(Core.position); color: Theme.muted; font.pixelSize: 12; font.features: { "tnum": 1 } }
                Scrubber {
                    Layout.fillWidth: true
                    position: Core.position
                    duration: root.book.durationMs ?? 1
                    chapters: root.book.chapters ?? []
                    onMoved: ms => Core.seekTo(ms)
                }
                Text { text: "-" + Theme.clock((root.book.durationMs ?? 0) - Core.position); color: Theme.muted; font.pixelSize: 12; font.features: { "tnum": 1 } }
            }
        }

        RowLayout {
            Layout.preferredWidth: 220
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            spacing: 2
            Item { Layout.fillWidth: true }
            AbstractButton {
                id: speedButton
                implicitWidth: 56
                implicitHeight: 34
                hoverEnabled: true
                background: Rectangle { radius: 17; color: speedButton.hovered ? Theme.raised : "transparent" }
                contentItem: Text {
                    text: Theme.speedLabel(Core.speed)
                    color: Core.speed !== 1 ? Theme.accent : Theme.secondary
                    font.pixelSize: 13
                    font.weight: Font.Bold
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.chooseSpeed(speedButton)
            }
            IconButton {
                id: sleepButton
                icon_: "moon"
                iconSize: 20
                tint: Core.sleepLeft > 0 || Core.sleepAtChapter ? Theme.accent : Theme.secondary
                tip: Core.sleepAtChapter ? "Stops at the chapter's end"
                    : Core.sleepLeft > 0 ? "Stops in " + Theme.clock(Core.sleepLeft) : "Sleep timer"
                onClicked: root.chooseSleep(sleepButton)
            }
            IconButton { icon_: "bookmark"; iconSize: 20; tint: Theme.secondary; tip: "Bookmark this place"; onClicked: Core.addBookmark() }
            IconButton { icon_: "list"; iconSize: 20; tint: Theme.secondary; tip: "Now playing"; onClicked: root.expand() }
        }
    }
}
