import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects
import QtQuick.Layouts
import Stories
import Stories.Native

Rectangle {
    id: root
    signal close()
    signal chooseSpeed(Item anchor)
    signal chooseSleep(Item anchor)
    color: Theme.bg
    clip: true

    readonly property var book: Core.current
    readonly property var chapterInfo: book.chapters?.[Core.chapter]

    Image {
        id: wash
        anchors.fill: parent
        source: root.book.cover ?? ""
        fillMode: Image.PreserveAspectCrop
        sourceSize: Qt.size(96, 96)
        visible: false
    }
    MultiEffect {
        anchors.fill: wash
        source: wash
        visible: wash.status === Image.Ready
        blurEnabled: true
        blur: 1
        blurMax: 48
        saturation: 0.15
        opacity: Theme.dark ? 0.32 : 0.22
    }

    IconButton {
        z: 2
        x: 14; y: 14
        icon_: "down"
        iconSize: 26
        tip: "Close (Esc)"
        onClicked: root.close()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 44
        spacing: 44

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 3
            spacing: 14

            Item { Layout.fillHeight: true }
            Cover {
                readonly property real side: Math.min(parent.width, root.height - 330, 460)
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: Math.max(140, side)
                Layout.preferredHeight: Math.max(140, side)
                source: root.book.cover ?? ""
                title: root.book.title ?? ""
                author: root.book.author ?? ""
            }
            Text {
                Layout.fillWidth: true
                Layout.topMargin: 8
                horizontalAlignment: Text.AlignHCenter
                text: root.book.title ?? ""
                color: Theme.text
                font.pixelSize: 26
                font.weight: Font.Bold
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: [root.book.author, root.chapterInfo?.title].filter(x => x).join("  ·  ")
                color: Theme.secondary
                font.pixelSize: 15
                elide: Text.ElideRight
            }
            Scrubber {
                Layout.fillWidth: true
                Layout.maximumWidth: 620
                Layout.alignment: Qt.AlignHCenter
                position: Core.position
                duration: root.book.durationMs ?? 1
                chapters: root.book.chapters ?? []
                onMoved: ms => Core.seekTo(ms)
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.maximumWidth: 620
                Layout.alignment: Qt.AlignHCenter
                Text { text: Theme.clock(Core.position); color: Theme.muted; font.pixelSize: 13 }
                Item { Layout.fillWidth: true }
                Text {
                    text: Theme.span(((root.book.durationMs ?? 0) - Core.position) / Core.speed) + " left"
                    color: Theme.muted
                    font.pixelSize: 13
                }
                Item { Layout.fillWidth: true }
                Text { text: Theme.clock(root.book.durationMs ?? 0); color: Theme.muted; font.pixelSize: 13 }
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 10
                AbstractButton {
                    id: speedButton
                    implicitWidth: 60
                    implicitHeight: 40
                    hoverEnabled: true
                    background: Rectangle { radius: 20; color: speedButton.hovered ? Theme.raised : "transparent" }
                    contentItem: Text {
                        text: Theme.speedLabel(Core.speed)
                        color: Core.speed !== 1 ? Theme.accent : Theme.secondary
                        font.pixelSize: 15
                        font.weight: Font.Bold
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: root.chooseSpeed(speedButton)
                }
                IconButton { icon_: "previous"; iconSize: 28; onClicked: Core.previousChapter() }
                IconButton { icon_: "back"; iconSize: 28; onClicked: Core.skip(-15000) }
                IconButton { icon_: Core.playing ? "pause" : "play"; iconSize: 36; filled: true; onClicked: Core.toggle() }
                IconButton { icon_: "forward"; iconSize: 28; onClicked: Core.skip(30000) }
                IconButton { icon_: "next"; iconSize: 28; onClicked: Core.nextChapter() }
                IconButton {
                    id: sleepButton
                    icon_: "moon"
                    iconSize: 24
                    tint: Core.sleepLeft > 0 || Core.sleepAtChapter ? Theme.accent : Theme.secondary
                    tip: Core.sleepLeft > 0 ? "Stops in " + Theme.clock(Core.sleepLeft) : "Sleep timer"
                    onClicked: root.chooseSleep(sleepButton)
                }
            }
            Item { Layout.fillHeight: true }
        }

        Rectangle {
            visible: root.width > 920
            Layout.fillHeight: true
            Layout.preferredWidth: 2
            Layout.minimumWidth: 320
            radius: 12
            color: Qt.alpha(Theme.surface, 0.72)

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4
                Text {
                    Layout.margins: 10
                    text: "Chapters"
                    color: Theme.text
                    font.pixelSize: 17
                    font.weight: Font.Bold
                }
                ListView {
                    id: chapterList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.book.chapters ?? []
                    currentIndex: Core.chapter
                    highlightFollowsCurrentItem: false
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar {}
                    onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)
                    delegate: Rectangle {
                        id: row
                        required property var modelData
                        required property int index
                        readonly property bool here: index === Core.chapter
                        width: chapterList.width
                        height: 40
                        radius: 7
                        color: here ? Qt.alpha(Theme.accent, 0.16) : rowHover.hovered ? Theme.raised : "transparent"
                        HoverHandler { id: rowHover }
                        TapHandler { onTapped: Core.seekTo(row.modelData.start) }
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 10
                            Text {
                                Layout.preferredWidth: 22
                                text: row.index + 1
                                color: row.here ? Theme.accent : Theme.muted
                                font.pixelSize: 12
                            }
                            Text {
                                Layout.fillWidth: true
                                text: row.modelData.title
                                color: row.here ? Theme.accent : row.index < Core.chapter ? Theme.muted : Theme.text
                                font.pixelSize: 14
                                font.weight: row.here ? Font.Bold : Font.Medium
                                elide: Text.ElideRight
                            }
                            Text { text: Theme.clock(row.modelData.durationMs); color: Theme.muted; font.pixelSize: 12 }
                        }
                    }
                }
            }
        }
    }
}
